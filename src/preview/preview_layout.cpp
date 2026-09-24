#include "preview_layout.h"

#include <dwrite_1.h>

#include <algorithm>
#include <cmath>

namespace Pluma::Preview {

namespace {

// Layout metrics (DIPs)
constexpr float kMaxContentWidth = 900.0f;
constexpr float kPagePaddingTop = 24.0f;
constexpr float kPagePaddingBottom = 48.0f;
constexpr float kBlockGap = 14.0f;
constexpr float kListItemGap = 5.0f;
constexpr float kNestedListGap = 4.0f;
constexpr float kQuoteIndent = 18.0f;
constexpr float kCodePaddingX = 14.0f;
constexpr float kCodePaddingY = 12.0f;
constexpr float kCellPaddingX = 10.0f;
constexpr float kCellPaddingY = 6.0f;
constexpr float kCellMinContentWidth = 16.0f;
constexpr float kCellMaxMinWidth = 140.0f; // Long unbreakable words beyond this are emergency-wrapped
constexpr float kMinTextWidth = 40.0f;
constexpr float kInlineCodeSpacing = 5.0f;

constexpr D2D1_COLOR_F Hex(uint32_t rgb) {
    return D2D1_COLOR_F{
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgb & 0xFF) / 255.0f,
        1.0f};
}

std::wstring Utf8ToUtf16(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring utf16(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), utf16.data(), count);
    return utf16;
}

enum StyleFlag : unsigned {
    kStyleNone = 0,
    kStyleBold = 1u << 0,
    kStyleItalic = 1u << 1,
    kStyleStrike = 1u << 2,
    kStyleCode = 1u << 3,
    kStyleLink = 1u << 4,
};

bool HasHeaderCells(const Markdown::Block& row) {
    return std::any_of(row.children.begin(), row.children.end(),
                       [](const auto& cell) { return cell && cell->isHeaderCell; });
}

std::wstring_view FileNameOf(std::wstring_view path) {
    const size_t slash = path.find_last_of(L"/\\");
    return slash == std::wstring_view::npos ? path : path.substr(slash + 1);
}

} // namespace

struct LayoutEngine::SpanRange {
    UINT32 start = 0;
    UINT32 length = 0;
    unsigned style = kStyleNone;
    std::string url;
};

struct LayoutEngine::RichText {
    ComPtr<IDWriteTextLayout> layout;
    std::vector<SpanRange> ranges;
    std::vector<EffectRange> effects;
    float codeFontSize = 12.0f;
    float lineHeight = 20.0f;
    float baseline = 16.0f;
};

namespace {

// Flattens a span tree into UTF-16 text plus styled leaf ranges.
template <typename Range>
void FlattenSpans(const std::vector<Markdown::Span>& spans, unsigned style, const std::string& url,
                  std::wstring& outText, std::vector<Range>& outRanges) {
    for (const auto& span : spans) {
        unsigned spanStyle = style;
        const std::string* spanUrl = &url;

        switch (span.type) {
        case Markdown::SpanType::Strong:        spanStyle |= kStyleBold; break;
        case Markdown::SpanType::Emphasis:      spanStyle |= kStyleItalic; break;
        case Markdown::SpanType::Strikethrough: spanStyle |= kStyleStrike; break;
        case Markdown::SpanType::Code:          spanStyle |= kStyleCode; break;
        case Markdown::SpanType::Link:
            spanStyle |= kStyleLink;
            spanUrl = &span.url;
            break;
        case Markdown::SpanType::Image:
            spanStyle |= kStyleLink | kStyleItalic;
            spanUrl = &span.url;
            break;
        default:
            break;
        }

        auto appendLeaf = [&](std::wstring_view w) {
            if (w.empty()) return;
            const auto start = static_cast<UINT32>(outText.size());
            outText.append(w);
            if (spanStyle != kStyleNone) {
                outRanges.push_back(Range{start, static_cast<UINT32>(w.size()), spanStyle, *spanUrl});
            }
        };

        switch (span.type) {
        case Markdown::SpanType::Text:
        case Markdown::SpanType::Code:
            appendLeaf(Utf8ToUtf16(span.text));
            break;
        case Markdown::SpanType::LineBreak:
            outText.push_back(L'\x2028'); // Line separator: new line inside the same paragraph
            break;
        case Markdown::SpanType::Image: {
            // No image decoding in the preview: show a clickable placeholder with the alt text.
            std::wstring label = L"\U0001F5BC️ ";
            std::wstring alt = Utf8ToUtf16(Markdown::ExtractPlainText(span.children));
            label += alt.empty() ? std::wstring(FileNameOf(Utf8ToUtf16(span.url))) : alt;
            appendLeaf(label);
            continue; // Alt text already emitted
        }
        default:
            break;
        }

        if (!span.children.empty()) {
            FlattenSpans(span.children, spanStyle, *spanUrl, outText, outRanges);
        }
    }
}

} // namespace

PreviewThemeColors PreviewThemeColors::Light() {
    PreviewThemeColors c{};
    c.background = Hex(0xFFFFFF);
    c.text = Hex(0x1F2328);
    c.mutedText = Hex(0x59636E);
    c.headingText = Hex(0x1F2328);
    c.linkText = Hex(0x0969DA);
    c.codeText = Hex(0x1F2328);
    c.codeBackground = Hex(0xF6F8FA);
    c.codeBorder = Hex(0xD1D9E0);
    c.inlineCodeBg = Hex(0xEEF1F4);
    c.inlineCodeText = Hex(0x1F2328);
    c.blockquoteBorder = Hex(0xD1D9E0);
    c.blockquoteText = Hex(0x59636E);
    c.ruleLine = Hex(0xD1D9E0);
    c.tableBorder = Hex(0xD1D9E0);
    c.tableHeaderBg = Hex(0xF6F8FA);
    c.tableStripeBg = Hex(0xFAFBFC);
    c.accent = Hex(0x0969DA);
    c.accentText = Hex(0xFFFFFF);
    return c;
}

PreviewThemeColors PreviewThemeColors::Dark() {
    PreviewThemeColors c{};
    c.background = Hex(0x1E1E1E); // Matches the editor background
    c.text = Hex(0xD4D4D4);
    c.mutedText = Hex(0x9198A1);
    c.headingText = Hex(0xF0F6FC);
    c.linkText = Hex(0x4493F8);
    c.codeText = Hex(0xE6EDF3);
    c.codeBackground = Hex(0x252526);
    c.codeBorder = Hex(0x3C3C3C);
    c.inlineCodeBg = Hex(0x34373B);
    c.inlineCodeText = Hex(0xE6EDF3);
    c.blockquoteBorder = Hex(0x3D444D);
    c.blockquoteText = Hex(0x9198A1);
    c.ruleLine = Hex(0x3D444D);
    c.tableBorder = Hex(0x3D444D);
    c.tableHeaderBg = Hex(0x252526);
    c.tableStripeBg = Hex(0x222223);
    c.accent = Hex(0x4493F8);
    c.accentText = Hex(0x0D1117);
    return c;
}

LayoutEngine::LayoutEngine() = default;

bool LayoutEngine::CreateStyle(const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight,
                               float lineSpacingFactor, TextStyle& out) {
    wchar_t locale[LOCALE_NAME_MAX_LENGTH] = L"es-es";
    GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH);

    out.format.Reset();
    if (FAILED(m_dwriteFactory->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL, size, locale, &out.format))) {
        return false;
    }
    out.fontSize = size;

    // Break long words (URLs, identifiers) instead of letting them overflow (Windows 8.1+).
    if (FAILED(out.format->SetWordWrapping(DWRITE_WORD_WRAPPING_EMERGENCY_BREAK))) {
        out.format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    }

    // Uniform line spacing keeps lines evenly spaced even when emoji or code fall back to other fonts.
    float ascent = size * 0.92f;
    float descent = size * 0.25f;
    ComPtr<IDWriteFontCollection> collection;
    if (SUCCEEDED(m_dwriteFactory->GetSystemFontCollection(&collection))) {
        UINT32 index = 0;
        BOOL exists = FALSE;
        ComPtr<IDWriteFontFamily> fontFamily;
        ComPtr<IDWriteFont> font;
        if (SUCCEEDED(collection->FindFamilyName(family, &index, &exists)) && exists &&
            SUCCEEDED(collection->GetFontFamily(index, &fontFamily)) &&
            SUCCEEDED(fontFamily->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL,
                                                       DWRITE_FONT_STYLE_NORMAL, &font))) {
            DWRITE_FONT_METRICS fm{};
            font->GetMetrics(&fm);
            if (fm.designUnitsPerEm > 0) {
                ascent = size * static_cast<float>(fm.ascent) / fm.designUnitsPerEm;
                descent = size * static_cast<float>(fm.descent) / fm.designUnitsPerEm;
            }
        }
    }
    out.lineHeight = std::ceil((std::max)(ascent + descent, size * lineSpacingFactor));
    out.baseline = std::round(ascent + (out.lineHeight - ascent - descent) * 0.5f);
    out.format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, out.lineHeight, out.baseline);
    return true;
}

bool LayoutEngine::Initialize(IDWriteFactory* dwriteFactory) {
    if (!dwriteFactory) return false;
    m_dwriteFactory = dwriteFactory;

    // Prefer the modern Cascadia Mono when installed, fall back to Consolas.
    ComPtr<IDWriteFontCollection> collection;
    if (SUCCEEDED(m_dwriteFactory->GetSystemFontCollection(&collection))) {
        for (const wchar_t* candidate : {L"Cascadia Mono", L"Consolas"}) {
            UINT32 index = 0;
            BOOL exists = FALSE;
            if (SUCCEEDED(collection->FindFamilyName(candidate, &index, &exists)) && exists) {
                m_codeFamily = candidate;
                break;
            }
        }
    }

    const wchar_t* body = L"Segoe UI";
    const float headingSizes[6] = {30.0f, 23.0f, 19.0f, 16.5f, 15.0f, 13.5f};
    const DWRITE_FONT_WEIGHT headingWeights[6] = {
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_WEIGHT_SEMI_BOLD};

    bool ok = true;
    for (int i = 0; i < 6; ++i) {
        ok &= CreateStyle(body, headingSizes[i], headingWeights[i], 1.25f, m_styleH[i]);
    }
    ok &= CreateStyle(body, 15.0f, DWRITE_FONT_WEIGHT_REGULAR, 1.6f, m_styleBody);
    ok &= CreateStyle(m_codeFamily.c_str(), 13.5f, DWRITE_FONT_WEIGHT_REGULAR, 1.45f, m_styleCode);
    ok &= CreateStyle(body, 14.5f, DWRITE_FONT_WEIGHT_REGULAR, 1.45f, m_styleTable);
    ok &= CreateStyle(body, 14.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, 1.45f, m_styleTableHeader);
    ok &= CreateStyle(body, 11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, 1.3f, m_styleLabel);
    if (!ok) {
        return false;
    }

    // Tab stops every 4 monospace characters in code blocks.
    if (auto probe = CreatePlainLayout(L"0000000000", m_styleCode, 10000.0f)) {
        DWRITE_TEXT_METRICS tm{};
        probe->GetMetrics(&tm);
        if (tm.width > 0.0f) {
            m_codeCharWidth = tm.width / 10.0f;
        }
    }
    m_styleCode.format->SetIncrementalTabStop(m_codeCharWidth * 4.0f);
    return true;
}

ComPtr<IDWriteTextLayout> LayoutEngine::CreatePlainLayout(std::wstring_view text, const TextStyle& style,
                                                          float maxWidth) const {
    ComPtr<IDWriteTextLayout> layout;
    if (!m_dwriteFactory || !style.format) return layout;
    if (text.empty()) text = L" ";
    m_dwriteFactory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), style.format.Get(),
                                      (std::max)(1.0f, maxWidth), 1.0e6f, &layout);
    return layout;
}

LayoutEngine::RichText LayoutEngine::BuildRichText(const std::vector<Markdown::Span>& spans,
                                                   const TextStyle& style, float maxWidth) {
    RichText rich;
    rich.lineHeight = style.lineHeight;
    rich.baseline = style.baseline;
    rich.codeFontSize = std::round(style.fontSize * 0.88f * 2.0f) / 2.0f;

    std::wstring text;
    FlattenSpans(spans, kStyleNone, std::string(), text, rich.ranges);

    rich.layout = CreatePlainLayout(text, style, (std::max)(kMinTextWidth, maxWidth));
    if (!rich.layout) {
        return rich;
    }

    ComPtr<IDWriteTextLayout1> layout1;
    rich.layout.As(&layout1);

    for (const auto& r : rich.ranges) {
        const DWRITE_TEXT_RANGE dwr{r.start, r.length};
        if (r.style & kStyleBold) {
            rich.layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, dwr);
        }
        if (r.style & kStyleItalic) {
            rich.layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, dwr);
        }
        if (r.style & kStyleStrike) {
            rich.layout->SetStrikethrough(TRUE, dwr);
        }
        if (r.style & kStyleCode) {
            rich.layout->SetFontFamilyName(m_codeFamily.c_str(), dwr);
            rich.layout->SetFontSize(rich.codeFontSize, dwr);
            rich.effects.push_back(EffectRange{r.start, r.length, TextEffect::Code});
            // Real spacing around inline code so its background pill never touches the neighbours.
            if (layout1) {
                layout1->SetCharacterSpacing(kInlineCodeSpacing, 0.0f, 0.0f, DWRITE_TEXT_RANGE{r.start, 1});
                layout1->SetCharacterSpacing(0.0f, kInlineCodeSpacing, 0.0f, DWRITE_TEXT_RANGE{r.start + r.length - 1, 1});
                if (r.length == 1) {
                    layout1->SetCharacterSpacing(kInlineCodeSpacing, kInlineCodeSpacing, 0.0f, dwr);
                }
            }
        }
        if (r.style & kStyleLink) {
            rich.effects.push_back(EffectRange{r.start, r.length, TextEffect::Link});
        }
    }

    return rich;
}

void LayoutEngine::CollectDecorations(const RichText& rich, D2D1_POINT_2F origin,
                                      std::vector<LinkHitBox>& outLinks,
                                      std::vector<D2D1_RECT_F>& outCodePills) const {
    if (!rich.layout) return;

    std::vector<DWRITE_HIT_TEST_METRICS> metrics;
    auto hitTest = [&](const SpanRange& r) -> UINT32 {
        UINT32 count = 0;
        rich.layout->HitTestTextRange(r.start, r.length, origin.x, origin.y, nullptr, 0, &count);
        if (count == 0) return 0;
        metrics.resize(count);
        rich.layout->HitTestTextRange(r.start, r.length, origin.x, origin.y, metrics.data(), count, &count);
        return count;
    };

    for (const auto& r : rich.ranges) {
        if (r.style & kStyleCode) {
            const UINT32 count = hitTest(r);
            // Pills hug the monospace glyphs instead of filling the whole (tall) line box.
            const float ascent = rich.codeFontSize * 1.02f;
            const float descent = rich.codeFontSize * 0.38f;
            for (UINT32 i = 0; i < count; ++i) {
                const auto& m = metrics[i];
                if (m.width <= 0.0f) continue;
                const float baselineY = m.top + rich.baseline;
                outCodePills.push_back(D2D1::RectF(m.left + 1.0f, baselineY - ascent,
                                                   m.left + m.width - 1.0f, baselineY + descent));
            }
        }
        if (r.style & kStyleLink) {
            const UINT32 count = hitTest(r);
            for (UINT32 i = 0; i < count; ++i) {
                const auto& m = metrics[i];
                outLinks.push_back(LinkHitBox{
                    D2D1::RectF(m.left, m.top, m.left + m.width, m.top + m.height),
                    r.url, rich.layout.Get(), r.start, r.length});
            }
        }
    }
}

void LayoutEngine::ComputeLayout(const Markdown::BlockTree& tree, float contentWidth, UINT dpi,
                                 const PreviewThemeColors& /*colors*/) {
    m_dpi = dpi;
    m_blocks.clear();
    m_syncBlocks.clear();
    m_anchors.clear();
    m_slugCounts.clear();
    m_totalHeight = 0.0f;
    m_layoutWidth = contentWidth;

    if (!m_dwriteFactory || !tree.root) {
        return;
    }

    // Centered reading column with comfortable side padding.
    const float padding = contentWidth < 520.0f ? 16.0f : 32.0f;
    const float columnWidth = (std::max)(120.0f, (std::min)(kMaxContentWidth, contentWidth - padding * 2.0f));
    const float left = (std::max)(padding, (contentWidth - columnWidth) * 0.5f);

    Context ctx;
    ctx.x = std::floor(left);
    ctx.width = columnWidth;

    float currentY = kPagePaddingTop;
    LayoutChildren(*tree.root, ctx, currentY);
    m_totalHeight = currentY + kPagePaddingBottom;

    for (size_t i = 0; i < m_blocks.size(); ++i) {
        const auto& b = m_blocks[i];
        if (!b.isQuoteBar && b.startLine > 0 && b.endLine >= b.startLine) {
            m_syncBlocks.push_back(i);
        }
    }
}

void LayoutEngine::LayoutChildren(const Markdown::Block& parent, const Context& ctx, float& currentY) {
    const float containerTop = currentY;
    for (const auto& child : parent.children) {
        if (child) {
            LayoutNode(*child, ctx, currentY, containerTop);
        }
    }
}

void LayoutEngine::LayoutNode(const Markdown::Block& block, const Context& ctx, float& currentY, float containerTop) {
    // Vertical gaps are only inserted between siblings, so containers end exactly at their content.
    auto addGap = [&](float gap) {
        if (currentY > containerTop + 0.5f) {
            currentY += gap;
        }
    };

    switch (block.type) {
    case Markdown::BlockType::Heading:
        addGap(block.level <= 2 ? kBlockGap + 12.0f : kBlockGap + 6.0f);
        LayoutHeading(block, ctx, currentY);
        break;
    case Markdown::BlockType::Paragraph:
        if (block.inlineContent.empty()) break; // e.g. an HTML comment block
        addGap(kBlockGap);
        LayoutParagraph(block, ctx, currentY);
        break;
    case Markdown::BlockType::Blockquote:
        addGap(kBlockGap);
        LayoutBlockquote(block, ctx, currentY);
        break;
    case Markdown::BlockType::List:
        addGap(ctx.listDepth > 0 ? kNestedListGap : kBlockGap);
        LayoutList(block, ctx, currentY);
        break;
    case Markdown::BlockType::CodeBlock:
        addGap(kBlockGap);
        LayoutCodeBlock(block, ctx, currentY);
        break;
    case Markdown::BlockType::ThematicBreak:
        addGap(kBlockGap + 6.0f);
        LayoutThematicBreak(block, ctx, currentY);
        break;
    case Markdown::BlockType::Table:
        addGap(kBlockGap);
        LayoutTable(block, ctx, currentY);
        break;
    default:
        break;
    }
}

void LayoutEngine::LayoutHeading(const Markdown::Block& block, const Context& ctx, float& currentY) {
    const int level = (std::clamp)(block.level, 1, 6);
    const TextStyle& style = m_styleH[level - 1];

    LayoutBlock lb{};
    lb.type = Markdown::BlockType::Heading;
    lb.level = level;
    lb.quoteDepth = ctx.quoteDepth;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;

    // Anchor slug for F-10 link navigation; duplicates get "-1", "-2"... like GitHub.
    std::string slug = Slugify(Markdown::ExtractPlainText(block.inlineContent));
    if (!slug.empty()) {
        const int seen = m_slugCounts[slug]++;
        if (seen > 0) {
            slug += "-" + std::to_string(seen);
        }
        m_anchors.emplace(slug, currentY);
    }
    lb.anchorSlug = std::move(slug);

    RichText rich = BuildRichText(block.inlineContent, style, ctx.width);
    float height = style.lineHeight;
    if (rich.layout) {
        DWRITE_TEXT_METRICS tm{};
        rich.layout->GetMetrics(&tm);
        height = tm.height;
    }
    if (level <= 2) {
        height += 10.0f; // Room for the bottom rule
    }

    lb.textOrigin = D2D1::Point2F(ctx.x, currentY);
    lb.bounds = D2D1::RectF(ctx.x, currentY, ctx.x + ctx.width, currentY + height);
    CollectDecorations(rich, lb.textOrigin, lb.links, lb.inlineCodePills);
    lb.textLayout = std::move(rich.layout);
    lb.effects = std::move(rich.effects);
    m_blocks.push_back(std::move(lb));

    currentY += height;
}

void LayoutEngine::LayoutParagraph(const Markdown::Block& block, const Context& ctx, float& currentY) {
    LayoutBlock lb{};
    lb.type = Markdown::BlockType::Paragraph;
    lb.quoteDepth = ctx.quoteDepth;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;

    RichText rich = BuildRichText(block.inlineContent, m_styleBody, ctx.width);
    float height = m_styleBody.lineHeight;
    if (rich.layout) {
        DWRITE_TEXT_METRICS tm{};
        rich.layout->GetMetrics(&tm);
        height = tm.height;
    }

    lb.textOrigin = D2D1::Point2F(ctx.x, currentY);
    lb.bounds = D2D1::RectF(ctx.x, currentY, ctx.x + ctx.width, currentY + height);
    CollectDecorations(rich, lb.textOrigin, lb.links, lb.inlineCodePills);
    lb.textLayout = std::move(rich.layout);
    lb.effects = std::move(rich.effects);
    m_blocks.push_back(std::move(lb));

    currentY += height;
}

void LayoutEngine::LayoutBlockquote(const Markdown::Block& block, const Context& ctx, float& currentY) {
    const float top = currentY;

    Context inner = ctx;
    inner.x = ctx.x + kQuoteIndent;
    inner.width = (std::max)(kMinTextWidth, ctx.width - kQuoteIndent);
    inner.quoteDepth = ctx.quoteDepth + 1;
    LayoutChildren(block, inner, currentY);

    if (currentY <= top) {
        currentY = top + m_styleBody.lineHeight; // Empty quote: keep a visible bar
    }

    LayoutBlock bar{};
    bar.type = Markdown::BlockType::Blockquote;
    bar.isQuoteBar = true;
    bar.quoteDepth = inner.quoteDepth;
    bar.bounds = D2D1::RectF(ctx.x + 1.0f, top, ctx.x + 4.5f, currentY);
    m_blocks.push_back(std::move(bar));
}

void LayoutEngine::LayoutList(const Markdown::Block& block, const Context& ctx, float& currentY) {
    // Marker column wide enough for the largest number ("10." etc.).
    float markerWidth = 26.0f;
    if (block.isOrdered) {
        const int lastNumber = block.startNumber + static_cast<int>(block.children.size()) - 1;
        const std::wstring widest = std::to_wstring((std::max)(block.startNumber, lastNumber)) + L".";
        if (auto probe = CreatePlainLayout(widest, m_styleBody, 1000.0f)) {
            DWRITE_TEXT_METRICS tm{};
            probe->GetMetrics(&tm);
            markerWidth = (std::max)(markerWidth, std::ceil(tm.width) + 10.0f);
        }
    }

    const float listTop = currentY;
    int itemNumber = block.startNumber;
    for (const auto& item : block.children) {
        if (!item || item->type != Markdown::BlockType::ListItem) continue;
        if (currentY > listTop + 0.5f) {
            currentY += kListItemGap;
        }
        LayoutListItem(*item, ctx, markerWidth, block.isOrdered, itemNumber++, currentY);
    }
}

void LayoutEngine::LayoutListItem(const Markdown::Block& block, const Context& ctx, float markerWidth,
                                  bool isOrdered, int number, float& currentY) {
    const float itemTop = currentY;
    const float textX = ctx.x + markerWidth;
    const float textWidth = (std::max)(kMinTextWidth, ctx.width - markerWidth);

    // Tight items carry their text inline; loose items wrap it in a first paragraph.
    const std::vector<Markdown::Span>* firstSpans = &block.inlineContent;
    size_t restIndex = 0;
    int ownEndLine = block.endLine;
    if (firstSpans->empty() && !block.children.empty() && block.children[0] &&
        block.children[0]->type == Markdown::BlockType::Paragraph) {
        firstSpans = &block.children[0]->inlineContent;
        ownEndLine = block.children[0]->endLine;
        restIndex = 1;
    }
    if (restIndex < block.children.size() && block.children[restIndex]) {
        ownEndLine = (std::min)(ownEndLine, (std::max)(block.startLine, block.children[restIndex]->startLine - 1));
    }

    LayoutBlock lb{};
    lb.type = Markdown::BlockType::ListItem;
    lb.isOrdered = isOrdered;
    lb.itemNumber = number;
    lb.isTask = block.isTask;
    lb.isTaskChecked = block.isTaskChecked;
    lb.listDepth = ctx.listDepth;
    lb.quoteDepth = ctx.quoteDepth;
    lb.startLine = block.startLine;
    lb.endLine = (std::max)(block.startLine, ownEndLine);

    RichText rich = BuildRichText(*firstSpans, m_styleBody, textWidth);
    const float lineHeight = m_styleBody.lineHeight;
    float height = lineHeight;
    if (rich.layout) {
        DWRITE_TEXT_METRICS tm{};
        rich.layout->GetMetrics(&tm);
        height = (std::max)(lineHeight, tm.height);
    }

    const float midY = currentY + lineHeight * 0.5f;
    if (block.isTask) {
        const float box = std::round(m_styleBody.fontSize * 1.0f);
        lb.markerRect = D2D1::RectF(ctx.x + 2.0f, std::round(midY - box * 0.5f),
                                    ctx.x + 2.0f + box, std::round(midY - box * 0.5f) + box);
    } else if (isOrdered) {
        lb.markerLayout = CreatePlainLayout(std::to_wstring(number) + L".", m_styleBody, markerWidth - 8.0f);
        if (lb.markerLayout) {
            lb.markerLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            lb.markerLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        lb.markerOrigin = D2D1::Point2F(ctx.x, currentY);
    } else {
        const float r = 2.8f;
        const float cx = ctx.x + markerWidth * 0.5f - 3.0f;
        lb.markerRect = D2D1::RectF(cx - r, midY - r, cx + r, midY + r);
    }

    lb.textOrigin = D2D1::Point2F(textX, currentY);
    lb.bounds = D2D1::RectF(ctx.x, currentY, ctx.x + ctx.width, currentY + height);
    CollectDecorations(rich, lb.textOrigin, lb.links, lb.inlineCodePills);
    lb.textLayout = std::move(rich.layout);
    lb.effects = std::move(rich.effects);
    m_blocks.push_back(std::move(lb));
    currentY += height;

    // Nested content (sub-lists, extra paragraphs, code blocks...) is indented under the text.
    Context inner = ctx;
    inner.x = textX;
    inner.width = textWidth;
    inner.listDepth = ctx.listDepth + 1;
    for (size_t i = restIndex; i < block.children.size(); ++i) {
        if (block.children[i]) {
            LayoutNode(*block.children[i], inner, currentY, itemTop);
        }
    }
}

void LayoutEngine::LayoutCodeBlock(const Markdown::Block& block, const Context& ctx, float& currentY) {
    LayoutBlock lb{};
    lb.type = Markdown::BlockType::CodeBlock;
    lb.info = block.info;
    lb.quoteDepth = ctx.quoteDepth;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;

    for (const auto& s : block.inlineContent) {
        lb.codeText.append(s.text);
    }

    std::wstring code = Utf8ToUtf16(lb.codeText);
    while (!code.empty() && (code.back() == L'\n' || code.back() == L'\r')) {
        code.pop_back();
    }

    float textTop = currentY + kCodePaddingY;
    if (!lb.info.empty()) {
        lb.labelLayout = CreatePlainLayout(Utf8ToUtf16(lb.info), m_styleLabel, ctx.width - 2.0f * kCodePaddingX);
        if (lb.labelLayout) {
            lb.labelLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        lb.labelOrigin = D2D1::Point2F(ctx.x + kCodePaddingX, currentY + 7.0f);
        textTop = currentY + 7.0f + m_styleLabel.lineHeight + 4.0f;
    }

    const float textWidth = (std::max)(kMinTextWidth, ctx.width - 2.0f * kCodePaddingX);
    lb.textLayout = CreatePlainLayout(code, m_styleCode, textWidth);

    float textHeight = m_styleCode.lineHeight;
    if (lb.textLayout) {
        DWRITE_TEXT_METRICS tm{};
        lb.textLayout->GetMetrics(&tm);
        textHeight = tm.height;
    }

    lb.textOrigin = D2D1::Point2F(ctx.x + kCodePaddingX, textTop);
    const float bottom = textTop + textHeight + kCodePaddingY;
    lb.bounds = D2D1::RectF(ctx.x, currentY, ctx.x + ctx.width, bottom);
    m_blocks.push_back(std::move(lb));

    currentY = bottom;
}

void LayoutEngine::LayoutThematicBreak(const Markdown::Block& block, const Context& ctx, float& currentY) {
    LayoutBlock lb{};
    lb.type = Markdown::BlockType::ThematicBreak;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;
    lb.bounds = D2D1::RectF(ctx.x, currentY, ctx.x + ctx.width, currentY + 2.0f);
    m_blocks.push_back(std::move(lb));

    currentY += 2.0f + 6.0f;
}

void LayoutEngine::LayoutTable(const Markdown::Block& block, const Context& ctx, float& currentY) {
    // Collect rows (header first) and remember which ones are header rows.
    struct RowRef {
        const Markdown::Block* row;
        bool isHeader;
    };
    std::vector<RowRef> rows;
    for (const auto& section : block.children) {
        if (!section) continue;
        if (section->type == Markdown::BlockType::TableRow) {
            rows.push_back({section.get(), HasHeaderCells(*section)});
        } else if (section->type == Markdown::BlockType::TableHead || section->type == Markdown::BlockType::TableBody) {
            const bool head = section->type == Markdown::BlockType::TableHead;
            for (const auto& row : section->children) {
                if (row && row->type == Markdown::BlockType::TableRow) {
                    rows.push_back({row.get(), head || HasHeaderCells(*row)});
                }
            }
        }
    }
    if (rows.empty()) return;

    size_t colCount = 0;
    for (const auto& r : rows) {
        colCount = (std::max)(colCount, r.row->children.size());
    }
    if (colCount == 0) return;

    // 1) Measure every cell unconstrained: natural (max) width and longest-word (min) width.
    struct CellInfo {
        RichText rich;
        const Markdown::Block* block = nullptr;
    };
    std::vector<std::vector<CellInfo>> grid(rows.size(), std::vector<CellInfo>(colCount));
    std::vector<float> colMin(colCount, kCellMinContentWidth);
    std::vector<float> colMax(colCount, kCellMinContentWidth);

    for (size_t r = 0; r < rows.size(); ++r) {
        const auto& cells = rows[r].row->children;
        for (size_t c = 0; c < colCount; ++c) {
            if (c >= cells.size() || !cells[c]) continue;
            CellInfo& info = grid[r][c];
            info.block = cells[c].get();
            const TextStyle& style = rows[r].isHeader ? m_styleTableHeader : m_styleTable;
            info.rich = BuildRichText(info.block->inlineContent, style, 1.0e5f);
            if (!info.rich.layout) continue;

            DWRITE_TEXT_METRICS tm{};
            info.rich.layout->GetMetrics(&tm);
            float minWidth = 0.0f;
            info.rich.layout->DetermineMinWidth(&minWidth);
            colMax[c] = (std::max)(colMax[c], std::ceil(tm.widthIncludingTrailingWhitespace) + 1.0f);
            colMin[c] = (std::max)(colMin[c], (std::min)(kCellMaxMinWidth, std::ceil(minWidth) + 1.0f));
        }
    }
    for (size_t c = 0; c < colCount; ++c) {
        colMax[c] = (std::max)(colMax[c], colMin[c]);
    }

    // 2) Distribute the available width (auto table layout, like browsers do).
    float sumMin = 0.0f;
    float sumMax = 0.0f;
    for (size_t c = 0; c < colCount; ++c) {
        sumMin += colMin[c];
        sumMax += colMax[c];
    }
    auto availableFor = [&](float padX) {
        const float chrome = static_cast<float>(colCount) * 2.0f * padX + static_cast<float>(colCount + 1);
        return (std::max)(static_cast<float>(colCount) * kCellMinContentWidth, ctx.width - chrome);
    };
    // Cramped tables trade cell padding for text room before breaking words.
    const float padX = (sumMin > availableFor(kCellPaddingX)) ? kCellPaddingX * 0.5f : kCellPaddingX;
    const float available = availableFor(padX);

    // Too narrow even for the longest words: "water-fill" a common cap so columns with short
    // words keep them whole and only the long tokens (URLs, e-mails) get broken.
    float cramCap = available;
    if (sumMin > available) {
        std::vector<float> sortedMin(colMin);
        std::sort(sortedMin.begin(), sortedMin.end());
        float remaining = available;
        for (size_t i = 0; i < sortedMin.size(); ++i) {
            const float share = remaining / static_cast<float>(sortedMin.size() - i);
            if (sortedMin[i] > share) {
                cramCap = (std::max)(kCellMinContentWidth, share);
                break;
            }
            remaining -= sortedMin[i];
        }
    }

    std::vector<float> widths(colCount);
    for (size_t c = 0; c < colCount; ++c) {
        if (sumMax <= available) {
            widths[c] = colMax[c];
        } else if (sumMin <= available) {
            const float t = (sumMax > sumMin) ? (available - sumMin) / (sumMax - sumMin) : 0.0f;
            widths[c] = colMin[c] + (colMax[c] - colMin[c]) * t;
        } else {
            widths[c] = (std::min)(colMin[c], cramCap);
        }
        widths[c] = std::floor(widths[c]);
    }

    // 3) Wrap every cell to its column, compute row heights and final geometry.
    LayoutBlock lb{};
    lb.type = Markdown::BlockType::Table;
    lb.quoteDepth = ctx.quoteDepth;
    lb.startLine = block.startLine;
    lb.endLine = block.endLine;
    lb.tableColWidths.resize(colCount);
    for (size_t c = 0; c < colCount; ++c) {
        lb.tableColWidths[c] = widths[c] + 2.0f * padX;
    }

    float rowY = currentY;
    int bodyRowIndex = 0;
    for (size_t r = 0; r < rows.size(); ++r) {
        const bool isHeader = rows[r].isHeader;
        if (isHeader) {
            lb.tableHeaderRows++;
        }
        const TextStyle& style = isHeader ? m_styleTableHeader : m_styleTable;

        float rowContentHeight = style.lineHeight;
        for (size_t c = 0; c < colCount; ++c) {
            CellInfo& info = grid[r][c];
            if (!info.rich.layout) continue;
            info.rich.layout->SetMaxWidth(widths[c]);
            DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
            if (info.block) {
                switch (info.block->align) {
                case Markdown::Alignment::Center: alignment = DWRITE_TEXT_ALIGNMENT_CENTER; break;
                case Markdown::Alignment::Right:  alignment = DWRITE_TEXT_ALIGNMENT_TRAILING; break;
                default: break;
                }
            }
            info.rich.layout->SetTextAlignment(alignment);
            DWRITE_TEXT_METRICS tm{};
            info.rich.layout->GetMetrics(&tm);
            rowContentHeight = (std::max)(rowContentHeight, tm.height);
        }
        const float rowHeight = std::ceil(rowContentHeight + 2.0f * kCellPaddingY);

        std::vector<TableCellLayout> rowCells(colCount);
        float cellX = ctx.x;
        for (size_t c = 0; c < colCount; ++c) {
            TableCellLayout& cell = rowCells[c];
            cell.isHeader = isHeader;
            cell.rowIndex = isHeader ? -1 : bodyRowIndex;
            cell.rect = D2D1::RectF(cellX, rowY, cellX + lb.tableColWidths[c] + 1.0f, rowY + rowHeight);
            cell.textOrigin = D2D1::Point2F(cellX + 1.0f + padX, rowY + kCellPaddingY);

            CellInfo& info = grid[r][c];
            if (info.block) {
                cell.align = info.block->align;
            }
            if (info.rich.layout) {
                CollectDecorations(info.rich, cell.textOrigin, lb.links, lb.inlineCodePills);
                cell.textLayout = std::move(info.rich.layout);
                cell.effects = std::move(info.rich.effects);
            }
            cellX += lb.tableColWidths[c] + 1.0f;
        }
        if (!isHeader) {
            bodyRowIndex++;
        }

        lb.tableRows.push_back(std::move(rowCells));
        rowY += rowHeight;
    }

    float tableRight = ctx.x + 1.0f;
    for (float w : lb.tableColWidths) {
        tableRight += w + 1.0f;
    }
    lb.bounds = D2D1::RectF(ctx.x, currentY, tableRight, rowY);
    m_blocks.push_back(std::move(lb));

    currentY = rowY;
}

const LinkHitBox* LayoutEngine::HitTestLinkBox(float x, float y) const {
    for (const auto& b : m_blocks) {
        if (b.links.empty() || y < b.bounds.top || y > b.bounds.bottom) {
            continue;
        }
        for (const auto& link : b.links) {
            if (x >= link.rect.left && x <= link.rect.right &&
                y >= link.rect.top && y <= link.rect.bottom) {
                return &link;
            }
        }
    }
    return nullptr;
}

std::string LayoutEngine::HitTestLink(float x, float y) const {
    const LinkHitBox* box = HitTestLinkBox(x, y);
    return box ? box->url : std::string();
}

float LayoutEngine::GetAnchorY(std::string_view slug) const {
    if (!slug.empty() && slug.front() == '#') {
        slug.remove_prefix(1);
    }
    const std::string decoded = Markdown::PercentDecode(slug);
    auto it = m_anchors.find(decoded);
    if (it == m_anchors.end()) {
        // Tolerate hand-written anchors such as "#Introducción" or "#My Title".
        it = m_anchors.find(Slugify(decoded));
    }
    return it != m_anchors.end() ? it->second : -1.0f;
}

float LayoutEngine::GetScrollYForLine(int docLine) const {
    if (m_syncBlocks.empty() || docLine <= 0) {
        return 0.0f;
    }
    const LayoutBlock& first = m_blocks[m_syncBlocks.front()];
    if (docLine <= first.startLine) {
        return 0.0f;
    }

    // Last block starting at or before docLine (blocks are in document order).
    auto it = std::upper_bound(m_syncBlocks.begin(), m_syncBlocks.end(), docLine,
                               [this](int line, size_t idx) { return line < m_blocks[idx].startLine; });
    const size_t pos = static_cast<size_t>(std::distance(m_syncBlocks.begin(), it)) - 1;
    const LayoutBlock& b = m_blocks[m_syncBlocks[pos]];

    if (docLine <= b.endLine) {
        const float lineSpan = static_cast<float>((std::max)(1, b.endLine - b.startLine + 1));
        const float t = static_cast<float>(docLine - b.startLine) / lineSpan;
        return b.bounds.top + t * (b.bounds.bottom - b.bounds.top);
    }
    if (pos + 1 < m_syncBlocks.size()) {
        const LayoutBlock& next = m_blocks[m_syncBlocks[pos + 1]];
        const float lineGap = static_cast<float>((std::max)(1, next.startLine - b.endLine));
        const float t = static_cast<float>(docLine - b.endLine) / lineGap;
        return b.bounds.bottom + t * (std::max)(0.0f, next.bounds.top - b.bounds.bottom);
    }
    return b.bounds.bottom;
}

int LayoutEngine::GetLineForScrollY(float scrollY) const {
    if (m_syncBlocks.empty() || scrollY <= 0.0f) {
        return 1;
    }
    const LayoutBlock& first = m_blocks[m_syncBlocks.front()];
    if (scrollY <= first.bounds.top) {
        return first.startLine;
    }

    // Last block whose top is at or above scrollY.
    auto it = std::upper_bound(m_syncBlocks.begin(), m_syncBlocks.end(), scrollY,
                               [this](float y, size_t idx) { return y < m_blocks[idx].bounds.top; });
    const size_t pos = static_cast<size_t>(std::distance(m_syncBlocks.begin(), it)) - 1;
    const LayoutBlock& b = m_blocks[m_syncBlocks[pos]];

    if (scrollY <= b.bounds.bottom) {
        const float h = (std::max)(1.0f, b.bounds.bottom - b.bounds.top);
        const float t = (scrollY - b.bounds.top) / h;
        return (std::min)(b.endLine, b.startLine + static_cast<int>(t * static_cast<float>(b.endLine - b.startLine + 1)));
    }
    if (pos + 1 < m_syncBlocks.size()) {
        const LayoutBlock& next = m_blocks[m_syncBlocks[pos + 1]];
        const float gap = (std::max)(1.0f, next.bounds.top - b.bounds.bottom);
        const float t = (scrollY - b.bounds.bottom) / gap;
        return b.endLine + static_cast<int>(t * static_cast<float>(next.startLine - b.endLine));
    }
    return b.endLine;
}

} // namespace Pluma::Preview
