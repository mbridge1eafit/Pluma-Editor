#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Pluma::IO {

enum class Encoding {
    Utf8,
    Utf8Bom,
    Utf16LE,
    Utf16BE
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

// Reads a document from disk with BOM detection, UTF-8 heuristic,
// and memory-mapped file reading for files > 1 MB (NF-02, F-03)
DocumentData ReadDocument(const std::filesystem::path& path);

// Saves a document atomically (writes to temporary file + ReplaceFileW)
// preserving original encoding and line ending (NF-08, F-02, F-03)
void WriteDocumentAtomic(const std::filesystem::path& path,
                         std::string_view contentUtf8,
                         Encoding encoding,
                         LineEnding lineEnding);

} // namespace Pluma::IO
