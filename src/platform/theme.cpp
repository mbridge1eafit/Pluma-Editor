#include "theme.h"

#include <dwmapi.h>

namespace Pluma::Platform {

// DWM attribute constants for immersive dark mode
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

bool IsSystemDarkMode() {
    HKEY hKey = nullptr;
    const wchar_t* subKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 1;
        DWORD size = sizeof(value);
        DWORD type = REG_DWORD;
        LONG result = RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, &type,
                                      reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(hKey);
        if (result == ERROR_SUCCESS) {
            return value == 0; // 0 = Dark Mode, 1 = Light Mode
        }
    }
    return false; // Default to Light mode if registry key is absent
}

void ApplyThemeToWindow(HWND hwnd, bool darkMode) {
    if (!hwnd) {
        return;
    }

    BOOL value = darkMode ? TRUE : FALSE;
    // Try modern attribute 20 (Windows 10 20H1+, Windows 11)
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                                       &value, sizeof(value));
    if (FAILED(hr)) {
        // Fallback for Windows 10 1809 - 1909
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1,
                              &value, sizeof(value));
    }
}

} // namespace Pluma::Platform
