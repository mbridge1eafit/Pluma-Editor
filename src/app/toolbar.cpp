#include "toolbar.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <string>

#include "../platform/dpi.h"
#include "../platform/icon_font.h"
#include "../../res/resource.h"

namespace Pluma::App {

namespace {

constexpr wchar_t kToolbarClassName[] = L"PlumaToolbarClass";
constexpr int kSegmentHeight = 28; // Logical pixels
constexpr int kIconSize = 16;
constexpr int kChevronSize = 8;
constexpr int kChevronWidth = 10;  // Extra width of a drop-down button
constexpr DWORD kMenuReopenGuardMs = 300;

struct Metrics {
    int height;
    int button;
};

Metrics GetMetrics(Toolbar::Style style) {
    return style == Toolbar::Style::Format ? Metrics{36, 28} : Metrics{40, 32};
}

struct Palette {
    COLORREF background;
    COLORREF border;
    COLORREF text;
    COLORREF hot;
    COLORREF pressed;
    COLORREF checkedBg;
    COLORREF accent;
    COLORREF segmentFrame;
    COLORREF segmentBg;
};

Palette GetPalette(bool dark) {
    if (dark) {
        return {RGB(32, 32, 32), RGB(48, 48, 48), RGB(222, 222, 222), RGB(55, 55, 55), RGB(70, 70, 70),
                RGB(38, 60, 86), RGB(96, 165, 230), RGB(62, 62, 62), RGB(40, 40, 40)};
    }
    return {RGB(249, 249, 249), RGB(224, 224, 224), RGB(32, 32, 32), RGB(234, 234, 234), RGB(220, 220, 220),
            RGB(212, 230, 247), RGB(0, 95, 184), RGB(208, 208, 208), RGB(255, 255, 255)};
}

void FillRounded(HDC hdc, const RECT& rc, int radius, COLORREF fill, COLORREF frame) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, frame);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

// "Negrita (Ctrl+B)" -> "Negrita\tCtrl+B": tooltips double as overflow menu labels.
std::wstring MenuText(const wchar_t* tooltip) {
    std::wstring text = tooltip ? tooltip : L"";
    const size_t open = text.rfind(L" (");
    if (open != std::wstring::npos && text.ends_with(L')')) {
        text = text.substr(0, open) + L'\t' + text.substr(open + 2, text.size() - open - 3);
    }
    return text;
}

constexpr Toolbar::MenuEntry kHeadingMenu[] = {
    {IDM_FORMAT_HEADING_0, L"&Párrafo"},
    {IDM_FORMAT_HEADING_1, L"Encabezado &1"},
    {IDM_FORMAT_HEADING_2, L"Encabezado &2"},
    {IDM_FORMAT_HEADING_3, L"Encabezado &3"},
    {IDM_FORMAT_HEADING_4, L"Encabezado &4"},
    {IDM_FORMAT_HEADING_5, L"Encabezado &5"},
    {IDM_FORMAT_HEADING_6, L"Encabezado &6"},
};

constexpr Toolbar::MenuEntry kDiagramMenu[] = {
    {IDM_FORMAT_DIAGRAM_FLOWCHART, L"Diagrama de &flujo"},
    {IDM_FORMAT_DIAGRAM_SEQUENCE, L"Diagrama de &secuencia"},
    {IDM_FORMAT_DIAGRAM_STATE, L"Diagrama de &estados"},
    {IDM_FORMAT_DIAGRAM_PIE, L"Gráfico &circular"},
};

} // namespace

std::vector<Toolbar::Item> Toolbar::MainItems() {
    using namespace Platform::Glyph;
    auto button = [](UINT command, const wchar_t* glyph, const wchar_t* tooltip) {
        return Item{.kind = Kind::Button, .command = command, .glyph = glyph, .tooltip = tooltip};
    };
    auto toggle = [](UINT command, const wchar_t* glyph, const wchar_t* tooltip) {
        return Item{.kind = Kind::Toggle, .command = command, .glyph = glyph, .tooltip = tooltip};
    };
    auto segment = [](UINT command, const wchar_t* label, const wchar_t* tooltip) {
        return Item{.kind = Kind::Segment, .command = command, .label = label, .tooltip = tooltip};
    };
    const Item separator{.kind = Kind::Separator};
    return {
        button(IDM_FILE_NEW, kNew, L"Nuevo (Ctrl+N)"),
        button(IDM_FILE_OPEN, kOpen, L"Abrir... (Ctrl+O)"),
        button(IDM_FILE_SAVE, kSave, L"Guardar (Ctrl+S)"),
        separator,
        button(IDM_EDIT_UNDO, kUndo, L"Deshacer (Ctrl+Z)"),
        button(IDM_EDIT_REDO, kRedo, L"Rehacer (Ctrl+Y)"),
        separator,
        button(IDM_EDIT_CUT, kCut, L"Cortar (Ctrl+X)"),
        button(IDM_EDIT_COPY, kCopy, L"Copiar (Ctrl+C)"),
        button(IDM_EDIT_PASTE, kPaste, L"Pegar (Ctrl+V)"),
        separator,
        button(IDM_EDIT_FIND, kFind, L"Buscar... (Ctrl+F)"),
        separator,
        toggle(IDM_VIEW_EXPLORER_PANEL, kExplorer, L"Explorador de archivos (Ctrl+Shift+F)"),
        toggle(IDM_VIEW_OUTLINE_PANEL, kList, L"Panel de encabezados (Ctrl+Shift+E)"),
        separator,
        segment(IDM_VIEW_EDITOR_ONLY, L"Editor", L"Solo editor (Ctrl+1)"),
        segment(IDM_VIEW_SPLIT, L"Dividida", L"Vista dividida (Ctrl+2)"),
        segment(IDM_VIEW_PREVIEW_ONLY, L"Vista previa", L"Solo vista previa (Ctrl+3)"),
        Item{.kind = Kind::Button, .command = IDM_SETTINGS_PREFERENCES, .glyph = kSettings,
             .tooltip = L"Preferencias (Ctrl+,)", .alignRight = true},
    };
}

// Grouped as in most Markdown editors (GitHub, EasyMDE, StackEdit, Typora): headings, inline
// emphasis, block structure (lists and quotes), then inserted elements.
std::vector<Toolbar::Item> Toolbar::FormatItems() {
    using namespace Platform::Glyph;
    auto button = [](UINT command, const wchar_t* glyph, const wchar_t* tooltip) {
        return Item{.kind = Kind::Button, .command = command, .glyph = glyph, .tooltip = tooltip};
    };
    auto toggle = [](UINT command, const wchar_t* glyph, const wchar_t* tooltip) {
        return Item{.kind = Kind::Toggle, .command = command, .glyph = glyph, .tooltip = tooltip};
    };
    const Item separator{.kind = Kind::Separator};
    return {
        Item{.kind = Kind::Dropdown, .command = IDM_FORMAT_HEADING_MENU, .label = L"H",
             .tooltip = L"Encabezado", .menu = kHeadingMenu},
        separator,
        button(IDM_FORMAT_BOLD, kBold, L"Negrita (Ctrl+B)"),
        button(IDM_FORMAT_ITALIC, kItalic, L"Cursiva (Ctrl+I)"),
        button(IDM_FORMAT_STRIKE, kStrikethrough, L"Tachado (Ctrl+Shift+X)"),
        button(IDM_FORMAT_CODE, kCode, L"Código en línea (Ctrl+Shift+C)"),
        separator,
        toggle(IDM_FORMAT_BULLET_LIST, kBulletList, L"Lista con viñetas (Ctrl+Shift+8)"),
        Item{.kind = Kind::Toggle, .command = IDM_FORMAT_NUMBERED_LIST, .label = L"1≡",
             .tooltip = L"Lista numerada (Ctrl+Shift+7)"},
        toggle(IDM_FORMAT_TASK_LIST, kTaskList, L"Lista de tareas (Ctrl+Shift+9)"),
        toggle(IDM_FORMAT_QUOTE, kQuote, L"Cita (Ctrl+Shift+Q)"),
        separator,
        button(IDM_FORMAT_LINK, kLink, L"Enlace (Ctrl+K)"),
        button(IDM_FORMAT_IMAGE, kImage, L"Imagen (Ctrl+Shift+I)"),
        button(IDM_FORMAT_TABLE, kTable, L"Tabla"),
        button(IDM_FORMAT_CODE_BLOCK, kCodeBlock, L"Bloque de código (Ctrl+Shift+K)"),
        button(IDM_FORMAT_HRULE, kHorizontalRule, L"Línea horizontal"),
        Item{.kind = Kind::Dropdown, .command = IDM_FORMAT_DIAGRAM_MENU, .glyph = kDiagram,
             .tooltip = L"Diagrama Mermaid", .menu = kDiagramMenu},
    };
}

Toolbar::~Toolbar() {
    if (m_iconFont) DeleteObject(m_iconFont);
    if (m_chevronFont) DeleteObject(m_chevronFont);
    if (m_textFont) DeleteObject(m_textFont);
    if (m_glyphTextFont) DeleteObject(m_glyphTextFont);
}

bool Toolbar::Create(HWND parent, HINSTANCE hInstance, Style style) {
    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
        wc.lpfnWndProc = StaticWndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kToolbarClassName;
        if (!RegisterClassExW(&wc)) return false;
        s_registered = true;
    }

    m_style = style;
    m_items = style == Style::Format ? FormatItems() : MainItems();
    m_items.push_back(Item{.kind = Kind::Overflow, .glyph = Platform::Glyph::kMore,
                           .tooltip = L"Más opciones", .hidden = true});

    m_hwnd = CreateWindowExW(0, kToolbarClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                             0, 0, 0, 0, parent, nullptr, hInstance, this);
    if (!m_hwnd) return false;
    m_dpi = GetDpiForWindow(m_hwnd);
    if (m_dpi == 0) m_dpi = 96;
    UpdateFonts();

    m_tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                                WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT,
                                CW_USEDEFAULT, CW_USEDEFAULT, m_hwnd, nullptr, hInstance, nullptr);
    if (m_tooltip) {
        for (size_t i = 0; i < m_items.size(); ++i) {
            if (!m_items[i].tooltip) continue;
            TTTOOLINFOW ti{};
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_SUBCLASS;
            ti.hwnd = m_hwnd;
            ti.uId = i;
            ti.lpszText = const_cast<wchar_t*>(m_items[i].tooltip);
            SendMessageW(m_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
        }
    }
    return true;
}

int Toolbar::GetHeight() const noexcept {
    return m_hwnd ? Platform::ScaleForDpi(GetMetrics(m_style).height, m_dpi) : 0;
}

void Toolbar::SetDpi(UINT dpi) {
    if (dpi == 0 || dpi == m_dpi) return;
    m_dpi = dpi;
    UpdateFonts();
    Layout();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Toolbar::ApplyTheme(bool dark) {
    m_dark = dark;
    if (m_tooltip) SetWindowTheme(m_tooltip, dark ? L"DarkMode_Explorer" : nullptr, nullptr);
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void Toolbar::UpdateState(Config::ViewLayout mode, bool showOutline, bool showExplorer) {
    SetChecked(IDM_VIEW_EDITOR_ONLY, mode == Config::ViewLayout::EditorOnly);
    SetChecked(IDM_VIEW_SPLIT, mode == Config::ViewLayout::Split);
    SetChecked(IDM_VIEW_PREVIEW_ONLY, mode == Config::ViewLayout::PreviewOnly);
    SetChecked(IDM_VIEW_OUTLINE_PANEL, showOutline);
    SetChecked(IDM_VIEW_EXPLORER_PANEL, showExplorer);
}

void Toolbar::SetChecked(UINT command, bool checked) {
    for (size_t i = 0; i < m_items.size(); ++i) {
        Item& item = m_items[i];
        const bool inMenu = std::ranges::any_of(item.menu, [&](const MenuEntry& e) { return e.command == command; });
        if (inMenu) {
            item.menuChecked = command;
        } else if (item.command != command) {
            continue;
        }
        if (item.checked != checked) {
            item.checked = checked;
            InvalidateItem(static_cast<int>(i));
        }
    }
}

void Toolbar::InvalidateItem(int index) {
    if (!m_hwnd || index < 0 || index >= static_cast<int>(m_items.size())) return;
    RECT rc = m_items[index].rc;
    InflateRect(&rc, 2, 2);
    InvalidateRect(m_hwnd, &rc, FALSE);
}

void Toolbar::UpdateFonts() {
    if (m_iconFont) DeleteObject(m_iconFont);
    if (m_chevronFont) DeleteObject(m_chevronFont);
    if (m_textFont) DeleteObject(m_textFont);
    if (m_glyphTextFont) DeleteObject(m_glyphTextFont);
    m_iconFont = Platform::CreateIconFont(Platform::ScaleForDpi(kIconSize, m_dpi));
    m_chevronFont = Platform::CreateIconFont(Platform::ScaleForDpi(kChevronSize, m_dpi));
    m_textFont = CreateFontW(-MulDiv(9, static_cast<int>(m_dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    // Text drawn in place of an icon ("H", "1≡"), sized to match the glyphs.
    m_glyphTextFont = CreateFontW(-Platform::ScaleForDpi(15, m_dpi), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

void Toolbar::Layout() {
    if (!m_hwnd) return;
    RECT client{};
    GetClientRect(m_hwnd, &client);
    const Metrics metrics = GetMetrics(m_style);
    const int height = Platform::ScaleForDpi(metrics.height, m_dpi);
    const int button = Platform::ScaleForDpi(metrics.button, m_dpi);
    const int segmentH = Platform::ScaleForDpi(kSegmentHeight, m_dpi);
    const int gap = Platform::ScaleForDpi(2, m_dpi);
    const int separatorW = Platform::ScaleForDpi(13, m_dpi);
    const int pad = Platform::ScaleForDpi(6, m_dpi);
    const int segmentPad = Platform::ScaleForDpi(12, m_dpi);
    const int chevronW = Platform::ScaleForDpi(kChevronWidth, m_dpi);
    const int buttonTop = (height - button) / 2;
    const int segmentTop = (height - segmentH) / 2;

    HDC hdc = GetDC(m_hwnd);
    HGDIOBJ oldFont = SelectObject(hdc, m_textFont);

    int x = pad;
    int right = client.right - pad;
    for (auto& item : m_items) {
        item.hidden = false;
        if (item.kind == Kind::Overflow) continue;
        if (item.alignRight) {
            item.rc = RECT{right - button, buttonTop, right, buttonTop + button};
            right -= button + gap;
            continue;
        }
        switch (item.kind) {
        case Kind::Separator:
            item.rc = RECT{x, 0, x + separatorW, height};
            x += separatorW;
            break;
        case Kind::Segment: {
            SIZE size{};
            GetTextExtentPoint32W(hdc, item.label, static_cast<int>(wcslen(item.label)), &size);
            const int w = size.cx + 2 * segmentPad;
            item.rc = RECT{x, segmentTop, x + w, segmentTop + segmentH};
            x += w; // Segments are contiguous
            break;
        }
        default: {
            const int w = item.kind == Kind::Dropdown ? button + chevronW : button;
            item.rc = RECT{x, buttonTop, x + w, buttonTop + button};
            x += w + gap;
            break;
        }
        }
    }

    SelectObject(hdc, oldFont);
    ReleaseDC(m_hwnd, hdc);

    // Items past the right edge move to the overflow menu, whose button takes their place.
    Item& overflow = m_items.back();
    overflow.hidden = true;
    overflow.rc = RECT{};
    size_t cut = m_items.size() - 1;
    for (size_t i = 0; i + 1 < m_items.size(); ++i) {
        if (!m_items[i].alignRight && m_items[i].rc.right > right) {
            cut = i;
            break;
        }
    }
    if (cut < m_items.size() - 1) {
        const int limit = right - button - gap;
        while (cut > 0 && !m_items[cut - 1].alignRight && m_items[cut - 1].rc.right > limit) --cut;
        // Never split the segmented control, nor leave a separator dangling before the button.
        while (cut > 0 && m_items[cut].kind == Kind::Segment && m_items[cut - 1].kind == Kind::Segment) --cut;
        while (cut > 0 && m_items[cut - 1].kind == Kind::Separator) --cut;
        int lastRight = pad - gap;
        for (size_t i = 0; i + 1 < m_items.size(); ++i) {
            if (m_items[i].alignRight) continue;
            if (i >= cut) {
                m_items[i].hidden = true;
                m_items[i].rc = RECT{};
            } else {
                lastRight = m_items[i].rc.right;
            }
        }
        overflow.hidden = false;
        overflow.rc = RECT{lastRight + gap, buttonTop, lastRight + gap + button, buttonTop + button};
    }

    if (m_tooltip) {
        for (size_t i = 0; i < m_items.size(); ++i) {
            if (!m_items[i].tooltip) continue;
            TTTOOLINFOW ti{};
            ti.cbSize = sizeof(ti);
            ti.hwnd = m_hwnd;
            ti.uId = i;
            ti.rect = m_items[i].rc;
            SendMessageW(m_tooltip, TTM_NEWTOOLRECTW, 0, reinterpret_cast<LPARAM>(&ti));
        }
    }
}

void Toolbar::PaintItemContent(HDC hdc, const Item& item, const RECT& rc, COLORREF color) {
    if (item.glyph) {
        HGDIOBJ oldFont = SelectObject(hdc, m_iconFont);
        Platform::DrawGlyph(hdc, item.glyph, rc, color);
        SelectObject(hdc, oldFont);
    } else if (item.label) {
        HGDIOBJ oldFont = SelectObject(hdc, m_glyphTextFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, color);
        RECT textRc = rc;
        DrawTextW(hdc, item.label, -1, &textRc, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
    }
}

void Toolbar::Paint(HDC target) {
    RECT client{};
    GetClientRect(m_hwnd, &client);
    if (client.right <= 0 || client.bottom <= 0) return;

    HDC hdc = CreateCompatibleDC(target);
    HBITMAP bmp = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ oldBmp = SelectObject(hdc, bmp);

    const Palette p = GetPalette(m_dark);
    HBRUSH bg = CreateSolidBrush(p.background);
    FillRect(hdc, &client, bg);
    DeleteObject(bg);
    RECT border{0, client.bottom - 1, client.right, client.bottom};
    HBRUSH borderBrush = CreateSolidBrush(p.border);
    FillRect(hdc, &border, borderBrush);
    DeleteObject(borderBrush);

    const int radius = Platform::ScaleForDpi(8, m_dpi);
    const int inset = Platform::ScaleForDpi(3, m_dpi);

    // Frame behind each run of contiguous segments.
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].kind != Kind::Segment || m_items[i].hidden) continue;
        if (i > 0 && m_items[i - 1].kind == Kind::Segment) continue;
        RECT frame = m_items[i].rc;
        for (size_t j = i; j < m_items.size() && m_items[j].kind == Kind::Segment && !m_items[j].hidden; ++j) {
            frame.right = m_items[j].rc.right;
        }
        FillRounded(hdc, frame, radius, p.segmentBg, p.segmentFrame);
    }

    for (size_t i = 0; i < m_items.size(); ++i) {
        const Item& item = m_items[i];
        if (item.hidden) continue;
        const bool hot = static_cast<int>(i) == m_hot;
        const bool pressed = static_cast<int>(i) == m_pressed && (hot || item.kind == Kind::Dropdown ||
                                                                  item.kind == Kind::Overflow);

        if (item.kind == Kind::Separator) {
            const int mid = (item.rc.left + item.rc.right) / 2;
            const int lineH = Platform::ScaleForDpi(20, m_dpi);
            RECT line{mid, (item.rc.bottom - lineH) / 2, mid + 1, (item.rc.bottom + lineH) / 2};
            HBRUSH b = CreateSolidBrush(p.border);
            FillRect(hdc, &line, b);
            DeleteObject(b);
            continue;
        }

        if (item.kind == Kind::Segment) {
            RECT inner = item.rc;
            InflateRect(&inner, -inset, -inset);
            if (item.checked) {
                FillRounded(hdc, inner, radius - inset, p.checkedBg, p.checkedBg);
            } else if (pressed || hot) {
                FillRounded(hdc, inner, radius - inset, pressed ? p.pressed : p.hot, pressed ? p.pressed : p.hot);
            }
            HGDIOBJ oldFont = SelectObject(hdc, m_textFont);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, item.checked ? p.accent : p.text);
            RECT textRc = item.rc;
            DrawTextW(hdc, item.label, -1, &textRc, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
            SelectObject(hdc, oldFont);
            continue;
        }

        if (pressed || hot || item.checked) {
            const COLORREF fill = pressed ? p.pressed : hot ? p.hot : p.checkedBg;
            FillRounded(hdc, item.rc, Platform::ScaleForDpi(6, m_dpi), fill, fill);
        }
        const COLORREF color = item.checked ? p.accent : p.text;
        if (item.kind == Kind::Dropdown) {
            const int chevronW = Platform::ScaleForDpi(kChevronWidth, m_dpi);
            RECT content = item.rc;
            content.right -= chevronW;
            RECT chevron = item.rc;
            chevron.left = content.right - Platform::ScaleForDpi(3, m_dpi);
            chevron.right -= Platform::ScaleForDpi(3, m_dpi);
            PaintItemContent(hdc, item, content, color);
            HGDIOBJ oldFont = SelectObject(hdc, m_chevronFont);
            Platform::DrawGlyph(hdc, Platform::Glyph::kChevronDown, chevron, color);
            SelectObject(hdc, oldFont);
        } else {
            PaintItemContent(hdc, item, item.rc, color);
        }
    }

    BitBlt(target, 0, 0, client.right, client.bottom, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(hdc);
}

int Toolbar::HitTest(POINT pt) const {
    for (size_t i = 0; i < m_items.size(); ++i) {
        const Item& item = m_items[i];
        if (item.kind != Kind::Separator && !item.hidden && PtInRect(&item.rc, pt)) return static_cast<int>(i);
    }
    return -1;
}

void Toolbar::ShowItemMenu(int index) {
    const Item& item = m_items[index];
    auto buildDropdown = [](const Item& dropdown) {
        HMENU menu = CreatePopupMenu();
        for (const MenuEntry& entry : dropdown.menu) {
            const UINT flags = MF_STRING | (entry.command == dropdown.menuChecked ? MF_CHECKED : 0);
            AppendMenuW(menu, flags, entry.command, entry.label);
        }
        return menu;
    };

    HMENU menu = nullptr;
    if (item.kind == Kind::Overflow) {
        menu = CreatePopupMenu();
        bool pendingSeparator = false;
        for (const Item& hidden : m_items) {
            if (!hidden.hidden || hidden.kind == Kind::Overflow) continue;
            if (hidden.kind == Kind::Separator) {
                pendingSeparator = GetMenuItemCount(menu) > 0;
                continue;
            }
            if (pendingSeparator) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            pendingSeparator = false;
            const std::wstring text = MenuText(hidden.tooltip);
            if (hidden.kind == Kind::Dropdown) {
                AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(buildDropdown(hidden)), text.c_str());
            } else {
                AppendMenuW(menu, MF_STRING | (hidden.checked ? MF_CHECKED : 0), hidden.command, text.c_str());
            }
        }
    } else {
        menu = buildDropdown(item);
    }
    if (!menu) return;

    RECT rc = item.rc;
    MapWindowPoints(m_hwnd, nullptr, reinterpret_cast<POINT*>(&rc), 2);
    TPMPARAMS params{sizeof(params), rc};
    m_pressed = index;
    InvalidateItem(index);
    UpdateWindow(m_hwnd);
    const UINT command = static_cast<UINT>(TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_VERTICAL, rc.left, rc.bottom, m_hwnd, &params));
    DestroyMenu(menu);
    m_pressed = -1;
    InvalidateItem(index);

    // A click on this same button dismissed the menu: the button-down that follows must not reopen it.
    POINT pt{};
    GetCursorPos(&pt);
    ScreenToClient(m_hwnd, &pt);
    if (HitTest(pt) == index && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        m_menuClosedItem = index;
        m_menuClosedTime = GetTickCount();
    }
    if (command != 0) {
        PostMessageW(GetParent(m_hwnd), WM_COMMAND, MAKEWPARAM(command, 0), reinterpret_cast<LPARAM>(m_hwnd));
    }
}

LRESULT CALLBACK Toolbar::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Toolbar* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Toolbar*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<Toolbar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Toolbar::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        Layout();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hwnd, &ps);
        Paint(hdc);
        EndPaint(m_hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        const int hit = HitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        if (hit != m_hot) {
            InvalidateItem(m_hot);
            m_hot = hit;
            InvalidateItem(m_hot);
        }
        if (!m_trackingMouse) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, m_hwnd, 0};
            m_trackingMouse = TrackMouseEvent(&tme) != FALSE;
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        if (m_hot >= 0 && m_pressed < 0) {
            InvalidateItem(m_hot);
            m_hot = -1;
        }
        return 0;

    case WM_LBUTTONDOWN: {
        const int hit = HitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        if (hit < 0) return 0;
        const Kind kind = m_items[hit].kind;
        if (kind == Kind::Dropdown || kind == Kind::Overflow) {
            // Menu buttons open on press, like the menu bar.
            const bool justClosed = hit == m_menuClosedItem && GetTickCount() - m_menuClosedTime < kMenuReopenGuardMs;
            m_menuClosedItem = -1;
            if (!justClosed) ShowItemMenu(hit);
            return 0;
        }
        m_pressed = hit;
        SetCapture(m_hwnd);
        InvalidateItem(hit);
        return 0;
    }

    case WM_LBUTTONUP: {
        if (m_pressed < 0) return 0;
        const int pressed = m_pressed;
        const int hit = HitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        m_pressed = -1;
        ReleaseCapture();
        InvalidateItem(pressed);
        if (hit == pressed) {
            PostMessageW(GetParent(m_hwnd), WM_COMMAND, MAKEWPARAM(m_items[pressed].command, 0),
                         reinterpret_cast<LPARAM>(m_hwnd));
        }
        return 0;
    }

    case WM_CAPTURECHANGED:
        if (m_pressed >= 0 && m_items[m_pressed].kind != Kind::Dropdown &&
            m_items[m_pressed].kind != Kind::Overflow) {
            InvalidateItem(m_pressed);
            m_pressed = -1;
        }
        return 0;

    case WM_DESTROY:
        if (m_tooltip) {
            DestroyWindow(m_tooltip);
            m_tooltip = nullptr;
        }
        break;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

} // namespace Pluma::App
