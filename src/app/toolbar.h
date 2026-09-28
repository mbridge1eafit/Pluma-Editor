#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <span>
#include <vector>

#include "../config/settings.h"

namespace Pluma::App {

// A custom drawn toolbar that follows the app theme (light/dark) and DPI: icon buttons drawn with
// the system icon font, toggles, drop-down menus and a segmented control. Clicks are posted to the
// parent as WM_COMMAND with the same IDM_* ids the menu uses. Buttons that do not fit are moved to
// an overflow ("...") menu at the end.
class Toolbar {
public:
    enum class Style {
        Main,   // File/edit commands, panels and view mode (top of the window)
        Format, // Markdown formatting (top of the editor)
    };

    Toolbar() = default;
    ~Toolbar();

    Toolbar(const Toolbar&) = delete;
    Toolbar& operator=(const Toolbar&) = delete;

    bool Create(HWND parent, HINSTANCE hInstance, Style style = Style::Main);
    HWND GetHwnd() const noexcept { return m_hwnd; }

    // Height in pixels at the current DPI.
    int GetHeight() const noexcept;

    void SetDpi(UINT dpi);
    void ApplyTheme(bool dark);

    // Main style: reflects the current view mode and panel visibility in the checked buttons.
    void UpdateState(Config::ViewLayout mode, bool showOutline, bool showExplorer);

    // Marks a button as active. For an entry of a drop-down menu, checks that entry in the menu
    // (unchecking its siblings) and highlights the drop-down button when `checked`.
    void SetChecked(UINT command, bool checked);

    struct MenuEntry {
        UINT command;
        const wchar_t* label;
    };

private:
    enum class Kind { Button, Toggle, Segment, Separator, Dropdown, Overflow };
    struct Item {
        Kind kind = Kind::Button;
        UINT command = 0;
        const wchar_t* glyph = nullptr;   // Icon font glyph; text is drawn when null
        const wchar_t* label = nullptr;   // Segment text, or the icon of a text button ("H")
        const wchar_t* tooltip = nullptr;
        std::span<const MenuEntry> menu;  // Dropdown entries
        bool alignRight = false;
        bool checked = false;
        UINT menuChecked = 0;             // Dropdown entry shown with a check mark
        bool hidden = false;              // Moved to the overflow menu
        RECT rc{};
    };

    static std::vector<Item> MainItems();
    static std::vector<Item> FormatItems();

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void UpdateFonts();
    void Layout();
    void Paint(HDC hdc);
    void PaintItemContent(HDC hdc, const Item& item, const RECT& rc, COLORREF color);
    int HitTest(POINT pt) const;
    void InvalidateItem(int index);
    void ShowItemMenu(int index);

    HWND m_hwnd = nullptr;
    HWND m_tooltip = nullptr;
    Style m_style = Style::Main;
    UINT m_dpi = 96;
    bool m_dark = false;
    HFONT m_iconFont = nullptr;
    HFONT m_chevronFont = nullptr;
    HFONT m_textFont = nullptr;
    HFONT m_glyphTextFont = nullptr;
    std::vector<Item> m_items;
    int m_hot = -1;
    int m_pressed = -1;
    bool m_trackingMouse = false;
    int m_menuClosedItem = -1;   // Drop-down whose menu was just dismissed by a click on it
    DWORD m_menuClosedTime = 0;
};

} // namespace Pluma::App
