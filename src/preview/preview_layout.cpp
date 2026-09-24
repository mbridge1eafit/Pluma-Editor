#include "preview_layout.h"

#include <algorithm>
#include <cwctype>

namespace Pluma::Preview {

namespace {

std::wstring Utf8ToUtf16(std::string_view utf8) {
    if (utf8.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring utf16(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), utf16.data(), count);
    return utf16;
}

enum class StyleFlag {
    None = 0,
    Bold = 1 << 0,
    Italic = 1 << 1,
    Strike = 1 << 2,
    Code = 1 << 3,
    Link = 1 << 4
};

inline StyleFlag operator|(StyleFlag a, StyleFlag b) {
    return static_cast<StyleFlag>(static_cast<int>(a) | static_cast<int>(b));
}

inline bool HasFlag(StyleFlag value, StyleFlag flag) {
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}

struct SpanRange {
    UINT32 start = 0;
    UINT32 length = 0;
    StyleFlag style = StyleFlag::None;
    std::string url;
};

void FlattenSpansRecursive(
    const std::vector<Markdown::Span>& spans,
    StyleFlag currentStyle,
    const std::string& currentUrl,
    std::wstring& outText,
    std::vector<SpanRange>& outRanges) {
    for (const auto& span : spans) {
        StyleFlag spanStyle = currentStyle;
        std::string spanUrl = currentUrl;

        switch (span.type) {
        case Markdown::SpanType::Strong:
            spanStyle = spanStyle | StyleFlag::Bold;
            break;
        case Markdown::SpanType::Emphasis:
            spanStyle = spanStyle | StyleFlag::Italic;
            break;
        case Markdown::SpanType::Strikethrough:
            spanStyle = spanStyle | StyleFlag::Strike;
            break;
        case Markdown::SpanType::Code:
            spanStyle = spanStyle | StyleFlag::Code;
            break;
        case Markdown::SpanType::Link:
            spanStyle = spanStyle | StyleFlag::Link;
            spanUrl = span.url;
            break;
        default:
            break;
        }

        UINT32 startPos = static_cast<UINT32>(outText.size());

        if (span.type == Markdown::SpanType::Text || span.type == Markdown::SpanType::Code) {
            std::wstring w = Utf8ToUtf16(span.text);
            outText.append(w);
            UINT32 len = static_cast<UINT32>(w.size());
            if (len > 0 && spanStyle != StyleFlag::None) {
                outRanges.push_back(SpanRange{startPos, len, spanStyle, spanUrl});
            }
        } else if (span.type == Markdown::SpanType::LineBreak) {
            outText.push_back(L'\n');
        }

        if (!span.children.empty()) {
            FlattenSpansRecursive(span.children, spanStyle, spanUrl, outText, outRanges);
            UINT32 totalLen = static_cast<UINT32>(outText.size()) - startPos;
            if (totalLen > 0 && spanStyle != StyleFlag::None) {
                outRanges.push_back(SpanRange{startPos, totalLen, spanStyle, spanUrl});
            }
        }
    }
}

std::string ExtractPlainText(const std::vector<Markdown::Span>& spans) {
    std::string res;
    for (const auto& s : spans) {
        if (!s.text.empty()) {
            res.append(s.text);
        }
        if (!s.children.empty()) {
            res.append(ExtractPlainText(s.children));
        }
    }
    return res;
}

} // namespace

std::string Slugify(std::string_view text) {
    std::string slug;
    slug.reserve(text.size());
    for (char c : text) {
        if (c >= 'A' && c <= 'Z') {
            slug.push_back(static_cast<char>(c - 'A' + 'a'));
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            slug.push_back(c);
        } else if (c == ' ' || c == '-' || c == '_') {
            if (!slug.empty() && slug.back() != '-') {
                slug.push_back('-');
            }
        }
    }
    if (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug;
}

PreviewThemeColors PreviewThemeColors::Light() {
    PreviewThemeColors c{};
    c.background = D2D1::ColorF(1.0f, 1.0f, 1.0f);
    c.text = D2D1::ColorF(0.14f, 0.16f, 0.18f);
    c.headingText = D2D1::ColorF(0.08f, 0.09f, 0.10f);
    c.linkText = D2D1::ColorF(0.035f, 0.41f, 0.85f);
    c.codeText = D2D1::ColorF(0.12f, 0.14f, 0.16f);
    c.codeBackground = D2D1::ColorF(0.965f, 0.972f, 0.98f);
    c.codeBorder = D2D1::ColorF(0.815f, 0.843f, 0.87f);
    c.inlineCodeBg = D2D1::ColorF(0.93f, 0.94f, 0.95f);
    c.blockquoteBorder = D2D1::ColorF(0.815f, 0.843f, 0.87f);
    c.blockquoteText = D2D1::ColorF(0.35f, 0.38f, 0.41f);
    c.ruleLine = D2D1::ColorF(0.815f, 0.843f, 0.87f);
    c.tableBorder = D2D1::ColorF(0.815f, 0.843f, 0.87f);
    c.tableHeaderBg = D2D1::ColorF(0.965f, 0.972f, 0.98f);
    return c;
}

PreviewThemeColors PreviewThemeColors::Dark() {
    PreviewThemeColors c{};
    c.background = D2D1::ColorF(0.118f, 0.118f, 0.118f); // #1E1E1E
    c.text = D2D1::ColorF(0.83f, 0.83f, 0.83f);
    c.headingText = D2D1::ColorF(1.0f, 1.0f, 1.0f);
    c.linkText = D2D1::ColorF(0.345f, 0.65f, 1.0f);
    c.codeText = D2D1::ColorF(0.90f, 0.93f, 0.95f);
    c.codeBackground = D2D1::ColorF(0.086f, 0.106f, 0.133f); // #161B22
    c.codeBorder = D2D1::ColorF(0.188f, 0.212f, 0.239f);    // #30363D
    c.inlineCodeBg = D2D1::ColorF(0.176f, 0.200f, 0.231f);  // #2D333B
    c.blockquoteBorder = D2D1::ColorF(0.23f, 0.26f, 0.30f);
    c.blockquoteText = D2D1::ColorF(0.545f, 0.58f, 0.62f);
    c.ruleLine = D2D1::ColorF(0.188f, 0.212f, 0.239f);
    c.tableBorder = D2D1::ColorF(0.188f, 0.212f, 0.239f);
    c.tableHeaderBg = D2D1::ColorF(0.086f, 0.106f, 0.133f);
    return c;
}

LayoutEngine::LayoutEngine() = default;

bool LayoutEngine::Initialize(IDWriteFactory* dwriteFactory) {
    if (!dwriteFactory) return false;
    m_dwriteFactory = dwriteFactory;

    // Helper to create text format
    auto createFormat = [&](const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight,
                            DWRITE_FONT_STYLE style, ComPtr<IDWriteTextFormat>& outFormat) {
        return SUCCEEDED(m_dwriteFactory->CreateTextFormat(
            family, nullptr, weight, style, DWRITE_FONT_STRETCH_NORMAL,
            size, L"en-us", &outFormat));
    };

    const wchar_t* bodyFamily = L"Segoe UI";
    const wchar_t* codeFamily = L"Consolas";

    createFormat(bodyFamily, 28.0f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, m_formatH1);
    createFormat(bodyFamily, 22.0f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, m_formatH2);
    createFormat(bodyFamily, 18.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, m_formatH3);
    createFormat(bodyFamily, 15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, m_formatH4);
    createFormat(bodyFamily, 13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, m_formatH5);
    createFormat(bodyFamily, 12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, m_formatH6);
    createFormat(bodyFamily, 14.0f, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, m_formatBody);
    createFormat(codeFamily, 13.0f, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, m_formatCode);
    createFormat(bodyFamily, 14.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, m_formatTableHeader);

    return true;
}

ComPtr<IDWriteTextLayout> LayoutEngine::BuildTextLayout(
    const std::vector<Markdown::Span>& spans,
    IDWriteTextFormat* baseFormat,
    float maxTextWidth,
    std::vector<LinkHitBox>& outLinks,
    std::vector<D2D1_RECT_F>& outCodePills,
    float originX, float originY) {
    if (!m_dwriteFactory || !baseFormat) return nullptr;

    std::wstring text;
    std::vector<SpanRange> ranges;
    FlattenSpansRecursive(spans, StyleFlag::None, "", text, ranges);

    if (text.empty()) {
        text = L" "; // DirectWrite requires at least one character for layout metrics
    }

    ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = m_dwriteFactory->CreateTextLayout(
        text.c_str(), static_cast<UINT32>(text.size()), baseFormat,
        (std::max)(10.0f, maxTextWidth), 100000.0f, &layout);

    if (FAILED(hr) || !layout) {
        return nullptr;
    }

    // Apply ranges
    for (const auto& r : ranges) {
        DWRITE_TEXT_RANGE dwr{r.start, r.length};
        if (HasFlag(r.style, StyleFlag::Bold)) {
            layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, dwr);
        }
        if (HasFlag(r.style, StyleFlag::Italic)) {
            layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, dwr);
        }
        if (HasFlag(r.style, StyleFlag::Strike)) {
            layout->SetStrikethrough(TRUE, dwr);
        }
        if (HasFlag(r.style, StyleFlag::Code)) {
            layout->SetFontFamilyName(L"Consolas", dwr);
            layout->SetFontSize(13.0f, dwr);

            // Compute hit boxes for inline code pill background
            UINT32 actualCount = 0;
            layout->HitTestTextRange(r.start, r.length, originX, originY, nullptr, 0, &actualCount);
            if (actualCount > 0) {
                std::vector<DWRITE_HIT_TEST_METRICS> metrics(actualCount);
                layout->HitTestTextRange(r.start, r.length, originX, originY, metrics.data(), actualCount, &actualCount);
                for (UINT32 i = 0; i < actualCount; ++i) {
                    outCodePills.push_back(D2D1::RectF(
                        metrics[i].left - 2.0f, metrics[i].top - 1.0f,
                        metrics[i].left + metrics[i].width + 2.0f, metrics[i].top + metrics[i].height + 1.0f));
                }
            }
        }
        if (HasFlag(r.style, StyleFlag::Link)) {
            layout->SetUnderline(TRUE, dwr);

            // Compute clickable bounding boxes
            UINT32 actualCount = 0;
            layout->HitTestTextRange(r.start, r.length, originX, originY, nullptr, 0, &actualCount);
            if (actualCount > 0) {
                std::vector<DWRITE_HIT_TEST_METRICS> metrics(actualCount);
                layout->HitTestTextRange(r.start, r.length, originX, originY, metrics.data(), actualCount, &actualCount);
                for (UINT32 i = 0; i < actualCount; ++i) {
                    outLinks.push_back(LinkHitBox{
                        D2D1::RectF(metrics[i].left, metrics[i].top,
                                   metrics[i].left + metrics[i].width, metrics[i].top + metrics[i].height),
                        r.url
                    });
                }
            }
        }
    }

    return layout;
}

void LayoutEngine::ComputeLayout(
    const Markdown::BlockTree& tree,
    float contentWidth,
    UINT dpi,
    const PreviewThemeColors& /*colors*/) {
    m_dpi = dpi;
    m_blocks.clear();
    m_anchors.clear();
    m_totalHeight = 0.0f;

    if (!m_dwriteFactory || !tree.root) {
        return;
    }

    float currentY = 24.0f; // Page top padding
    float availableWidth = (std::max)(50.0f, contentWidth - 48.0f); // 24px left/right margins

    for (const auto& child : tree.root->children) {
        if (child) {
            LayoutDocumentBlock(*child, availableWidth, currentY);
        }
    }

    m_totalHeight = currentY + 32.0f; // Bottom padding
}

void LayoutEngine::LayoutDocumentBlock(const Markdown::Block& block, float contentWidth, float& currentY) {
    switch (block.type) {
    case Markdown::BlockType::Heading:
        LayoutHeading(block, contentWidth, currentY);
        break;
    case Markdown::BlockType::Paragraph:
        LayoutParagraph(block, contentWidth, currentY);
        break;
    case Markdown::BlockType::Blockquote:
        LayoutBlockquote(block, contentWidth, currentY);
        break;
    case Markdown::BlockType::List:
        LayoutList(block, contentWidth, currentY);
        break;
    case Markdown::BlockType::CodeBlock:
        LayoutCodeBlock(block, contentWidth, currentY);
        break;
    case Markdown::BlockType::ThematicBreak:
        LayoutThematicBreak(block, contentWidth, currentY);
        break;
    case Markdown::BlockType::Table:
        LayoutTable(block, contentWidth, currentY);
        break;
    default:
        break;
    }
}

void LayoutEngine::LayoutHeading(const Markdown::Block& block, float contentWidth, float& currentY) {
    IDWriteTextFormat* format = m_formatBody.Get();
    float topMargin = 16.0f;
    float bottomMargin = 8.0f;

    switch (block.level) {
    case 1: format = m_formatH1.Get(); topMargin = 24.0f; bottomMargin = 12.0f; break;
    case 2: format = m_formatH2.Get(); topMargin = 20.0f; bottomMargin = 10.0f; break;
    case 3: format = m_formatH3.Get(); topMargin = 16.0f; bottomMargin = 8.0f; break;
    case 4: format = m_formatH4.Get(); topMargin = 14.0f; bottomMargin = 6.0f; break;
    case 5: format = m_formatH5.Get(); topMargin = 12.0f; bottomMargin = 6.0f; break;
    case 6: format = m_formatH6.Get(); topMargin = 10.0f; bottomMargin = 6.0f; break;
    default: break;
    }

    currentY += topMargin;

    LayoutBlock lb{};
    lb.type = Markdown::BlockType::Heading;
    lb.level = block.level;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;

    // Generate anchor slug for F-10 link navigation
    std::string plain = ExtractPlainText(block.inlineContent);
    lb.anchorSlug = Slugify(plain);
    if (!lb.anchorSlug.empty()) {
        m_anchors[lb.anchorSlug] = currentY;
    }

    float originX = 24.0f;
    float originY = currentY;
    lb.textLayout = BuildTextLayout(block.inlineContent, format, contentWidth, lb.links, lb.inlineCodePills, originX, originY);

    float blockHeight = 24.0f;
    if (lb.textLayout) {
        DWRITE_TEXT_METRICS tm{};
        lb.textLayout->GetMetrics(&tm);
        blockHeight = tm.height;
    }

    // Extra spacing for H1/H2 bottom line
    if (block.level <= 2) {
        blockHeight += 8.0f;
    }

    lb.bounds = D2D1::RectF(originX, originY, originX + contentWidth, originY + blockHeight);
    m_blocks.push_back(std::move(lb));

    currentY += blockHeight + bottomMargin;
}

void LayoutEngine::LayoutParagraph(const Markdown::Block& block, float contentWidth, float& currentY) {
    LayoutBlock lb{};
    lb.type = Markdown::BlockType::Paragraph;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;

    float originX = 24.0f;
    float originY = currentY;
    lb.textLayout = BuildTextLayout(block.inlineContent, m_formatBody.Get(), contentWidth, lb.links, lb.inlineCodePills, originX, originY);

    float blockHeight = 16.0f;
    if (lb.textLayout) {
        DWRITE_TEXT_METRICS tm{};
        lb.textLayout->GetMetrics(&tm);
        blockHeight = tm.height;
    }

    lb.bounds = D2D1::RectF(originX, originY, originX + contentWidth, originY + blockHeight);
    m_blocks.push_back(std::move(lb));

    currentY += blockHeight + 12.0f; // Paragraph bottom margin
}

void LayoutEngine::LayoutBlockquote(const Markdown::Block& block, float contentWidth, float& currentY) {
    currentY += 4.0f;
    float startY = currentY;
    float quoteMargin = 16.0f; // Indentation
    float innerWidth = (std::max)(20.0f, contentWidth - quoteMargin);

    for (const auto& child : block.children) {
        if (!child) continue;
        if (child->type == Markdown::BlockType::Paragraph) {
            LayoutBlock lb{};
            lb.type = Markdown::BlockType::Blockquote;
            lb.startLine = child->startLine;
            lb.endLine = child->endLine;

            float originX = 24.0f + quoteMargin;
            float originY = currentY;
            lb.textLayout = BuildTextLayout(child->inlineContent, m_formatBody.Get(), innerWidth, lb.links, lb.inlineCodePills, originX, originY);

            float blockHeight = 16.0f;
            if (lb.textLayout) {
                DWRITE_TEXT_METRICS tm{};
                lb.textLayout->GetMetrics(&tm);
                blockHeight = tm.height;
            }

            lb.bounds = D2D1::RectF(originX, originY, originX + innerWidth, originY + blockHeight);
            m_blocks.push_back(std::move(lb));
            currentY += blockHeight + 8.0f;
        } else {
            LayoutDocumentBlock(*child, innerWidth, currentY);
        }
    }

    // Register a blockquote border block spanning startY to currentY
    LayoutBlock quoteBar{};
    quoteBar.type = Markdown::BlockType::Blockquote;
    quoteBar.bounds = D2D1::RectF(24.0f, startY, 28.0f, currentY);
    m_blocks.push_back(std::move(quoteBar));

    currentY += 8.0f;
}

void LayoutEngine::LayoutList(const Markdown::Block& block, float contentWidth, float& currentY) {
    int itemNumber = block.startNumber;
    for (const auto& item : block.children) {
        if (item && item->type == Markdown::BlockType::ListItem) {
            LayoutListItem(*item, contentWidth, currentY, block.isOrdered, itemNumber++);
        }
    }
    currentY += 6.0f;
}

void LayoutEngine::LayoutListItem(const Markdown::Block& block, float contentWidth, float& currentY, bool isOrdered, int number) {
    LayoutBlock lb{};
    lb.type = Markdown::BlockType::ListItem;
    lb.isOrdered = isOrdered;
    lb.itemNumber = number;
    lb.isTask = block.isTask;
    lb.isTaskChecked = block.isTaskChecked;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;

    float indent = 28.0f;
    float originX = 24.0f + indent;
    float originY = currentY;
    float textWidth = (std::max)(20.0f, contentWidth - indent);

    // If item has children paragraphs, extract first inline or use item's own inlineContent
    const std::vector<Markdown::Span>* inlineSpans = &block.inlineContent;
    if (inlineSpans->empty() && !block.children.empty() && block.children[0]) {
        inlineSpans = &block.children[0]->inlineContent;
    }

    lb.textLayout = BuildTextLayout(*inlineSpans, m_formatBody.Get(), textWidth, lb.links, lb.inlineCodePills, originX, originY);

    float blockHeight = 18.0f;
    if (lb.textLayout) {
        DWRITE_TEXT_METRICS tm{};
        lb.textLayout->GetMetrics(&tm);
        blockHeight = (std::max)(blockHeight, tm.height);
    }

    lb.bounds = D2D1::RectF(24.0f, originY, 24.0f + contentWidth, originY + blockHeight);
    m_blocks.push_back(std::move(lb));

    currentY += blockHeight + 6.0f;
}

void LayoutEngine::LayoutCodeBlock(const Markdown::Block& block, float contentWidth, float& currentY) {
    currentY += 6.0f;

    LayoutBlock lb{};
    lb.type = Markdown::BlockType::CodeBlock;
    lb.info = block.info;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;

    std::string code;
    for (const auto& s : block.inlineContent) {
        code.append(s.text);
    }
    lb.codeText = code;

    std::wstring wCode = Utf8ToUtf16(code);
    if (wCode.empty()) wCode = L" ";

    float padding = 12.0f;
    float textWidth = (std::max)(20.0f, contentWidth - padding * 2);

    if (m_dwriteFactory && m_formatCode) {
        m_dwriteFactory->CreateTextLayout(
            wCode.c_str(), static_cast<UINT32>(wCode.size()), m_formatCode.Get(),
            textWidth, 100000.0f, &lb.textLayout);
    }

    float blockHeight = 30.0f;
    if (lb.textLayout) {
        DWRITE_TEXT_METRICS tm{};
        lb.textLayout->GetMetrics(&tm);
        blockHeight = tm.height + padding * 2;
    }

    lb.bounds = D2D1::RectF(24.0f, currentY, 24.0f + contentWidth, currentY + blockHeight);
    m_blocks.push_back(std::move(lb));

    currentY += blockHeight + 12.0f;
}

void LayoutEngine::LayoutThematicBreak(const Markdown::Block& block, float contentWidth, float& currentY) {
    currentY += 12.0f;

    LayoutBlock lb{};
    lb.type = Markdown::BlockType::ThematicBreak;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;
    lb.bounds = D2D1::RectF(24.0f, currentY, 24.0f + contentWidth, currentY + 2.0f);
    m_blocks.push_back(std::move(lb));

    currentY += 14.0f;
}

void LayoutEngine::LayoutTable(const Markdown::Block& block, float contentWidth, float& currentY) {
    currentY += 8.0f;

    LayoutBlock lb{};
    lb.type = Markdown::BlockType::Table;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;

    // Collect rows
    std::vector<const Markdown::Block*> rowBlocks;
    for (const auto& section : block.children) {
        if (!section) continue;
        if (section->type == Markdown::BlockType::TableRow) {
            rowBlocks.push_back(section.get());
        } else if (section->type == Markdown::BlockType::TableHead || section->type == Markdown::BlockType::TableBody) {
            for (const auto& row : section->children) {
                if (row && row->type == Markdown::BlockType::TableRow) {
                    rowBlocks.push_back(row.get());
                }
            }
        }
    }

    if (rowBlocks.empty()) return;

    size_t colCount = 0;
    for (const auto* r : rowBlocks) {
        colCount = (std::max)(colCount, r->children.size());
    }
    if (colCount == 0) return;

    // Equal column distribution (fits contentWidth)
    float colWidth = contentWidth / static_cast<float>(colCount);
    lb.tableColWidths.assign(colCount, colWidth);

    float tableY = currentY;
    for (const auto* r : rowBlocks) {
        std::vector<TableCellLayout> rowCells;
        float maxCellH = 24.0f;

        for (size_t c = 0; c < colCount; ++c) {
            TableCellLayout cell{};
            cell.isHeader = false;
            cell.align = Markdown::Alignment::Default;

            if (c < r->children.size() && r->children[c]) {
                const auto& cellBlock = *r->children[c];
                cell.isHeader = cellBlock.isHeaderCell;
                cell.align = cellBlock.align;

                IDWriteTextFormat* fmt = cell.isHeader ? m_formatTableHeader.Get() : m_formatBody.Get();
                float cellTextW = (std::max)(10.0f, colWidth - 16.0f);
                cell.textLayout = BuildTextLayout(cellBlock.inlineContent, fmt, cellTextW, lb.links, lb.inlineCodePills,
                                                  24.0f + static_cast<float>(c) * colWidth + 8.0f, tableY + 6.0f);
                if (cell.textLayout) {
                    DWRITE_TEXT_METRICS tm{};
                    cell.textLayout->GetMetrics(&tm);
                    maxCellH = (std::max)(maxCellH, tm.height + 12.0f);
                }
            }
            rowCells.push_back(std::move(cell));
        }

        // Assign cell rects
        for (size_t c = 0; c < colCount; ++c) {
            float cx = 24.0f + static_cast<float>(c) * colWidth;
            rowCells[c].rect = D2D1::RectF(cx, tableY, cx + colWidth, tableY + maxCellH);
        }

        lb.tableRows.push_back(std::move(rowCells));
        tableY += maxCellH;
    }

    lb.bounds = D2D1::RectF(24.0f, currentY, 24.0f + contentWidth, tableY);
    m_blocks.push_back(std::move(lb));

    currentY = tableY + 12.0f;
}

std::string LayoutEngine::HitTestLink(float x, float y) const {
    for (const auto& b : m_blocks) {
        if (x >= b.bounds.left && x <= b.bounds.right &&
            y >= b.bounds.top && y <= b.bounds.bottom) {
            for (const auto& link : b.links) {
                if (x >= link.rect.left && x <= link.rect.right &&
                    y >= link.rect.top && y <= link.rect.bottom) {
                    return link.url;
                }
            }
        }
    }
    return {};
}

float LayoutEngine::GetAnchorY(std::string_view slug) const {
    std::string s(slug);
    if (!s.empty() && s[0] == '#') {
        s = s.substr(1);
    }
    auto it = m_anchors.find(s);
    if (it != m_anchors.end()) {
        return it->second;
    }
    return -1.0f;
}

} // namespace Pluma::Preview
