#include <gtest/gtest.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "io/document_io.h"

namespace fs = std::filesystem;

class DocumentIoTest : public ::testing::Test {
protected:
    fs::path tempDir;

    void SetUp() override {
        tempDir = fs::temp_directory_path() / ("pluma_test_" + std::to_string(GetTickCount64()));
        fs::create_directories(tempDir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir, ec);
    }

    std::vector<uint8_t> ReadBinaryFile(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
    }

    void WriteBinaryFile(const fs::path& path, const std::vector<uint8_t>& bytes) {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
};

TEST_F(DocumentIoTest, Utf8NoBomRoundTrip) {
    fs::path testFile = tempDir / "test_utf8.md";
    std::string originalContent = "# Heading\r\n\r\nParagraph with UTF-8 accents: áéíóú ñ, emojis: 🚀✨.\r\n";
    std::vector<uint8_t> originalBytes(originalContent.begin(), originalContent.end());
    WriteBinaryFile(testFile, originalBytes);

    auto doc = Pluma::IO::ReadDocument(testFile);
    EXPECT_EQ(doc.encoding, Pluma::IO::Encoding::Utf8);
    EXPECT_EQ(doc.lineEnding, Pluma::IO::LineEnding::CRLF);
    EXPECT_FALSE(doc.hasBom);
    EXPECT_EQ(doc.contentUtf8, originalContent);

    fs::path outFile = tempDir / "out_utf8.md";
    Pluma::IO::WriteDocumentAtomic(outFile, doc.contentUtf8, doc.encoding, doc.lineEnding);

    auto outBytes = ReadBinaryFile(outFile);
    EXPECT_EQ(originalBytes, outBytes);
}

TEST_F(DocumentIoTest, Utf8BomRoundTrip) {
    fs::path testFile = tempDir / "test_utf8_bom.md";
    std::vector<uint8_t> originalBytes = {0xEF, 0xBB, 0xBF}; // BOM
    std::string text = "# Markdown with BOM\nSecond line.\n";
    originalBytes.insert(originalBytes.end(), text.begin(), text.end());
    WriteBinaryFile(testFile, originalBytes);

    auto doc = Pluma::IO::ReadDocument(testFile);
    EXPECT_EQ(doc.encoding, Pluma::IO::Encoding::Utf8Bom);
    EXPECT_EQ(doc.lineEnding, Pluma::IO::LineEnding::LF);
    EXPECT_TRUE(doc.hasBom);
    EXPECT_EQ(doc.contentUtf8, text);

    fs::path outFile = tempDir / "out_utf8_bom.md";
    Pluma::IO::WriteDocumentAtomic(outFile, doc.contentUtf8, doc.encoding, doc.lineEnding);

    auto outBytes = ReadBinaryFile(outFile);
    EXPECT_EQ(originalBytes, outBytes);
}

TEST_F(DocumentIoTest, Utf16LERoundTrip) {
    fs::path testFile = tempDir / "test_utf16le.md";
    // UTF-16 LE BOM: 0xFF, 0xFE
    std::vector<uint8_t> originalBytes = {0xFF, 0xFE};
    // "Hi\r\n" in UTF-16 LE: 'H'(0x48, 0x00), 'i'(0x69, 0x00), '\r'(0x0D, 0x00), '\n'(0x0A, 0x00)
    std::vector<uint8_t> textBytes = {0x48, 0x00, 0x69, 0x00, 0x0D, 0x00, 0x0A, 0x00};
    originalBytes.insert(originalBytes.end(), textBytes.begin(), textBytes.end());
    WriteBinaryFile(testFile, originalBytes);

    auto doc = Pluma::IO::ReadDocument(testFile);
    EXPECT_EQ(doc.encoding, Pluma::IO::Encoding::Utf16LE);
    EXPECT_EQ(doc.lineEnding, Pluma::IO::LineEnding::CRLF);
    EXPECT_TRUE(doc.hasBom);
    EXPECT_EQ(doc.contentUtf8, "Hi\r\n");

    fs::path outFile = tempDir / "out_utf16le.md";
    Pluma::IO::WriteDocumentAtomic(outFile, doc.contentUtf8, doc.encoding, doc.lineEnding);

    auto outBytes = ReadBinaryFile(outFile);
    EXPECT_EQ(originalBytes, outBytes);
}

TEST_F(DocumentIoTest, Utf16BERoundTrip) {
    fs::path testFile = tempDir / "test_utf16be.md";
    // UTF-16 BE BOM: 0xFE, 0xFF
    std::vector<uint8_t> originalBytes = {0xFE, 0xFF};
    // "Hi\n" in UTF-16 BE: 'H'(0x00, 0x48), 'i'(0x00, 0x69), '\n'(0x00, 0x0A)
    std::vector<uint8_t> textBytes = {0x00, 0x48, 0x00, 0x69, 0x00, 0x0A};
    originalBytes.insert(originalBytes.end(), textBytes.begin(), textBytes.end());
    WriteBinaryFile(testFile, originalBytes);

    auto doc = Pluma::IO::ReadDocument(testFile);
    EXPECT_EQ(doc.encoding, Pluma::IO::Encoding::Utf16BE);
    EXPECT_EQ(doc.lineEnding, Pluma::IO::LineEnding::LF);
    EXPECT_TRUE(doc.hasBom);
    EXPECT_EQ(doc.contentUtf8, "Hi\n");

    fs::path outFile = tempDir / "out_utf16be.md";
    Pluma::IO::WriteDocumentAtomic(outFile, doc.contentUtf8, doc.encoding, doc.lineEnding);

    auto outBytes = ReadBinaryFile(outFile);
    EXPECT_EQ(originalBytes, outBytes);
}

TEST_F(DocumentIoTest, LargeFileMemoryMappedRead) {
    fs::path testFile = tempDir / "large_2mb.md";
    // Generate ~2 MB of text to trigger memory mapped reading (> 1 MB, NF-02)
    std::string line = "1234567890 ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz\r\n";
    std::ofstream out(testFile, std::ios::binary);
    size_t targetBytes = 2 * 1024 * 1024;
    size_t written = 0;
    while (written < targetBytes) {
        out.write(line.data(), line.size());
        written += line.size();
    }
    out.close();

    auto doc = Pluma::IO::ReadDocument(testFile);
    EXPECT_EQ(doc.encoding, Pluma::IO::Encoding::Utf8);
    EXPECT_EQ(doc.lineEnding, Pluma::IO::LineEnding::CRLF);
    EXPECT_EQ(doc.contentUtf8.size(), written);

    // Verify atomic save on existing large file
    Pluma::IO::WriteDocumentAtomic(testFile, doc.contentUtf8, doc.encoding, doc.lineEnding);
    EXPECT_EQ(fs::file_size(testFile), written);
}

TEST_F(DocumentIoTest, AtomicSaveOverwritesSafely) {
    fs::path testFile = tempDir / "existing.md";
    Pluma::IO::WriteDocumentAtomic(testFile, "Initial content\r\n", Pluma::IO::Encoding::Utf8, Pluma::IO::LineEnding::CRLF);
    EXPECT_TRUE(fs::exists(testFile));

    // Overwrite atomically
    std::string updated = "Updated content atomically\r\n";
    Pluma::IO::WriteDocumentAtomic(testFile, updated, Pluma::IO::Encoding::Utf8, Pluma::IO::LineEnding::CRLF);

    auto doc = Pluma::IO::ReadDocument(testFile);
    EXPECT_EQ(doc.contentUtf8, updated);

    // Verify no temporary .tmp files left in the directory
    for (const auto& entry : fs::directory_iterator(tempDir)) {
        std::string ext = entry.path().extension().string();
        EXPECT_NE(ext, ".tmp");
    }
}
