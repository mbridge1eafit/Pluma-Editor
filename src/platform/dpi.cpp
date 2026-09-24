#include "dpi.h"

namespace Pluma::Platform {

UINT GetWindowDpi(HWND hwnd) {
    if (hwnd) {
        UINT dpi = GetDpiForWindow(hwnd);
        if (dpi > 0) {
            return dpi;
        }
    }
    return 96; // Standard 100% DPI default
}

int ScaleForDpi(int value, UINT dpi) {
    return MulDiv(value, static_cast<int>(dpi), 96);
}

} // namespace Pluma::Platform
