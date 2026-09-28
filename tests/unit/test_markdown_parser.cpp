#include <gtest/gtest.h>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "markdown/block_tree.h"
#include "markdown/md4c_adapter.h"
#include "markdown/parse_worker.h"

using namespace Pluma::Markdown;

TEST(Md4cAdapterTest, HeadingLevels) {
    std::string md = "# Heading 1\n## Heading 2\n### Heading 3\n#### Heading 4\n##### Heading 5\n###### Heading 6\n";
    auto tree = Md4cAdapter::Parse(md);

    ASSERT_NE(tree, nullptr);
    ASSERT_NE(tree->root, nullptr);
    ASSERT_EQ(tree->root->children.size(), 6u);

    for (int i = 0; i < 6; ++i) {
        const auto& block = tree->root->children[i];
        EXPECT_EQ(block->type, BlockType::Heading);
        EXPECT_EQ(block->level, i + 1);
        EXPECT_EQ(block->startLine, i + 1);
        EXPECT_EQ(block->endLine, i + 1);
        ASSERT_FALSE(block->inlineContent.empty());
        EXPECT_EQ(block->inlineContent[0].type, SpanType::Text);
    }
}

TEST(Md4cAdapterTest, ParagraphAndInlineFormatting) {
    std::string md = "Normal **bold** *italic* ~~strike~~ `code` [link](https://example.com \"Title\")\n";
    auto tree = Md4cAdapter::Parse(md);

    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(tree->root->children.size(), 1u);
    const auto& p = tree->root->children[0];
    EXPECT_EQ(p->type, BlockType::Paragraph);

    // Verify inline spans exist
    bool hasBold = false, hasItalic = false, hasStrike = false, hasCode = false, hasLink = false;
    for (const auto& span : p->inlineContent) {
        if (span.type == SpanType::Strong) hasBold = true;
        if (span.type == SpanType::Emphasis) hasItalic = true;
        if (span.type == SpanType::Strikethrough) hasStrike = true;
        if (span.type == SpanType::Code) {
            hasCode = true;
            EXPECT_EQ(span.text, "code");
        }
        if (span.type == SpanType::Link) {
            hasLink = true;
            EXPECT_EQ(span.url, "https://example.com");
            EXPECT_EQ(span.title, "Title");
        }
    }

    EXPECT_TRUE(hasBold);
    EXPECT_TRUE(hasItalic);
    EXPECT_TRUE(hasStrike);
    EXPECT_TRUE(hasCode);
    EXPECT_TRUE(hasLink);
}

TEST(Md4cAdapterTest, ListsOrderedUnorderedAndTasks) {
    std::string md = "- Unordered 1\n- Unordered 2\n\n1. Ordered 1\n2. Ordered 2\n\n- [ ] Todo item\n- [x] Done item\n";
    auto tree = Md4cAdapter::Parse(md);

    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(tree->root->children.size(), 3u);

    // 1. Unordered list
    const auto& ul = tree->root->children[0];
    EXPECT_EQ(ul->type, BlockType::List);
    EXPECT_FALSE(ul->isOrdered);
    EXPECT_EQ(ul->children.size(), 2u);

    // 2. Ordered list
    const auto& ol = tree->root->children[1];
    EXPECT_EQ(ol->type, BlockType::List);
    EXPECT_TRUE(ol->isOrdered);
    EXPECT_EQ(ol->startNumber, 1);
    EXPECT_EQ(ol->children.size(), 2u);

    // 3. Task list (GFM)
    const auto& tl = tree->root->children[2];
    EXPECT_EQ(tl->type, BlockType::List);
    ASSERT_EQ(tl->children.size(), 2u);
    EXPECT_TRUE(tl->children[0]->isTask);
    EXPECT_FALSE(tl->children[0]->isTaskChecked);
    EXPECT_TRUE(tl->children[1]->isTask);
    EXPECT_TRUE(tl->children[1]->isTaskChecked);
}

TEST(Md4cAdapterTest, FencedCodeBlockWithLanguage) {
    std::string md = "```cpp\n#include <iostream>\nint main() { return 0; }\n```\n";
    auto tree = Md4cAdapter::Parse(md);

    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(tree->root->children.size(), 1u);
    const auto& cb = tree->root->children[0];
    EXPECT_EQ(cb->type, BlockType::CodeBlock);
    EXPECT_EQ(cb->info, "cpp");
    ASSERT_FALSE(cb->inlineContent.empty());
    EXPECT_NE(cb->inlineContent[0].text.find("#include <iostream>"), std::string::npos);
}

TEST(Md4cAdapterTest, TablesWithAlignment) {
    std::string md = "| Left | Center | Right |\n| :--- | :---: | ---: |\n| L1   | C1     | R1    |\n";
    auto tree = Md4cAdapter::Parse(md);

    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(tree->root->children.size(), 1u);
    const auto& table = tree->root->children[0];
    EXPECT_EQ(table->type, BlockType::Table);

    // Expect TableHead and TableBody
    ASSERT_GE(table->children.size(), 2u);
    const auto& head = table->children[0];
    EXPECT_EQ(head->type, BlockType::TableHead);
    ASSERT_EQ(head->children.size(), 1u); // 1 header row
    const auto& headerRow = head->children[0];
    ASSERT_EQ(headerRow->children.size(), 3u);

    EXPECT_TRUE(headerRow->children[0]->isHeaderCell);
    EXPECT_EQ(headerRow->children[0]->align, Alignment::Left);
    EXPECT_TRUE(headerRow->children[1]->isHeaderCell);
    EXPECT_EQ(headerRow->children[1]->align, Alignment::Center);
    EXPECT_TRUE(headerRow->children[2]->isHeaderCell);
    EXPECT_EQ(headerRow->children[2]->align, Alignment::Right);
}

TEST(Md4cAdapterTest, ThematicBreakAndBlockquote) {
    std::string md = "> Quote line 1\n> Quote line 2\n\n---\n";
    auto tree = Md4cAdapter::Parse(md);

    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(tree->root->children.size(), 2u);

    EXPECT_EQ(tree->root->children[0]->type, BlockType::Blockquote);
    EXPECT_EQ(tree->root->children[1]->type, BlockType::ThematicBreak);
}

TEST(Md4cAdapterTest, EntityDecoding) {
    std::string md = "Symbols: &amp; &lt; &gt; &quot; &copy;\n";
    auto tree = Md4cAdapter::Parse(md);

    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(tree->root->children.size(), 1u);
    const auto& p = tree->root->children[0];
    ASSERT_FALSE(p->inlineContent.empty());

    std::string combined;
    for (const auto& span : p->inlineContent) {
        combined += span.text;
    }
    EXPECT_NE(combined.find("&"), std::string::npos);
    EXPECT_NE(combined.find("<"), std::string::npos);
    EXPECT_NE(combined.find(">"), std::string::npos);
    EXPECT_NE(combined.find("\""), std::string::npos);
    EXPECT_NE(combined.find("\xC2\xA9"), std::string::npos); // ©
}

TEST(Md4cAdapterTest, LineNumberAccuracy) {
    std::string md = "# Heading 1\n\nLine 3 paragraph\nLine 4 paragraph\n\n```\nLine 7 code\n```\n";
    auto tree = Md4cAdapter::Parse(md);

    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(tree->root->children.size(), 3u);

    // H1 is on line 1
    EXPECT_EQ(tree->root->children[0]->startLine, 1);
    EXPECT_EQ(tree->root->children[0]->endLine, 1);

    // Paragraph is lines 3..4
    EXPECT_EQ(tree->root->children[1]->startLine, 3);
    EXPECT_EQ(tree->root->children[1]->endLine, 4);

    // Code block starts line 6 or 7
    EXPECT_GE(tree->root->children[2]->startLine, 6);
}

TEST(Md4cAdapterTest, LargeDocumentPerformance) {
    // Generate a 100 KB document
    std::string largeMd;
    largeMd.reserve(100 * 1024);
    for (int i = 0; i < 1000; ++i) {
        largeMd += "## Section " + std::to_string(i) + "\n\n";
        largeMd += "This is a paragraph with **bold**, *italic*, and `code` span for testing.\n\n";
        largeMd += "- Item 1\n- Item 2\n- [x] Done task\n\n";
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto tree = Md4cAdapter::Parse(largeMd);
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start);

    ASSERT_NE(tree, nullptr);
    EXPECT_GT(tree->root->children.size(), 1000u);

    // Parsing 100 KB with md4c should be sub-10 ms in release; allow headroom for unoptimized debug/ASan
#if defined(_DEBUG)
    EXPECT_LT(elapsed.count(), 5'000'000); // < 5 s in unoptimized ASan debug build
#else
    EXPECT_LT(elapsed.count(), 20'000);    // < 20 ms in optimized release build
#endif
}

// ==============================================================================
// ParseWorker Async & Debounce Tests (F-08)
// ==============================================================================

namespace {

class MessageReceiverWindow {
public:
    MessageReceiverWindow() {
        WNDCLASSEXW wcex{};
        wcex.cbSize = sizeof(wcex);
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = GetModuleHandleW(nullptr);
        wcex.lpszClassName = L"PlumaParseWorkerTestClass";
        RegisterClassExW(&wcex);

        m_hwnd = CreateWindowExW(0, L"PlumaParseWorkerTestClass", L"", 0, 0, 0, 0, 0,
                                 HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    }

    ~MessageReceiverWindow() {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
        }
        UnregisterClassW(L"PlumaParseWorkerTestClass", GetModuleHandleW(nullptr));
    }

    HWND GetHwnd() const { return m_hwnd; }

    bool WaitForMessage(std::chrono::milliseconds timeout, uint64_t& outVersion, std::unique_ptr<BlockTree>& outTree) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            MSG msg;
            while (PeekMessageW(&msg, m_hwnd, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_USER_PARSE_COMPLETE) {
                    outVersion = static_cast<uint64_t>(msg.wParam);
                    outTree.reset(reinterpret_cast<BlockTree*>(msg.lParam));
                    return true;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

    int DrainRemainingMessages() {
        int count = 0;
        MSG msg;
        while (PeekMessageW(&msg, m_hwnd, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_USER_PARSE_COMPLETE) {
                // Free leaked trees from queue if any
                BlockTree* tree = reinterpret_cast<BlockTree*>(msg.lParam);
                delete tree;
                count++;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return count;
    }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    HWND m_hwnd = nullptr;
};

} // namespace

TEST(ParseWorkerTest, ImmediateRequestSkipsDebounce) {
    MessageReceiverWindow receiver;
    ASSERT_NE(receiver.GetHwnd(), nullptr);

    // A debounce far longer than the wait: only an immediate request can arrive in time.
    ParseWorker worker(receiver.GetHwnd(), WM_USER_PARSE_COMPLETE, std::chrono::milliseconds(5000));
    worker.RequestParse("# Opened document", 7, true);

    uint64_t deliveredVersion = 0;
    std::unique_ptr<BlockTree> deliveredTree;
    ASSERT_TRUE(receiver.WaitForMessage(std::chrono::milliseconds(1000), deliveredVersion, deliveredTree));
    EXPECT_EQ(deliveredVersion, 7u);

    // Typing afterwards is debounced again.
    worker.RequestParse("# Opened document edited", 8);
    EXPECT_FALSE(receiver.WaitForMessage(std::chrono::milliseconds(300), deliveredVersion, deliveredTree));
    worker.Stop();
}

TEST(ParseWorkerTest, AsyncDeliverySingleSnapshot) {
    MessageReceiverWindow receiver;
    ASSERT_NE(receiver.GetHwnd(), nullptr);

    // Fast debounce of 30 ms for unit tests
    ParseWorker worker(receiver.GetHwnd(), WM_USER_PARSE_COMPLETE, std::chrono::milliseconds(30));

    worker.RequestParse("# Async Heading\n\nContent paragraph.", 42);

    uint64_t deliveredVersion = 0;
    std::unique_ptr<BlockTree> deliveredTree;
    bool received = receiver.WaitForMessage(std::chrono::milliseconds(300), deliveredVersion, deliveredTree);

    EXPECT_TRUE(received);
    EXPECT_EQ(deliveredVersion, 42u);
    ASSERT_NE(deliveredTree, nullptr);
    ASSERT_GE(deliveredTree->root->children.size(), 2u);
    EXPECT_EQ(deliveredTree->root->children[0]->type, BlockType::Heading);
}

TEST(ParseWorkerTest, DebounceAndObsoleteDiscard) {
    MessageReceiverWindow receiver;
    ASSERT_NE(receiver.GetHwnd(), nullptr);

    // Debounce of 40 ms
    ParseWorker worker(receiver.GetHwnd(), WM_USER_PARSE_COMPLETE, std::chrono::milliseconds(40));

    // Rapid bursts of snapshots (simulating typing "a", "b", "c")
    worker.RequestParse("# Version 1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    worker.RequestParse("# Version 2", 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    worker.RequestParse("# Version 3", 3);

    uint64_t deliveredVersion = 0;
    std::unique_ptr<BlockTree> deliveredTree;
    bool received = receiver.WaitForMessage(std::chrono::milliseconds(300), deliveredVersion, deliveredTree);

    EXPECT_TRUE(received);
    // Must deliver Version 3; versions 1 and 2 were debounced and discarded
    EXPECT_EQ(deliveredVersion, 3u);
    ASSERT_NE(deliveredTree, nullptr);

    // Wait 80 ms and ensure NO other messages were queued
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    int leftover = receiver.DrainRemainingMessages();
    EXPECT_EQ(leftover, 0);
}

TEST(ParseWorkerTest, StopWithoutDeadlock) {
    MessageReceiverWindow receiver;
    ASSERT_NE(receiver.GetHwnd(), nullptr);

    ParseWorker worker(receiver.GetHwnd(), WM_USER_PARSE_COMPLETE, std::chrono::milliseconds(50));
    worker.RequestParse("# Fast shutdown test", 100);

    // Immediate stop while parse is pending debounce
    worker.Stop();
    SUCCEED();
}

// ==============================================================================
// Entities, raw HTML, emoji shortcodes and nesting
// ==============================================================================

namespace {

std::string ParagraphText(const Block& block) {
    return ExtractPlainText(block.inlineContent);
}

} // namespace

TEST(Md4cAdapterTest, EntityDecodingBeyondAscii) {
    auto tree = Md4cAdapter::Parse("&euro; &ntilde; &#241; &#x1F600; &hearts;\n");
    ASSERT_EQ(tree->root->children.size(), 1u);
    const std::string text = ParagraphText(*tree->root->children[0]);
    EXPECT_NE(text.find("€"), std::string::npos);
    EXPECT_NE(text.find("ñ ñ"), std::string::npos);
    EXPECT_NE(text.find("😀"), std::string::npos);
    EXPECT_NE(text.find("♥"), std::string::npos);
    EXPECT_EQ(text.find('&'), std::string::npos);
}

TEST(Md4cAdapterTest, EmojiShortcodesOutsideCode) {
    auto tree = Md4cAdapter::Parse("Listo :rocket: :tada: :no_existe: `:rocket:`\n");
    ASSERT_EQ(tree->root->children.size(), 1u);
    const auto& p = *tree->root->children[0];
    EXPECT_NE(ParagraphText(p).find("Listo 🚀 🎉 :no_existe:"), std::string::npos);

    bool codeUntouched = false;
    for (const auto& span : p.inlineContent) {
        if (span.type == SpanType::Code) codeUntouched = (span.text == ":rocket:");
    }
    EXPECT_TRUE(codeUntouched);
}

TEST(Md4cAdapterTest, EmojiShortcodesNotInCodeBlocks) {
    auto tree = Md4cAdapter::Parse("```\n:tada:\n```\n");
    ASSERT_EQ(tree->root->children.size(), 1u);
    EXPECT_NE(tree->root->children[0]->inlineContent[0].text.find(":tada:"), std::string::npos);
}

TEST(Md4cAdapterTest, InlineHtmlBreaksAndHiddenTags) {
    auto tree = Md4cAdapter::Parse("uno<br>dos <span class=\"x\">tres</span> <!-- oculto --> cuatro\n");
    ASSERT_EQ(tree->root->children.size(), 1u);
    const auto& p = *tree->root->children[0];

    bool hasBreak = false;
    for (const auto& span : p.inlineContent) {
        if (span.type == SpanType::LineBreak) hasBreak = true;
    }
    EXPECT_TRUE(hasBreak);
    const std::string text = ParagraphText(p);
    EXPECT_EQ(text.find('<'), std::string::npos);
    EXPECT_EQ(text.find("oculto"), std::string::npos);
    EXPECT_NE(text.find("tres"), std::string::npos);
    EXPECT_NE(text.find("cuatro"), std::string::npos);
}

TEST(Md4cAdapterTest, HtmlBlockIsStrippedAndDecoded) {
    auto tree = Md4cAdapter::Parse("<div align=\"center\">\n  <b>Hola</b> &amp; adiós\n</div>\n\n<!-- nota -->\n");
    ASSERT_GE(tree->root->children.size(), 1u);
    const auto& html = *tree->root->children[0];
    EXPECT_TRUE(html.isRawHtml);
    EXPECT_EQ(ParagraphText(html), "Hola & adiós");
    if (tree->root->children.size() > 1) {
        EXPECT_TRUE(tree->root->children[1]->inlineContent.empty()); // Comment-only block
    }
}

TEST(Md4cAdapterTest, NestedListsKeepStructureAndLines) {
    std::string md = "1. Uno\n2. Dos\n   - Anidado\n     - Profundo\n3. Tres\n";
    auto tree = Md4cAdapter::Parse(md);
    ASSERT_EQ(tree->root->children.size(), 1u);
    const auto& ol = *tree->root->children[0];
    ASSERT_EQ(ol.children.size(), 3u);

    const auto& second = *ol.children[1];
    ASSERT_EQ(second.children.size(), 1u);
    EXPECT_EQ(second.children[0]->type, BlockType::List);
    EXPECT_EQ(second.children[0]->children[0]->children[0]->type, BlockType::List);
    // The item covers its nested lines (used by synchronized scroll).
    EXPECT_EQ(second.startLine, 2);
    EXPECT_GE(second.endLine, 4);
}

TEST(Md4cAdapterTest, LinkDestinationEntitiesDecoded) {
    auto tree = Md4cAdapter::Parse("[a](https://x.com/?a=1&amp;b=2)\n");
    const auto& link = tree->root->children[0]->inlineContent[0];
    ASSERT_EQ(link.type, SpanType::Link);
    EXPECT_EQ(link.url, "https://x.com/?a=1&b=2");
}

// ==============================================================================
// Source runs (preview -> source mapping)
// ==============================================================================

namespace {

// Every source run must be a verbatim, ordered, in-range copy of the source.
void ExpectRunsVerbatim(const std::vector<Span>& spans, std::string_view md, size_t& runCount) {
    for (const auto& span : spans) {
        uint32_t lastEnd = 0;
        for (const auto& run : span.source) {
            EXPECT_GE(run.textOffset, lastEnd);
            ASSERT_LE(static_cast<size_t>(run.textOffset) + run.length, span.text.size());
            ASSERT_LE(static_cast<size_t>(run.srcOffset) + run.length, md.size());
            EXPECT_EQ(std::string_view(span.text).substr(run.textOffset, run.length),
                      md.substr(run.srcOffset, run.length));
            lastEnd = run.textOffset + run.length;
            ++runCount;
        }
        ExpectRunsVerbatim(span.children, md, runCount);
    }
}

void ExpectBlockRunsVerbatim(const Block& block, std::string_view md, size_t& runCount) {
    ExpectRunsVerbatim(block.inlineContent, md, runCount);
    for (const auto& child : block.children) {
        if (child) ExpectBlockRunsVerbatim(*child, md, runCount);
    }
}

// Source offset of `utf8` inside the text of a span tree, through its runs (npos if unmapped).
size_t SourceOffsetOf(const std::vector<Span>& spans, std::string_view needle) {
    for (const auto& span : spans) {
        const size_t at = span.text.find(needle);
        if (at != std::string::npos) {
            for (const auto& run : span.source) {
                if (at >= run.textOffset && at < static_cast<size_t>(run.textOffset) + run.length) {
                    return run.srcOffset + (at - run.textOffset);
                }
            }
        }
        const size_t nested = SourceOffsetOf(span.children, needle);
        if (nested != std::string::npos) return nested;
    }
    return std::string::npos;
}

} // namespace

TEST(Md4cAdapterTest, SourceRunsAreVerbatimCopies) {
    const std::string md =
        "# Título con *énfasis*\n\n"
        "Párrafo con **negrita**, `código`, [enlace](https://x.com) y &amp; entidad.\n"
        "Segunda línea :rocket: con emoji y ~~tachado~~.\n\n"
        "> Cita con **texto**\n> en dos líneas\n\n"
        "- [ ] tarea *pendiente*\n- [x] tarea hecha\n  1. anidada\n\n"
        "| Col A | Col B |\n|:--|--:|\n| celda `x` | ñandú |\n\n"
        "```cpp\nint main() {\n    return 0; // ñ\n}\n```\n\n"
        "    indentado\n    código\n\n"
        "<div>html <b>crudo</b> &copy;</div>\n";
    auto tree = Md4cAdapter::Parse(md);
    size_t runCount = 0;
    ExpectBlockRunsVerbatim(*tree->root, md, runCount);
    EXPECT_GT(runCount, 20u);
}

TEST(Md4cAdapterTest, SourceRunsLocateTextAcrossMarkup) {
    const std::string md = "Hola **mundo** &amp; :rocket: fin\ncon *salto*\n";
    auto tree = Md4cAdapter::Parse(md);
    const auto& spans = tree->root->children[0]->inlineContent;
    EXPECT_EQ(SourceOffsetOf(spans, "Hola"), 0u);
    EXPECT_EQ(SourceOffsetOf(spans, "mundo"), md.find("mundo"));
    // Text after an entity and an emoji keeps its real position.
    EXPECT_EQ(SourceOffsetOf(spans, "fin"), md.find("fin"));
    EXPECT_EQ(SourceOffsetOf(spans, "con"), md.find("con"));
    EXPECT_EQ(SourceOffsetOf(spans, "salto"), md.find("salto"));
    // Generated text has no source position.
    EXPECT_EQ(SourceOffsetOf(spans, "&"), std::string::npos);
    EXPECT_EQ(SourceOffsetOf(spans, "🚀"), std::string::npos);
}

TEST(Md4cAdapterTest, CodeBlockRunsSkipLineEnds) {
    const std::string md = "> ```\n> uno\n> dos\n> ```\n";
    auto tree = Md4cAdapter::Parse(md);
    const auto& code = *tree->root->children[0]->children[0];
    ASSERT_EQ(code.type, BlockType::CodeBlock);
    EXPECT_EQ(SourceOffsetOf(code.inlineContent, "uno"), md.find("uno"));
    EXPECT_EQ(SourceOffsetOf(code.inlineContent, "dos"), md.find("dos"));
}

TEST(Md4cAdapterTest, TaskMarkOffsets) {
    const std::string md = "- [ ] uno\n- [x] dos\n\n> 1. [X] tres\n\n- normal\n";
    auto tree = Md4cAdapter::Parse(md);
    const auto& list = *tree->root->children[0];
    ASSERT_EQ(list.children.size(), 2u);
    EXPECT_EQ(list.children[0]->taskMarkOffset, 3);
    EXPECT_EQ(md[3], ' ');
    EXPECT_EQ(md[static_cast<size_t>(list.children[1]->taskMarkOffset)], 'x');

    const auto& quoted = *tree->root->children[1]->children[0]->children[0];
    ASSERT_TRUE(quoted.isTask);
    EXPECT_EQ(static_cast<size_t>(quoted.taskMarkOffset), md.find('X'));

    EXPECT_EQ(tree->root->children[2]->children[0]->taskMarkOffset, -1);
}
