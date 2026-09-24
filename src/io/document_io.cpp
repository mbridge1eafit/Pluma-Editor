#include "document_io.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdexcept>
#include <vector>

namespace Pluma::IO {

namespace {

struct ScopedHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~ScopedHandle() {
        if (handle != INVALID_HANDLE_VALUE && handle != nullptr) {
            CloseHandle(handle);
        }
    }
    operator HANDLE() const { return handle; }
};

struct ScopedMapView {
    const void* ptr = nullptr;
    ~ScopedMapView() {
        if (ptr) {
            UnmapViewOfFile(ptr);
        }
    }
};

LineEnding DetectLineEnding(std::string_view content) {
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\r') {
            if (i + 1 < content.size() && content[i + 1] == '\n') {
                return LineEnding::CRLF;
            }
            return LineEnding::CR;
        }
        if (content[i] == '\n') {
            return LineEnding::LF;
        }
    }
    return LineEnding::CRLF;
}

std::string NormalizeLineEndings(std::string_view input, LineEnding target) {
    std::string result;
    result.reserve(input.size() + input.size() / 10);

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\r') {
            if (i + 1 < input.size() && input[i + 1] == '\n') {
                ++i; // skip \n
            }
            if (target == LineEnding::CRLF) {
                result.push_back('\r');
                result.push_back('\n');
            } else if (target == LineEnding::LF) {
                result.push_back('\n');
            } else {
                result.push_back('\r');
            }
        } else if (input[i] == '\n') {
            if (target == LineEnding::CRLF) {
                result.push_back('\r');
                result.push_back('\n');
            } else if (target == LineEnding::LF) {
                result.push_back('\n');
            } else {
                result.push_back('\r');
            }
        } else {
            result.push_back(input[i]);
        }
    }
    return result;
}

std::string Utf16ToUtf8(const wchar_t* utf16, size_t length) {
    if (length == 0) {
        return {};
    }
    int required = WideCharToMultiByte(CP_UTF8, 0, utf16, static_cast<int>(length),
                                       nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string utf8(required, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16, static_cast<int>(length),
                        utf8.data(), required, nullptr, nullptr);
    return utf8;
}

std::wstring Utf8ToUtf16(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    int required = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                       nullptr, 0);
    if (required <= 0) {
        return {};
    }
    std::wstring utf16(required, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                        utf16.data(), required);
    return utf16;
}

} // namespace

DocumentData ReadDocument(const std::filesystem::path& path) {
    DocumentData doc;

    ScopedHandle hFile{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (hFile == INVALID_HANDLE_VALUE) {
        return doc;
    }

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0) {
        return doc;
    }

    uint64_t totalBytes = static_cast<uint64_t>(fileSize.QuadPart);

    // Memory-mapped reading (NF-02)
    ScopedHandle hMapping{CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr)};
    if (!hMapping) {
        return doc;
    }

    ScopedMapView view{MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0)};
    if (!view.ptr) {
        return doc;
    }

    const auto* bytes = static_cast<const uint8_t*>(view.ptr);
    size_t offset = 0;

    // BOM and encoding detection
    if (totalBytes >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        doc.encoding = Encoding::Utf8Bom;
        doc.hasBom = true;
        offset = 3;
    } else if (totalBytes >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        doc.encoding = Encoding::Utf16LE;
        doc.hasBom = true;
        offset = 2;
    } else if (totalBytes >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        doc.encoding = Encoding::Utf16BE;
        doc.hasBom = true;
        offset = 2;
    } else {
        doc.encoding = Encoding::Utf8;
        doc.hasBom = false;
        offset = 0;
    }

    size_t payloadSize = totalBytes - offset;

    if (doc.encoding == Encoding::Utf8 || doc.encoding == Encoding::Utf8Bom) {
        doc.contentUtf8.assign(reinterpret_cast<const char*>(bytes + offset), payloadSize);
    } else if (doc.encoding == Encoding::Utf16LE) {
        size_t wcharCount = payloadSize / sizeof(wchar_t);
        doc.contentUtf8 = Utf16ToUtf8(reinterpret_cast<const wchar_t*>(bytes + offset), wcharCount);
    } else if (doc.encoding == Encoding::Utf16BE) {
        size_t wcharCount = payloadSize / sizeof(wchar_t);
        std::vector<wchar_t> swapped(wcharCount);
        for (size_t i = 0; i < wcharCount; ++i) {
            uint8_t hi = bytes[offset + i * 2];
            uint8_t lo = bytes[offset + i * 2 + 1];
            swapped[i] = static_cast<wchar_t>((hi << 8) | lo);
        }
        doc.contentUtf8 = Utf16ToUtf8(swapped.data(), wcharCount);
    }

    doc.lineEnding = DetectLineEnding(doc.contentUtf8);
    return doc;
}

void WriteDocumentAtomic(const std::filesystem::path& path,
                         std::string_view contentUtf8,
                         Encoding encoding,
                         LineEnding lineEnding) {
    std::string normalized = NormalizeLineEndings(contentUtf8, lineEnding);
    std::vector<uint8_t> encodedBytes;

    if (encoding == Encoding::Utf8Bom) {
        encodedBytes = {0xEF, 0xBB, 0xBF};
        encodedBytes.insert(encodedBytes.end(), normalized.begin(), normalized.end());
    } else if (encoding == Encoding::Utf8) {
        encodedBytes.assign(normalized.begin(), normalized.end());
    } else if (encoding == Encoding::Utf16LE) {
        encodedBytes = {0xFF, 0xFE};
        std::wstring utf16 = Utf8ToUtf16(normalized);
        const auto* ptr = reinterpret_cast<const uint8_t*>(utf16.data());
        size_t byteCount = utf16.size() * sizeof(wchar_t);
        encodedBytes.insert(encodedBytes.end(), ptr, ptr + byteCount);
    } else if (encoding == Encoding::Utf16BE) {
        encodedBytes = {0xFE, 0xFF};
        std::wstring utf16 = Utf8ToUtf16(normalized);
        for (wchar_t ch : utf16) {
            encodedBytes.push_back(static_cast<uint8_t>((ch >> 8) & 0xFF));
            encodedBytes.push_back(static_cast<uint8_t>(ch & 0xFF));
        }
    }

    // Atomic write to temporary file in the same directory (NF-08)
    std::filesystem::path parentDir = path.parent_path();
    if (parentDir.empty()) {
        parentDir = ".";
    }

    std::wstring tempFileName = path.stem().wstring() + L".tmp_" +
                                std::to_wstring(GetTickCount64()) + L"_" +
                                std::to_wstring(GetCurrentProcessId());
    std::filesystem::path tempPath = parentDir / tempFileName;

    ScopedHandle hTemp{CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (hTemp == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to create temporary file for atomic save");
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(hTemp, encodedBytes.data(), static_cast<DWORD>(encodedBytes.size()),
                   &bytesWritten, nullptr) || bytesWritten != encodedBytes.size()) {
        DeleteFileW(tempPath.c_str());
        throw std::runtime_error("Failed to write temporary file");
    }

    FlushFileBuffers(hTemp);
    CloseHandle(hTemp.handle);
    hTemp.handle = INVALID_HANDLE_VALUE;

    // Atomic replacement
    if (std::filesystem::exists(path)) {
        if (!ReplaceFileW(path.c_str(), tempPath.c_str(), nullptr,
                          REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
            if (!MoveFileExW(tempPath.c_str(), path.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
                DeleteFileW(tempPath.c_str());
                throw std::runtime_error("Failed to replace target file atomically");
            }
        }
    } else {
        if (!MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_COPY_ALLOWED)) {
            DeleteFileW(tempPath.c_str());
            throw std::runtime_error("Failed to move temporary file to destination");
        }
    }
}

} // namespace Pluma::IO
