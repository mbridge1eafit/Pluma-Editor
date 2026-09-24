#include <gtest/gtest.h>
#include "export/pdf_exporter.h"
#include <filesystem>
#include <string>

using namespace Pluma::Export;

TEST(PdfExporterTest, ValidPdfHeaderAndStructure) {
    std::string markdown =
        "# Document Title\n\n"
        "This is a test paragraph with **bold** text and `inline code`.\n\n"
        "## Subheading\n\n"
        "- Bullet point 1\n"
        "- Bullet point 2\n"
        "- [x] Task done\n\n"
        "```cpp\n"
        "int main() { return 0; }\n"
        "```\n";

    PdfExportOptions options;
    options.title = "Test PDF Document";
    options.pageSize = PageSize::A4;
    options.marginMm = 20.0f;

    auto pdfBytes = PdfExporter::ExportMarkdownToBytes(markdown, options);

    ASSERT_GT(pdfBytes.size(), 100u);

    std::string pdfStr(pdfBytes.begin(), pdfBytes.end());

    // Magic PDF header
    EXPECT_EQ(pdfStr.substr(0, 5), "%PDF-");

    // Must contain Catalog, Pages, fonts, xref, trailer, and %%EOF
    EXPECT_NE(pdfStr.find("/Type /Catalog"), std::string::npos);
    EXPECT_NE(pdfStr.find("/Type /Pages"), std::string::npos);
    EXPECT_NE(pdfStr.find("/Type /Page"), std::string::npos);
    EXPECT_NE(pdfStr.find("/BaseFont /Helvetica"), std::string::npos);
    EXPECT_NE(pdfStr.find("/BaseFont /Courier"), std::string::npos);
    EXPECT_NE(pdfStr.find("xref"), std::string::npos);
    EXPECT_NE(pdfStr.find("trailer"), std::string::npos);
    EXPECT_NE(pdfStr.find("%%EOF"), std::string::npos);
}

TEST(PdfExporterTest, MultiPagePaginationAndPageNumbers) {
    // Generate many headings and paragraphs to force page breaks
    std::string markdown;
    for (int i = 1; i <= 60; ++i) {
        markdown += "## Section " + std::to_string(i) + "\n\n";
        markdown += "Long descriptive paragraph content to fill up space and trigger multiple pages cleanly.\n\n";
    }

    PdfExportOptions options;
    options.showPageNumbers = true;
    options.marginMm = 20.0f;

    auto pdfBytes = PdfExporter::ExportMarkdownToBytes(markdown, options);
    std::string pdfStr(pdfBytes.begin(), pdfBytes.end());

    // Should contain multiple pages (/Count > 1)
    EXPECT_NE(pdfStr.find("/Count "), std::string::npos);
    EXPECT_NE(pdfStr.find("P\\341gina "), std::string::npos);
}

TEST(PdfExporterTest, TablesAndCodeBlocks) {
    std::string markdown =
        "| Header 1 | Header 2 |\n"
        "| :--- | :--- |\n"
        "| Cell A | Cell B |\n"
        "| Cell C | Cell D |\n\n"
        "```python\n"
        "def hello():\n"
        "    print('world')\n"
        "```\n";

    auto pdfBytes = PdfExporter::ExportMarkdownToBytes(markdown);
    EXPECT_GT(pdfBytes.size(), 200u);

    std::string pdfStr(pdfBytes.begin(), pdfBytes.end());
    EXPECT_NE(pdfStr.find("Header 1"), std::string::npos);
    EXPECT_NE(pdfStr.find("hello"), std::string::npos);
}

TEST(PdfExporterTest, ExportToFileOnDisk) {
    std::string markdown = "# Disk Test\n\nWriting PDF to disk.";
    auto tempFile = std::filesystem::temp_directory_path() / "pluma_test_export.pdf";

    EXPECT_TRUE(PdfExporter::ExportMarkdownToFile(tempFile, markdown));
    EXPECT_TRUE(std::filesystem::exists(tempFile));
    EXPECT_GT(std::filesystem::file_size(tempFile), 200u);

    std::filesystem::remove(tempFile);
}
