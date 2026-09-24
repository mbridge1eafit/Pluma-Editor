#include <gtest/gtest.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <cmath>
#include <cstdlib>

#include "preview/preview_layout.h"
#include "preview/preview_renderer.h"
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

// ==============================================================================
// Unicode anchors, nested lists and overflow-free tables
// ==============================================================================

#include "markdown/emoji.h"

TEST(PreviewLayoutTest, SlugifyUnicodeLikeGitHub) {
    EXPECT_EQ(Slugify("Introducción"), "introducción");
    EXPECT_EQ(Slugify("ÁRBOL Ñandú"), "árbol-ñandú");
    EXPECT_EQ(Slugify("Hola 🚀 mundo ✅"), "hola-mundo");
    EXPECT_EQ(Slugify("¿Qué es?"), "qué-es");
}

TEST(PreviewLayoutTest, EmojiShortcodeTable) {
    EXPECT_EQ(Pluma::Markdown::LookupEmojiShortcode("rocket"), "🚀");
    EXPECT_EQ(Pluma::Markdown::LookupEmojiShortcode("+1"), "👍");
    EXPECT_TRUE(Pluma::Markdown::LookupEmojiShortcode("desconocido").empty());
    EXPECT_EQ(Pluma::Markdown::ReplaceEmojiShortcodes("a:b:smile: 10:30"), "a:b😄 10:30");
}

class PreviewLayoutFixture : public ::testing::Test {
protected:
    void SetUp() override {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(factory.GetAddressOf()));
        ASSERT_TRUE(engine.Initialize(factory.Get()));
    }

    ComPtr<IDWriteFactory> factory;
    LayoutEngine engine;
};

TEST_F(PreviewLayoutFixture, AccentedAnchorsResolveFromLinks) {
    auto tree = Md4cAdapter::Parse("# Introducción\n\n## Introducción\n");
    engine.ComputeLayout(*tree, 800.0f, 96, PreviewThemeColors::Light());
    const float y = engine.GetAnchorY("#introducci%C3%B3n"); // md4c percent-encodes hrefs
    EXPECT_GE(y, 0.0f);
    EXPECT_GT(engine.GetAnchorY("introducción-1"), y); // Duplicates get a suffix
    EXPECT_GE(engine.GetAnchorY("#Introducción"), 0.0f);
}

TEST_F(PreviewLayoutFixture, NestedListsAreRenderedAndIndented) {
    auto tree = Md4cAdapter::Parse("1. Uno\n2. Dos\n   - Anidado\n     - Profundo\n3. Tres\n");
    engine.ComputeLayout(*tree, 800.0f, 96, PreviewThemeColors::Light());

    std::vector<const LayoutBlock*> items;
    for (const auto& b : engine.GetBlocks()) {
        if (b.type == BlockType::ListItem) items.push_back(&b);
    }
    ASSERT_EQ(items.size(), 5u); // Previously nested items were dropped
    EXPECT_TRUE(items[0]->isOrdered);
    EXPECT_NE(items[0]->markerLayout, nullptr); // "1." is actually drawn
    EXPECT_EQ(items[2]->listDepth, 1);
    EXPECT_EQ(items[3]->listDepth, 2);
    EXPECT_GT(items[2]->textOrigin.x, items[1]->textOrigin.x);
    EXPECT_GT(items[3]->textOrigin.x, items[2]->textOrigin.x);
    EXPECT_GT(items[4]->bounds.top, items[3]->bounds.bottom); // "Tres" after the nested items
}

TEST_F(PreviewLayoutFixture, TablesNeverOverflowTheColumn) {
    std::string md =
        "| A | Descripción larga | URL |\n|---|:---:|--:|\n"
        "| 1 | Un texto bastante largo que debe ajustarse dentro de la celda sin salirse | "
        "https://example.com/una/url/muy/larga/sin/espacios/que/no/cabe/en/ninguna/parte |\n";
    auto tree = Md4cAdapter::Parse(md);

    for (float viewWidth : {900.0f, 480.0f, 300.0f}) {
        engine.ComputeLayout(*tree, viewWidth, 96, PreviewThemeColors::Light());
        const LayoutBlock* table = nullptr;
        for (const auto& b : engine.GetBlocks()) {
            if (b.type == BlockType::Table) table = &b;
        }
        ASSERT_NE(table, nullptr);
        EXPECT_LE(table->bounds.right, viewWidth) << "view " << viewWidth;

        for (const auto& row : table->tableRows) {
            for (const auto& cell : row) {
                ASSERT_NE(cell.textLayout, nullptr);
                DWRITE_TEXT_METRICS tm{};
                cell.textLayout->GetMetrics(&tm);
                // Text (including wrapped long words) stays inside its cell.
                EXPECT_LE(cell.textOrigin.x + tm.left + tm.width, cell.rect.right + 0.5f) << "view " << viewWidth;
                EXPECT_LE(cell.textOrigin.y + tm.height, cell.rect.bottom + 0.5f);
            }
        }
    }
}

TEST_F(PreviewLayoutFixture, LongWordsWrapInsideParagraphs) {
    auto tree = Md4cAdapter::Parse("https://example.com/" + std::string(300, 'a') + "\n");
    engine.ComputeLayout(*tree, 400.0f, 96, PreviewThemeColors::Light());
    const auto& p = engine.GetBlocks().at(0);
    DWRITE_TEXT_METRICS tm{};
    p.textLayout->GetMetrics(&tm);
    EXPECT_LE(tm.width, p.bounds.right - p.bounds.left + 0.5f);
    EXPECT_GT(tm.lineCount, 1u);
}

TEST_F(PreviewLayoutFixture, LinksAndCodeGetColorEffects) {
    auto tree = Md4cAdapter::Parse("Ver [docs](https://x.com) y `code`.\n");
    engine.ComputeLayout(*tree, 800.0f, 96, PreviewThemeColors::Light());
    const auto& effects = engine.GetBlocks().at(0).effects;
    ASSERT_EQ(effects.size(), 2u);
    EXPECT_EQ(effects[0].effect, TextEffect::Link);
    EXPECT_EQ(effects[1].effect, TextEffect::Code);
}

namespace {

const char* kMermaidDoc =
    "Texto\n\n"
    "```mermaid\n"
    "graph LR\n"
    "  A[Inicio] --> B{Decidir} -->|Si| C[Una etiqueta bastante larga para forzar el ancho]\n"
    "  C --> D[Fin] --> E[Otro nodo] --> F[Y otro mas]\n"
    "```\n\n"
    "Despues\n";

const LayoutBlock* FindDiagram(const LayoutEngine& engine) {
    for (const auto& b : engine.GetBlocks()) {
        if (b.diagram) return &b;
    }
    return nullptr;
}

} // namespace

TEST_F(PreviewLayoutFixture, MermaidBlocksRenderAsDiagrams) {
    auto tree = Md4cAdapter::Parse(kMermaidDoc);
    engine.ComputeLayout(*tree, 1200.0f, 96, PreviewThemeColors::Light());
    const LayoutBlock* block = FindDiagram(engine);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(block->type, BlockType::CodeBlock);
    EXPECT_EQ(block->startLine, 3);
    ASSERT_TRUE(block->diagram->scene);
    const auto& scene = *block->diagram->scene;
    ASSERT_EQ(block->diagram->texts.size(), scene.items.size());
    size_t labels = 0;
    for (size_t i = 0; i < scene.items.size(); ++i) {
        if (scene.items[i].kind == Pluma::Diagram::PrimitiveKind::Text) {
            EXPECT_NE(block->diagram->texts[i].layout, nullptr);
            ++labels;
        }
    }
    EXPECT_GE(labels, 7u);

    // The layout is reused while the source does not change.
    const auto* visual = block->diagram.get();
    engine.ComputeLayout(*tree, 1000.0f, 96, PreviewThemeColors::Dark());
    ASSERT_NE(FindDiagram(engine), nullptr);
    EXPECT_EQ(FindDiagram(engine)->diagram.get(), visual);

    // Narrow viewports scale the diagram down to the column.
    engine.ComputeLayout(*tree, 360.0f, 96, PreviewThemeColors::Light());
    block = FindDiagram(engine);
    ASSERT_NE(block, nullptr);
    EXPECT_LT(block->diagramScale, 1.0f);
    EXPECT_GE(block->diagramOrigin.x, block->bounds.left - 0.5f);
    EXPECT_LE(block->diagramOrigin.x + scene.width * block->diagramScale, block->bounds.right + 0.5f);
    EXPECT_NEAR(block->bounds.bottom - block->bounds.top, std::ceil(scene.height * block->diagramScale), 0.01f);
}

TEST_F(PreviewLayoutFixture, InvalidMermaidFallsBackToCodeWithReason) {
    auto tree = Md4cAdapter::Parse("```mermaid\ngraph TD\n  A[sin cerrar --> B\n```\n");
    engine.ComputeLayout(*tree, 800.0f, 96, PreviewThemeColors::Light());
    ASSERT_EQ(engine.GetBlocks().size(), 1u);
    const LayoutBlock& block = engine.GetBlocks()[0];
    EXPECT_EQ(block.diagram, nullptr);
    EXPECT_NE(block.textLayout, nullptr);
    ASSERT_NE(block.labelLayout, nullptr);
    DWRITE_TEXT_METRICS tm{};
    block.labelLayout->GetMetrics(&tm);
    EXPECT_GT(tm.width, 100.0f); // "mermaid — Falta el cierre..."
}

TEST_F(PreviewLayoutFixture, MermaidDiagramIsPaintedOffscreen) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); // S_FALSE when already initialised
    ComPtr<IWICImagingFactory> wic;
    ASSERT_TRUE(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&wic))));
    ComPtr<ID2D1Factory> d2d;
    ASSERT_TRUE(SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d.GetAddressOf())));

    auto tree = Md4cAdapter::Parse(kMermaidDoc);
    for (const bool dark : {false, true}) {
        const PreviewThemeColors colors = dark ? PreviewThemeColors::Dark() : PreviewThemeColors::Light();
        engine.ComputeLayout(*tree, 900.0f, 96, colors);
        const LayoutBlock* block = FindDiagram(engine);
        ASSERT_NE(block, nullptr);

        const UINT width = 900;
        const UINT height = static_cast<UINT>(std::ceil(engine.GetTotalHeight()));
        ComPtr<IWICBitmap> bitmap;
        ASSERT_TRUE(SUCCEEDED(wic->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap)));
        ComPtr<ID2D1RenderTarget> rt;
        ASSERT_TRUE(SUCCEEDED(d2d->CreateWicBitmapRenderTarget(bitmap.Get(), D2D1::RenderTargetProperties(), &rt)));

        PreviewRenderer renderer;
        ASSERT_TRUE(renderer.CreateResources(rt.Get(), colors));
        rt->BeginDraw();
        rt->Clear(colors.background);
        renderer.Draw(rt.Get(), engine, 0.0f, static_cast<float>(height));
        ASSERT_TRUE(SUCCEEDED(rt->EndDraw()));

        // Count painted pixels inside the diagram area.
        ComPtr<IWICBitmapLock> lock;
        WICRect rect{0, 0, static_cast<INT>(width), static_cast<INT>(height)};
        ASSERT_TRUE(SUCCEEDED(bitmap->Lock(&rect, WICBitmapLockRead, &lock)));
        UINT stride = 0;
        UINT size = 0;
        BYTE* data = nullptr;
        lock->GetStride(&stride);
        lock->GetDataPointer(&size, &data);
        const auto bg = static_cast<BYTE>(colors.background.r * 255.0f + 0.5f);
        size_t painted = 0;
        for (UINT y = static_cast<UINT>(block->bounds.top); y < static_cast<UINT>(block->bounds.bottom) && y < height; ++y) {
            for (UINT x = 0; x < width; ++x) {
                const BYTE* px = data + y * stride + x * 4;
                if (std::abs(px[0] - bg) > 12 || std::abs(px[1] - bg) > 12 || std::abs(px[2] - bg) > 12) ++painted;
            }
        }
        EXPECT_GT(painted, 3000u) << (dark ? "dark" : "light");
    }
}
