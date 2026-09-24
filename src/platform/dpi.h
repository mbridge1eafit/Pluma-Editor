#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Pluma::Platform {

// Returns the effective DPI for the window, fallback to 96
UINT GetWindowDpi(HWND hwnd);

// Scales an integer value by DPI factor
int ScaleForDpi(int value, UINT dpi);

} // namespace Pluma::Platform
