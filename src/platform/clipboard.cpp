#include "clipboard.h"

#include <cstring>

namespace Pluma::Platform {

bool CopyTextToClipboard(HWND owner, std::wstring_view text) {
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    bool copied = false;
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        auto* target = static_cast<wchar_t*>(GlobalLock(memory));
        if (target) {
            std::memcpy(target, text.data(), text.size() * sizeof(wchar_t));
            target[text.size()] = L'\0';
            GlobalUnlock(memory);
            copied = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
        }
        if (!copied) GlobalFree(memory); // On success the clipboard owns the memory
    }
    CloseClipboard();
    return copied;
}

} // namespace Pluma::Platform
