#include <gtest/gtest.h>
#include "export/html_exporter.h"
#include <chrono>
#include <filesystem>

using namespace Pluma::Export;

TEST(HtmlExporterTest, StandaloneStructureAndTags) {
    std::string markdown =
        "# Main Title\n\n"
        "A paragraph with **bold** and *italic* text, and `inline code`.\n\n"
        "[Link to test](https://example.com)\n";

    HtmlExportOptions options;
    options.title = "Test Doc";
    options.theme = HtmlTheme::Auto;

    std::string html = HtmlExporter::ExportToString(markdown, options);

    EXPECT_NE(html.find("<!DOCTYPE html>"), std::string::npos);
    EXPECT_NE(html.find("<title>Test Doc</title>"), std::string::npos);
    EXPECT_NE(html.find("<style>"), std::string::npos);
    EXPECT_NE(html.find("<h1 id=\"main-title\">Main Title</h1>"), std::string::npos);
    EXPECT_NE(html.find("<strong>bold</strong>"), std::string::npos);
    EXPECT_NE(html.find("<em>italic</em>"), std::string::npos);
    EXPECT_NE(html.find("<code>inline code</code>"), std::string::npos);
    EXPECT_NE(html.find("<a href=\"https://example.com\">Link to test</a>"), std::string::npos);
    EXPECT_NE(html.find("</html>"), std::string::npos);
}

TEST(HtmlExporterTest, GfmExtensionsTablesAndTasks) {
    std::string markdown =
        "| Name | Age |\n"
        "| :--- | --: |\n"
        "| Alice | 30 |\n\n"
        "- [x] Completed task\n"
        "- [ ] Incomplete task\n";

    std::string html = HtmlExporter::ExportToString(markdown);

    EXPECT_NE(html.find("<table>"), std::string::npos);
    EXPECT_NE(html.find("Name</th>"), std::string::npos);
    EXPECT_NE(html.find("Alice</td>"), std::string::npos);
    EXPECT_NE(html.find("type=\"checkbox\""), std::string::npos);
    EXPECT_NE(html.find("checked"), std::string::npos);
}

TEST(HtmlExporterTest, ThemeVariants) {
    std::string markdown = "# Hello";

    HtmlExportOptions lightOpt;
    lightOpt.theme = HtmlTheme::Light;
    std::string lightHtml = HtmlExporter::ExportToString(markdown, lightOpt);
    EXPECT_NE(lightHtml.find("--bg: #ffffff;"), std::string::npos);

    HtmlExportOptions darkOpt;
    darkOpt.theme = HtmlTheme::Dark;
    std::string darkHtml = HtmlExporter::ExportToString(markdown, darkOpt);
    EXPECT_NE(darkHtml.find("--bg: #1e1e1e;"), std::string::npos);

    HtmlExportOptions autoOpt;
    autoOpt.theme = HtmlTheme::Auto;
    std::string autoHtml = HtmlExporter::ExportToString(markdown, autoOpt);
    EXPECT_NE(autoHtml.find("@media (prefers-color-scheme: dark)"), std::string::npos);
}

TEST(HtmlExporterTest, ExportToFileAndRoundTrip) {
    std::string markdown = "# Export Test\n\nContent for file export test.";
    auto tempFile = std::filesystem::temp_directory_path() / "pluma_test_export.html";

    EXPECT_TRUE(HtmlExporter::ExportToFile(tempFile, markdown));
    EXPECT_TRUE(std::filesystem::exists(tempFile));
    EXPECT_GT(std::filesystem::file_size(tempFile), 100u);

    std::filesystem::remove(tempFile);
}

TEST(HtmlExporterTest, HighPerformanceLargeDocument) {
    // Generate a ~300 KB markdown document
    std::string largeMd;
    largeMd.reserve(300 * 1024);
    for (int i = 0; i < 2000; ++i) {
        largeMd += "## Section " + std::to_string(i) + "\n\n";
        largeMd += "This is a detailed paragraph with **bold formatting**, *italics*, and `code`.\n\n";
        largeMd += "| Col A | Col B |\n|---|---|\n| Data 1 | Data 2 |\n\n";
    }

    auto start = std::chrono::high_resolution_clock::now();
    std::string html = HtmlExporter::ExportToString(largeMd);
    auto end = std::chrono::high_resolution_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_FALSE(html.empty());
    // F-09 requires export in < 500 ms for 1 MB
    EXPECT_LT(ms, 500);
}

TEST(HtmlExporterTest, HeadingAnchorsAndEmojiShortcodes) {
    std::string markdown =
        "# Introducción :rocket:\n\n"
        "## Introducción\n\n"
        "[ir](#introducción) :tada: `:tada:`\n";

    std::string html = HtmlExporter::ExportToString(markdown);

    EXPECT_NE(html.find("<h1 id=\"introducción\">"), std::string::npos);
    EXPECT_NE(html.find("<h2 id=\"introducción-1\">"), std::string::npos);
    EXPECT_NE(html.find("🎉"), std::string::npos);
    EXPECT_NE(html.find("<code>:tada:</code>"), std::string::npos); // Code stays literal
    EXPECT_NE(html.find("lang=\"es\""), std::string::npos);
}

TEST(HtmlExporterTest, TablesCannotOverflowThePage) {
    std::string css = HtmlExporter::GetEmbeddedCss(HtmlTheme::Auto);
    EXPECT_NE(css.find("overflow-x: auto"), std::string::npos);
    EXPECT_NE(css.find("overflow-wrap: anywhere"), std::string::npos);
}

TEST(HtmlExporterTest, MermaidBlocksBecomeInlineSvg) {
    const std::string markdown =
        "# Flujo\n\n"
        "```mermaid\n"
        "graph LR\n"
        "  A[Inicio] --> B{\"a < b\"}\n"
        "```\n\n"
        "```mermaid\n"
        "classDiagram\n"
        "  Animal <|-- Pato\n"
        "```\n";
    const std::string html = HtmlExporter::ExportToString(markdown, HtmlExportOptions{});

    // The valid diagram is replaced by a self-contained SVG with escaped labels.
    EXPECT_NE(html.find("<figure class=\"mermaid-diagram\"><svg"), std::string::npos);
    EXPECT_NE(html.find(">Inicio</text>"), std::string::npos);
    EXPECT_NE(html.find(">a &lt; b</text>"), std::string::npos);
    // The unsupported one keeps its source as a code block.
    EXPECT_NE(html.find("<code class=\"language-mermaid\">classDiagram"), std::string::npos);
    // Diagram colours follow the page theme.
    EXPECT_NE(html.find("--pluma-mm-node-fill:"), std::string::npos);
}
