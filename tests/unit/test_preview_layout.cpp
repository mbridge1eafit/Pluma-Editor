#include <gtest/gtest.h>
#include <dwrite.h>
#include <wrl/client.h>

#include "preview/preview_layout.h"
#include "markdown/md4c_adapter.h"

using namespace Pluma::Preview;
using namespace Pluma::Markdown;
using Microsoft::WRL::ComPtr;

TEST(PreviewLayoutTest, Slugify) {
    EXPECT_EQ(Slugify("Resumen y contexto"), "resumen-y-contexto");
    EXPECT_EQ(Slugify("Section 1: Hello World!"), "section-1-hello-world");
    EXPECT_EQ(Slugify("   Leading & Trailing Spaces   "), "leading-trailing-spaces");
    EXPECT_EQ(Slugify("Multiple---Hyphens___Underlines"), "multiple-hyphens-underlines");
}

TEST(PreviewLayoutTest, LayoutEngineInitialization) {
    ComPtr<IDWriteFactory> dwriteFactory;
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf())
    );
    ASSERT_TRUE(SUCCEEDED(hr));
    ASSERT_NE(dwriteFactory, nullptr);

    LayoutEngine engine;
    EXPECT_TRUE(engine.Initialize(dwriteFactory.Get()));
}

TEST(PreviewLayoutTest, ComputeLayoutHeadingsAndAnchors) {
    ComPtr<IDWriteFactory> dwriteFactory;
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf()));

    LayoutEngine engine;
    ASSERT_TRUE(engine.Initialize(dwriteFactory.Get()));

    std::string md = "# Main Header\n\nParagraph text.\n\n## Sub Section\n";
    auto tree = Md4cAdapter::Parse(md);
    ASSERT_NE(tree, nullptr);

    engine.ComputeLayout(*tree, 800.0f, 96, PreviewThemeColors::Light());

    EXPECT_GT(engine.GetTotalHeight(), 50.0f);
    const auto& blocks = engine.GetBlocks();
    ASSERT_GE(blocks.size(), 3u);

    EXPECT_EQ(blocks[0].type, BlockType::Heading);
    EXPECT_EQ(blocks[0].level, 1);
    EXPECT_EQ(blocks[0].anchorSlug, "main-header");

    EXPECT_EQ(blocks[1].type, BlockType::Paragraph);

    EXPECT_EQ(blocks[2].type, BlockType::Heading);
    EXPECT_EQ(blocks[2].level, 2);
    EXPECT_EQ(blocks[2].anchorSlug, "sub-section");

    // Anchor lookup (F-10)
    EXPECT_GE(engine.GetAnchorY("main-header"), 0.0f);
    EXPECT_GE(engine.GetAnchorY("sub-section"), engine.GetAnchorY("main-header"));
    EXPECT_LT(engine.GetAnchorY("non-existent"), 0.0f);
}

TEST(PreviewLayoutTest, HitTestLink) {
    ComPtr<IDWriteFactory> dwriteFactory;
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf()));

    LayoutEngine engine;
    ASSERT_TRUE(engine.Initialize(dwriteFactory.Get()));

    std::string md = "Click [here](https://pluma.example.com) to visit.\n";
    auto tree = Md4cAdapter::Parse(md);
    ASSERT_NE(tree, nullptr);

    engine.ComputeLayout(*tree, 600.0f, 96, PreviewThemeColors::Light());

    const auto& blocks = engine.GetBlocks();
    ASSERT_EQ(blocks.size(), 1u);
    ASSERT_FALSE(blocks[0].links.empty());

    const auto& link = blocks[0].links[0];
    EXPECT_EQ(link.url, "https://pluma.example.com");

    // Test inside link rect
    float midX = (link.rect.left + link.rect.right) / 2.0f;
    float midY = (link.rect.top + link.rect.bottom) / 2.0f;
    std::string hit = engine.HitTestLink(midX, midY);
    EXPECT_EQ(hit, "https://pluma.example.com");

    // Test outside
    EXPECT_TRUE(engine.HitTestLink(0.0f, 0.0f).empty());
}

TEST(PreviewLayoutTest, TablesAndCodeBlocks) {
    ComPtr<IDWriteFactory> dwriteFactory;
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf()));

    LayoutEngine engine;
    ASSERT_TRUE(engine.Initialize(dwriteFactory.Get()));

    std::string md = "```python\nprint('hello')\n```\n\n| A | B |\n|---|---|\n| 1 | 2 |\n";
    auto tree = Md4cAdapter::Parse(md);
    ASSERT_NE(tree, nullptr);

    engine.ComputeLayout(*tree, 600.0f, 96, PreviewThemeColors::Dark());

    const auto& blocks = engine.GetBlocks();
    ASSERT_GE(blocks.size(), 2u);

    // Code block
    EXPECT_EQ(blocks[0].type, BlockType::CodeBlock);
    EXPECT_EQ(blocks[0].info, "python");
    EXPECT_NE(blocks[0].codeText.find("print('hello')"), std::string::npos);

    // Table block
    EXPECT_EQ(blocks[1].type, BlockType::Table);
    EXPECT_EQ(blocks[1].tableColWidths.size(), 2u);
    ASSERT_GE(blocks[1].tableRows.size(), 2u);
    EXPECT_TRUE(blocks[1].tableRows[0][0].isHeader);
    EXPECT_FALSE(blocks[1].tableRows[1][0].isHeader);
}
