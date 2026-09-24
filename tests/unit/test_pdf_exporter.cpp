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

TEST(PdfExporterTest, KeepsFormattedTextAndNestedContent) {
    std::string markdown =
        "Texto con **negrita**, *cursiva* y [enlace](https://x.com).\n\n"
        "- Padre\n"
        "  - Hijo anidado\n\n"
        "> Cita con **fuerza**\n";

    auto pdfBytes = PdfExporter::ExportMarkdownToBytes(markdown);
    std::string pdfStr(pdfBytes.begin(), pdfBytes.end());

    // Text inside emphasis, links and nested blocks used to be dropped.
    EXPECT_NE(pdfStr.find("(negrita)"), std::string::npos);
    EXPECT_NE(pdfStr.find("(cursiva)"), std::string::npos);
    EXPECT_NE(pdfStr.find("(enlace)"), std::string::npos);
    EXPECT_NE(pdfStr.find("Hijo anidado"), std::string::npos);
    EXPECT_NE(pdfStr.find("(fuerza)"), std::string::npos);
    EXPECT_NE(pdfStr.find("/F2 "), std::string::npos); // Bold font actually used
    EXPECT_NE(pdfStr.find("/Producer (Pluma)"), std::string::npos);
}

TEST(PdfExporterTest, LongTableCellsWrapInsideTheirColumn) {
    std::string markdown =
        "| Clave | Valor |\n|---|---|\n"
        "| x | Un texto muy largo que no cabe en una sola línea de la celda y que antes se salía de la tabla "
        "por la derecha, atravesando el margen de la página y cortándose en el borde del papel |\n";

    auto pdfBytes = PdfExporter::ExportMarkdownToBytes(markdown);
    std::string pdfStr(pdfBytes.begin(), pdfBytes.end());

    // The long cell is split over several text operations instead of one overflowing line.
    // Before, the whole cell was a single Tj running past the right margin.
    EXPECT_EQ(pdfStr.find("de la tabla por la derecha, atravesando el margen de la p\\341gina y cort\\341ndose"),
              std::string::npos);
    EXPECT_NE(pdfStr.find("(Un texto muy largo"), std::string::npos);
    EXPECT_NE(pdfStr.find("papel"), std::string::npos);
}

TEST(PdfExporterTest, EmojiAreSkippedNotQuestionMarks) {
    auto pdfBytes = PdfExporter::ExportMarkdownToBytes("Hecho \xE2\x9C\x85 listo \xF0\x9F\x9A\x80\n");
    std::string pdfStr(pdfBytes.begin(), pdfBytes.end());
    EXPECT_NE(pdfStr.find("(Hecho listo)"), std::string::npos);
}

TEST(PdfExporterTest, MermaidDiagramsAreVectorGraphics) {
    const std::string markdown =
        "Antes\n\n"
        "```mermaid\n"
        "flowchart TD\n"
        "  A[Inicio] --> B{Decidir}\n"
        "  B -->|Si| C[Fin]\n"
        "```\n\n"
        "Despues\n";
    const auto bytes = PdfExporter::ExportMarkdownToBytes(markdown);
    ASSERT_FALSE(bytes.empty());
    const std::string pdf(bytes.begin(), bytes.end());
    EXPECT_NE(pdf.find("(Inicio) Tj"), std::string::npos);
    EXPECT_NE(pdf.find("(Decidir) Tj"), std::string::npos);
    EXPECT_NE(pdf.find("(Si) Tj"), std::string::npos);
    EXPECT_EQ(pdf.find("flowchart TD"), std::string::npos); // Not printed as code
    EXPECT_NE(pdf.find(" c"), std::string::npos);            // Curved edges
    EXPECT_NE(pdf.find("(Despues) Tj"), std::string::npos);

    // Invalid diagrams fall back to the code listing.
    const auto fallback = PdfExporter::ExportMarkdownToBytes("```mermaid\nclassDiagram\n  A <|-- B\n```\n");
    const std::string fallbackPdf(fallback.begin(), fallback.end());
    EXPECT_NE(fallbackPdf.find("(classDiagram) Tj"), std::string::npos);
}
