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

namespace Pluma::Preview {

using Microsoft::WRL::ComPtr;

struct PreviewThemeColors {
    D2D1_COLOR_F background;
    D2D1_COLOR_F text;
    D2D1_COLOR_F headingText;
    D2D1_COLOR_F linkText;
    D2D1_COLOR_F codeText;
    D2D1_COLOR_F codeBackground;
    D2D1_COLOR_F codeBorder;
    D2D1_COLOR_F inlineCodeBg;
    D2D1_COLOR_F blockquoteBorder;
    D2D1_COLOR_F blockquoteText;
    D2D1_COLOR_F ruleLine;
    D2D1_COLOR_F tableBorder;
    D2D1_COLOR_F tableHeaderBg;

    static PreviewThemeColors Light();
    static PreviewThemeColors Dark();
};

struct LinkHitBox {
    D2D1_RECT_F rect{};
    std::string url;
};

struct TableCellLayout {
    D2D1_RECT_F rect{};
    ComPtr<IDWriteTextLayout> textLayout;
    bool isHeader = false;
    Markdown::Alignment align = Markdown::Alignment::Default;
};

struct LayoutBlock {
    Markdown::BlockType type = Markdown::BlockType::Paragraph;
    D2D1_RECT_F bounds{};
    ComPtr<IDWriteTextLayout> textLayout;
    int level = 0;              // Heading level (1-6)
    bool isTask = false;
    bool isTaskChecked = false;
    bool isOrdered = false;
    int itemNumber = 1;
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
};

class LayoutEngine {
public:
    LayoutEngine();
    ~LayoutEngine() = default;

    bool Initialize(IDWriteFactory* dwriteFactory);

    void ComputeLayout(const Markdown::BlockTree& tree,
                       float contentWidth,
                       UINT dpi,
                       const PreviewThemeColors& colors);

    float GetTotalHeight() const noexcept { return m_totalHeight; }
    const std::vector<LayoutBlock>& GetBlocks() const noexcept { return m_blocks; }
    const std::map<std::string, float>& GetAnchors() const noexcept { return m_anchors; }

    // Hit-testing: returns URL if point (x, y) in content space hits a link
    std::string HitTestLink(float x, float y) const;

    // Returns Y position for an anchor slug, or negative value if not found (F-10)
    float GetAnchorY(std::string_view slug) const;

    // Line-to-coordinate and coordinate-to-line mapping for synchronized scroll (F-14)
    float GetScrollYForLine(int docLine) const;
    int GetLineForScrollY(float scrollY) const;

private:
    void LayoutDocumentBlock(const Markdown::Block& block, float contentWidth, float& currentY);
    void LayoutHeading(const Markdown::Block& block, float contentWidth, float& currentY);
    void LayoutParagraph(const Markdown::Block& block, float contentWidth, float& currentY);
    void LayoutBlockquote(const Markdown::Block& block, float contentWidth, float& currentY);
    void LayoutList(const Markdown::Block& block, float contentWidth, float& currentY);
    void LayoutListItem(const Markdown::Block& block, float contentWidth, float& currentY, bool isOrdered, int number);
    void LayoutCodeBlock(const Markdown::Block& block, float contentWidth, float& currentY);
    void LayoutThematicBreak(const Markdown::Block& block, float contentWidth, float& currentY);
    void LayoutTable(const Markdown::Block& block, float contentWidth, float& currentY);

    ComPtr<IDWriteTextLayout> BuildTextLayout(
        const std::vector<Markdown::Span>& spans,
        IDWriteTextFormat* baseFormat,
        float maxTextWidth,
        std::vector<LinkHitBox>& outLinks,
        std::vector<D2D1_RECT_F>& outCodePills,
        float originX, float originY);

    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<IDWriteTextFormat> m_formatH1;
    ComPtr<IDWriteTextFormat> m_formatH2;
    ComPtr<IDWriteTextFormat> m_formatH3;
    ComPtr<IDWriteTextFormat> m_formatH4;
    ComPtr<IDWriteTextFormat> m_formatH5;
    ComPtr<IDWriteTextFormat> m_formatH6;
    ComPtr<IDWriteTextFormat> m_formatBody;
    ComPtr<IDWriteTextFormat> m_formatCode;
    ComPtr<IDWriteTextFormat> m_formatTableHeader;

    UINT m_dpi = 96;
    float m_totalHeight = 0.0f;
    std::vector<LayoutBlock> m_blocks;
    std::map<std::string, float> m_anchors;
};

std::string Slugify(std::string_view text);

} // namespace Pluma::Preview
