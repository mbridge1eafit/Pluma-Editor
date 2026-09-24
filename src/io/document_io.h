#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Pluma::IO {

enum class Encoding {
    Utf8,
    Utf8Bom,
    Utf16LE,
    Utf16BE,
    Ansi     // Legacy Windows-1252 file (not valid UTF-8); converted back on save
};

enum class LineEnding {
    CRLF,
    LF,
    CR
};

struct DocumentData {
    std::string contentUtf8;
    Encoding encoding = Encoding::Utf8;
    LineEnding lineEnding = LineEnding::CRLF;
    bool hasBom = false;
};

// Reads a document from disk with BOM detection, UTF-8 validation (Windows-1252 fallback)
// and memory-mapped reading (NF-02, F-03). Throws std::runtime_error when the file cannot be read.
DocumentData ReadDocument(const std::filesystem::path& path);

// Returns true when the bytes form valid UTF-8.
bool IsValidUtf8(std::string_view bytes);

// Returns false when saving `contentUtf8` with `encoding` would replace characters
// (e.g. an emoji typed into a Windows-1252 document).
bool CanEncodeLosslessly(std::string_view contentUtf8, Encoding encoding);

// Saves a document atomically (writes to temporary file + ReplaceFileW)
// preserving original encoding and line ending (NF-08, F-02, F-03)
void WriteDocumentAtomic(const std::filesystem::path& path,
                         std::string_view contentUtf8,
                         Encoding encoding,
                         LineEnding lineEnding);

} // namespace Pluma::IO
