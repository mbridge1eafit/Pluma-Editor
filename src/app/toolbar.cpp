#include "toolbar.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>

#include "../platform/dpi.h"
#include "../platform/icon_font.h"
#include "../../res/resource.h"

namespace Pluma::App {

namespace {

constexpr wchar_t kToolbarClassName[] = L"PlumaToolbarClass";
constexpr int kHeight = 40;        // Logical pixels
constexpr int kButtonSize = 32;
constexpr int kSegmentHeight = 28;
constexpr int kIconSize = 16;

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

} // namespace

Toolbar::~Toolbar() {
    if (m_iconFont) DeleteObject(m_iconFont);
    if (m_textFont) DeleteObject(m_textFont);
}

bool Toolbar::Create(HWND parent, HINSTANCE hInstance) {
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

    using namespace Platform::Glyph;
    m_items = {
        {Kind::Button, IDM_FILE_NEW, kNew, nullptr, L"Nuevo (Ctrl+N)"},
        {Kind::Button, IDM_FILE_OPEN, kOpen, nullptr, L"Abrir... (Ctrl+O)"},
        {Kind::Button, IDM_FILE_SAVE, kSave, nullptr, L"Guardar (Ctrl+S)"},
        {Kind::Separator, 0, nullptr, nullptr, nullptr},
        {Kind::Button, IDM_EDIT_UNDO, kUndo, nullptr, L"Deshacer (Ctrl+Z)"},
        {Kind::Button, IDM_EDIT_REDO, kRedo, nullptr, L"Rehacer (Ctrl+Y)"},
        {Kind::Separator, 0, nullptr, nullptr, nullptr},
        {Kind::Button, IDM_EDIT_CUT, kCut, nullptr, L"Cortar (Ctrl+X)"},
        {Kind::Button, IDM_EDIT_COPY, kCopy, nullptr, L"Copiar (Ctrl+C)"},
        {Kind::Button, IDM_EDIT_PASTE, kPaste, nullptr, L"Pegar (Ctrl+V)"},
        {Kind::Separator, 0, nullptr, nullptr, nullptr},
        {Kind::Button, IDM_EDIT_FIND, kFind, nullptr, L"Buscar... (Ctrl+F)"},
        {Kind::Separator, 0, nullptr, nullptr, nullptr},
        {Kind::Toggle, IDM_VIEW_EXPLORER_PANEL, kFolder, nullptr, L"Explorador de archivos (Ctrl+Shift+F)"},
        {Kind::Toggle, IDM_VIEW_OUTLINE_PANEL, kList, nullptr, L"Panel de encabezados (Ctrl+Shift+E)"},
        {Kind::Separator, 0, nullptr, nullptr, nullptr},
        {Kind::Segment, IDM_VIEW_EDITOR_ONLY, nullptr, L"Editor", L"Solo editor (Ctrl+1)"},
        {Kind::Segment, IDM_VIEW_SPLIT, nullptr, L"Dividida", L"Vista dividida (Ctrl+2)"},
        {Kind::Segment, IDM_VIEW_PREVIEW_ONLY, nullptr, L"Vista previa", L"Solo vista previa (Ctrl+3)"},
        {Kind::Button, IDM_SETTINGS_PREFERENCES, kSettings, nullptr, L"Preferencias (Ctrl+,)", true},
    };

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
    return m_hwnd ? Platform::ScaleForDpi(kHeight, m_dpi) : 0;
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
        if (m_items[i].command == command && m_items[i].checked != checked) {
            m_items[i].checked = checked;
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
    if (m_textFont) DeleteObject(m_textFont);
    m_iconFont = Platform::CreateIconFont(Platform::ScaleForDpi(kIconSize, m_dpi));
    m_textFont = CreateFontW(-MulDiv(9, static_cast<int>(m_dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

void Toolbar::Layout() {
    if (!m_hwnd) return;
    RECT client{};
    GetClientRect(m_hwnd, &client);
    const int height = Platform::ScaleForDpi(kHeight, m_dpi);
    const int button = Platform::ScaleForDpi(kButtonSize, m_dpi);
    const int segmentH = Platform::ScaleForDpi(kSegmentHeight, m_dpi);
    const int gap = Platform::ScaleForDpi(2, m_dpi);
    const int separatorW = Platform::ScaleForDpi(13, m_dpi);
    const int pad = Platform::ScaleForDpi(6, m_dpi);
    const int segmentPad = Platform::ScaleForDpi(12, m_dpi);
    const int buttonTop = (height - button) / 2;
    const int segmentTop = (height - segmentH) / 2;

    HDC hdc = GetDC(m_hwnd);
    HGDIOBJ oldFont = SelectObject(hdc, m_textFont);

    int x = pad;
    int right = client.right - pad;
    for (auto& item : m_items) {
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
        default:
            item.rc = RECT{x, buttonTop, x + button, buttonTop + button};
            x += button + gap;
            break;
        }
    }

    SelectObject(hdc, oldFont);
    ReleaseDC(m_hwnd, hdc);

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
        if (m_items[i].kind != Kind::Segment || (i > 0 && m_items[i - 1].kind == Kind::Segment)) continue;
        RECT frame = m_items[i].rc;
        for (size_t j = i; j < m_items.size() && m_items[j].kind == Kind::Segment; ++j) {
            frame.right = m_items[j].rc.right;
        }
        FillRounded(hdc, frame, radius, p.segmentBg, p.segmentFrame);
    }

    for (size_t i = 0; i < m_items.size(); ++i) {
        const Item& item = m_items[i];
        const bool hot = static_cast<int>(i) == m_hot;
        const bool pressed = hot && static_cast<int>(i) == m_pressed;

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
        HGDIOBJ oldFont = SelectObject(hdc, m_iconFont);
        Platform::DrawGlyph(hdc, item.glyph, item.rc, item.checked ? p.accent : p.text);
        SelectObject(hdc, oldFont);
    }

    BitBlt(target, 0, 0, client.right, client.bottom, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(hdc);
}

int Toolbar::HitTest(POINT pt) const {
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].kind != Kind::Separator && PtInRect(&m_items[i].rc, pt)) return static_cast<int>(i);
    }
    return -1;
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
        if (hit >= 0) {
            m_pressed = hit;
            SetCapture(m_hwnd);
            InvalidateItem(hit);
        }
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
        if (m_pressed >= 0) {
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
