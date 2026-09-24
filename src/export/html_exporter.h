#pragma once

#include <string>
#include <string_view>
#include <filesystem>

namespace Pluma::Export {

enum class HtmlTheme {
    Auto,  // Light/Dark via prefers-color-scheme media query
    Light,
    Dark
};

struct HtmlExportOptions {
    std::string title = "Pluma Document";
    HtmlTheme theme = HtmlTheme::Auto;
    bool embedStyles = true;
};

class HtmlExporter {
public:
    // Exports Markdown text to a self-contained standalone HTML5 string
    static std::string ExportToString(std::string_view markdown, const HtmlExportOptions& options = {});

    // Exports Markdown text to a self-contained HTML5 file
    static bool ExportToFile(const std::filesystem::path& path, std::string_view markdown, const HtmlExportOptions& options = {});

    // Returns the embedded CSS stylesheet for custom styling or inspection
    static std::string GetEmbeddedCss(HtmlTheme theme);
};

} // namespace Pluma::Export
