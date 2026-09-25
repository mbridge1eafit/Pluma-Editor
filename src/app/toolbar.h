#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <vector>

#include "../config/settings.h"

namespace Pluma::App {

// The main toolbar, custom drawn so it follows the app theme (light/dark) and DPI: icon buttons
// drawn with the system icon font, panel toggles, and a segmented control for the view mode.
// Clicks are posted to the parent as WM_COMMAND with the same IDM_* ids the menu uses.
class Toolbar {
public:
    Toolbar() = default;
    ~Toolbar();

    Toolbar(const Toolbar&) = delete;
    Toolbar& operator=(const Toolbar&) = delete;

    bool Create(HWND parent, HINSTANCE hInstance);
    HWND GetHwnd() const noexcept { return m_hwnd; }

    // Height in pixels at the current DPI.
    int GetHeight() const noexcept;

    void SetDpi(UINT dpi);
    void ApplyTheme(bool dark);

    // Reflects the current view mode and panel visibility in the checked buttons.
    void UpdateState(Config::ViewLayout mode, bool showOutline, bool showExplorer);

private:
    enum class Kind { Button, Toggle, Segment, Separator };
    struct Item {
        Kind kind;
        UINT command;
        const wchar_t* glyph;   // Button/Toggle
        const wchar_t* label;   // Segment
        const wchar_t* tooltip;
        bool alignRight = false;
        bool checked = false;
        RECT rc{};
    };

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void UpdateFonts();
    void Layout();
    void Paint(HDC hdc);
    int HitTest(POINT pt) const;
    void SetChecked(UINT command, bool checked);
    void InvalidateItem(int index);

    HWND m_hwnd = nullptr;
    HWND m_tooltip = nullptr;
    UINT m_dpi = 96;
    bool m_dark = false;
    HFONT m_iconFont = nullptr;
    HFONT m_textFont = nullptr;
    std::vector<Item> m_items;
    int m_hot = -1;
    int m_pressed = -1;
    bool m_trackingMouse = false;
};

} // namespace Pluma::App
