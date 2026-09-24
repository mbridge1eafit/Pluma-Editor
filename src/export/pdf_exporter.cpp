#include "pdf_exporter.h"
#include "../markdown/md4c_adapter.h"
#include "../io/document_io.h"

#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <fstream>

namespace Pluma::Export {

namespace {

// Converts UTF-8 text to PDF literal string with WinAnsiEncoding octal escapes
std::string ToPdfString(std::string_view utf8Text) {
    std::string out;
    out.reserve(utf8Text.size() + 16);

    for (size_t i = 0; i < utf8Text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(utf8Text[i]);

        // Escape PDF special chars
        if (c == '\\') {
            out += "\\\\";
        } else if (c == '(') {
            out += "\\(";
        } else if (c == ')') {
            out += "\\)";
        } else if (c >= 32 && c <= 126) {
            out += static_cast<char>(c);
        } else if (c >= 0xC0) {
            // Multi-byte UTF-8 sequence
            uint32_t codepoint = 0;
            size_t bytes = 0;
            if ((c & 0xE0) == 0xC0 && i + 1 < utf8Text.size()) {
                codepoint = (c & 0x1F) << 6 | (static_cast<unsigned char>(utf8Text[i + 1]) & 0x3F);
                bytes = 1;
            } else if ((c & 0xF0) == 0xE0 && i + 2 < utf8Text.size()) {
                codepoint = (c & 0x0F) << 12 | ((static_cast<unsigned char>(utf8Text[i + 1]) & 0x3F) << 6) | (static_cast<unsigned char>(utf8Text[i + 2]) & 0x3F);
                bytes = 2;
            } else if ((c & 0xF8) == 0xF0 && i + 3 < utf8Text.size()) {
                codepoint = (c & 0x07) << 18 | ((static_cast<unsigned char>(utf8Text[i + 1]) & 0x3F) << 12) | ((static_cast<unsigned char>(utf8Text[i + 2]) & 0x3F) << 6) | (static_cast<unsigned char>(utf8Text[i + 3]) & 0x3F);
                bytes = 3;
            }

            i += bytes;

            // Map standard Latin-1 / WinAnsi codepoints
            if (codepoint >= 0xA0 && codepoint <= 0xFF) {
                char octalBuf[8];
                snprintf(octalBuf, sizeof(octalBuf), "\\%03o", codepoint);
                out += octalBuf;
            } else if (codepoint == 0x2014) { // Em-dash
                out += "\\227";
            } else if (codepoint == 0x2013) { // En-dash
                out += "\\226";
            } else if (codepoint == 0x201C) { // Left double quote
                out += "\\223";
            } else if (codepoint == 0x201D) { // Right double quote
                out += "\\224";
            } else if (codepoint == 0x2018) { // Left single quote
                out += "\\221";
            } else if (codepoint == 0x2019) { // Right single quote
                out += "\\222";
            } else if (codepoint == 0x2022) { // Bullet
                out += "\\225";
            } else if (codepoint == 0x2026) { // Ellipsis
                out += "\\205";
            } else {
                out += '?';
            }
        } else if (c == '\t') {
            out += "    ";
        }
    }
    return out;
}

float MeasureTextWidth(std::string_view text, float fontSize, bool isCourier) {
    if (isCourier) {
        return static_cast<float>(text.size()) * fontSize * 0.60f;
    }
    // Proportional Helvetica approximation
    return static_cast<float>(text.size()) * fontSize * 0.52f;
}

std::vector<std::string> WordWrap(std::string_view text, float fontSize, float maxWidth, bool isCourier) {
    std::vector<std::string> lines;
    if (text.empty()) {
        lines.emplace_back("");
        return lines;
    }

    std::string currentLine;
    std::string currentWord;

    for (size_t i = 0; i <= text.size(); ++i) {
        char ch = (i < text.size()) ? text[i] : ' ';
        if (ch == ' ' || ch == '\n' || i == text.size()) {
            if (!currentWord.empty()) {
                std::string testLine = currentLine.empty() ? currentWord : (currentLine + " " + currentWord);
                if (MeasureTextWidth(testLine, fontSize, isCourier) <= maxWidth) {
                    currentLine = std::move(testLine);
                } else {
                    if (!currentLine.empty()) {
                        lines.push_back(std::move(currentLine));
                        currentLine = currentWord;
                    } else {
                        // Word itself exceeds maxWidth, force wrap
                        lines.push_back(std::move(currentWord));
                    }
                }
                currentWord.clear();
            }
            if (ch == '\n') {
                lines.push_back(std::move(currentLine));
                currentLine.clear();
            }
        } else {
            currentWord += ch;
        }
    }

    if (!currentLine.empty()) {
        lines.push_back(std::move(currentLine));
    }

    if (lines.empty()) {
        lines.emplace_back("");
    }

    return lines;
}

} // namespace

std::vector<uint8_t> PdfExporter::ExportToBytes(const Markdown::BlockTree& tree, const PdfExportOptions& options) {
    float pageWidth = (options.pageSize == PageSize::Letter) ? 612.0f : 595.28f;
    float pageHeight = (options.pageSize == PageSize::Letter) ? 792.0f : 841.89f;

    float margin = options.marginMm * 2.83464567f;
    float printableWidth = pageWidth - 2.0f * margin;
    float bottomMargin = margin + (options.showPageNumbers ? 24.0f : 0.0f);
    float topMargin = pageHeight - margin;

    struct Page {
        std::ostringstream stream;
    };

    std::vector<Page> pages;
    pages.emplace_back();

    float currentY = topMargin;

    auto StartNewPage = [&]() {
        pages.emplace_back();
        currentY = topMargin;
    };

    auto CurrentStream = [&]() -> std::ostringstream& {
        return pages.back().stream;
    };

    // Helper to draw spans
    auto RenderSpansToText = [](const std::vector<Markdown::Span>& spans) -> std::string {
        std::string s;
        for (const auto& span : spans) {
            s += span.text;
        }
        return s;
    };

    if (!tree.root) {
        return {};
    }

    // Process blocks
    for (size_t bIdx = 0; bIdx < tree.root->children.size(); ++bIdx) {
        if (!tree.root->children[bIdx]) continue;
        const auto& block = *tree.root->children[bIdx];

        switch (block.type) {
        case Markdown::BlockType::Heading: {
            float fontSize = 12.0f;
            float leading = 16.0f;
            switch (block.level) {
            case 1: fontSize = 20.0f; leading = 24.0f; break;
            case 2: fontSize = 16.0f; leading = 20.0f; break;
            case 3: fontSize = 13.5f; leading = 17.0f; break;
            case 4: fontSize = 12.0f; leading = 15.0f; break;
            case 5: fontSize = 10.5f; leading = 14.0f; break;
            case 6: fontSize = 9.5f;  leading = 13.0f; break;
            default: fontSize = 12.0f; leading = 15.0f; break;
            }

            // Orphan heading prevention (F-09): ensure heading + at least 35pt of content fits
            if (currentY - (leading + 40.0f) < bottomMargin) {
                StartNewPage();
            }

            std::string text = RenderSpansToText(block.inlineContent);
            auto lines = WordWrap(text, fontSize, printableWidth, false);

            for (const auto& line : lines) {
                if (currentY - leading < bottomMargin) {
                    StartNewPage();
                }
                CurrentStream() << "BT /F2 " << fontSize << " Tf "
                                << margin << " " << (currentY - fontSize) << " Td ("
                                << ToPdfString(line) << ") Tj ET\n";
                currentY -= leading;
            }

            // Bottom border for H1 and H2
            if (block.level <= 2) {
                currentY -= 3.0f;
                CurrentStream() << "q 0.8 0.8 0.8 RG 0.75 w "
                                << margin << " " << currentY << " m "
                                << (margin + printableWidth) << " " << currentY << " l S Q\n";
                currentY -= 8.0f;
            } else {
                currentY -= 6.0f;
            }
            break;
        }

        case Markdown::BlockType::Paragraph: {
            float fontSize = 10.0f;
            float leading = 14.5f;
            std::string text = RenderSpansToText(block.inlineContent);
            auto lines = WordWrap(text, fontSize, printableWidth, false);

            for (const auto& line : lines) {
                if (currentY - leading < bottomMargin) {
                    StartNewPage();
                }
                CurrentStream() << "BT /F1 " << fontSize << " Tf "
                                << margin << " " << (currentY - fontSize) << " Td ("
                                << ToPdfString(line) << ") Tj ET\n";
                currentY -= leading;
            }
            currentY -= 8.0f; // Paragraph spacing
            break;
        }

        case Markdown::BlockType::Blockquote: {
            float fontSize = 10.0f;
            float leading = 14.5f;
            float indent = 18.0f;
            float quoteWidth = printableWidth - indent;
            float startY = currentY;

            for (const auto& child : block.children) {
                if (!child) continue;
                std::string text = RenderSpansToText(child->inlineContent);
                auto lines = WordWrap(text, fontSize, quoteWidth, false);

                for (const auto& line : lines) {
                    if (currentY - leading < bottomMargin) {
                        StartNewPage();
                        startY = currentY;
                    }
                    CurrentStream() << "BT /F3 " << fontSize << " Tf 0.3 0.3 0.3 rg "
                                    << (margin + indent) << " " << (currentY - fontSize) << " Td ("
                                    << ToPdfString(line) << ") Tj ET 0 0 0 rg\n";
                    currentY -= leading;
                }
            }

            // Draw left quote bar
            CurrentStream() << "q 0.6 0.7 0.85 RG 2.5 w "
                            << (margin + 6.0f) << " " << currentY << " m "
                            << (margin + 6.0f) << " " << startY << " l S Q\n";

            currentY -= 8.0f;
            break;
        }

        case Markdown::BlockType::List: {
            float fontSize = 10.0f;
            float leading = 14.5f;
            float indent = 20.0f;
            float itemWidth = printableWidth - indent;
            int itemNum = block.startNumber;

            for (const auto& item : block.children) {
                if (!item) continue;
                std::string text = RenderSpansToText(item->inlineContent);
                if (text.empty() && !item->children.empty() && item->children[0]) {
                    text = RenderSpansToText(item->children[0]->inlineContent);
                }

                auto lines = WordWrap(text, fontSize, itemWidth, false);

                if (currentY - leading < bottomMargin) {
                    StartNewPage();
                }

                // Draw bullet, number, or checkbox
                if (item->isTask) {
                    // Checkbox
                    float boxY = currentY - fontSize + 1.0f;
                    CurrentStream() << "q 0.2 0.2 0.2 RG 1 w "
                                    << (margin + 2.0f) << " " << boxY << " 8 8 re S Q\n";
                    if (item->isTaskChecked) {
                        CurrentStream() << "q 0.1 0.5 0.1 RG 1.5 w "
                                        << (margin + 3.0f) << " " << (boxY + 4.0f) << " m "
                                        << (margin + 5.5f) << " " << (boxY + 1.5f) << " l "
                                        << (margin + 8.5f) << " " << (boxY + 7.0f) << " l S Q\n";
                    }
                } else if (block.isOrdered) {
                    std::string numStr = std::to_string(itemNum++) + ".";
                    CurrentStream() << "BT /F1 " << fontSize << " Tf "
                                    << margin << " " << (currentY - fontSize) << " Td ("
                                    << numStr << ") Tj ET\n";
                } else {
                    // Bullet dot
                    CurrentStream() << "q 0.2 0.2 0.2 rg "
                                    << (margin + 6.0f) << " " << (currentY - fontSize + 3.5f) << " 2.0 0 360 arc f Q\n";
                }

                // Item lines
                for (const auto& line : lines) {
                    if (currentY - leading < bottomMargin) {
                        StartNewPage();
                    }
                    CurrentStream() << "BT /F1 " << fontSize << " Tf "
                                    << (margin + indent) << " " << (currentY - fontSize) << " Td ("
                                    << ToPdfString(line) << ") Tj ET\n";
                    currentY -= leading;
                }
                currentY -= 2.0f;
            }
            currentY -= 6.0f;
            break;
        }

        case Markdown::BlockType::CodeBlock: {
            float fontSize = 9.0f;
            float leading = 12.5f;

            std::string fullCode;
            for (const auto& s : block.inlineContent) {
                fullCode.append(s.text);
            }

            // Split verbatim code into lines
            std::vector<std::string> codeLines;
            std::istringstream iss(fullCode);
            std::string codeLine;
            while (std::getline(iss, codeLine)) {
                if (!codeLine.empty() && codeLine.back() == '\r') codeLine.pop_back();
                codeLines.push_back(std::move(codeLine));
            }
            if (codeLines.empty()) codeLines.emplace_back("");

            float blockHeight = static_cast<float>(codeLines.size()) * leading + 16.0f;

            // If it doesn't fit on this page but fits on an empty page, break
            if (currentY - blockHeight < bottomMargin && blockHeight <= (topMargin - bottomMargin)) {
                StartNewPage();
            }

            float boxTop = currentY;
            float boxBottom = (std::max)(bottomMargin, currentY - blockHeight);

            // Draw background rectangle
            CurrentStream() << "q 0.96 0.96 0.97 rg 0.85 0.85 0.88 RG 0.5 w "
                            << margin << " " << boxBottom << " "
                            << printableWidth << " " << (boxTop - boxBottom) << " re B Q\n";

            currentY -= 10.0f;
            for (const auto& cLine : codeLines) {
                if (currentY - leading < bottomMargin) {
                    StartNewPage();
                }
                CurrentStream() << "BT /F5 " << fontSize << " Tf 0.1 0.1 0.1 rg "
                                << (margin + 8.0f) << " " << (currentY - fontSize) << " Td ("
                                << ToPdfString(cLine) << ") Tj ET 0 0 0 rg\n";
                currentY -= leading;
            }
            currentY -= 12.0f;
            break;
        }

        case Markdown::BlockType::ThematicBreak: {
            currentY -= 8.0f;
            if (currentY < bottomMargin) {
                StartNewPage();
            }
            CurrentStream() << "q 0.75 0.75 0.75 RG 1 w "
                            << margin << " " << currentY << " m "
                            << (margin + printableWidth) << " " << currentY << " l S Q\n";
            currentY -= 12.0f;
            break;
        }

        case Markdown::BlockType::Table: {
            // Collect rows (header + body)
            std::vector<const Markdown::Block*> rows;
            for (const auto& child : block.children) {
                if (!child) continue;
                if (child->type == Markdown::BlockType::TableHead || child->type == Markdown::BlockType::TableBody) {
                    for (const auto& r : child->children) {
                        if (r && r->type == Markdown::BlockType::TableRow) {
                            rows.push_back(r.get());
                        }
                    }
                } else if (child->type == Markdown::BlockType::TableRow) {
                    rows.push_back(child.get());
                }
            }

            if (rows.empty()) break;

            size_t colCount = 0;
            for (const auto* r : rows) {
                colCount = (std::max)(colCount, r->children.size());
            }
            if (colCount == 0) break;

            float colWidth = printableWidth / static_cast<float>(colCount);
            float rowHeight = 18.0f;
            float fontSize = 9.5f;

            for (size_t rIdx = 0; rIdx < rows.size(); ++rIdx) {
                const auto* row = rows[rIdx];
                if (currentY - rowHeight < bottomMargin) {
                    StartNewPage();
                }

                bool isHeader = (rIdx == 0);
                if (isHeader) {
                    // Header background
                    CurrentStream() << "q 0.93 0.94 0.96 rg "
                                    << margin << " " << (currentY - rowHeight) << " "
                                    << printableWidth << " " << rowHeight << " re f Q\n";
                }

                // Row borders
                CurrentStream() << "q 0.8 0.8 0.8 RG 0.5 w "
                                << margin << " " << (currentY - rowHeight) << " "
                                << printableWidth << " " << rowHeight << " re S Q\n";

                // Cells
                for (size_t c = 0; c < (std::min)(colCount, row->children.size()); ++c) {
                    const auto& cell = row->children[c];
                    if (!cell) continue;

                    float cx = margin + static_cast<float>(c) * colWidth;
                    std::string text = RenderSpansToText(cell->inlineContent);

                    std::string font = isHeader ? "/F2" : "/F1";
                    CurrentStream() << "BT " << font << " " << fontSize << " Tf "
                                    << (cx + 5.0f) << " " << (currentY - fontSize - 4.0f) << " Td ("
                                    << ToPdfString(text) << ") Tj ET\n";

                    // Vertical col dividers
                    if (c > 0) {
                        CurrentStream() << "q 0.8 0.8 0.8 RG 0.5 w "
                                        << cx << " " << (currentY - rowHeight) << " m "
                                        << cx << " " << currentY << " l S Q\n";
                    }
                }

                currentY -= rowHeight;
            }
            currentY -= 12.0f;
            break;
        }

        default:
            break;
        }
    }

    // Add page numbers and headers if requested
    size_t totalPages = pages.size();
    for (size_t i = 0; i < totalPages; ++i) {
        if (options.showPageNumbers) {
            std::string utf8PageStr = "Página " + std::to_string(i + 1) + " de " + std::to_string(totalPages);
            std::string pdfPageStr = ToPdfString(utf8PageStr);
            float numWidth = MeasureTextWidth("Pagina " + std::to_string(i + 1) + " de " + std::to_string(totalPages), 8.5f, false);
            float footerY = margin * 0.6f;

            // Thin rule above footer
            pages[i].stream << "q 0.85 0.85 0.85 RG 0.5 w "
                            << margin << " " << (footerY + 12.0f) << " m "
                            << (margin + printableWidth) << " " << (footerY + 12.0f) << " l S Q\n";

            // Title on left
            if (!options.title.empty()) {
                pages[i].stream << "BT /F1 8.5 Tf 0.5 0.5 0.5 rg "
                                << margin << " " << footerY << " Td ("
                                << ToPdfString(options.title) << ") Tj ET 0 0 0 rg\n";
            }

            // Page number on right
            pages[i].stream << "BT /F1 8.5 Tf 0.5 0.5 0.5 rg "
                            << (margin + printableWidth - numWidth) << " " << footerY << " Td ("
                            << pdfPageStr << ") Tj ET 0 0 0 rg\n";
        }
    }

    // Assemble PDF document objects
    std::ostringstream pdf;
    pdf << "%PDF-1.4\n";
    pdf << "%\xE2\xE3\xCF\xD3\n";

    std::vector<size_t> objOffsets;
    objOffsets.push_back(0); // 1-indexed

    auto StartObject = [&](int objId) {
        while (objOffsets.size() <= static_cast<size_t>(objId)) {
            objOffsets.push_back(0);
        }
        objOffsets[objId] = static_cast<size_t>(pdf.tellp());
        pdf << objId << " 0 obj\n";
    };

    // 1: Catalog
    StartObject(1);
    pdf << "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

    // 2: Pages
    StartObject(2);
    pdf << "<< /Type /Pages /Kids [";
    for (size_t i = 0; i < totalPages; ++i) {
        int pageObjId = 3 + static_cast<int>(i) * 2;
        pdf << " " << pageObjId << " 0 R";
    }
    pdf << " ] /Count " << totalPages
        << " /MediaBox [ 0 0 " << std::fixed << std::setprecision(2) << pageWidth << " " << pageHeight << " ] >>\nendobj\n";

    // Fonts: 3 + totalPages * 2 ...
    int fontBaseId = 3 + static_cast<int>(totalPages) * 2;
    int f1Id = fontBaseId;
    int f2Id = fontBaseId + 1;
    int f3Id = fontBaseId + 2;
    int f4Id = fontBaseId + 3;
    int f5Id = fontBaseId + 4;
    int f6Id = fontBaseId + 5;

    // Page objects and content streams
    for (size_t i = 0; i < totalPages; ++i) {
        int pageObjId = 3 + static_cast<int>(i) * 2;
        int streamObjId = pageObjId + 1;

        StartObject(pageObjId);
        pdf << "<< /Type /Page /Parent 2 0 R /Contents " << streamObjId << " 0 R "
            << "/Resources << /Font << "
            << "/F1 " << f1Id << " 0 R "
            << "/F2 " << f2Id << " 0 R "
            << "/F3 " << f3Id << " 0 R "
            << "/F4 " << f4Id << " 0 R "
            << "/F5 " << f5Id << " 0 R "
            << "/F6 " << f6Id << " 0 R "
            << ">> >> >>\nendobj\n";

        std::string streamData = pages[i].stream.str();
        StartObject(streamObjId);
        pdf << "<< /Length " << streamData.size() << " >>\nstream\n"
            << streamData
            << "\nendstream\nendobj\n";
    }

    // Font definitions
    StartObject(f1Id);
    pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\nendobj\n";
    StartObject(f2Id);
    pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>\nendobj\n";
    StartObject(f3Id);
    pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Oblique /Encoding /WinAnsiEncoding >>\nendobj\n";
    StartObject(f4Id);
    pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-BoldOblique /Encoding /WinAnsiEncoding >>\nendobj\n";
    StartObject(f5Id);
    pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Courier /Encoding /WinAnsiEncoding >>\nendobj\n";
    StartObject(f6Id);
    pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Courier-Bold /Encoding /WinAnsiEncoding >>\nendobj\n";

    // XRef table
    size_t xrefOffset = static_cast<size_t>(pdf.tellp());
    size_t totalObjs = objOffsets.size();
    pdf << "xref\n0 " << totalObjs << "\n";
    pdf << "0000000000 65535 f \n";
    for (size_t i = 1; i < totalObjs; ++i) {
        pdf << std::setw(10) << std::setfill('0') << objOffsets[i] << " 00000 n \n";
    }

    // Trailer
    pdf << "trailer\n<< /Size " << totalObjs << " /Root 1 0 R >>\n";
    pdf << "startxref\n" << xrefOffset << "\n%%EOF\n";

    std::string pdfStr = pdf.str();
    return std::vector<uint8_t>(pdfStr.begin(), pdfStr.end());
}

std::vector<uint8_t> PdfExporter::ExportMarkdownToBytes(std::string_view markdown, const PdfExportOptions& options) {
    Markdown::Md4cAdapter adapter;
    auto tree = adapter.Parse(markdown);
    if (!tree) {
        return {};
    }
    return ExportToBytes(*tree, options);
}

bool PdfExporter::ExportToFile(const std::filesystem::path& path, const Markdown::BlockTree& tree, const PdfExportOptions& options) {
    auto bytes = ExportToBytes(tree, options);
    if (bytes.empty()) {
        return false;
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs.is_open()) {
        return false;
    }

    ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return ofs.good();
}

bool PdfExporter::ExportMarkdownToFile(const std::filesystem::path& path, std::string_view markdown, const PdfExportOptions& options) {
    Markdown::Md4cAdapter adapter;
    auto tree = adapter.Parse(markdown);
    if (!tree) {
        return false;
    }
    return ExportToFile(path, *tree, options);
}

} // namespace Pluma::Export
