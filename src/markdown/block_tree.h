#pragma once

#include <string>
#include <vector>
#include <memory>
#include <string_view>
#include <cstdint>

namespace Pluma::Markdown {

enum class BlockType {
    Document,
    Heading,
    Paragraph,
    Blockquote,
    List,
    ListItem,
    CodeBlock,
    ThematicBreak,
    Table,
    TableHead,
    TableBody,
    TableRow,
    TableCell
};

enum class SpanType {
    Text,
    Emphasis,
    Strong,
    Code,
    Link,
    Image,
    Strikethrough,
    LineBreak
};

enum class Alignment {
    Default,
    Left,
    Center,
    Right
};

struct Span {
    SpanType type = SpanType::Text;
    std::string text;
    std::string url;      // For links and images
    std::string title;    // For links and images
    std::vector<Span> children;
};

struct Block {
    BlockType type = BlockType::Paragraph;
    int level = 0;                        // For Heading (1-6)
    bool isOrdered = false;               // For List
    int startNumber = 1;                  // For ordered list
    bool isTask = false;                  // For task list items (GFM)
    bool isTaskChecked = false;           // [x] vs [ ]
    bool isHeaderCell = false;            // For TableCell (TH vs TD)
    Alignment align = Alignment::Default; // For TableCell
    std::string info;                     // For CodeBlock language info
    int startLine = 0;                    // Source document line range for sync scroll (F-14)
    int endLine = 0;
    std::vector<Span> inlineContent;
    std::vector<std::unique_ptr<Block>> children;

    void AddChild(std::unique_ptr<Block> child) {
        children.push_back(std::move(child));
    }
};

struct BlockTree {
    uint64_t version = 0;
    std::unique_ptr<Block> root;

    BlockTree() : root(std::make_unique<Block>()) {
        root->type = BlockType::Document;
    }

    // Dumps tree structure to a deterministic string for unit tests
    std::string DumpToString() const;
};

} // namespace Pluma::Markdown
