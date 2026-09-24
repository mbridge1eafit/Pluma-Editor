#include "outline_panel.h"

#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>

#include "../platform/dpi.h"

namespace Pluma::Outline {

namespace {

constexpr wchar_t kPanelClassName[] = L"PlumaOutlinePanelClass";
constexpr UINT WM_APP_NAVIGATE_ITEM = WM_APP + 10; // wParam = heading index, lParam = focus editor
constexpr int kTreeControlId = 1;
constexpr int kMaxTitleChars = 120;

std::wstring Utf8ToUtf16(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), count);
    return result;
}

struct Palette {
    COLORREF background;
    COLORREF text;
    COLORREF mutedText;
    COLORREF border;
    COLORREF buttonHot;
    COLORREF buttonPressed;
};

Palette GetPalette(bool dark) {
    if (dark) {
        return {RGB(37, 37, 38), RGB(212, 212, 212), RGB(145, 145, 145),
                RGB(50, 50, 50), RGB(62, 62, 64), RGB(75, 75, 78)};
    }
    return {RGB(246, 248, 250), RGB(36, 41, 47), RGB(101, 109, 118),
            RGB(222, 225, 229), RGB(226, 229, 233), RGB(210, 214, 219)};
}

} // namespace

std::vector<Heading> CollectHeadings(const Markdown::BlockTree& tree, size_t maxItems) {
    std::vector<Heading> headings;
    if (!tree.root) return headings;
    for (const auto& child : tree.root->children) {
        if (headings.size() >= maxItems) break;
        if (!child || child->type != Markdown::BlockType::Heading) continue;
        // Full visible text, including **bold**, `code` and [link] parts.
        std::wstring title = Utf8ToUtf16(Markdown::ExtractPlainText(child->inlineContent));
        if (title.empty()) title = L"(Sin título)";
        if (title.size() > kMaxTitleChars) title = title.substr(0, kMaxTitleChars - 1) + L"…";
        headings.push_back({child->startLine, std::clamp(child->level, 1, 6), std::move(title)});
    }
    return headings;
}

int FindSectionIndex(const std::vector<Heading>& headings, int line) {
    const auto it = std::upper_bound(headings.begin(), headings.end(), line,
                                     [](int value, const Heading& h) { return value < h.line; });
    return static_cast<int>(it - headings.begin()) - 1;
}

OutlinePanel::~OutlinePanel() {
    if (m_font) DeleteObject(m_font);
    if (m_captionFont) DeleteObject(m_captionFont);
}

bool OutlinePanel::Create(HWND parent, HINSTANCE hInstance) {
    m_hInstance = hInstance;

    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
        wc.lpfnWndProc = StaticWndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kPanelClassName;
        if (!RegisterClassExW(&wc)) return false;
        s_registered = true;
    }

    m_hwnd = CreateWindowExW(0, kPanelClassName, L"", WS_CHILD | WS_CLIPCHILDREN,
                             0, 0, 0, 0, parent, nullptr, hInstance, this);
    if (!m_hwnd) return false;

    m_dpi = GetDpiForWindow(m_hwnd);
    if (m_dpi == 0) m_dpi = 96;

    m_tree = CreateWindowExW(0, WC_TREEVIEWW, L"",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_LINESATROOT |
                                 TVS_SHOWSELALWAYS | TVS_FULLROWSELECT | TVS_NOHSCROLL |
                                 TVS_DISABLEDRAGDROP,
                             0, 0, 0, 0, m_hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTreeControlId)), hInstance, nullptr);
    if (!m_tree) return false;

    const DWORD exStyle = TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS;
    TreeView_SetExtendedStyle(m_tree, exStyle, exStyle);
    UpdateFonts();
    ApplyColors();
    Rebuild();
    return true;
}

void OutlinePanel::SetBounds(int x, int y, int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void OutlinePanel::SetDarkMode(bool dark) {
    m_dark = dark;
    ApplyColors();
}

void OutlinePanel::SetDpi(UINT dpi) {
    if (dpi == 0 || dpi == m_dpi) return;
    m_dpi = dpi;
    UpdateFonts();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    SendMessageW(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void OutlinePanel::Focus() {
    if (m_tree) SetFocus(m_tree);
}

void OutlinePanel::SetHeadings(std::vector<Heading> headings) {
    bool sameShape = !m_items.empty() && headings.size() == m_headings.size();
    for (size_t i = 0; sameShape && i < headings.size(); ++i) {
        sameShape = headings[i].level == m_headings[i].level;
    }

    if (!sameShape) {
        if (headings.empty() && m_headings.empty() && m_placeholder) return;
        m_headings = std::move(headings);
        Rebuild();
        return;
    }

    for (size_t i = 0; i < headings.size(); ++i) {
        if (headings[i].title != m_headings[i].title) {
            TVITEMW item{};
            item.mask = TVIF_TEXT;
            item.hItem = m_items[i];
            item.pszText = headings[i].title.data();
            TreeView_SetItem(m_tree, &item);
        }
    }
    m_headings = std::move(headings); // Lines may have shifted
    m_selectedIndex = -2;             // Force re-evaluation with the new lines
    HighlightLine(m_lastLine);
}

void OutlinePanel::HighlightLine(int line) {
    m_lastLine = line;
    if (!m_tree) return;
    const int index = FindSectionIndex(m_headings, line);
    if (index == m_selectedIndex) return;
    m_selectedIndex = index;
    m_selecting = true;
    TreeView_SelectItem(m_tree, index >= 0 && index < static_cast<int>(m_items.size()) ? m_items[index] : nullptr);
    m_selecting = false;
}

void OutlinePanel::Rebuild() {
    if (!m_tree) return;
    SendMessageW(m_tree, WM_SETREDRAW, FALSE, 0);
    m_selecting = true;
    TreeView_DeleteAllItems(m_tree);
    m_selecting = false;
    m_items.clear();
    m_placeholder = nullptr;
    m_selectedIndex = -2;

    TVINSERTSTRUCTW ins{};
    ins.hInsertAfter = TVI_LAST;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    ins.item.stateMask = TVIS_BOLD | TVIS_EXPANDED;

    if (m_headings.empty()) {
        std::wstring text = L"No hay encabezados";
        ins.hParent = TVI_ROOT;
        ins.item.pszText = text.data();
        ins.item.lParam = -1;
        ins.item.state = 0;
        m_placeholder = TreeView_InsertItem(m_tree, &ins);
    } else {
        // Nest each heading under the closest previous heading of a higher level.
        std::vector<std::pair<int, HTREEITEM>> stack;
        m_items.reserve(m_headings.size());
        for (size_t i = 0; i < m_headings.size(); ++i) {
            const Heading& h = m_headings[i];
            while (!stack.empty() && stack.back().first >= h.level) stack.pop_back();
            ins.hParent = stack.empty() ? TVI_ROOT : stack.back().second;
            ins.item.pszText = const_cast<wchar_t*>(h.title.c_str());
            ins.item.lParam = static_cast<LPARAM>(i);
            ins.item.state = TVIS_EXPANDED | (h.level == 1 ? TVIS_BOLD : 0);
            HTREEITEM item = TreeView_InsertItem(m_tree, &ins);
            m_items.push_back(item);
            stack.emplace_back(h.level, item);
        }
        for (HTREEITEM item : m_items) {
            if (TreeView_GetChild(m_tree, item)) TreeView_Expand(m_tree, item, TVE_EXPAND);
        }
    }

    SendMessageW(m_tree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_tree, nullptr, TRUE);
    HighlightLine(m_lastLine);
}

void OutlinePanel::ApplyColors() {
    if (!m_tree) return;
    const Palette p = GetPalette(m_dark);
    SetWindowTheme(m_tree, m_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    TreeView_SetBkColor(m_tree, p.background);
    TreeView_SetTextColor(m_tree, p.text);
    InvalidateRect(m_hwnd, nullptr, TRUE);
    InvalidateRect(m_tree, nullptr, TRUE);
}

void OutlinePanel::UpdateFonts() {
    if (m_font) DeleteObject(m_font);
    if (m_captionFont) DeleteObject(m_captionFont);
    m_font = CreateFontW(-MulDiv(9, static_cast<int>(m_dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_captionFont = CreateFontW(-MulDiv(8, static_cast<int>(m_dpi), 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE,
                                FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (m_tree) {
        SendMessageW(m_tree, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
        TreeView_SetItemHeight(m_tree, Platform::ScaleForDpi(24, m_dpi));
        TreeView_SetIndent(m_tree, Platform::ScaleForDpi(14, m_dpi));
    }
}

int OutlinePanel::CaptionHeight() const {
    return Platform::ScaleForDpi(32, m_dpi);
}

RECT OutlinePanel::CloseButtonRect() const {
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    const int size = Platform::ScaleForDpi(24, m_dpi);
    const int margin = (CaptionHeight() - size) / 2;
    return RECT{rc.right - margin - size, margin, rc.right - margin, margin + size};
}

void OutlinePanel::Paint(HDC hdc) {
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    const int captionH = CaptionHeight();
    RECT caption{0, 0, rc.right, captionH};
    if (caption.right <= 0) return;

    const Palette p = GetPalette(m_dark);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, caption.right, caption.bottom);
    HGDIOBJ oldBmp = SelectObject(memDC, bmp);

    HBRUSH bg = CreateSolidBrush(p.background);
    FillRect(memDC, &caption, bg);
    DeleteObject(bg);

    HGDIOBJ oldFont = SelectObject(memDC, m_captionFont);
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, p.mutedText);
    const RECT closeRc = CloseButtonRect();
    RECT textRc{Platform::ScaleForDpi(12, m_dpi), 0, closeRc.left - Platform::ScaleForDpi(4, m_dpi), captionH};
    DrawTextW(memDC, L"ENCABEZADOS", -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    // Close button: flat square with an X, highlighted on hover.
    if (m_closeHot || m_closePressed) {
        HBRUSH hot = CreateSolidBrush(m_closePressed ? p.buttonPressed : p.buttonHot);
        FillRect(memDC, &closeRc, hot);
        DeleteObject(hot);
    }
    const int cx = (closeRc.left + closeRc.right) / 2;
    const int cy = (closeRc.top + closeRc.bottom) / 2;
    const int arm = Platform::ScaleForDpi(4, m_dpi);
    HPEN pen = CreatePen(PS_SOLID, (std::max)(1, Platform::ScaleForDpi(1, m_dpi)), p.text);
    HGDIOBJ oldPen = SelectObject(memDC, pen);
    MoveToEx(memDC, cx - arm, cy - arm, nullptr);
    LineTo(memDC, cx + arm + 1, cy + arm + 1);
    MoveToEx(memDC, cx + arm, cy - arm, nullptr);
    LineTo(memDC, cx - arm - 1, cy + arm + 1);
    SelectObject(memDC, oldPen);
    DeleteObject(pen);

    SelectObject(memDC, oldFont);
    BitBlt(hdc, 0, 0, caption.right, caption.bottom, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

int OutlinePanel::ItemIndex(HTREEITEM item) const {
    if (!item) return -1;
    TVITEMW tvi{};
    tvi.mask = TVIF_PARAM;
    tvi.hItem = item;
    if (!TreeView_GetItem(m_tree, &tvi)) return -1;
    const auto index = static_cast<int>(tvi.lParam);
    return (index >= 0 && index < static_cast<int>(m_headings.size())) ? index : -1;
}

void OutlinePanel::Navigate(int index, bool focusEditor) {
    if (index < 0 || index >= static_cast<int>(m_headings.size()) || !m_onNavigate) return;
    m_onNavigate(m_headings[index].line, focusEditor);
}

LRESULT CALLBACK OutlinePanel::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OutlinePanel* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<OutlinePanel*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<OutlinePanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT OutlinePanel::HandleTreeNotify(NMHDR* hdr) {
    switch (hdr->code) {
    case TVN_SELCHANGEDW: {
        if (m_selecting) return 0;
        auto* nm = reinterpret_cast<NMTREEVIEWW*>(hdr);
        const int index = ItemIndex(nm->itemNew.hItem);
        m_selectedIndex = index;
        // Mouse clicks navigate in NM_CLICK (which also covers the already selected item).
        if (nm->action == TVC_BYKEYBOARD) Navigate(index, false);
        return 0;
    }
    case NM_CLICK: {
        TVHITTESTINFO hit{};
        GetCursorPos(&hit.pt);
        ScreenToClient(m_tree, &hit.pt);
        const HTREEITEM item = TreeView_HitTest(m_tree, &hit);
        constexpr UINT kOnRow = TVHT_ONITEM | TVHT_ONITEMINDENT | TVHT_ONITEMRIGHT;
        if (item && (hit.flags & kOnRow) && !(hit.flags & TVHT_ONITEMBUTTON)) {
            const int index = ItemIndex(item);
            // Posted: the tree view is still processing the click and would take the focus back.
            if (index >= 0) PostMessageW(m_hwnd, WM_APP_NAVIGATE_ITEM, static_cast<WPARAM>(index), TRUE);
        }
        return 0;
    }
    case TVN_KEYDOWN: {
        auto* nm = reinterpret_cast<NMTVKEYDOWN*>(hdr);
        if (nm->wVKey == VK_RETURN) {
            Navigate(ItemIndex(TreeView_GetSelection(m_tree)), true);
            return TRUE; // Not part of incremental search (no beep)
        }
        return 0;
    }
    case NM_CUSTOMDRAW: {
        auto* cd = reinterpret_cast<NMTVCUSTOMDRAW*>(hdr);
        if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            const auto index = static_cast<int>(cd->nmcd.lItemlParam);
            const bool deep = index >= 0 && index < static_cast<int>(m_headings.size()) &&
                              m_headings[index].level >= 3;
            if (index < 0 || deep) cd->clrText = GetPalette(m_dark).mutedText;
        }
        return CDRF_DODEFAULT;
    }
    default:
        return 0;
    }
}

LRESULT OutlinePanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        const int captionH = CaptionHeight();
        if (m_tree) {
            SetWindowPos(m_tree, nullptr, 0, captionH, width, (std::max)(0, height - captionH),
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hwnd, &ps);
        Paint(hdc);
        EndPaint(m_hwnd, &ps);
        return 0;
    }

    case WM_SETFOCUS:
        if (m_tree) SetFocus(m_tree);
        return 0;

    case WM_MOUSEMOVE: {
        const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const RECT closeRc = CloseButtonRect();
        const bool hot = PtInRect(&closeRc, pt) != FALSE;
        if (hot != m_closeHot) {
            m_closeHot = hot;
            InvalidateRect(m_hwnd, &closeRc, FALSE);
        }
        if (!m_trackingMouse) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, m_hwnd, 0};
            m_trackingMouse = TrackMouseEvent(&tme) != FALSE;
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        if (m_closeHot) {
            m_closeHot = false;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const RECT closeRc = CloseButtonRect();
        if (PtInRect(&closeRc, pt)) {
            m_closePressed = true;
            SetCapture(m_hwnd);
            InvalidateRect(m_hwnd, &closeRc, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (m_closePressed) {
            m_closePressed = false;
            ReleaseCapture();
            const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const RECT closeRc = CloseButtonRect();
            InvalidateRect(m_hwnd, &closeRc, FALSE);
            if (PtInRect(&closeRc, pt) && m_onClose) m_onClose();
        }
        return 0;
    }

    case WM_CAPTURECHANGED:
        if (m_closePressed) {
            m_closePressed = false;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (hdr->hwndFrom == m_tree) return HandleTreeNotify(hdr);
        break;
    }

    case WM_APP_NAVIGATE_ITEM:
        Navigate(static_cast<int>(wParam), lParam != 0);
        return 0;

    case WM_DESTROY:
        m_tree = nullptr;
        break;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

} // namespace Pluma::Outline
