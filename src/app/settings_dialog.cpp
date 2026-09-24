#include "settings_dialog.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <windowsx.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../platform/dpi.h"
#include "../platform/theme.h"
#include "../../res/resource.h"

namespace Pluma::App {

namespace {

constexpr int kHeaderIds[] = {IDC_SET_HEADER_APPEARANCE, IDC_SET_HEADER_EDITOR, IDC_SET_HEADER_VIEW,
                              IDC_SET_HEADER_EXPORT, IDC_SET_HEADER_WINDOWS};
constexpr int kCheckboxIds[] = {IDC_SET_WORDWRAP, IDC_SET_LINENUMBERS, IDC_SET_CURRENTLINE,
                                IDC_SET_USETABS, IDC_SET_OUTLINE, IDC_SET_STATUSBAR,
                                IDC_SET_SYNCSCROLL, IDC_SET_REMEMBERWINDOW, IDC_SET_REOPENLAST};
constexpr int kComboIds[] = {IDC_SET_THEME, IDC_SET_FONT, IDC_SET_FONTSIZE, IDC_SET_ZOOM,
                             IDC_SET_TABWIDTH, IDC_SET_EOL, IDC_SET_STARTVIEW, IDC_SET_PDFPAGE,
                             IDC_SET_PDFMARGIN, IDC_SET_HTMLTHEME};
constexpr int kButtonIds[] = {IDC_SET_ASSOC_BUTTON, IDC_SET_RESET, IDOK, IDCANCEL, IDC_SET_APPLY};

struct Colors {
    COLORREF background;
    COLORREF text;
    COLORREF disabledText;
    COLORREF header;
    COLORREF listBackground;
};

Colors GetColors(bool dark) {
    if (dark) return {RGB(32, 32, 32), RGB(225, 225, 225), RGB(120, 120, 120), RGB(96, 165, 230), RGB(43, 43, 43)};
    return {RGB(243, 243, 243), RGB(30, 30, 30), RGB(140, 140, 140), RGB(0, 95, 184), RGB(255, 255, 255)};
}

struct DialogState {
    Config::Settings settings;
    const SettingsDialogCallbacks* callbacks = nullptr;
    bool applied = false;
    bool dark = false;
    HBRUSH backgroundBrush = nullptr;
    HBRUSH listBrush = nullptr;
    HFONT headerFont = nullptr;

    ~DialogState() {
        if (backgroundBrush) DeleteObject(backgroundBrush);
        if (listBrush) DeleteObject(listBrush);
        if (headerFont) DeleteObject(headerFont);
    }
};

// ---------------------------------------------------------------------------------------------
// Combo box helpers: every item carries its value as item data.

void AddItem(HWND combo, const std::wstring& text, LPARAM value) {
    const auto index = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    if (index >= 0) SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(index), value);
}

void SelectValue(HWND combo, LPARAM value) {
    const auto count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
    for (LRESULT i = 0; i < count; ++i) {
        if (SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(i), 0) == value) {
            SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
            return;
        }
    }
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
}

LPARAM SelectedValue(HWND combo, LPARAM fallback) {
    const auto index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index < 0) return fallback;
    return static_cast<LPARAM>(SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(index), 0));
}

// Numeric list with the standard choices plus the current value when it is not one of them.
void FillNumbers(HWND combo, std::vector<int> values, int current, const wchar_t* suffix) {
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    if (std::find(values.begin(), values.end(), current) == values.end()) values.push_back(current);
    std::sort(values.begin(), values.end());
    for (int v : values) AddItem(combo, std::to_wstring(v) + suffix, v);
    SelectValue(combo, current);
}

int CALLBACK CollectMonospaceFont(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lParam) {
    auto* fonts = reinterpret_cast<std::vector<std::wstring>*>(lParam);
    // Vertical (@) variants are not usable in the editor.
    if ((lf->lfPitchAndFamily & 0x3) == FIXED_PITCH && lf->lfFaceName[0] != L'@') {
        fonts->emplace_back(lf->lfFaceName);
    }
    return 1;
}

std::vector<std::wstring> MonospaceFonts() {
    std::vector<std::wstring> fonts;
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    HDC hdc = GetDC(nullptr);
    EnumFontFamiliesExW(hdc, &lf, CollectMonospaceFont, reinterpret_cast<LPARAM>(&fonts), 0);
    ReleaseDC(nullptr, hdc);
    std::sort(fonts.begin(), fonts.end(), [](const std::wstring& a, const std::wstring& b) {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    });
    fonts.erase(std::unique(fonts.begin(), fonts.end()), fonts.end());
    return fonts;
}

// ---------------------------------------------------------------------------------------------

void FillControls(HWND hDlg, const Config::Settings& s) {
    HWND theme = GetDlgItem(hDlg, IDC_SET_THEME);
    SendMessageW(theme, CB_RESETCONTENT, 0, 0);
    AddItem(theme, L"Automático (según Windows)", static_cast<LPARAM>(Platform::AppTheme::System));
    AddItem(theme, L"Oscuro", static_cast<LPARAM>(Platform::AppTheme::Dark));
    AddItem(theme, L"Claro", static_cast<LPARAM>(Platform::AppTheme::Light));
    SelectValue(theme, static_cast<LPARAM>(s.theme));

    // Font names are kept in a vector owned by the combo (item data = index + 1; 0 = automatic).
    HWND font = GetDlgItem(hDlg, IDC_SET_FONT);
    SendMessageW(font, CB_RESETCONTENT, 0, 0);
    std::vector<std::wstring> fonts = MonospaceFonts();
    if (!s.editorFont.empty() && std::find(fonts.begin(), fonts.end(), s.editorFont) == fonts.end()) {
        fonts.push_back(s.editorFont);
    }
    AddItem(font, L"(Automática)", 0);
    int selected = 0;
    for (size_t i = 0; i < fonts.size(); ++i) {
        AddItem(font, fonts[i], static_cast<LPARAM>(i + 1));
        if (fonts[i] == s.editorFont) selected = static_cast<int>(i + 1);
    }
    SelectValue(font, selected);

    FillNumbers(GetDlgItem(hDlg, IDC_SET_FONTSIZE), {8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 28, 32},
                s.editorFontSize, L" pt");
    FillNumbers(GetDlgItem(hDlg, IDC_SET_ZOOM), {75, 90, 100, 110, 125, 150, 175, 200}, s.previewZoom, L" %");
    FillNumbers(GetDlgItem(hDlg, IDC_SET_TABWIDTH), {2, 3, 4, 8}, s.tabWidth, L" espacios");
    FillNumbers(GetDlgItem(hDlg, IDC_SET_PDFMARGIN), {10, 15, 20, 25, 30}, s.pdfMarginMm, L" mm");

    HWND eol = GetDlgItem(hDlg, IDC_SET_EOL);
    SendMessageW(eol, CB_RESETCONTENT, 0, 0);
    AddItem(eol, L"CRLF (Windows)", static_cast<LPARAM>(IO::LineEnding::CRLF));
    AddItem(eol, L"LF (Unix / macOS)", static_cast<LPARAM>(IO::LineEnding::LF));
    SelectValue(eol, static_cast<LPARAM>(s.newFileLineEnding));

    HWND view = GetDlgItem(hDlg, IDC_SET_STARTVIEW);
    SendMessageW(view, CB_RESETCONTENT, 0, 0);
    AddItem(view, L"Recordar la última", static_cast<LPARAM>(Config::StartupView::Remember));
    AddItem(view, L"Solo editor", static_cast<LPARAM>(Config::StartupView::EditorOnly));
    AddItem(view, L"Vista dividida", static_cast<LPARAM>(Config::StartupView::Split));
    AddItem(view, L"Solo vista previa", static_cast<LPARAM>(Config::StartupView::PreviewOnly));
    SelectValue(view, static_cast<LPARAM>(s.startupView));

    HWND page = GetDlgItem(hDlg, IDC_SET_PDFPAGE);
    SendMessageW(page, CB_RESETCONTENT, 0, 0);
    AddItem(page, L"A4 (210 × 297 mm)", static_cast<LPARAM>(Export::PageSize::A4));
    AddItem(page, L"Carta (8,5 × 11 in)", static_cast<LPARAM>(Export::PageSize::Letter));
    SelectValue(page, static_cast<LPARAM>(s.pdfPageSize));

    HWND html = GetDlgItem(hDlg, IDC_SET_HTMLTHEME);
    SendMessageW(html, CB_RESETCONTENT, 0, 0);
    AddItem(html, L"Automático", static_cast<LPARAM>(Export::HtmlTheme::Auto));
    AddItem(html, L"Claro", static_cast<LPARAM>(Export::HtmlTheme::Light));
    AddItem(html, L"Oscuro", static_cast<LPARAM>(Export::HtmlTheme::Dark));
    SelectValue(html, static_cast<LPARAM>(s.htmlTheme));

    CheckDlgButton(hDlg, IDC_SET_WORDWRAP, s.wordWrap ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SET_LINENUMBERS, s.showLineNumbers ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SET_CURRENTLINE, s.highlightCurrentLine ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SET_USETABS, s.useTabs ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SET_OUTLINE, s.showOutline ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SET_STATUSBAR, s.showStatusBar ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SET_SYNCSCROLL, s.syncScroll ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SET_REMEMBERWINDOW, s.rememberWindow ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SET_REOPENLAST, s.reopenLastFile ? BST_CHECKED : BST_UNCHECKED);
}

// Reads the controls into a copy of `base` (runtime-only values are kept).
Config::Settings ReadControls(HWND hDlg, const Config::Settings& base) {
    Config::Settings s = base;
    const auto value = [hDlg](int id, LPARAM fallback) { return SelectedValue(GetDlgItem(hDlg, id), fallback); };
    const auto checked = [hDlg](int id) { return IsDlgButtonChecked(hDlg, id) == BST_CHECKED; };

    s.theme = static_cast<Platform::AppTheme>(value(IDC_SET_THEME, static_cast<LPARAM>(s.theme)));

    HWND font = GetDlgItem(hDlg, IDC_SET_FONT);
    const auto fontIndex = SendMessageW(font, CB_GETCURSEL, 0, 0);
    s.editorFont.clear();
    if (fontIndex > 0) {
        const auto len = SendMessageW(font, CB_GETLBTEXTLEN, static_cast<WPARAM>(fontIndex), 0);
        if (len > 0) {
            std::wstring name(static_cast<size_t>(len) + 1, L'\0');
            SendMessageW(font, CB_GETLBTEXT, static_cast<WPARAM>(fontIndex), reinterpret_cast<LPARAM>(name.data()));
            name.resize(static_cast<size_t>(len));
            s.editorFont = std::move(name);
        }
    }

    s.editorFontSize = static_cast<int>(value(IDC_SET_FONTSIZE, s.editorFontSize));
    s.previewZoom = static_cast<int>(value(IDC_SET_ZOOM, s.previewZoom));
    s.tabWidth = static_cast<int>(value(IDC_SET_TABWIDTH, s.tabWidth));
    s.newFileLineEnding = static_cast<IO::LineEnding>(value(IDC_SET_EOL, static_cast<LPARAM>(s.newFileLineEnding)));
    s.startupView = static_cast<Config::StartupView>(value(IDC_SET_STARTVIEW, static_cast<LPARAM>(s.startupView)));
    s.pdfPageSize = static_cast<Export::PageSize>(value(IDC_SET_PDFPAGE, static_cast<LPARAM>(s.pdfPageSize)));
    s.pdfMarginMm = static_cast<int>(value(IDC_SET_PDFMARGIN, s.pdfMarginMm));
    s.htmlTheme = static_cast<Export::HtmlTheme>(value(IDC_SET_HTMLTHEME, static_cast<LPARAM>(s.htmlTheme)));

    s.wordWrap = checked(IDC_SET_WORDWRAP);
    s.showLineNumbers = checked(IDC_SET_LINENUMBERS);
    s.highlightCurrentLine = checked(IDC_SET_CURRENTLINE);
    s.useTabs = checked(IDC_SET_USETABS);
    s.showOutline = checked(IDC_SET_OUTLINE);
    s.showStatusBar = checked(IDC_SET_STATUSBAR);
    s.syncScroll = checked(IDC_SET_SYNCSCROLL);
    s.rememberWindow = checked(IDC_SET_REMEMBERWINDOW);
    s.reopenLastFile = checked(IDC_SET_REOPENLAST);
    s.Sanitize();
    return s;
}

void UpdateAssociationStatus(HWND hDlg, const DialogState& state) {
    const bool isDefault = state.callbacks->isDefaultEditor && state.callbacks->isDefaultEditor();
    SetDlgItemTextW(hDlg, IDC_SET_ASSOC_STATUS,
                    isDefault ? L"Pluma es el editor predeterminado de archivos Markdown (.md)."
                              : L"Pluma no es el editor predeterminado de archivos Markdown (.md).");
}

// ---------------------------------------------------------------------------------------------
// Theming

void ApplyDialogTheme(HWND hDlg, DialogState& state, bool dark) {
    state.dark = dark;
    const Colors c = GetColors(dark);
    if (state.backgroundBrush) DeleteObject(state.backgroundBrush);
    if (state.listBrush) DeleteObject(state.listBrush);
    state.backgroundBrush = CreateSolidBrush(c.background);
    state.listBrush = CreateSolidBrush(c.listBackground);

    Platform::ApplyThemeToWindow(hDlg, dark);
    for (int id : kComboIds) {
        HWND h = GetDlgItem(hDlg, id);
        Platform::ApplyThemeToWindow(h, dark);
        SetWindowTheme(h, dark ? L"DarkMode_CFD" : nullptr, nullptr);
    }
    for (int id : kButtonIds) {
        HWND h = GetDlgItem(hDlg, id);
        Platform::ApplyThemeToWindow(h, dark);
        SetWindowTheme(h, dark ? L"DarkMode_Explorer" : nullptr, nullptr);
    }
    for (int id : kCheckboxIds) {
        HWND h = GetDlgItem(hDlg, id);
        Platform::ApplyThemeToWindow(h, dark);
        SetWindowTheme(h, dark ? L"DarkMode_Explorer" : nullptr, nullptr);
    }
    RedrawWindow(hDlg, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

// Themed check boxes ignore WM_CTLCOLORSTATIC's text colour, which leaves black text on the
// dark background: in dark mode they are painted here (glyph from the theme, text in our colour).
bool PaintDarkCheckbox(const DialogState& state, NMCUSTOMDRAW* cd) {
    if (cd->dwDrawStage != CDDS_PREPAINT) return false;
    HWND hwnd = cd->hdr.hwndFrom;
    HDC hdc = cd->hdc;
    const RECT rc = cd->rc;
    const Colors c = GetColors(true);
    const UINT dpi = Platform::GetWindowDpi(hwnd);

    FillRect(hdc, &rc, state.backgroundBrush);

    const bool checked = Button_GetCheck(hwnd) == BST_CHECKED;
    const bool disabled = !IsWindowEnabled(hwnd);
    int part = checked ? CBS_CHECKEDNORMAL : CBS_UNCHECKEDNORMAL;
    if (disabled) part += 3;
    else if (cd->uItemState & CDIS_SELECTED) part += 2;
    else if (cd->uItemState & CDIS_HOT) part += 1;

    SIZE box{Platform::ScaleForDpi(13, dpi), Platform::ScaleForDpi(13, dpi)};
    HTHEME theme = OpenThemeData(hwnd, L"Button");
    if (theme) GetThemePartSize(theme, hdc, BP_CHECKBOX, part, nullptr, TS_DRAW, &box);
    RECT boxRc{rc.left, rc.top + (rc.bottom - rc.top - box.cy) / 2, rc.left + box.cx, 0};
    boxRc.bottom = boxRc.top + box.cy;
    if (theme) {
        DrawThemeBackground(theme, hdc, BP_CHECKBOX, part, &boxRc, nullptr);
        CloseThemeData(theme);
    } else {
        DrawFrameControl(hdc, &boxRc, DFC_BUTTON, DFCS_BUTTONCHECK | (checked ? DFCS_CHECKED : 0));
    }

    wchar_t text[256]{};
    GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
    HGDIOBJ oldFont = SelectObject(hdc, reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0)));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, disabled ? c.disabledText : c.text);

    const auto uiState = static_cast<UINT>(SendMessageW(hwnd, WM_QUERYUISTATE, 0, 0));
    UINT format = DT_SINGLELINE | DT_VCENTER | DT_LEFT;
    if (uiState & UISF_HIDEACCEL) format |= DT_HIDEPREFIX;
    RECT textRc = rc;
    textRc.left = boxRc.right + Platform::ScaleForDpi(5, dpi);
    DrawTextW(hdc, text, -1, &textRc, format);

    if ((cd->uItemState & CDIS_FOCUS) && !(uiState & UISF_HIDEFOCUS)) {
        RECT focusRc = textRc;
        DrawTextW(hdc, text, -1, &focusRc, format | DT_CALCRECT);
        const LONG textHeight = focusRc.bottom - focusRc.top;
        focusRc.top = textRc.top + (textRc.bottom - textRc.top - textHeight) / 2;
        focusRc.bottom = focusRc.top + textHeight;
        InflateRect(&focusRc, 1, 0);
        DrawFocusRect(hdc, &focusRc);
    }
    SelectObject(hdc, oldFont);
    return true;
}

bool IsCheckbox(int id) {
    return std::find(std::begin(kCheckboxIds), std::end(kCheckboxIds), id) != std::end(kCheckboxIds);
}

bool IsHeader(int id) {
    return std::find(std::begin(kHeaderIds), std::end(kHeaderIds), id) != std::end(kHeaderIds);
}

void CreateHeaderFont(HWND hDlg, DialogState& state) {
    if (state.headerFont) DeleteObject(state.headerFont);
    LOGFONTW lf{};
    const auto dialogFont = reinterpret_cast<HFONT>(SendMessageW(hDlg, WM_GETFONT, 0, 0));
    if (dialogFont && GetObjectW(dialogFont, sizeof(lf), &lf)) {
        lf.lfWeight = FW_SEMIBOLD;
        state.headerFont = CreateFontIndirectW(&lf);
    }
    for (int id : kHeaderIds) {
        SendMessageW(GetDlgItem(hDlg, id), WM_SETFONT, reinterpret_cast<WPARAM>(state.headerFont), TRUE);
    }
}

void ApplyChanges(HWND hDlg, DialogState& state) {
    state.settings = ReadControls(hDlg, state.settings);
    if (state.callbacks->apply) state.callbacks->apply(state.settings);
    state.applied = true;
    const bool dark = Platform::IsDarkModeActive(state.settings.theme);
    if (dark != state.dark) ApplyDialogTheme(hDlg, state, dark);
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hDlg, DWLP_USER));

    switch (msg) {
    case WM_INITDIALOG: {
        state = reinterpret_cast<DialogState*>(lParam);
        SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
        CreateHeaderFont(hDlg, *state);
        FillControls(hDlg, state->settings);
        UpdateAssociationStatus(hDlg, *state);
        ApplyDialogTheme(hDlg, *state, Platform::IsDarkModeActive(state->settings.theme));
        return TRUE; // Focus on the first control
    }

    case WM_DPICHANGED:
        // Dialog fonts are rescaled by the system; the header font is derived from them.
        if (state) PostMessageW(hDlg, WM_APP, 0, 0);
        break;

    case WM_APP:
        if (state) CreateHeaderFont(hDlg, *state);
        return TRUE;

    case WM_CTLCOLORDLG:
        if (!state) break;
        return reinterpret_cast<INT_PTR>(state->backgroundBrush);

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        if (!state) break;
        const Colors c = GetColors(state->dark);
        HDC hdc = reinterpret_cast<HDC>(wParam);
        const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
        SetTextColor(hdc, IsHeader(id) ? c.header : c.text);
        SetBkColor(hdc, c.background);
        return reinterpret_cast<INT_PTR>(state->backgroundBrush);
    }

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT: {
        if (!state || !state->dark) break;
        const Colors c = GetColors(true);
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, c.text);
        SetBkColor(hdc, c.listBackground);
        return reinterpret_cast<INT_PTR>(state->listBrush);
    }

    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (state && state->dark && hdr->code == NM_CUSTOMDRAW && IsCheckbox(static_cast<int>(hdr->idFrom))) {
            if (PaintDarkCheckbox(*state, reinterpret_cast<NMCUSTOMDRAW*>(lParam))) {
                SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, CDRF_SKIPDEFAULT);
                return TRUE;
            }
        }
        break;
    }

    case WM_COMMAND: {
        if (!state) break;
        switch (LOWORD(wParam)) {
        case IDOK:
            ApplyChanges(hDlg, *state);
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        case IDC_SET_APPLY:
            ApplyChanges(hDlg, *state);
            return TRUE;
        case IDC_SET_RESET: {
            // Defaults for the preferences; window placement, layout and last file are kept.
            Config::Settings defaults;
            const Config::Settings& cur = state->settings;
            defaults.lastView = cur.lastView;
            defaults.outlineWidth = cur.outlineWidth;
            defaults.splitRatio = cur.splitRatio;
            defaults.hasWindowRect = cur.hasWindowRect;
            defaults.windowX = cur.windowX;
            defaults.windowY = cur.windowY;
            defaults.windowWidth = cur.windowWidth;
            defaults.windowHeight = cur.windowHeight;
            defaults.windowMaximized = cur.windowMaximized;
            defaults.lastFile = cur.lastFile;
            FillControls(hDlg, defaults);
            return TRUE;
        }
        case IDC_SET_ASSOC_BUTTON:
            if (state->callbacks->makeDefaultEditor) state->callbacks->makeDefaultEditor();
            UpdateAssociationStatus(hDlg, *state);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

} // namespace

bool ShowSettingsDialog(HWND owner, HINSTANCE hInstance, const Config::Settings& settings,
                        const SettingsDialogCallbacks& callbacks) {
    DialogState state;
    state.settings = settings;
    state.callbacks = &callbacks;
    DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_SETTINGS), owner, DialogProc,
                    reinterpret_cast<LPARAM>(&state));
    return state.applied;
}

} // namespace Pluma::App
