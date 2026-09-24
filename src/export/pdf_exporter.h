#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <vector>
#include "../markdown/block_tree.h"

namespace Pluma::Export {

enum class PageSize {
    A4,     // 595.28 x 841.89 pt (Standard ISO)
    Letter  // 612.00 x 792.00 pt (Standard US)
};

struct PdfExportOptions {
    std::string title = "Pluma Document";
    PageSize pageSize = PageSize::A4;
    float marginMm = 20.0f; // 20 mm standard margin (F-09)
    bool showPageNumbers = true;
};

class PdfExporter {
public:
    // Exports Markdown AST BlockTree to PDF bytes
    static std::vector<uint8_t> ExportToBytes(const Markdown::BlockTree& tree, const PdfExportOptions& options = {});

    // Exports Markdown text to PDF bytes (parses internally)
    static std::vector<uint8_t> ExportMarkdownToBytes(std::string_view markdown, const PdfExportOptions& options = {});

    // Exports Markdown AST BlockTree to a .pdf file
    static bool ExportToFile(const std::filesystem::path& path, const Markdown::BlockTree& tree, const PdfExportOptions& options = {});

    // Exports Markdown text to a .pdf file
    static bool ExportMarkdownToFile(const std::filesystem::path& path, std::string_view markdown, const PdfExportOptions& options = {});
};

} // namespace Pluma::Export
