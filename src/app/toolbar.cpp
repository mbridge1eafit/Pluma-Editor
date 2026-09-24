#include "toolbar.h"

#include <uxtheme.h>

#include <vector>

#include "../platform/dpi.h"
#include "../platform/theme.h"
#include "../../res/resource.h"

namespace Pluma::App {

namespace {

constexpr int kToolbarControlId = 1012;

// Tooltip text for icon-only buttons, and the visible label for the text buttons (BTNS_SHOWTEXT).
// Order matches the iString index assigned to each TBBUTTON below.
constexpr const wchar_t* kStrings[] = {
    L"Nuevo (Ctrl+N)",         // 0
    L"Abrir... (Ctrl+O)",      // 1
    L"Guardar (Ctrl+S)",       // 2
    L"Deshacer (Ctrl+Z)",      // 3
    L"Rehacer (Ctrl+Y)",       // 4
    L"Cortar (Ctrl+X)",        // 5
    L"Copiar (Ctrl+C)",        // 6
    L"Pegar (Ctrl+V)",         // 7
    L"Buscar... (Ctrl+F)",     // 8
    L"Archivos",               // 9
    L"Encabezados",            // 10
    L"Editor",                 // 11
    L"Dividida",               // 12
    L"Vista previa",           // 13
    L"Preferencias... (Ctrl+,)", // 14
};

int AddStrings(HWND hwnd) {
    std::vector<wchar_t> buf;
    for (const wchar_t* s : kStrings) {
        for (const wchar_t* p = s; *p; ++p) buf.push_back(*p);
        buf.push_back(L'\0');
    }
    buf.push_back(L'\0'); // Double NUL terminates the block.
    return static_cast<int>(SendMessageW(hwnd, TB_ADDSTRINGW, 0, reinterpret_cast<LPARAM>(buf.data())));
}

TBBUTTON Icon(int stdBitmap, UINT idCommand, int iString) {
    TBBUTTON b{};
    b.iBitmap = stdBitmap;
    b.idCommand = static_cast<int>(idCommand);
    b.fsState = TBSTATE_ENABLED;
    b.fsStyle = BTNS_BUTTON;
    b.iString = iString;
    return b;
}

TBBUTTON TextToggle(UINT idCommand, int iString, bool group) {
    TBBUTTON b{};
    b.iBitmap = I_IMAGENONE;
    b.idCommand = static_cast<int>(idCommand);
    b.fsState = TBSTATE_ENABLED;
    b.fsStyle = static_cast<BYTE>((group ? BTNS_CHECKGROUP : BTNS_CHECK) | BTNS_SHOWTEXT | BTNS_AUTOSIZE);
    b.iString = iString;
    return b;
}

TBBUTTON Separator(int widthPx) {
    TBBUTTON b{};
    b.iBitmap = widthPx;
    b.fsState = TBSTATE_ENABLED;
    b.fsStyle = BTNS_SEP;
    return b;
}

} // namespace

bool Toolbar::Create(HWND parent, HINSTANCE hInstance) {
    m_dpi = GetDpiForWindow(parent);
    if (m_dpi == 0) m_dpi = 96;

    m_hwnd = CreateWindowExW(0, TOOLBARCLASSNAME, nullptr,
                             WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS |
                                 CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN,
                             0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolbarControlId)),
                             hInstance, nullptr);
    if (!m_hwnd) return false;

    SendMessageW(m_hwnd, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageW(m_hwnd, TB_SETMAXTEXTROWS, 1, 0);
    // BTNS_SHOWTEXT (used by the panel/view-mode text buttons) only takes effect under this
    // extended style; it does not affect the icon buttons, which don't set BTNS_SHOWTEXT.
    SendMessageW(m_hwnd, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);
    ReloadImages();

    const int s = AddStrings(m_hwnd);
    const int sepW = Platform::ScaleForDpi(10, m_dpi);

    TBBUTTON buttons[] = {
        Icon(STD_FILENEW, IDM_FILE_NEW, s + 0),
        Icon(STD_FILEOPEN, IDM_FILE_OPEN, s + 1),
        Icon(STD_FILESAVE, IDM_FILE_SAVE, s + 2),
        Separator(sepW),
        Icon(STD_UNDO, IDM_EDIT_UNDO, s + 3),
        Icon(STD_REDOW, IDM_EDIT_REDO, s + 4),
        Separator(sepW),
        Icon(STD_CUT, IDM_EDIT_CUT, s + 5),
        Icon(STD_COPY, IDM_EDIT_COPY, s + 6),
        Icon(STD_PASTE, IDM_EDIT_PASTE, s + 7),
        Separator(sepW),
        Icon(STD_FIND, IDM_EDIT_FIND, s + 8),
        Separator(sepW),
        TextToggle(IDM_VIEW_EXPLORER_PANEL, s + 9, false),
        TextToggle(IDM_VIEW_OUTLINE_PANEL, s + 10, false),
        Separator(sepW),
        TextToggle(IDM_VIEW_EDITOR_ONLY, s + 11, true),
        TextToggle(IDM_VIEW_SPLIT, s + 12, true),
        TextToggle(IDM_VIEW_PREVIEW_ONLY, s + 13, true),
        Separator(sepW),
        Icon(STD_PROPERTIES, IDM_SETTINGS_PREFERENCES, s + 14),
    };
    SendMessageW(m_hwnd, TB_ADDBUTTONSW, ARRAYSIZE(buttons), reinterpret_cast<LPARAM>(buttons));

    RecalcSize();
    return true;
}

void Toolbar::ReloadImages() {
    if (!m_hwnd) return;
    const double scale = static_cast<double>(m_dpi) / 96.0;
    const UINT stdId = scale >= 1.5 ? IDB_STD_LARGE_COLOR : IDB_STD_SMALL_COLOR;
    SendMessageW(m_hwnd, TB_LOADIMAGES, stdId, reinterpret_cast<LPARAM>(HINST_COMMCTRL));
}

void Toolbar::RecalcSize() {
    if (!m_hwnd) return;
    SendMessageW(m_hwnd, TB_AUTOSIZE, 0, 0);
    RECT rc{};
    GetWindowRect(m_hwnd, &rc);
    m_height = rc.bottom - rc.top;
    if (m_height <= 0) m_height = Platform::ScaleForDpi(28, m_dpi);
}

void Toolbar::SetDpi(UINT dpi) {
    if (dpi == 0 || dpi == m_dpi) return;
    m_dpi = dpi;
    ReloadImages();
    RecalcSize();
}

void Toolbar::ApplyTheme(bool dark) {
    m_dark = dark;
    if (!m_hwnd) return;
    Platform::ApplyThemeToWindow(m_hwnd, dark);
    SetWindowTheme(m_hwnd, dark ? L"DarkMode_Explorer" : nullptr, nullptr);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void Toolbar::UpdateState(Config::ViewLayout mode, bool showOutline, bool showExplorer) {
    if (!m_hwnd) return;
    SendMessageW(m_hwnd, TB_CHECKBUTTON, IDM_VIEW_EDITOR_ONLY,
                MAKELPARAM(mode == Config::ViewLayout::EditorOnly, 0));
    SendMessageW(m_hwnd, TB_CHECKBUTTON, IDM_VIEW_SPLIT, MAKELPARAM(mode == Config::ViewLayout::Split, 0));
    SendMessageW(m_hwnd, TB_CHECKBUTTON, IDM_VIEW_PREVIEW_ONLY,
                MAKELPARAM(mode == Config::ViewLayout::PreviewOnly, 0));
    SendMessageW(m_hwnd, TB_CHECKBUTTON, IDM_VIEW_OUTLINE_PANEL, MAKELPARAM(showOutline, 0));
    SendMessageW(m_hwnd, TB_CHECKBUTTON, IDM_VIEW_EXPLORER_PANEL, MAKELPARAM(showExplorer, 0));
}

} // namespace Pluma::App
