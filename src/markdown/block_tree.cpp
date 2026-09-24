#include "block_tree.h"

#include <sstream>

namespace Pluma::Markdown {

namespace {

void DumpSpan(const Span& span, std::ostringstream& oss) {
    switch (span.type) {
    case SpanType::Text:
        oss << "Text(\"" << span.text << "\")";
        break;
    case SpanType::Emphasis:
        oss << "Em(";
        for (size_t i = 0; i < span.children.size(); ++i) {
            if (i > 0) oss << ", ";
            DumpSpan(span.children[i], oss);
        }
        oss << ")";
        break;
    case SpanType::Strong:
        oss << "Strong(";
        for (size_t i = 0; i < span.children.size(); ++i) {
            if (i > 0) oss << ", ";
            DumpSpan(span.children[i], oss);
        }
        oss << ")";
        break;
    case SpanType::Code:
        oss << "Code(\"" << span.text << "\")";
        break;
    case SpanType::Link:
        oss << "Link(url=\"" << span.url << "\", title=\"" << span.title << "\", ";
        for (size_t i = 0; i < span.children.size(); ++i) {
            if (i > 0) oss << ", ";
            DumpSpan(span.children[i], oss);
        }
        oss << ")";
        break;
    case SpanType::Image:
        oss << "Image(url=\"" << span.url << "\", title=\"" << span.title << "\")";
        break;
    case SpanType::Strikethrough:
        oss << "Del(";
        for (size_t i = 0; i < span.children.size(); ++i) {
            if (i > 0) oss << ", ";
            DumpSpan(span.children[i], oss);
        }
        oss << ")";
        break;
    case SpanType::LineBreak:
        oss << "LineBreak";
        break;
    }
}

void DumpBlock(const Block& block, std::ostringstream& oss, int depth) {
    std::string indent(depth * 2, ' ');
    oss << indent;

    switch (block.type) {
    case BlockType::Document:
        oss << "Document";
        break;
    case BlockType::Heading:
        oss << "Heading(H" << block.level << ", lines " << block.startLine << ".." << block.endLine << ")";
        break;
    case BlockType::Paragraph:
        oss << "Paragraph(lines " << block.startLine << ".." << block.endLine << ")";
        break;
    case BlockType::Blockquote:
        oss << "Blockquote(lines " << block.startLine << ".." << block.endLine << ")";
        break;
    case BlockType::List:
        oss << "List(" << (block.isOrdered ? "ordered" : "unordered") << ", lines " << block.startLine << ".." << block.endLine << ")";
        break;
    case BlockType::ListItem:
        oss << "ListItem";
        if (block.isTask) {
            oss << (block.isTaskChecked ? "[x]" : "[ ]");
        }
        oss << "(lines " << block.startLine << ".." << block.endLine << ")";
        break;
    case BlockType::CodeBlock:
        oss << "CodeBlock(lang=\"" << block.info << "\", lines " << block.startLine << ".." << block.endLine << ")";
        break;
    case BlockType::ThematicBreak:
        oss << "ThematicBreak(lines " << block.startLine << ".." << block.endLine << ")";
        break;
    case BlockType::Table:
        oss << "Table(lines " << block.startLine << ".." << block.endLine << ")";
        break;
    case BlockType::TableHead:
        oss << "TableHead";
        break;
    case BlockType::TableBody:
        oss << "TableBody";
        break;
    case BlockType::TableRow:
        oss << "TableRow";
        break;
    case BlockType::TableCell:
        oss << (block.isHeaderCell ? "TableHeaderCell" : "TableCell");
        if (block.align != Alignment::Default) {
            oss << "(align=";
            switch (block.align) {
            case Alignment::Left: oss << "left"; break;
            case Alignment::Center: oss << "center"; break;
            case Alignment::Right: oss << "right"; break;
            default: break;
            }
            oss << ")";
        }
        break;
    }

    if (!block.inlineContent.empty()) {
        oss << " [";
        for (size_t i = 0; i < block.inlineContent.size(); ++i) {
            if (i > 0) oss << ", ";
            DumpSpan(block.inlineContent[i], oss);
        }
        oss << "]";
    }
    oss << "\n";

    for (const auto& child : block.children) {
        if (child) {
            DumpBlock(*child, oss, depth + 1);
        }
    }
}

} // namespace

std::string BlockTree::DumpToString() const {
    std::ostringstream oss;
    if (root) {
        DumpBlock(*root, oss, 0);
    }
    return oss.str();
}

} // namespace Pluma::Markdown
