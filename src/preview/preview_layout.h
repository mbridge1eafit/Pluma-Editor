#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <string_view>

#include "../markdown/block_tree.h"
#include "../markdown/slug.h"

namespace Pluma::Preview {

using Microsoft::WRL::ComPtr;
using Markdown::Slugify;

struct PreviewThemeColors {
    D2D1_COLOR_F background;
    D2D1_COLOR_F text;
    D2D1_COLOR_F mutedText;
    D2D1_COLOR_F headingText;
    D2D1_COLOR_F linkText;
    D2D1_COLOR_F codeText;
    D2D1_COLOR_F codeBackground;
    D2D1_COLOR_F codeBorder;
    D2D1_COLOR_F inlineCodeBg;
    D2D1_COLOR_F inlineCodeText;
    D2D1_COLOR_F blockquoteBorder;
    D2D1_COLOR_F blockquoteText;
    D2D1_COLOR_F ruleLine;
    D2D1_COLOR_F tableBorder;
    D2D1_COLOR_F tableHeaderBg;
    D2D1_COLOR_F tableStripeBg;
    D2D1_COLOR_F accent;      // Checked task boxes
    D2D1_COLOR_F accentText;  // Check mark drawn on top of the accent

    static PreviewThemeColors Light();
    static PreviewThemeColors Dark();
};

// Text ranges that receive a coloured drawing effect at render time.
enum class TextEffect {
    Link,
    Code
};

struct EffectRange {
    UINT32 start = 0;
    UINT32 length = 0;
    TextEffect effect = TextEffect::Link;
};

struct LinkHitBox {
    D2D1_RECT_F rect{};
    std::string url;
    IDWriteTextLayout* layout = nullptr; // Owned by the LayoutBlock/TableCellLayout (hover underline)
    UINT32 textStart = 0;
    UINT32 textLength = 0;
};

struct TableCellLayout {
    D2D1_RECT_F rect{};
    D2D1_POINT_2F textOrigin{};
    ComPtr<IDWriteTextLayout> textLayout;
    std::vector<EffectRange> effects;
    bool isHeader = false;
    int rowIndex = 0;
    Markdown::Alignment align = Markdown::Alignment::Default;
};

struct LayoutBlock {
    Markdown::BlockType type = Markdown::BlockType::Paragraph;
    D2D1_RECT_F bounds{};
    D2D1_POINT_2F textOrigin{};
    ComPtr<IDWriteTextLayout> textLayout;
    std::vector<EffectRange> effects;
    int level = 0;              // Heading level (1-6)
    bool isTask = false;
    bool isTaskChecked = false;
    bool isOrdered = false;
    int itemNumber = 1;
    int listDepth = 0;          // Nesting depth of the list (bullet style)
    int quoteDepth = 0;         // > 0 when rendered inside a blockquote
    bool isQuoteBar = false;    // Vertical bar decoration of a blockquote
    D2D1_RECT_F markerRect{};   // Bullet / checkbox area (list items)
    ComPtr<IDWriteTextLayout> markerLayout; // "1." for ordered items
    D2D1_POINT_2F markerOrigin{};
    ComPtr<IDWriteTextLayout> labelLayout;  // Language label of code blocks
    D2D1_POINT_2F labelOrigin{};
    std::string info;           // CodeBlock language
    std::string codeText;       // Verbatim code content
    int startLine = 0;          // Source document line range (F-14)
    int endLine = 0;
    std::string anchorSlug;
    std::vector<LinkHitBox> links;
    std::vector<D2D1_RECT_F> inlineCodePills;

    // Table elements
    std::vector<std::vector<TableCellLayout>> tableRows;
    std::vector<float> tableColWidths;
    int tableHeaderRows = 0;
};

class LayoutEngine {
public:
    LayoutEngine();
    ~LayoutEngine() = default;

    bool Initialize(IDWriteFactory* dwriteFactory);

    // Lays out the tree for a viewport of `contentWidth` DIPs. All coordinates are in DIPs.
    void ComputeLayout(const Markdown::BlockTree& tree,
                       float contentWidth,
                       UINT dpi,
                       const PreviewThemeColors& colors);

    float GetTotalHeight() const noexcept { return m_totalHeight; }
    float GetLayoutWidth() const noexcept { return m_layoutWidth; }
    const std::vector<LayoutBlock>& GetBlocks() const noexcept { return m_blocks; }
    const std::map<std::string, float>& GetAnchors() const noexcept { return m_anchors; }

    // Hit-testing: returns URL if point (x, y) in content space hits a link
    std::string HitTestLink(float x, float y) const;
    const LinkHitBox* HitTestLinkBox(float x, float y) const;

    // Returns Y position for an anchor slug, or negative value if not found (F-10)
    float GetAnchorY(std::string_view slug) const;

    // Line-to-coordinate and coordinate-to-line mapping for synchronized scroll (F-14)
    float GetScrollYForLine(int docLine) const;
    int GetLineForScrollY(float scrollY) const;

private:
    struct TextStyle {
        ComPtr<IDWriteTextFormat> format;
        float fontSize = 14.0f;
        float lineHeight = 20.0f;
        float baseline = 16.0f;
    };

    struct Context {
        float x = 0.0f;
        float width = 0.0f;
        int quoteDepth = 0;
        int listDepth = 0;
    };

    struct SpanRange;
    struct RichText;

    void LayoutChildren(const Markdown::Block& parent, const Context& ctx, float& currentY);
    void LayoutNode(const Markdown::Block& block, const Context& ctx, float& currentY, float containerTop);
    void LayoutHeading(const Markdown::Block& block, const Context& ctx, float& currentY);
    void LayoutParagraph(const Markdown::Block& block, const Context& ctx, float& currentY);
    void LayoutBlockquote(const Markdown::Block& block, const Context& ctx, float& currentY);
    void LayoutList(const Markdown::Block& block, const Context& ctx, float& currentY);
    void LayoutListItem(const Markdown::Block& block, const Context& ctx, float markerWidth,
                        bool isOrdered, int number, float& currentY);
    void LayoutCodeBlock(const Markdown::Block& block, const Context& ctx, float& currentY);
    void LayoutThematicBreak(const Markdown::Block& block, const Context& ctx, float& currentY);
    void LayoutTable(const Markdown::Block& block, const Context& ctx, float& currentY);

    RichText BuildRichText(const std::vector<Markdown::Span>& spans, const TextStyle& style, float maxWidth);
    void CollectDecorations(const RichText& rich, D2D1_POINT_2F origin,
                            std::vector<LinkHitBox>& outLinks,
                            std::vector<D2D1_RECT_F>& outCodePills) const;
    ComPtr<IDWriteTextLayout> CreatePlainLayout(std::wstring_view text, const TextStyle& style, float maxWidth) const;

    bool CreateStyle(const wchar_t* family, float size, DWRITE_FONT_WEIGHT weight,
                     float lineSpacingFactor, TextStyle& out);

    ComPtr<IDWriteFactory> m_dwriteFactory;
    std::wstring m_codeFamily = L"Consolas";
    TextStyle m_styleH[6];
    TextStyle m_styleBody;
    TextStyle m_styleCode;
    TextStyle m_styleTable;
    TextStyle m_styleTableHeader;
    TextStyle m_styleLabel;
    float m_codeCharWidth = 7.0f;

    UINT m_dpi = 96;
    float m_totalHeight = 0.0f;
    float m_layoutWidth = 0.0f;
    std::vector<LayoutBlock> m_blocks;
    std::vector<size_t> m_syncBlocks; // Indices of blocks used for line <-> Y mapping (document order)
    std::map<std::string, float> m_anchors;
    std::map<std::string, int> m_slugCounts;
};

} // namespace Pluma::Preview
