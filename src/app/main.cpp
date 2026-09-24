#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <string_view>

#include "../platform/dpi.h"
#include "../platform/theme.h"
#include "../../res/resource.h"

namespace {

constexpr wchar_t kWindowClassName[] = L"PlumaMainWindowClass";
bool g_firstPaintSignaled = false;

void SignalStartupEvent() {
    if (g_firstPaintSignaled) {
        return;
    }
    g_firstPaintSignaled = true;

    // Signal named event for startup benchmark if listening
    DWORD pid = GetCurrentProcessId();
    std::wstring eventName = L"Local\\PlumaStartupEvent_" + std::to_wstring(pid);
    HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
    if (hEvent) {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        bool darkMode = Pluma::Platform::IsSystemDarkMode();
        Pluma::Platform::ApplyThemeToWindow(hwnd, darkMode);
        return 0;
    }

    case WM_SETTINGCHANGE: {
        if (lParam && wcscmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0) {
            bool darkMode = Pluma::Platform::IsSystemDarkMode();
            Pluma::Platform::ApplyThemeToWindow(hwnd, darkMode);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }

    case WM_DPICHANGED: {
        auto* const prcNewWindow = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd, nullptr,
                     prcNewWindow->left, prcNewWindow->top,
                     prcNewWindow->right - prcNewWindow->left,
                     prcNewWindow->bottom - prcNewWindow->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        bool darkMode = Pluma::Platform::IsSystemDarkMode();
        HBRUSH bgBrush = CreateSolidBrush(darkMode ? RGB(30, 30, 30) : RGB(255, 255, 255));
        FillRect(hdc, &ps.rcPaint, bgBrush);
        DeleteObject(bgBrush);
        EndPaint(hwnd, &ps);

        SignalStartupEvent();
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDM_FILE_EXIT:
            DestroyWindow(hwnd);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nShowCmd) {
    // Initialize Common Controls
    INITCOMMONCONTROLSEX iccex{};
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&iccex);

    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr; // Handled in WM_PAINT
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDR_MAIN_MENU);
    wcex.lpszClassName = kWindowClassName;
    wcex.hIconSm = nullptr;

    if (!RegisterClassExW(&wcex)) {
        return 1;
    }

    // Default window dimensions scaled for standard DPI
    int width = Pluma::Platform::ScaleForDpi(1024, 96);
    int height = Pluma::Platform::ScaleForDpi(720, 96);

    HWND hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        kWindowClassName,
        L"Pluma",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) {
        return 1;
    }

    HACCEL hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDR_ACCELERATOR));

    ShowWindow(hwnd, nShowCmd);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!hAccelTable || !TranslateAcceleratorW(hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return static_cast<int>(msg.wParam);
}
