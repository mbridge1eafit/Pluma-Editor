#include "theme.h"

#include <dwmapi.h>
#include <vssym32.h>
#include <algorithm>

namespace Pluma::Platform {

namespace {

// DWM attribute constants for immersive dark mode
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Windows 11 DWM attributes (Build >= 22000)
constexpr DWORD kDwmwaWindowCornerPreference = 33;
constexpr DWORD kDwmwaBorderColor = 34;
constexpr DWORD kDwmwaCaptionColor = 35;
constexpr DWORD kDwmwaTextColor = 36;

// Undocumented UAH menu bar messages
#define WM_UAHDRAWMENU         0x0091
#define WM_UAHDRAWMENUITEM     0x0092
#define WM_UAHMEASUREMENUITEM  0x0094

typedef union tagUAHMENUITEMMETRICS
{
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizeBar[2];
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizePopup[4];
} UAHMENUITEMMETRICS;

typedef struct tagUAHMENUPOPUPMETRICS
{
    DWORD rgcx[4];
    DWORD fUpdateMaxWidths : 2;
} UAHMENUPOPUPMETRICS;

typedef struct tagUAHMENU
{
    HMENU hmenu;
    HDC hdc;
    DWORD dwFlags;
} UAHMENU;

typedef struct tagUAHMENUITEM
{
    int iPosition;
    UAHMENUITEMMETRICS umim;
    UAHMENUPOPUPMETRICS umpm;
} UAHMENUITEM;

typedef struct UAHDRAWMENUITEM
{
    DRAWITEMSTRUCT dis;
    UAHMENU um;
    UAHMENUITEM umi;
} UAHDRAWMENUITEM;

typedef struct tagUAHMEASUREMENUITEM
{
    MEASUREITEMSTRUCT mis;
    UAHMENU um;
    UAHMENUITEM umi;
} UAHMEASUREMENUITEM;

enum PreferredAppMode {
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
    Max
};

using fnRtlGetNtVersionNumbers = void(WINAPI*)(LPDWORD major, LPDWORD minor, LPDWORD build);
using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode mode);
using fnAllowDarkModeForApp = bool(WINAPI*)(bool allow);
using fnAllowDarkModeForWindow = bool(WINAPI*)(HWND hWnd, bool allow);
using fnFlushMenuThemes = void(WINAPI*)();
using fnRefreshImmersiveColorPolicyState = void(WINAPI*)();
using fnShouldAppsUseDarkMode = bool(WINAPI*)();

static fnSetPreferredAppMode _SetPreferredAppMode = nullptr;
static fnAllowDarkModeForApp _AllowDarkModeForApp = nullptr;
static fnAllowDarkModeForWindow _AllowDarkModeForWindow = nullptr;
static fnFlushMenuThemes _FlushMenuThemes = nullptr;
static fnRefreshImmersiveColorPolicyState _RefreshImmersiveColorPolicyState = nullptr;
static fnShouldAppsUseDarkMode _ShouldAppsUseDarkMode = nullptr;

static bool g_darkModeSupported = false;
static DWORD g_buildNumber = 0;



void UAHDrawMenuNCBottomLine(HWND hWnd, COLORREF color) {
    MENUBARINFO mbi = { sizeof(mbi) };
    if (!GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi)) {
        return;
    }

    RECT rcClient = { 0 };
    GetClientRect(hWnd, &rcClient);
    MapWindowPoints(hWnd, nullptr, reinterpret_cast<POINT*>(&rcClient), 2);

    RECT rcWindow = { 0 };
    GetWindowRect(hWnd, &rcWindow);

    OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);

    RECT rcLine = rcClient;
    rcLine.bottom = rcLine.top;
    rcLine.top--;

    HDC hdc = GetWindowDC(hWnd);
    if (hdc) {
        HBRUSH hbr = CreateSolidBrush(color);
        FillRect(hdc, &rcLine, hbr);
        DeleteObject(hbr);
        ReleaseDC(hWnd, hdc);
    }
}

} // namespace

void InitializeDarkMode() {
    auto RtlGetNtVersionNumbers = reinterpret_cast<fnRtlGetNtVersionNumbers>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetNtVersionNumbers"));
    if (RtlGetNtVersionNumbers) {
        DWORD major = 0, minor = 0;
        RtlGetNtVersionNumbers(&major, &minor, &g_buildNumber);
        g_buildNumber &= ~0xF0000000;
        if (major == 10 && minor == 0 && g_buildNumber >= 17763) {
            HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (hUxtheme) {
                _RefreshImmersiveColorPolicyState = reinterpret_cast<fnRefreshImmersiveColorPolicyState>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(104)));
                _ShouldAppsUseDarkMode = reinterpret_cast<fnShouldAppsUseDarkMode>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132)));
                _AllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133)));

                auto ord135 = GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
                if (g_buildNumber < 18362) {
                    _AllowDarkModeForApp = reinterpret_cast<fnAllowDarkModeForApp>(ord135);
                } else {
                    _SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(ord135);
                }

                _FlushMenuThemes = reinterpret_cast<fnFlushMenuThemes>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136)));

                if (_AllowDarkModeForWindow && (_AllowDarkModeForApp || _SetPreferredAppMode)) {
                    g_darkModeSupported = true;

                    if (_SetPreferredAppMode) {
                        _SetPreferredAppMode(AllowDark);
                    } else if (_AllowDarkModeForApp) {
                        _AllowDarkModeForApp(true);
                    }

                    if (_RefreshImmersiveColorPolicyState) {
                        _RefreshImmersiveColorPolicyState();
                    }
                }
            }
        }
    }
}

bool IsSystemDarkMode() {
    if (_ShouldAppsUseDarkMode) {
        return _ShouldAppsUseDarkMode();
    }

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
    return false;
}

bool IsDarkModeActive(AppTheme theme) {
    switch (theme) {
    case AppTheme::Dark: return true;
    case AppTheme::Light: return false;
    case AppTheme::System: return IsSystemDarkMode();
    }
    return false;
}

void SetPreferredThemeMode(AppTheme theme) {
    if (_SetPreferredAppMode) {
        switch (theme) {
        case AppTheme::Dark:
            _SetPreferredAppMode(ForceDark);
            break;
        case AppTheme::Light:
            _SetPreferredAppMode(ForceLight);
            break;
        case AppTheme::System:
            _SetPreferredAppMode(AllowDark);
            break;
        }
    } else if (_AllowDarkModeForApp) {
        _AllowDarkModeForApp(theme == AppTheme::Dark || (theme == AppTheme::System && IsSystemDarkMode()));
    }

    if (_FlushMenuThemes) {
        _FlushMenuThemes();
    }
}

void ApplyThemeToWindow(HWND hwnd, bool darkMode) {
    if (!hwnd) {
        return;
    }

    if (_AllowDarkModeForWindow) {
        _AllowDarkModeForWindow(hwnd, darkMode);
    }

    if (_FlushMenuThemes) {
        _FlushMenuThemes();
    }

    BOOL value = darkMode ? TRUE : FALSE;
    // Modern attribute 20 (Windows 10 20H1+, Windows 11)
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                                       &value, sizeof(value));
    if (FAILED(hr)) {
        // Fallback for Windows 10 1809 - 1909
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1,
                              &value, sizeof(value));
    }

    // Windows 11 seamless title bar and border coloring
    if (g_buildNumber >= 22000) {
        if (darkMode) {
            COLORREF captionCol = RGB(30, 30, 30); // Seamlessly matches editor & menu bar
            COLORREF textCol = RGB(240, 240, 240);
            COLORREF borderCol = RGB(45, 45, 45);
            DwmSetWindowAttribute(hwnd, kDwmwaCaptionColor, &captionCol, sizeof(captionCol));
            DwmSetWindowAttribute(hwnd, kDwmwaTextColor, &textCol, sizeof(textCol));
            DwmSetWindowAttribute(hwnd, kDwmwaBorderColor, &borderCol, sizeof(borderCol));
        } else {
            COLORREF defaultCol = 0xFFFFFFFF; // DWMWA_COLOR_DEFAULT
            DwmSetWindowAttribute(hwnd, kDwmwaCaptionColor, &defaultCol, sizeof(defaultCol));
            DwmSetWindowAttribute(hwnd, kDwmwaTextColor, &defaultCol, sizeof(defaultCol));
            DwmSetWindowAttribute(hwnd, kDwmwaBorderColor, &defaultCol, sizeof(defaultCol));
        }
    }
}

bool HandleUAHMenuBarMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr, bool isDark) {
    if (!isDark) {
        return false;
    }

    switch (message) {
    case WM_UAHDRAWMENU: {
        auto* pUDM = reinterpret_cast<UAHMENU*>(lParam);
        if (!pUDM || !pUDM->hdc) return false;

        MENUBARINFO mbi = { sizeof(mbi) };
        if (GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi)) {
            RECT rcWindow{};
            GetWindowRect(hWnd, &rcWindow);
            RECT rc = mbi.rcBar;
            OffsetRect(&rc, -rcWindow.left, -rcWindow.top);

            rc.left = 0;
            rc.right = rcWindow.right - rcWindow.left;

            HBRUSH hbr = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(pUDM->hdc, &rc, hbr);
            DeleteObject(hbr);
        }
        *lr = 0;
        return true;
    }

    case WM_UAHDRAWMENUITEM: {
        auto* pUDMI = reinterpret_cast<UAHDRAWMENUITEM*>(lParam);
        if (!pUDMI || !pUDMI->um.hdc) return false;

        wchar_t menuString[256] = { 0 };
        MENUITEMINFOW mii{ sizeof(mii) };
        mii.fMask = MIIM_STRING;
        mii.dwTypeData = menuString;
        mii.cch = ARRAYSIZE(menuString) - 1;
        GetMenuItemInfoW(pUDMI->um.hmenu, static_cast<UINT>(pUDMI->umi.iPosition), TRUE, &mii);

        COLORREF bgCol = RGB(30, 30, 30);
        COLORREF textCol = RGB(220, 220, 220);
        COLORREF borderCol = RGB(30, 30, 30);

        bool isHot = (pUDMI->dis.itemState & ODS_HOTLIGHT) != 0;
        bool isSelected = (pUDMI->dis.itemState & ODS_SELECTED) != 0;
        bool isDisabled = (pUDMI->dis.itemState & (ODS_GRAYED | ODS_DISABLED)) != 0;

        if (isDisabled) {
            textCol = RGB(110, 110, 110);
        } else if (isSelected) {
            bgCol = RGB(60, 60, 60);
            borderCol = RGB(80, 80, 80);
            textCol = RGB(255, 255, 255);
        } else if (isHot) {
            bgCol = RGB(48, 48, 48);
            borderCol = RGB(65, 65, 65);
            textCol = RGB(255, 255, 255);
        }

        HBRUSH hbrBg = CreateSolidBrush(bgCol);
        FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, hbrBg);
        DeleteObject(hbrBg);

        if (isHot || isSelected) {
            HBRUSH hbrBorder = CreateSolidBrush(borderCol);
            FrameRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, hbrBorder);
            DeleteObject(hbrBorder);
        }

        int oldBk = SetBkMode(pUDMI->um.hdc, TRANSPARENT);
        COLORREF oldText = SetTextColor(pUDMI->um.hdc, textCol);

        NONCLIENTMETRICSW ncm{ sizeof(ncm) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        HFONT hFont = CreateFontIndirectW(&ncm.lfMenuFont);
        HFONT hOldFont = static_cast<HFONT>(SelectObject(pUDMI->um.hdc, hFont));

        DWORD dtFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
        if (pUDMI->dis.itemState & ODS_NOACCEL) {
            dtFlags |= DT_HIDEPREFIX;
        }
        DrawTextW(pUDMI->um.hdc, menuString, mii.cch, &pUDMI->dis.rcItem, dtFlags);

        SelectObject(pUDMI->um.hdc, hOldFont);
        DeleteObject(hFont);
        SetTextColor(pUDMI->um.hdc, oldText);
        SetBkMode(pUDMI->um.hdc, oldBk);

        *lr = 0;
        return true;
    }

    case WM_UAHMEASUREMENUITEM: {
        *lr = DefWindowProcW(hWnd, message, wParam, lParam);
        return true;
    }

    case WM_NCPAINT:
    case WM_NCACTIVATE: {
        static bool s_inNcPaint = false;
        *lr = DefWindowProcW(hWnd, message, wParam, lParam);
        if (!s_inNcPaint) {
            s_inNcPaint = true;
            UAHDrawMenuNCBottomLine(hWnd, RGB(45, 45, 45));
            s_inNcPaint = false;
        }
        return true;
    }

    default:
        return false;
    }
}

void RefreshWindowFrame(HWND hwnd) {
    if (!hwnd) return;
    DrawMenuBar(hwnd);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

bool IsColorSchemeChangeMessage(LPARAM lParam) {
    if (lParam && CompareStringOrdinal(reinterpret_cast<LPCWCH>(lParam), -1, L"ImmersiveColorSet", -1, TRUE) == CSTR_EQUAL) {
        if (_RefreshImmersiveColorPolicyState) {
            _RefreshImmersiveColorPolicyState();
        }
        return true;
    }
    return false;
}

} // namespace Pluma::Platform
