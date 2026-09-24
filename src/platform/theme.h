#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Pluma::Platform {

// Checks whether the user's Windows theme is currently dark mode for apps
bool IsSystemDarkMode();

// Configures the window's title bar to match dark/light mode via DWM
void ApplyThemeToWindow(HWND hwnd, bool darkMode);

} // namespace Pluma::Platform
