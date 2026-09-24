#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <uxtheme.h>

namespace Pluma::Platform {

enum class AppTheme {
    System,
    Dark,
    Light
};

// Initializes dark mode support globally for the process (uxtheme ordinals, dark scrollbars)
void InitializeDarkMode();

// Checks whether the Windows system is currently in dark mode for apps
bool IsSystemDarkMode();

// Determines whether dark mode is active given the user setting
bool IsDarkModeActive(AppTheme theme);

// Sets the preferred app mode for UxTheme (popups, dialogs, context menus)
void SetPreferredThemeMode(AppTheme theme);

// Configures the window's title bar and frame via DWM and UxTheme
void ApplyThemeToWindow(HWND hwnd, bool darkMode);

// Handles UAH messages for custom dark menu bar rendering
// Returns true if handled, storing result in *lr
bool HandleUAHMenuBarMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr, bool isDark);

// Forces window non-client frame and menu bar to repaint
void RefreshWindowFrame(HWND hwnd);

// Checks if a WM_SETTINGCHANGE message is related to system theme/color changes
bool IsColorSchemeChangeMessage(LPARAM lParam);

} // namespace Pluma::Platform
