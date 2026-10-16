#include "preview_view.h"
#include <shellapi.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cwctype>

#include "../markdown/slug.h"
#include "../platform/clipboard.h"

namespace Pluma::Preview {

namespace {

constexpr float kLineScrollDip = 40.0f;       // Arrow keys / scrollbar arrows
constexpr float kWheelLineDip = 24.0f;        // One wheel "line"
constexpr UINT_PTR kAutoScrollTimer = 1;      // Selection dragged above or below the view
constexpr UINT kAutoScrollIntervalMs = 30;

std::wstring Utf8ToUtf16(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), count);
    return result;
}

bool StartsWithIgnoreCase(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(text[i])) != prefix[i]) return false;
    }
    return true;
}

std::wstring LowerExtension(const std::filesystem::path& path) {
    std::wstring ext = path.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return ext;
}

bool IsMarkdownExtension(const std::wstring& ext) {
    return ext == L".md" || ext == L".markdown" || ext == L".mdown" || ext == L".mkd" || ext == L".txt";
}

// Local files that are safe to hand to their default viewer (never executables or scripts).
bool IsSafeToOpenExtension(const std::wstring& ext) {
    static constexpr std::array<const wchar_t*, 14> kSafe = {
        L".png", L".jpg", L".jpeg", L".gif", L".bmp", L".webp", L".svg", L".ico",
        L".pdf", L".html", L".htm", L".csv", L".json", L".xml"};
    return std::any_of(kSafe.begin(), kSafe.end(), [&ext](const wchar_t* e) { return ext == e; });
}

} // namespace

PreviewView::PreviewView() = default;

PreviewView::~PreviewView() {
    DiscardDirect2DResources();
    if (m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
    }
}

bool PreviewView::Create(HWND hParent, HINSTANCE hInstance, int x, int y, int width, int height) {
    m_hParent = hParent;
    m_hInstance = hInstance;

    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_DBLCLKS;
    wcex.lpfnWndProc = StaticWndProc;
    wcex.hInstance = m_hInstance;
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.lpszClassName = kPreviewViewClassName;
    RegisterClassExW(&wcex);

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory),
        reinterpret_cast<void**>(m_d2dFactory.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    if (FAILED(hr)) return false;

    if (!m_layout.Initialize(m_dwriteFactory.Get())) return false;

    m_hHandCursor = LoadCursorW(nullptr, IDC_HAND);
    m_hArrowCursor = LoadCursorW(nullptr, IDC_ARROW);
    m_hTextCursor = LoadCursorW(nullptr, IDC_IBEAM);

    m_hwnd = CreateWindowExW(
        0,
        kPreviewViewClassName,
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN | WS_TABSTOP,
        x, y, width, height,
        m_hParent, nullptr, m_hInstance, this);
    if (!m_hwnd) {
        return false;
    }

    m_dpi = GetDpiForWindow(m_hwnd);
    if (m_dpi == 0) m_dpi = 96;
    SetWindowTheme(m_hwnd, m_isDarkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    OnResize(rc.right - rc.left, rc.bottom - rc.top);
    return true;
}

LRESULT CALLBACK PreviewView::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PreviewView* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<PreviewView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            self->m_hwnd = hwnd;
        }
    } else {
        self = reinterpret_cast<PreviewView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT PreviewView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
        Render();
        return 0;

    case WM_ERASEBKGND:
        return 1; // Prevent flicker; Direct2D clears background

    case WM_SIZE:
        OnResize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_VSCROLL: {
        float newPos = m_scrollPos;
        switch (LOWORD(wParam)) {
        case SB_LINEUP:        newPos -= kLineScrollDip; break;
        case SB_LINEDOWN:      newPos += kLineScrollDip; break;
        case SB_PAGEUP:        newPos -= m_viewHeight * 0.9f; break;
        case SB_PAGEDOWN:      newPos += m_viewHeight * 0.9f; break;
        case SB_TOP:           newPos = 0.0f; break;
        case SB_BOTTOM:        newPos = m_maxScroll; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;
            GetScrollInfo(m_hwnd, SB_VERT, &si);
            newPos = static_cast<float>(si.nTrackPos);
            break;
        }
        default: break;
        }
        ScrollTo(newPos);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        // Keep sub-notch precision so high-resolution touchpads scroll smoothly.
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        UINT scrollLines = 3;
        SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
        float amount = 0.0f;
        if (scrollLines == WHEEL_PAGESCROLL) {
            amount = m_viewHeight * static_cast<float>(delta) / WHEEL_DELTA;
        } else {
            amount = kWheelLineDip * static_cast<float>(scrollLines) * static_cast<float>(delta) / WHEEL_DELTA;
        }
        ScrollTo(m_scrollPos - amount);
        return 0;
    }

    case WM_KEYDOWN: {
        if (GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_MENU) >= 0) { // Not AltGr
            if (wParam == 'A') {
                SelectAll();
                return 0;
            }
            if (wParam == 'C' || wParam == VK_INSERT) {
                CopySelection();
                return 0;
            }
        }
        if (wParam == VK_ESCAPE && HasSelection()) {
            ClearSelection();
            return 0;
        }
        switch (wParam) {
        case VK_UP:    ScrollTo(m_scrollPos - kLineScrollDip); return 0;
        case VK_DOWN:  ScrollTo(m_scrollPos + kLineScrollDip); return 0;
        case VK_PRIOR: ScrollTo(m_scrollPos - m_viewHeight * 0.9f); return 0;
        case VK_NEXT:
        case VK_SPACE: ScrollTo(m_scrollPos + m_viewHeight * 0.9f); return 0;
        case VK_HOME:  ScrollTo(0.0f); return 0;
        case VK_END:   ScrollTo(m_maxScroll); return 0;
        default: break;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        if (!m_trackingMouse) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, m_hwnd, 0};
            m_trackingMouse = TrackMouseEvent(&tme) != FALSE;
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (m_mouseDown) {
            if (!m_dragging && (std::abs(x - m_downPoint.x) > GetSystemMetrics(SM_CXDRAG) ||
                                std::abs(y - m_downPoint.y) > GetSystemMetrics(SM_CYDRAG))) {
                m_dragging = true;
                m_pressedUrl.clear(); // A drag that starts on a link selects instead of opening it
                SetCursor(m_hTextCursor); // No WM_SETCURSOR while the mouse is captured
            }
            if (m_dragging) {
                ExtendSelection(PxToDip(static_cast<float>(x)), PxToDip(static_cast<float>(y)));
                UpdateAutoScroll(y);
            }
            return 0;
        }
        UpdateHover(PxToDip(static_cast<float>(x)), PxToDip(static_cast<float>(y)));
        return 0;
    }

    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        if (!m_mouseDown) SetHoverLink(nullptr);
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(CursorForHover());
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN: {
        SetFocus(m_hwnd);
        const float x = PxToDip(static_cast<float>(GET_X_LPARAM(lParam)));
        const float y = PxToDip(static_cast<float>(GET_Y_LPARAM(lParam)));
        // A click can arrive without a previous WM_MOUSEMOVE (pen, touch, inactive window).
        UpdateHover(x, y);
        m_pressedUrl.clear();
        m_pressedTaskOffset = -1;
        if (m_hoverTask) {
            m_pressedTaskOffset = m_hoverTask->taskMarkOffset;
            return 0;
        }
        if (wParam & MK_CONTROL) {
            EditAt(x, y, false); // Ctrl+click edits, even on a link
            return 0;
        }
        m_pressedUrl = m_hoverUrl;
        const bool triple = IsTripleClick(lParam);
        BeginSelection(x, y, triple ? SelectMode::Unit : SelectMode::Character, (wParam & MK_SHIFT) != 0);
        m_downPoint = POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        StartMouseSelection();
        m_dragging = triple || (wParam & MK_SHIFT) != 0;
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        const float x = PxToDip(static_cast<float>(GET_X_LPARAM(lParam)));
        const float y = PxToDip(static_cast<float>(GET_Y_LPARAM(lParam)));
        UpdateHover(x, y);
        // The second click of a double-click on a checkbox toggles it again, like a single click.
        if (m_hoverTask) {
            m_pressedTaskOffset = m_hoverTask->taskMarkOffset;
            return 0;
        }
        if (wParam & MK_CONTROL) {
            return 0; // The first click already went to the editor
        }
        m_lastDoubleClickTime = GetMessageTime();
        m_lastDoubleClickPoint = POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        m_pressedUrl.clear();
        BeginSelection(x, y, SelectMode::Word, false);
        StartMouseSelection();
        m_dragging = true; // Dragging now extends word by word
        return 0;
    }

    case WM_LBUTTONUP: {
        const bool dragged = m_dragging;
        EndMouseSelection();
        UpdateHover(PxToDip(static_cast<float>(GET_X_LPARAM(lParam))),
                    PxToDip(static_cast<float>(GET_Y_LPARAM(lParam))));
        // Only follow a link (or toggle a checkbox) when press and release happen on it.
        const int taskOffset = m_pressedTaskOffset;
        m_pressedTaskOffset = -1;
        if (taskOffset >= 0 && m_hoverTask && m_hoverTask->taskMarkOffset == taskOffset) {
            ToggleTask(taskOffset);
            return 0;
        }
        std::string url;
        url.swap(m_pressedUrl);
        if (!dragged && !url.empty() && url == m_hoverUrl) {
            OpenLink(url);
            return 0;
        }
        break;
    }

    case WM_CAPTURECHANGED:
        if (reinterpret_cast<HWND>(lParam) != m_hwnd) {
            EndMouseSelection();
        }
        return 0;

    case WM_TIMER:
        if (wParam == kAutoScrollTimer) {
            AutoScrollStep();
            return 0;
        }
        break;

    case WM_CONTEXTMENU: {
        const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ShowContextMenu(pt, pt.x == -1 && pt.y == -1); // (-1, -1): Shift+F10 or the menu key
        return 0;
    }

    case WM_DESTROY:
        DiscardDirect2DResources();
        return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

bool PreviewView::EnsureDirect2DResources() {
    if (m_renderTarget) return true;
    if (!m_d2dFactory || !m_hwnd) return false;

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    const D2D1_SIZE_U size = D2D1::SizeU(
        (std::max)(1u, static_cast<UINT32>(rc.right - rc.left)),
        (std::max)(1u, static_cast<UINT32>(rc.bottom - rc.top)));

    // Explicit DPI: layout works in DIPs and Direct2D scales to physical pixels (Per-Monitor v2).
    const float dpi = static_cast<float>(m_dpi);
    const D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), dpi, dpi);
    const D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(m_hwnd, size);

    if (FAILED(m_d2dFactory->CreateHwndRenderTarget(rtProps, hwndProps, &m_renderTarget))) {
        return false;
    }
    if (!m_renderer.CreateResources(m_renderTarget.Get(), m_colors)) {
        m_renderTarget.Reset();
        return false;
    }
    m_effectsApplied = false;
    return true;
}

void PreviewView::DiscardDirect2DResources() {
    m_renderer.DiscardResources();
    m_renderTarget.Reset();
    m_effectsApplied = false;
}

void PreviewView::Render() {
    PAINTSTRUCT ps{};
    BeginPaint(m_hwnd, &ps);

    if (EnsureDirect2DResources()) {
        if (!m_effectsApplied) {
            m_renderer.ApplyTextEffects(m_layout);
            m_effectsApplied = true;
        }

        m_renderTarget->BeginDraw();
        m_renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
        m_renderTarget->Clear(m_colors.background);
        const float scrollY = std::round(m_scrollPos);
        // Only the visible part of the selection is measured: Ctrl+A on a long document stays cheap.
        m_selectionRects.clear();
        if (HasSelection()) {
            m_layout.GetSelectionRects(m_selAnchor, m_selFocus, scrollY, scrollY + m_viewHeight, m_selectionRects);
        }
        m_renderer.Draw(m_renderTarget.Get(), m_layout, scrollY, m_viewHeight, m_selectionRects);

        const HRESULT hr = m_renderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            DiscardDirect2DResources();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    }

    EndPaint(m_hwnd, &ps);
}

void PreviewView::Relayout() {
    // Hover pointers reference the previous layout objects.
    m_hoverLink = nullptr;
    m_hoverUrl.clear();
    m_pressedUrl.clear();
    m_hoverTask = nullptr;
    m_pressedTaskOffset = -1;
    m_hoverText = false;

    if (m_tree) {
        m_layoutWidthDip = PxToDip(static_cast<float>(m_clientWidthPx));
        m_layout.ComputeLayout(*m_tree, m_layoutWidthDip, m_dpi, m_colors);
        m_effectsApplied = false;
    }
    UpdateScrollbars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void PreviewView::SetBlockTree(std::unique_ptr<Markdown::BlockTree> tree) {
    if (!tree) return;
    m_tree = std::move(tree);
    // Text positions belong to the previous tree (a resize or DPI change keeps them).
    EndMouseSelection();
    m_selAnchor = m_selFocus = TextPosition{};
    m_anchorRange = TextRange{};
    Relayout();
}

void PreviewView::SetDarkMode(bool isDark) {
    m_isDarkMode = isDark;
    m_colors = isDark ? PreviewThemeColors::Dark() : PreviewThemeColors::Light();

    // Colours do not affect geometry: only the brushes (and text effects) are rebuilt.
    DiscardDirect2DResources();
    if (m_hwnd) {
        SetWindowTheme(m_hwnd, isDark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void PreviewView::SetDpi(UINT dpi) {
    if (dpi == 0 || dpi == m_dpi) return;
    const float anchorLine = static_cast<float>(GetLineForCurrentScroll());
    m_dpi = dpi;
    if (m_renderTarget) {
        m_renderTarget->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
    }
    m_viewHeight = PxToDip(static_cast<float>(m_clientHeightPx));
    Relayout();
    ScrollToLine(static_cast<int>(anchorLine));
}

void PreviewView::OnResize(int width, int height) {
    m_clientWidthPx = width;
    m_clientHeightPx = height;
    m_viewHeight = PxToDip(static_cast<float>(height));

    if (m_renderTarget) {
        m_renderTarget->Resize(D2D1::SizeU(
            (std::max)(1u, static_cast<UINT32>(width)),
            (std::max)(1u, static_cast<UINT32>(height))));
    }

    // Height-only changes (status bar, maximise of a docked window) need no relayout.
    const float widthDip = PxToDip(static_cast<float>(width));
    if (m_tree && std::fabs(widthDip - m_layoutWidthDip) > 0.5f) {
        Relayout();
    } else {
        UpdateScrollbars();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void PreviewView::UpdateScrollbars() {
    const float totalH = m_layout.GetTotalHeight();
    m_maxScroll = (std::max)(0.0f, totalH - m_viewHeight);
    m_scrollPos = (std::clamp)(m_scrollPos, 0.0f, m_maxScroll);

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = (std::max)(0, static_cast<int>(std::ceil(totalH)) - 1);
    si.nPage = static_cast<UINT>((std::max)(0.0f, m_viewHeight));
    si.nPos = static_cast<int>(m_scrollPos);
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

void PreviewView::ScrollTo(float newPos, bool notify) {
    newPos = (std::clamp)(newPos, 0.0f, m_maxScroll);
    if (std::fabs(newPos - m_scrollPos) < 0.01f) {
        return;
    }
    m_scrollPos = newPos;
    SetScrollPos(m_hwnd, SB_VERT, static_cast<int>(m_scrollPos), TRUE);
    InvalidateRect(m_hwnd, nullptr, FALSE);

    // Content moved under the cursor: refresh the hovered link.
    POINT pt{};
    if (GetCursorPos(&pt) && ScreenToClient(m_hwnd, &pt)) {
        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        if (PtInRect(&rc, pt)) {
            UpdateHover(PxToDip(static_cast<float>(pt.x)), PxToDip(static_cast<float>(pt.y)));
        }
    }

    if (notify && m_onScrollCallback) {
        m_onScrollCallback(GetLineForCurrentScroll());
    }
}

void PreviewView::ResetScroll() {
    m_scrollPos = 0.0f;
    UpdateScrollbars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void PreviewView::UpdateHover(float xDip, float yDip) {
    const HCURSOR previous = CursorForHover();
    const float y = yDip + m_scrollPos;
    m_hoverTask = m_onToggleTask ? m_layout.HitTestTaskBox(xDip, y) : nullptr;
    m_hoverText = m_layout.IsOverText(xDip, y);
    SetHoverLink(m_layout.HitTestLinkBox(xDip, y));
    if (const HCURSOR cursor = CursorForHover(); cursor != previous) {
        SetCursor(cursor);
    }
}

HCURSOR PreviewView::CursorForHover() const {
    if (m_dragging) return m_hTextCursor;
    if (m_hoverLink || m_hoverTask) return m_hHandCursor;
    return m_hoverText ? m_hTextCursor : m_hArrowCursor;
}

void PreviewView::ToggleTask(int taskMarkOffset) {
    const LayoutBlock* task = m_hoverTask;
    if (!task || !m_tree || !m_onToggleTask) return;
    const bool checked = !task->isTaskChecked;
    if (m_onToggleTask(m_tree->version, taskMarkOffset, checked)) {
        m_layout.SetTaskChecked(taskMarkOffset, checked);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void PreviewView::SetHoverLink(const LinkHitBox* link) {
    if (link == m_hoverLink) return;
    if (m_hoverLink && link && m_hoverLink->layout == link->layout && m_hoverLink->url == link->url) {
        m_hoverLink = link; // Another fragment of the same (wrapped) link
        return;
    }

    // Underline every fragment of the hovered link, like a browser does.
    auto setUnderline = [this](const LinkHitBox* target, BOOL on) {
        if (!target || !target->layout) return;
        for (const auto& block : m_layout.GetBlocks()) {
            for (const auto& l : block.links) {
                if (l.layout == target->layout && l.url == target->url) {
                    l.layout->SetUnderline(on, DWRITE_TEXT_RANGE{l.textStart, l.textLength});
                }
            }
        }
    };
    setUnderline(m_hoverLink, FALSE);
    setUnderline(link, TRUE);

    m_hoverLink = link;
    m_hoverUrl = link ? link->url : std::string();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void PreviewView::OpenLink(const std::string& url) {
    if (url.empty()) return;

    if (url.front() == '#') {
        ScrollToAnchor(url);
        return;
    }

    // Web and mail links go to the default browser / mail client.
    if (StartsWithIgnoreCase(url, "http://") || StartsWithIgnoreCase(url, "https://") ||
        StartsWithIgnoreCase(url, "mailto:") || StartsWithIgnoreCase(url, "ftp://")) {
        const std::wstring wideUrl = Utf8ToUtf16(url);
        ShellExecuteW(m_hwnd, L"open", wideUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    if (StartsWithIgnoreCase(url, "www.")) {
        const std::wstring wideUrl = L"https://" + Utf8ToUtf16(url);
        ShellExecuteW(m_hwnd, L"open", wideUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }

    // Anything else is treated as a local path, relative to the document folder.
    std::string target = url;
    if (StartsWithIgnoreCase(target, "file:///")) {
        target = target.substr(8);
    } else if (target.find(':') != std::string::npos && target.find(":\\") == std::string::npos &&
               target.find(":/") == std::string::npos) {
        MessageBeep(MB_ICONWARNING); // Unknown scheme (javascript:, ms-settings:, ...): ignored
        return;
    }
    std::string fragment;
    if (const size_t hash = target.find('#'); hash != std::string::npos) {
        fragment = target.substr(hash);
        target.erase(hash);
    }
    std::filesystem::path path(Utf8ToUtf16(Markdown::PercentDecode(target)));
    if (path.is_relative() && !m_baseDirectory.empty()) {
        path = m_baseDirectory / path;
    }
    std::error_code ec;
    path = std::filesystem::weakly_canonical(path, ec);
    if (ec || !std::filesystem::exists(path, ec)) {
        MessageBeep(MB_ICONWARNING);
        return;
    }

    const std::wstring ext = LowerExtension(path);
    if (IsMarkdownExtension(ext) && m_onOpenDocument) {
        m_onOpenDocument(path);
    } else if (IsSafeToOpenExtension(ext) || std::filesystem::is_directory(path, ec)) {
        ShellExecuteW(m_hwnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else {
        // Never execute arbitrary files from a document: reveal them in Explorer instead.
        const std::wstring args = L"/select,\"" + path.wstring() + L"\"";
        ShellExecuteW(m_hwnd, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
}

int PreviewView::GetLineForCurrentScroll() const {
    return m_layout.GetLineForScrollY(m_scrollPos);
}

void PreviewView::ScrollToAnchor(std::string_view slug) {
    const float y = m_layout.GetAnchorY(slug);
    if (y >= 0.0f) {
        ScrollTo(y - 12.0f, true);
    } else {
        MessageBeep(MB_ICONWARNING);
    }
}

void PreviewView::ScrollToLine(int line) {
    ScrollTo(m_layout.GetScrollYForLine(line), false);
}

// ==============================================================================
// Text selection
// ==============================================================================

void PreviewView::SetSelection(TextPosition anchor, TextPosition focus) {
    if (anchor == m_selAnchor && focus == m_selFocus) return;
    const bool repaint = HasSelection() || anchor != focus;
    m_selAnchor = anchor;
    m_selFocus = focus;
    if (repaint) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void PreviewView::SelectAll() {
    if (!m_layout.HasText()) return;
    m_anchorRange = TextRange{};
    SetSelection(TextPosition{}, m_layout.DocumentEnd());
}

void PreviewView::ClearSelection() {
    SetSelection(m_selFocus, m_selFocus);
}

void PreviewView::CopySelection() const {
    if (!HasSelection()) return;
    const std::wstring text = m_layout.GetSelectedText(m_selAnchor, m_selFocus);
    if (!text.empty()) {
        Platform::CopyTextToClipboard(m_hwnd, text);
    }
}

TextRange PreviewView::RangeAt(float xDip, float yDip) const {
    const float y = yDip + m_scrollPos;
    switch (m_selectMode) {
    case SelectMode::Word:
        return m_layout.WordAt(xDip, y);
    case SelectMode::Unit:
        return m_layout.UnitAt(xDip, y);
    default: {
        const TextPosition caret = m_layout.HitTestTextPosition(xDip, y);
        return TextRange{caret, caret};
    }
    }
}

void PreviewView::BeginSelection(float xDip, float yDip, SelectMode mode, bool extend) {
    m_selectMode = mode;
    if (extend) {
        // Shift+click: from the anchor of the current selection (or the last click) to here.
        m_anchorRange = TextRange{m_selAnchor, m_selAnchor};
        ExtendSelection(xDip, yDip);
        return;
    }
    m_anchorRange = RangeAt(xDip, yDip);
    SetSelection(m_anchorRange.start, m_anchorRange.end);
}

void PreviewView::ExtendSelection(float xDip, float yDip) {
    // A word or unit selection grows by whole words or units, keeping the one it started on.
    const TextRange target = RangeAt(xDip, yDip);
    if (target.start < m_anchorRange.start) {
        SetSelection(m_anchorRange.end, target.start);
    } else {
        SetSelection(m_anchorRange.start, (std::max)(target.end, m_anchorRange.end));
    }
}

void PreviewView::StartMouseSelection() {
    m_mouseDown = true;
    m_dragging = false;
    SetCapture(m_hwnd);
}

void PreviewView::EndMouseSelection() {
    const bool wasDown = m_mouseDown;
    m_mouseDown = false;
    m_dragging = false;
    if (m_autoScrolling) {
        KillTimer(m_hwnd, kAutoScrollTimer);
        m_autoScrolling = false;
    }
    if (wasDown && GetCapture() == m_hwnd) {
        ReleaseCapture(); // Sends WM_CAPTURECHANGED, which finds nothing left to end
    }
}

bool PreviewView::IsTripleClick(LPARAM lParam) const {
    // Windows reports double-clicks only: a third press soon after one, close to it, is a triple-click.
    const DWORD elapsed = static_cast<DWORD>(GetMessageTime()) - static_cast<DWORD>(m_lastDoubleClickTime);
    if (m_lastDoubleClickTime == 0 || elapsed > GetDoubleClickTime()) {
        return false;
    }
    return std::abs(GET_X_LPARAM(lParam) - m_lastDoubleClickPoint.x) <= GetSystemMetrics(SM_CXDOUBLECLK) / 2 &&
           std::abs(GET_Y_LPARAM(lParam) - m_lastDoubleClickPoint.y) <= GetSystemMetrics(SM_CYDOUBLECLK) / 2;
}

bool PreviewView::IsInsideSelection(float xDip, float yDip) const {
    if (!HasSelection()) return false;
    const float y = yDip + m_scrollPos;
    std::vector<SelectionRect> rects;
    m_layout.GetSelectionRects(m_selAnchor, m_selFocus, y - 1.0f, y + 1.0f, rects);
    return std::any_of(rects.begin(), rects.end(), [&](const SelectionRect& s) {
        return xDip >= s.rect.left && xDip <= s.rect.right && y >= s.rect.top && y <= s.rect.bottom;
    });
}

void PreviewView::UpdateAutoScroll(int yPx) {
    const bool outside = yPx < 0 || yPx >= m_clientHeightPx;
    if (outside && !m_autoScrolling) {
        m_autoScrolling = SetTimer(m_hwnd, kAutoScrollTimer, kAutoScrollIntervalMs, nullptr) != 0;
    } else if (!outside && m_autoScrolling) {
        KillTimer(m_hwnd, kAutoScrollTimer);
        m_autoScrolling = false;
    }
}

void PreviewView::AutoScrollStep() {
    POINT pt{};
    if (!m_mouseDown || !GetCursorPos(&pt) || !ScreenToClient(m_hwnd, &pt)) {
        EndMouseSelection();
        return;
    }
    // Faster the further the pointer is from the view, like a text editor.
    float distance = 0.0f;
    if (pt.y < 0) {
        distance = -PxToDip(static_cast<float>(-pt.y));
    } else if (pt.y >= m_clientHeightPx) {
        distance = PxToDip(static_cast<float>(pt.y - m_clientHeightPx + 1));
    }
    if (distance == 0.0f) return;
    const float step = std::copysign(4.0f + (std::min)(std::fabs(distance), 200.0f) * 0.5f, distance);
    ScrollTo(m_scrollPos + step);
    ExtendSelection(PxToDip(static_cast<float>(pt.x)), PxToDip(static_cast<float>(pt.y)));
}

void PreviewView::EditAt(float xDip, float yDip, bool useSelection) {
    if (!m_tree) return;
    if (useSelection && HasSelection() && m_onSourceSelect) {
        m_onSourceSelect(m_layout.GetSourceSelection(m_selAnchor, m_selFocus));
    } else if (m_onSourceNavigate) {
        m_onSourceNavigate(m_layout.HitTestSource(xDip, yDip + m_scrollPos));
    }
}

void PreviewView::ShowContextMenu(POINT screenPt, bool fromKeyboard) {
    // The point the menu acts on, in view DIPs: the click, or the selection (top of the view) from the keyboard.
    float x = 0.0f;
    float y = 0.0f;
    if (fromKeyboard) {
        POINT client{0, 0};
        if (HasSelection() && !m_selectionRects.empty()) {
            const D2D1_RECT_F& first = m_selectionRects.front().rect;
            x = first.left;
            y = first.bottom - std::round(m_scrollPos);
            client = POINT{static_cast<LONG>(x * static_cast<float>(m_dpi) / 96.0f),
                           static_cast<LONG>(y * static_cast<float>(m_dpi) / 96.0f)};
        } else {
            x = m_layout.GetLayoutWidth() * 0.5f;
        }
        screenPt = client;
        ClientToScreen(m_hwnd, &screenPt);
    } else {
        POINT client = screenPt;
        ScreenToClient(m_hwnd, &client);
        x = PxToDip(static_cast<float>(client.x));
        y = PxToDip(static_cast<float>(client.y));
    }
    const bool onSelection = fromKeyboard ? HasSelection() : IsInsideSelection(x, y);

    enum : UINT { kCopy = 1, kCopyMarkdown, kSelectAll, kEditHere };
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    const auto item = [](bool enabled) { return static_cast<UINT>(MF_STRING | (enabled ? MF_ENABLED : MF_GRAYED)); };
    AppendMenuW(menu, item(HasSelection()), kCopy, L"&Copiar\tCtrl+C");
    AppendMenuW(menu, item(HasSelection() && m_onCopyMarkdown), kCopyMarkdown, L"Copiar como &Markdown");
    AppendMenuW(menu, item(m_layout.HasText()), kSelectAll, L"Seleccionar &todo\tCtrl+A");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, item(m_tree && m_onSourceNavigate), kEditHere, L"&Editar aquí\tCtrl+clic");
    const auto command = static_cast<UINT>(
        TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPt.x, screenPt.y, m_hwnd, nullptr));
    DestroyMenu(menu);

    switch (command) {
    case kCopy:
        CopySelection();
        break;
    case kCopyMarkdown:
        if (m_tree && HasSelection() && m_onCopyMarkdown &&
            !m_onCopyMarkdown(m_tree->version, m_layout.GetSourceSelection(m_selAnchor, m_selFocus))) {
            MessageBeep(MB_ICONWARNING);
        }
        break;
    case kSelectAll:
        SelectAll();
        break;
    case kEditHere:
        EditAt(x, y, onSelection);
        break;
    default:
        break;
    }
}

} // namespace Pluma::Preview
