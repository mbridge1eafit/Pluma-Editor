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

// ==============================================================================
// Preview -> source hit-testing (Ctrl+click to edit, task checkboxes)
// ==============================================================================

namespace {

class SourceHitTest : public ::testing::Test {
protected:
    void SetUp() override {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(m_factory.GetAddressOf()));
        ASSERT_TRUE(m_engine.Initialize(m_factory.Get()));
    }

    void Layout(const std::string& md) {
        m_md = md;
        m_tree = Md4cAdapter::Parse(m_md);
        m_engine.ComputeLayout(*m_tree, 800.0f, 96, PreviewThemeColors::Light());
    }

    const LayoutBlock& Block(BlockType type, size_t nth = 0) const {
        for (const auto& b : m_engine.GetBlocks()) {
            if (b.type == type && !b.isQuoteBar && nth-- == 0) return b;
        }
        ADD_FAILURE() << "block not found";
        return m_engine.GetBlocks().front();
    }

    // Content-space point on the left quarter of the character at `pos` of a text layout.
    static D2D1_POINT_2F PointOf(IDWriteTextLayout* layout, D2D1_POINT_2F origin, UINT32 pos) {
        float x = 0.0f;
        float y = 0.0f;
        DWRITE_HIT_TEST_METRICS m{};
        layout->HitTestTextPosition(pos, FALSE, &x, &y, &m);
        return D2D1::Point2F(origin.x + x + m.width * 0.25f, origin.y + y + m.height * 0.5f);
    }

    // Byte offset in the source that a hit designates.
    size_t ByteOffset(const SourceHit& hit) const {
        size_t pos = hit.srcOffset;
        UINT32 units = 0;
        while (units < hit.utf16Delta && pos < m_md.size()) {
            const auto c = static_cast<unsigned char>(m_md[pos]);
            const size_t len = c < 0x80 ? 1 : (c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : 2));
            units += len == 4 ? 2 : 1;
            pos += len;
        }
        return pos;
    }

    SourceHit HitChar(const LayoutBlock& b, UINT32 pos) const {
        const D2D1_POINT_2F p = PointOf(b.textLayout.Get(), b.textOrigin, pos);
        return m_engine.HitTestSource(p.x, p.y);
    }

    ComPtr<IDWriteFactory> m_factory;
    LayoutEngine m_engine;
    std::string m_md;
    std::unique_ptr<BlockTree> m_tree;
};

} // namespace

TEST_F(SourceHitTest, ParagraphCharactersMapThroughMarkup) {
    Layout("Intro\n\nHola **mundo** y m\xC3\xA1s `c\xC3\xB3" "digo` texto\n");
    const LayoutBlock& p = Block(BlockType::Paragraph, 1);
    const std::wstring visible = L"Hola mundo y más có" L"digo texto";

    SourceHit hit = HitChar(p, static_cast<UINT32>(visible.find(L"mundo")));
    EXPECT_TRUE(hit.exact);
    EXPECT_EQ(ByteOffset(hit), m_md.find("mundo"));
    EXPECT_EQ(hit.line, 3);

    hit = HitChar(p, static_cast<UINT32>(visible.find(L"digo")));
    EXPECT_EQ(ByteOffset(hit), m_md.find("digo"));

    hit = HitChar(p, static_cast<UINT32>(visible.find(L"texto")));
    EXPECT_EQ(ByteOffset(hit), m_md.find("texto"));
}

TEST_F(SourceHitTest, GeneratedTextMapsToItsSourceStart) {
    Layout("a :rocket: b\n");
    const LayoutBlock& p = Block(BlockType::Paragraph);
    const SourceHit hit = HitChar(p, 2); // The emoji
    EXPECT_TRUE(hit.exact);
    EXPECT_EQ(ByteOffset(hit), m_md.find(':'));
}

TEST_F(SourceHitTest, HeadingListAndCodeBlock) {
    Layout("## T\xC3\xADtulo\n\n- uno\n- dos\n\n```\nabc\ndef\n```\n");
    SourceHit hit = HitChar(Block(BlockType::Heading), 2);
    EXPECT_EQ(ByteOffset(hit), m_md.find("tulo"));

    hit = HitChar(Block(BlockType::ListItem, 1), 2);
    EXPECT_EQ(ByteOffset(hit), m_md.find("dos") + 2);

    hit = HitChar(Block(BlockType::CodeBlock), 5); // Second character of "def"
    EXPECT_EQ(ByteOffset(hit), m_md.find("def") + 1);
}

TEST_F(SourceHitTest, TableCellsResolveRowLineAndText) {
    Layout("| A | B |\n|---|---|\n| uno | dos |\n| tres | cuatro |\n");
    const LayoutBlock& table = Block(BlockType::Table);
    ASSERT_EQ(table.tableRows.size(), 3u);
    const TableCellLayout& cell = table.tableRows[2][1];
    const D2D1_POINT_2F p = PointOf(cell.textLayout.Get(), cell.textOrigin, 2);
    const SourceHit hit = m_engine.HitTestSource(p.x, p.y);
    EXPECT_EQ(hit.line, 4);
    EXPECT_TRUE(hit.exact);
    EXPECT_EQ(ByteOffset(hit), m_md.find("cuatro") + 2);

    const TableCellLayout& header = table.tableRows[0][0];
    const SourceHit headerHit = m_engine.HitTestSource(header.rect.left + 2.0f, header.rect.top + 2.0f);
    EXPECT_EQ(headerHit.line, 1);
}

TEST_F(SourceHitTest, PointBelowContentUsesNearestBlock) {
    Layout("uno\n\ndos\n\n---\n");
    const SourceHit hit = m_engine.HitTestSource(10.0f, m_engine.GetTotalHeight() + 500.0f);
    EXPECT_EQ(hit.line, 5);
    EXPECT_FALSE(hit.exact);
}

TEST_F(SourceHitTest, TaskBoxesAreHitAndToggled) {
    Layout("- [ ] uno\n- [x] dos\n- normal\n");
    const LayoutBlock& first = Block(BlockType::ListItem, 0);
    const D2D1_RECT_F& box = first.markerRect;
    const LayoutBlock* hit = m_engine.HitTestTaskBox((box.left + box.right) * 0.5f, (box.top + box.bottom) * 0.5f);
    ASSERT_NE(hit, nullptr);
    EXPECT_EQ(hit->taskMarkOffset, 3);
    EXPECT_FALSE(hit->isTaskChecked);

    m_engine.SetTaskChecked(3, true);
    EXPECT_TRUE(first.isTaskChecked);

    // The item text and plain bullets are not checkboxes.
    const D2D1_POINT_2F text = PointOf(first.textLayout.Get(), first.textOrigin, 1);
    EXPECT_EQ(m_engine.HitTestTaskBox(text.x, text.y), nullptr);
    const LayoutBlock& bullet = Block(BlockType::ListItem, 2);
    EXPECT_EQ(m_engine.HitTestTaskBox((bullet.markerRect.left + bullet.markerRect.right) * 0.5f,
                                      (bullet.markerRect.top + bullet.markerRect.bottom) * 0.5f),
              nullptr);
}

// ==============================================================================
// Text selection (drag, double/triple-click, copy, "Editar aquí", "Copiar como Markdown")
// ==============================================================================

TEST(PreviewSelectionTest, WordBoundsFollowCharacterClasses) {
    const std::wstring text = L"Una canción, snake_case...  fin \U0001F600!";
    const auto at = [&](std::wstring_view part) { return static_cast<UINT32>(text.find(part)); };
    const auto span = [&](std::wstring_view part) {
        return std::make_pair(at(part), at(part) + static_cast<UINT32>(part.size()));
    };
    EXPECT_EQ(WordBounds(text, at(L"ón")), span(L"canción"));
    EXPECT_EQ(WordBounds(text, at(L"_case")), span(L"snake_case"));
    EXPECT_EQ(WordBounds(text, at(L"...") + 1), span(L"..."));
    EXPECT_EQ(WordBounds(text, at(L"  ")), span(L"  "));
    EXPECT_EQ(WordBounds(text, at(L"\U0001F600") + 1), span(L"\U0001F600"));
    // Past the end: the last character.
    EXPECT_EQ(WordBounds(text, 1000), std::make_pair(static_cast<UINT32>(text.size() - 1), static_cast<UINT32>(text.size())));
    EXPECT_EQ(WordBounds(L"", 0), std::make_pair(0u, 0u));
}

namespace {

class TextSelectionTest : public SourceHitTest {
protected:
    std::wstring Text(const TextRange& range) const { return m_engine.GetSelectedText(range.start, range.end); }

    size_t BlockIndex(BlockType type) const {
        const auto& blocks = m_engine.GetBlocks();
        for (size_t i = 0; i < blocks.size(); ++i) {
            if (blocks[i].type == type) return i;
        }
        ADD_FAILURE() << "block not found";
        return 0;
    }
};

} // namespace

TEST_F(TextSelectionTest, SelectedTextJoinsBlocksRowsAndCells) {
    Layout("Hola **mundo**  \nfin\n\n- uno\n- dos\n\n| A | B |\n|---|---|\n| x | y |\n\n```\nab\ncd\n```\n");
    // Paragraph, two list items, four cells and the code block.
    ASSERT_EQ(m_engine.GetTextUnits().size(), 8u);
    EXPECT_EQ(m_engine.GetSelectedText(TextPosition{}, m_engine.DocumentEnd()),
              L"Hola mundo\r\nfin\r\n\r\nuno\r\ndos\r\n\r\nA\tB\r\nx\ty\r\n\r\nab\r\ncd");
}

TEST_F(TextSelectionTest, DiagramsAndListMarkersAreNotSelectable) {
    Layout("1. uno\n2. dos\n\n```mermaid\ngraph TD\nA-->B\n```\n\nfin\n");
    ASSERT_EQ(m_engine.GetTextUnits().size(), 3u);
    EXPECT_EQ(m_engine.GetSelectedText(TextPosition{}, m_engine.DocumentEnd()), L"uno\r\ndos\r\n\r\nfin");
}

TEST_F(TextSelectionTest, PartialSelectionSkipsMarkupInBothDirections) {
    Layout("Hola **mundo** feliz\n");
    // Visible text "Hola mundo feliz": [6, 13) is "undo fe".
    const TextPosition a{0, 6};
    const TextPosition b{0, 13};
    EXPECT_EQ(m_engine.GetSelectedText(a, b), L"undo fe");
    EXPECT_EQ(m_engine.GetSelectedText(b, a), L"undo fe");

    SourceSelection sel = m_engine.GetSourceSelection(b, a);
    EXPECT_FALSE(sel.wholeLines);
    EXPECT_TRUE(sel.start.exact);
    EXPECT_EQ(ByteOffset(sel.start), m_md.find("undo"));
    EXPECT_EQ(ByteOffset(sel.end), m_md.find("feliz") + 2);

    // Exactly the bold word: its source without the asterisks around it.
    sel = m_engine.GetSourceSelection(TextPosition{0, 5}, TextPosition{0, 10});
    EXPECT_EQ(ByteOffset(sel.start), m_md.find("mundo"));
    EXPECT_EQ(ByteOffset(sel.end), m_md.find("mundo") + 5);
}

TEST_F(TextSelectionTest, SourceSelectionUsesWholeLinesAcrossBlocks) {
    Layout("# T\n\nuno\ndos\n\n| A |\n|---|\n| x |\n");
    ASSERT_EQ(m_engine.GetTextUnits().size(), 4u); // Heading, paragraph, two cells

    SourceSelection sel = m_engine.GetSourceSelection(TextPosition{1, 2}, TextPosition{3, 1});
    EXPECT_TRUE(sel.wholeLines);
    EXPECT_EQ(sel.startLine, 3);
    EXPECT_EQ(sel.endLine, 8); // The body row, after the delimiter row

    // A whole unit is copied with its markup too.
    sel = m_engine.GetSourceSelection(TextPosition{1, 0}, TextPosition{1, 7});
    EXPECT_TRUE(sel.wholeLines);
    EXPECT_EQ(sel.startLine, 3);
    EXPECT_EQ(sel.endLine, 4);
}

TEST_F(TextSelectionTest, HitTestingFindsCaretsWordsAndUnits) {
    Layout("Primero\n\nHola mundo feliz\n\n| uno | dos |\n|---|---|\n| tres | cuatro |\n\n```\nabc def\n```\n");
    const LayoutBlock& p = Block(BlockType::Paragraph, 1);
    D2D1_POINT_2F pt = PointOf(p.textLayout.Get(), p.textOrigin, 6); // 'u' of "mundo"
    const TextPosition caret = m_engine.HitTestTextPosition(pt.x, pt.y);
    EXPECT_EQ(caret.unit, 1u);
    EXPECT_EQ(caret.offset, 6u);
    EXPECT_EQ(Text(m_engine.WordAt(pt.x, pt.y)), L"mundo");
    EXPECT_EQ(Text(m_engine.UnitAt(pt.x, pt.y)), L"Hola mundo feliz");
    EXPECT_TRUE(m_engine.IsOverText(pt.x, pt.y));
    EXPECT_FALSE(m_engine.IsOverText(m_engine.GetLayoutWidth() - 1.0f, pt.y));

    const TableCellLayout& cell = Block(BlockType::Table).tableRows[1][1];
    pt = PointOf(cell.textLayout.Get(), cell.textOrigin, 2);
    EXPECT_EQ(Text(m_engine.WordAt(pt.x, pt.y)), L"cuatro");

    const LayoutBlock& code = Block(BlockType::CodeBlock);
    pt = PointOf(code.textLayout.Get(), code.textOrigin, 5);
    EXPECT_EQ(Text(m_engine.WordAt(pt.x, pt.y)), L"def");

    // Far below the content: the nearest unit, the code block.
    const TextPosition below = m_engine.HitTestTextPosition(10.0f, m_engine.GetTotalHeight() + 500.0f);
    EXPECT_EQ(below.unit, m_engine.GetTextUnits().size() - 1);
}

TEST_F(TextSelectionTest, SelectionRectsCoverOnlyTheVisibleSelection) {
    Layout("uno dos\n\ntres\n\n| a | b |\n|---|---|\n| largo texto | c |\n");
    const float height = m_engine.GetTotalHeight();
    std::vector<SelectionRect> rects;
    m_engine.GetSelectionRects(TextPosition{}, m_engine.DocumentEnd(), 0.0f, height, rects);
    ASSERT_GE(rects.size(), 6u); // Two paragraphs and four cells

    // Ordered by block, as the renderer expects, and table rectangles stay inside a cell.
    const size_t tableIndex = BlockIndex(BlockType::Table);
    const LayoutBlock& table = m_engine.GetBlocks()[tableIndex];
    for (size_t i = 0; i < rects.size(); ++i) {
        if (i > 0) EXPECT_LE(rects[i - 1].block, rects[i].block);
        EXPECT_GT(rects[i].rect.right, rects[i].rect.left);
        if (rects[i].block != tableIndex) continue;
        bool inCell = false;
        for (const auto& row : table.tableRows) {
            for (const auto& c : row) {
                if (rects[i].rect.left >= c.rect.left - 0.01f && rects[i].rect.right <= c.rect.right + 0.01f &&
                    rects[i].rect.top >= c.rect.top - 0.01f && rects[i].rect.bottom <= c.rect.bottom + 0.01f) {
                    inCell = true;
                }
            }
        }
        EXPECT_TRUE(inCell);
    }

    std::vector<SelectionRect> reversed;
    m_engine.GetSelectionRects(m_engine.DocumentEnd(), TextPosition{}, 0.0f, height, reversed);
    EXPECT_EQ(reversed.size(), rects.size());

    m_engine.GetSelectionRects(TextPosition{}, m_engine.DocumentEnd(), height + 10.0f, height + 100.0f, rects);
    EXPECT_TRUE(rects.empty());
    m_engine.GetSelectionRects(TextPosition{0, 2}, TextPosition{0, 2}, 0.0f, height, rects);
    EXPECT_TRUE(rects.empty());
}
