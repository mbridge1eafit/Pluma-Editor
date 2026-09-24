#include "file_explorer_panel.h"

#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>

#include "../platform/dpi.h"

namespace Pluma::Explorer {

namespace {

constexpr wchar_t kPanelClassName[] = L"PlumaFileExplorerPanelClass";
constexpr UINT WM_APP_OPEN_ITEM = WM_APP + 20; // wParam = file index
constexpr int kTreeControlId = 1;

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

bool IsMarkdownFile(const std::filesystem::path& path) {
    std::wstring ext = path.extension().wstring();
    for (wchar_t& c : ext) c = static_cast<wchar_t>(std::towlower(c));
    return ext == L".md" || ext == L".markdown" || ext == L".mdown";
}

bool SameFile(const std::filesystem::path& a, const std::filesystem::path& b) {
    if (a.empty() || b.empty()) return false;
    std::error_code ec;
    const bool equivalent = std::filesystem::equivalent(a, b, ec);
    return !ec && equivalent;
}

} // namespace

FileExplorerPanel::~FileExplorerPanel() {
    if (m_font) DeleteObject(m_font);
    if (m_captionFont) DeleteObject(m_captionFont);
}

bool FileExplorerPanel::Create(HWND parent, HINSTANCE hInstance) {
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
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_SHOWSELALWAYS |
                                 TVS_FULLROWSELECT | TVS_NOHSCROLL | TVS_DISABLEDRAGDROP,
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

void FileExplorerPanel::SetBounds(int x, int y, int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void FileExplorerPanel::SetDarkMode(bool dark) {
    m_dark = dark;
    ApplyColors();
}

void FileExplorerPanel::SetDpi(UINT dpi) {
    if (dpi == 0 || dpi == m_dpi) return;
    m_dpi = dpi;
    UpdateFonts();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    SendMessageW(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FileExplorerPanel::Focus() {
    if (m_tree) SetFocus(m_tree);
}

void FileExplorerPanel::SetFolder(const std::filesystem::path& folder) {
    const bool unchanged = (folder.empty() && m_folder.empty()) || SameFile(folder, m_folder);
    if (unchanged) return;
    m_folder = folder;
    Rebuild();
}

void FileExplorerPanel::Refresh() {
    Rebuild();
}

void FileExplorerPanel::SetCurrentFile(const std::filesystem::path& file) {
    m_currentFile = file;
    const std::filesystem::path folder = file.empty() ? std::filesystem::path{} : file.parent_path();
    if (!SameFile(folder, m_folder) && !(folder.empty() && m_folder.empty())) {
        m_folder = folder;
        Rebuild();
        return;
    }
    // Same folder: just move the bold/selected marker without a full rescan.
    HTREEITEM match = nullptr;
    for (size_t i = 0; i < m_files.size(); ++i) {
        const bool isCurrent = SameFile(m_files[i], m_currentFile);
        TVITEMW item{};
        item.mask = TVIF_STATE;
        item.hItem = m_items[i];
        item.stateMask = TVIS_BOLD;
        item.state = isCurrent ? TVIS_BOLD : 0;
        TreeView_SetItem(m_tree, &item);
        if (isCurrent) match = m_items[i];
    }
    TreeView_SelectItem(m_tree, match);
    InvalidateRect(m_tree, nullptr, TRUE);
}

void FileExplorerPanel::Rebuild() {
    if (!m_tree) return;
    SendMessageW(m_tree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(m_tree);
    m_items.clear();
    m_files.clear();
    m_placeholder = nullptr;

    std::error_code ec;
    if (!m_folder.empty() && std::filesystem::is_directory(m_folder, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(
                 m_folder, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (entry.is_regular_file(ec) && IsMarkdownFile(entry.path())) {
                m_files.push_back(entry.path());
            }
        }
    }
    std::sort(m_files.begin(), m_files.end(), [](const auto& a, const auto& b) {
        return _wcsicmp(a.filename().c_str(), b.filename().c_str()) < 0;
    });

    TVINSERTSTRUCTW ins{};
    ins.hParent = TVI_ROOT;
    ins.hInsertAfter = TVI_LAST;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    ins.item.stateMask = TVIS_BOLD;

    if (m_files.empty()) {
        std::wstring text = m_folder.empty() ? L"Abra un archivo para ver su carpeta"
                                             : L"No hay archivos Markdown en esta carpeta";
        ins.item.pszText = text.data();
        ins.item.lParam = -1;
        ins.item.state = 0;
        m_placeholder = TreeView_InsertItem(m_tree, &ins);
    } else {
        m_items.reserve(m_files.size());
        for (size_t i = 0; i < m_files.size(); ++i) {
            std::wstring name = m_files[i].filename().wstring();
            const bool isCurrent = SameFile(m_files[i], m_currentFile);
            ins.item.pszText = name.data();
            ins.item.lParam = static_cast<LPARAM>(i);
            ins.item.state = isCurrent ? TVIS_BOLD : 0;
            HTREEITEM item = TreeView_InsertItem(m_tree, &ins);
            m_items.push_back(item);
            if (isCurrent) TreeView_SelectItem(m_tree, item);
        }
    }

    SendMessageW(m_tree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_tree, nullptr, TRUE);
}

void FileExplorerPanel::ApplyColors() {
    if (!m_tree) return;
    const Palette p = GetPalette(m_dark);
    SetWindowTheme(m_tree, m_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    TreeView_SetBkColor(m_tree, p.background);
    TreeView_SetTextColor(m_tree, p.text);
    InvalidateRect(m_hwnd, nullptr, TRUE);
    InvalidateRect(m_tree, nullptr, TRUE);
}

void FileExplorerPanel::UpdateFonts() {
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
        TreeView_SetIndent(m_tree, Platform::ScaleForDpi(6, m_dpi));
    }
}

int FileExplorerPanel::CaptionHeight() const {
    return Platform::ScaleForDpi(32, m_dpi);
}

RECT FileExplorerPanel::CloseButtonRect() const {
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    const int size = Platform::ScaleForDpi(24, m_dpi);
    const int margin = (CaptionHeight() - size) / 2;
    return RECT{rc.right - margin - size, margin, rc.right - margin, margin + size};
}

RECT FileExplorerPanel::RefreshButtonRect() const {
    const RECT closeRc = CloseButtonRect();
    const int size = closeRc.right - closeRc.left;
    const int gap = Platform::ScaleForDpi(2, m_dpi);
    return RECT{closeRc.left - gap - size, closeRc.top, closeRc.left - gap, closeRc.bottom};
}

void FileExplorerPanel::Paint(HDC hdc) {
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
    const RECT refreshRc = RefreshButtonRect();
    RECT textRc{Platform::ScaleForDpi(12, m_dpi), 0, refreshRc.left - Platform::ScaleForDpi(4, m_dpi), captionH};
    DrawTextW(memDC, L"ARCHIVOS", -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    // Refresh button: circular-arrow glyph approximated with an open arc and an arrowhead.
    if (m_refreshHot || m_refreshPressed) {
        HBRUSH hot = CreateSolidBrush(m_refreshPressed ? p.buttonPressed : p.buttonHot);
        FillRect(memDC, &refreshRc, hot);
        DeleteObject(hot);
    }
    {
        const int cx = (refreshRc.left + refreshRc.right) / 2;
        const int cy = (refreshRc.top + refreshRc.bottom) / 2;
        const int r = Platform::ScaleForDpi(5, m_dpi);
        HPEN pen = CreatePen(PS_SOLID, (std::max)(1, Platform::ScaleForDpi(1, m_dpi)), p.text);
        HGDIOBJ oldPen = SelectObject(memDC, pen);
        HGDIOBJ oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));
        Arc(memDC, cx - r, cy - r, cx + r + 1, cy + r + 1, cx, cy - r, cx + r, cy);
        POINT arrow[3] = {{cx + r - Platform::ScaleForDpi(3, m_dpi), cy - r - Platform::ScaleForDpi(2, m_dpi)},
                          {cx + r + Platform::ScaleForDpi(3, m_dpi), cy - r},
                          {cx + r - Platform::ScaleForDpi(1, m_dpi), cy - r + Platform::ScaleForDpi(3, m_dpi)}};
        Polyline(memDC, arrow, 3);
        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(pen);
    }

    // Close button: flat square with an X, highlighted on hover.
    const RECT closeRc = CloseButtonRect();
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

int FileExplorerPanel::ItemIndex(HTREEITEM item) const {
    if (!item) return -1;
    TVITEMW tvi{};
    tvi.mask = TVIF_PARAM;
    tvi.hItem = item;
    if (!TreeView_GetItem(m_tree, &tvi)) return -1;
    const auto index = static_cast<int>(tvi.lParam);
    return (index >= 0 && index < static_cast<int>(m_files.size())) ? index : -1;
}

void FileExplorerPanel::OpenIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_files.size()) || !m_onOpenFile) return;
    m_onOpenFile(m_files[index]);
}

LRESULT CALLBACK FileExplorerPanel::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FileExplorerPanel* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<FileExplorerPanel*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<FileExplorerPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT FileExplorerPanel::HandleTreeNotify(NMHDR* hdr) {
    switch (hdr->code) {
    case NM_DBLCLK: {
        TVHITTESTINFO hit{};
        GetCursorPos(&hit.pt);
        ScreenToClient(m_tree, &hit.pt);
        const HTREEITEM item = TreeView_HitTest(m_tree, &hit);
        constexpr UINT kOnRow = TVHT_ONITEM | TVHT_ONITEMINDENT | TVHT_ONITEMRIGHT;
        if (item && (hit.flags & kOnRow)) {
            const int index = ItemIndex(item);
            if (index >= 0) PostMessageW(m_hwnd, WM_APP_OPEN_ITEM, static_cast<WPARAM>(index), 0);
        }
        return 0;
    }
    case TVN_KEYDOWN: {
        auto* nm = reinterpret_cast<NMTVKEYDOWN*>(hdr);
        if (nm->wVKey == VK_RETURN) {
            OpenIndex(ItemIndex(TreeView_GetSelection(m_tree)));
            return TRUE;
        }
        return 0;
    }
    case NM_CUSTOMDRAW: {
        auto* cd = reinterpret_cast<NMTVCUSTOMDRAW*>(hdr);
        if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            const auto index = static_cast<int>(cd->nmcd.lItemlParam);
            if (index < 0) cd->clrText = GetPalette(m_dark).mutedText;
        }
        return CDRF_DODEFAULT;
    }
    default:
        return 0;
    }
}

LRESULT FileExplorerPanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
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
        const RECT refreshRc = RefreshButtonRect();
        const bool closeHot = PtInRect(&closeRc, pt) != FALSE;
        const bool refreshHot = PtInRect(&refreshRc, pt) != FALSE;
        if (closeHot != m_closeHot) {
            m_closeHot = closeHot;
            InvalidateRect(m_hwnd, &closeRc, FALSE);
        }
        if (refreshHot != m_refreshHot) {
            m_refreshHot = refreshHot;
            InvalidateRect(m_hwnd, &refreshRc, FALSE);
        }
        if (!m_trackingMouse) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, m_hwnd, 0};
            m_trackingMouse = TrackMouseEvent(&tme) != FALSE;
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        if (m_closeHot || m_refreshHot) {
            m_closeHot = false;
            m_refreshHot = false;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const RECT closeRc = CloseButtonRect();
        const RECT refreshRc = RefreshButtonRect();
        if (PtInRect(&closeRc, pt)) {
            m_closePressed = true;
            SetCapture(m_hwnd);
            InvalidateRect(m_hwnd, &closeRc, FALSE);
        } else if (PtInRect(&refreshRc, pt)) {
            m_refreshPressed = true;
            SetCapture(m_hwnd);
            InvalidateRect(m_hwnd, &refreshRc, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (m_closePressed) {
            m_closePressed = false;
            ReleaseCapture();
            const RECT closeRc = CloseButtonRect();
            InvalidateRect(m_hwnd, &closeRc, FALSE);
            if (PtInRect(&closeRc, pt) && m_onClose) m_onClose();
        } else if (m_refreshPressed) {
            m_refreshPressed = false;
            ReleaseCapture();
            const RECT refreshRc = RefreshButtonRect();
            InvalidateRect(m_hwnd, &refreshRc, FALSE);
            if (PtInRect(&refreshRc, pt)) Refresh();
        }
        return 0;
    }

    case WM_CAPTURECHANGED:
        if (m_closePressed || m_refreshPressed) {
            m_closePressed = false;
            m_refreshPressed = false;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (hdr->hwndFrom == m_tree) return HandleTreeNotify(hdr);
        break;
    }

    case WM_APP_OPEN_ITEM:
        OpenIndex(static_cast<int>(wParam));
        return 0;

    case WM_DESTROY:
        m_tree = nullptr;
        break;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

} // namespace Pluma::Explorer
