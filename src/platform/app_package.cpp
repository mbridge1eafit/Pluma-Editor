#include "app_package.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <appmodel.h>

namespace Pluma::Platform {

bool IsPackaged() {
    // Without identity it fails with APPMODEL_ERROR_NO_PACKAGE; with identity, a zero-length buffer
    // reports ERROR_INSUFFICIENT_BUFFER.
    static const bool packaged = [] {
        UINT32 length = 0;
        return GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
    }();
    return packaged;
}

std::wstring GetAppUserModelId() {
    if (!IsPackaged()) return {};
    UINT32 length = 0;
    if (GetCurrentApplicationUserModelId(&length, nullptr) != ERROR_INSUFFICIENT_BUFFER || length == 0) return {};
    std::wstring aumid(length, L'\0');
    if (GetCurrentApplicationUserModelId(&length, aumid.data()) != ERROR_SUCCESS) return {};
    aumid.resize(length > 0 ? length - 1 : 0); // `length` counts the terminating null
    return aumid;
}

std::wstring EscapeUriComponent(std::wstring_view text) {
    constexpr wchar_t kHex[] = L"0123456789ABCDEF";
    std::wstring result;
    result.reserve(text.size());
    const auto appendByte = [&](unsigned char byte) {
        result += L'%';
        result += kHex[byte >> 4];
        result += kHex[byte & 0x0F];
    };
    for (size_t i = 0; i < text.size(); ++i) {
        const wchar_t c = text[i];
        if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'-' ||
            c == L'_' || c == L'.' || c == L'~') {
            result += c;
            continue;
        }
        // UTF-16 -> UTF-8 code point by code point (a lone surrogate becomes U+FFFD).
        char32_t cp = c;
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((static_cast<char32_t>(c) - 0xD800) << 10) + (static_cast<char32_t>(text[i + 1]) - 0xDC00);
            ++i;
        } else if (c >= 0xD800 && c <= 0xDFFF) {
            cp = 0xFFFD;
        }
        if (cp < 0x80) {
            appendByte(static_cast<unsigned char>(cp));
        } else if (cp < 0x800) {
            appendByte(static_cast<unsigned char>(0xC0 | (cp >> 6)));
            appendByte(static_cast<unsigned char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            appendByte(static_cast<unsigned char>(0xE0 | (cp >> 12)));
            appendByte(static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F)));
            appendByte(static_cast<unsigned char>(0x80 | (cp & 0x3F)));
        } else {
            appendByte(static_cast<unsigned char>(0xF0 | (cp >> 18)));
            appendByte(static_cast<unsigned char>(0x80 | ((cp >> 12) & 0x3F)));
            appendByte(static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F)));
            appendByte(static_cast<unsigned char>(0x80 | (cp & 0x3F)));
        }
    }
    return result;
}

} // namespace Pluma::Platform
