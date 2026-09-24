#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>

#include "../config/settings.h"

namespace Pluma::App {

// The main toolbar: standard file/edit icon buttons plus text buttons for the panels and view
// modes. Button clicks are ordinary WM_COMMAND notifications routed to the same IDM_* ids the
// menu uses, so the main window's existing command handling drives it with no extra plumbing;
// this class only owns the control, its images, and keeping its pressed/checked state in sync.
class Toolbar {
public:
    bool Create(HWND parent, HINSTANCE hInstance);
    HWND GetHwnd() const noexcept { return m_hwnd; }

    // Current height in pixels at the active DPI (0 before Create).
    int GetHeight() const noexcept { return m_height; }

    void SetDpi(UINT dpi);
    void ApplyTheme(bool dark);

    // Reflects the current view mode and panel visibility in the pressed/checked buttons.
    void UpdateState(Config::ViewLayout mode, bool showOutline, bool showExplorer);

private:
    void ReloadImages();
    void RecalcSize();

    HWND m_hwnd = nullptr;
    UINT m_dpi = 96;
    int m_height = 0;
    bool m_dark = false;
};

} // namespace Pluma::App
