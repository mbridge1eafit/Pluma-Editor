#include "preview_view.h"
#include <shellapi.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cwctype>

#include "../markdown/slug.h"

namespace Pluma::Preview {

namespace {

constexpr float kLineScrollDip = 40.0f;       // Arrow keys / scrollbar arrows
constexpr float kWheelLineDip = 24.0f;        // One wheel "line"

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
        UpdateHover(PxToDip(static_cast<float>(GET_X_LPARAM(lParam))),
                    PxToDip(static_cast<float>(GET_Y_LPARAM(lParam))));
        return 0;
    }

    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        SetHoverLink(nullptr);
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(m_hoverLink ? m_hHandCursor : m_hArrowCursor);
            return TRUE;
        }
        break;

    case WM_LBUTTONDOWN:
        SetFocus(m_hwnd);
        m_pressedUrl = m_hoverUrl;
        return 0;

    case WM_LBUTTONUP: {
        // Only follow a link when press and release happen on the same link.
        std::string url;
        url.swap(m_pressedUrl);
        if (!url.empty() && url == m_hoverUrl) {
            OpenLink(url);
            return 0;
        }
        break;
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
        m_renderer.Draw(m_renderTarget.Get(), m_layout, std::round(m_scrollPos), m_viewHeight);

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
    SetHoverLink(m_layout.HitTestLinkBox(xDip, yDip + m_scrollPos));
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
    SetCursor(link ? m_hHandCursor : m_hArrowCursor);
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

} // namespace Pluma::Preview
