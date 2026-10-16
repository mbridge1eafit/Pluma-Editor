#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string_view>

namespace Pluma::Platform {

// Replaces the clipboard contents with `text` (CF_UNICODETEXT). Returns false when the clipboard
// could not be opened or written.
bool CopyTextToClipboard(HWND owner, std::wstring_view text);

} // namespace Pluma::Platform
