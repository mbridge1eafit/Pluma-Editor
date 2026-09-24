#include "md4c_adapter.h"

#include <vector>
#include <algorithm>
#include <string_view>
#include "md4c.h"

namespace Pluma::Markdown {

namespace {

struct ParserState {
    std::string_view markdown;
    std::vector<size_t> lineStarts;
    std::unique_ptr<BlockTree> tree;
    std::vector<Block*> blockStack;
    std::vector<Span*> spanStack;
    int currentLine = 1;
    size_t lastOffset = 0;

    mutable size_t cachedLineIndex = 0;

    int OffsetToLine(size_t offset) const {
        if (lineStarts.empty()) return 1;
        while (cachedLineIndex + 1 < lineStarts.size() && lineStarts[cachedLineIndex + 1] <= offset) {
            cachedLineIndex++;
        }
        return static_cast<int>(cachedLineIndex + 1);
    }

    size_t FindNextNonWhitespace(size_t from) const {
        while (from < markdown.size()) {
            char c = markdown[from];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
                return from;
            }
            from++;
        }
        return from < markdown.size() ? from : markdown.size();
    }

    void AddSpanToCurrent(Span span) {
        if (!spanStack.empty()) {
            spanStack.back()->children.push_back(std::move(span));
        } else if (!blockStack.empty()) {
            blockStack.back()->inlineContent.push_back(std::move(span));
        }
    }
};

std::string AttrToString(const MD_ATTRIBUTE& attr) {
    if (attr.text && attr.size > 0) {
        return std::string(attr.text, attr.size);
    }
    return {};
}

std::string DecodeEntity(std::string_view entity) {
    if (entity == "&amp;") return "&";
    if (entity == "&lt;") return "<";
    if (entity == "&gt;") return ">";
    if (entity == "&quot;") return "\"";
    if (entity == "&apos;" || entity == "&#39;") return "'";
    if (entity == "&copy;") return "\xC2\xA9";
    if (entity == "&nbsp;") return " ";
    if (entity == "&mdash;") return "\xE2\x80\x94";
    if (entity == "&ndash;") return "\xE2\x80\x93";
    if (entity.size() > 3 && entity[1] == '#' && entity[2] != 'x' && entity[2] != 'X') {
        int code = 0;
        for (size_t i = 2; i < entity.size() - 1; ++i) {
            if (entity[i] >= '0' && entity[i] <= '9') {
                code = code * 10 + (entity[i] - '0');
            } else {
                return std::string(entity);
            }
        }
        if (code > 0 && code <= 0x7F) {
            return std::string(1, static_cast<char>(code));
        }
    }
    if (entity.size() > 4 && entity[1] == '#' && (entity[2] == 'x' || entity[2] == 'X')) {
        int code = 0;
        for (size_t i = 3; i < entity.size() - 1; ++i) {
            char c = entity[i];
            if (c >= '0' && c <= '9') code = code * 16 + (c - '0');
            else if (c >= 'a' && c <= 'f') code = code * 16 + (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') code = code * 16 + (c - 'A' + 10);
            else return std::string(entity);
        }
        if (code > 0 && code <= 0x7F) {
            return std::string(1, static_cast<char>(code));
        }
    }
    return std::string(entity);
}

int OnEnterBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
    auto* state = static_cast<ParserState*>(userdata);

    auto block = std::make_unique<Block>();
    size_t nextPos = state->FindNextNonWhitespace(state->lastOffset);
    state->lastOffset = nextPos;
    block->startLine = state->OffsetToLine(nextPos);
    block->endLine = block->startLine;

    switch (type) {
    case MD_BLOCK_DOC:
        block->type = BlockType::Document;
        block->startLine = 1;
        break;
    case MD_BLOCK_QUOTE:
        block->type = BlockType::Blockquote;
        break;
    case MD_BLOCK_UL:
        block->type = BlockType::List;
        block->isOrdered = false;
        break;
    case MD_BLOCK_OL: {
        block->type = BlockType::List;
        block->isOrdered = true;
        auto* olDetail = static_cast<MD_BLOCK_OL_DETAIL*>(detail);
        if (olDetail) {
            block->startNumber = static_cast<int>(olDetail->start);
        }
        break;
    }
    case MD_BLOCK_LI: {
        block->type = BlockType::ListItem;
        auto* liDetail = static_cast<MD_BLOCK_LI_DETAIL*>(detail);
        if (liDetail && liDetail->is_task) {
            block->isTask = true;
            block->isTaskChecked = (liDetail->task_mark == 'x' || liDetail->task_mark == 'X');
        }
        break;
    }
    case MD_BLOCK_HR:
        block->type = BlockType::ThematicBreak;
        break;
    case MD_BLOCK_H: {
        block->type = BlockType::Heading;
        auto* hDetail = static_cast<MD_BLOCK_H_DETAIL*>(detail);
        if (hDetail) {
            block->level = static_cast<int>(hDetail->level);
        }
        break;
    }
    case MD_BLOCK_P:
        block->type = BlockType::Paragraph;
        break;
    case MD_BLOCK_CODE: {
        block->type = BlockType::CodeBlock;
        auto* codeDetail = static_cast<MD_BLOCK_CODE_DETAIL*>(detail);
        if (codeDetail) {
            block->info = AttrToString(codeDetail->info);
            if (block->info.empty()) {
                block->info = AttrToString(codeDetail->lang);
            }
        }
        break;
    }
    case MD_BLOCK_TABLE:
        block->type = BlockType::Table;
        break;
    case MD_BLOCK_THEAD:
        block->type = BlockType::TableHead;
        break;
    case MD_BLOCK_TBODY:
        block->type = BlockType::TableBody;
        break;
    case MD_BLOCK_TR:
        block->type = BlockType::TableRow;
        break;
    case MD_BLOCK_TH: {
        block->type = BlockType::TableCell;
        block->isHeaderCell = true;
        auto* tdDetail = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
        if (tdDetail) {
            switch (tdDetail->align) {
            case MD_ALIGN_LEFT:   block->align = Alignment::Left; break;
            case MD_ALIGN_CENTER: block->align = Alignment::Center; break;
            case MD_ALIGN_RIGHT:  block->align = Alignment::Right; break;
            default:              block->align = Alignment::Default; break;
            }
        }
        break;
    }
    case MD_BLOCK_TD: {
        block->type = BlockType::TableCell;
        block->isHeaderCell = false;
        auto* tdDetail = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
        if (tdDetail) {
            switch (tdDetail->align) {
            case MD_ALIGN_LEFT:   block->align = Alignment::Left; break;
            case MD_ALIGN_CENTER: block->align = Alignment::Center; break;
            case MD_ALIGN_RIGHT:  block->align = Alignment::Right; break;
            default:              block->align = Alignment::Default; break;
            }
        }
        break;
    }
    default:
        block->type = BlockType::Paragraph;
        break;
    }

    Block* rawBlock = block.get();
    if (state->blockStack.empty()) {
        state->tree->root = std::move(block);
        state->blockStack.push_back(rawBlock);
    } else {
        state->blockStack.back()->AddChild(std::move(block));
        state->blockStack.push_back(rawBlock);
    }

    return 0;
}

int OnLeaveBlock(MD_BLOCKTYPE /*type*/, void* /*detail*/, void* userdata) {
    auto* state = static_cast<ParserState*>(userdata);
    if (!state->blockStack.empty()) {
        Block* top = state->blockStack.back();
        top->endLine = std::max(top->startLine, state->currentLine);
        state->blockStack.pop_back();
    }
    return 0;
}

int OnEnterSpan(MD_SPANTYPE type, void* detail, void* userdata) {
    auto* state = static_cast<ParserState*>(userdata);

    Span span;
    switch (type) {
    case MD_SPAN_EM:
        span.type = SpanType::Emphasis;
        break;
    case MD_SPAN_STRONG:
        span.type = SpanType::Strong;
        break;
    case MD_SPAN_A: {
        span.type = SpanType::Link;
        auto* aDetail = static_cast<MD_SPAN_A_DETAIL*>(detail);
        if (aDetail) {
            span.url = AttrToString(aDetail->href);
            span.title = AttrToString(aDetail->title);
        }
        break;
    }
    case MD_SPAN_IMG: {
        span.type = SpanType::Image;
        auto* imgDetail = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
        if (imgDetail) {
            span.url = AttrToString(imgDetail->src);
            span.title = AttrToString(imgDetail->title);
        }
        break;
    }
    case MD_SPAN_CODE:
        span.type = SpanType::Code;
        break;
    case MD_SPAN_DEL:
        span.type = SpanType::Strikethrough;
        break;
    default:
        span.type = SpanType::Text;
        break;
    }

    if (!state->spanStack.empty()) {
        state->spanStack.back()->children.push_back(std::move(span));
        state->spanStack.push_back(&state->spanStack.back()->children.back());
    } else if (!state->blockStack.empty()) {
        state->blockStack.back()->inlineContent.push_back(std::move(span));
        state->spanStack.push_back(&state->blockStack.back()->inlineContent.back());
    }

    return 0;
}

int OnLeaveSpan(MD_SPANTYPE /*type*/, void* /*detail*/, void* userdata) {
    auto* state = static_cast<ParserState*>(userdata);
    if (!state->spanStack.empty()) {
        state->spanStack.pop_back();
    }
    return 0;
}

int OnText(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata) {
    auto* state = static_cast<ParserState*>(userdata);

    // Update current source line estimate
    if (text >= state->markdown.data() && text <= state->markdown.data() + state->markdown.size()) {
        size_t offset = static_cast<size_t>(text - state->markdown.data());
        state->lastOffset = offset + size;
        state->currentLine = state->OffsetToLine(offset);
        if (!state->blockStack.empty()) {
            state->blockStack.back()->endLine = state->currentLine;
        }
    }

    if (type == MD_TEXT_BR) {
        Span brSpan;
        brSpan.type = SpanType::LineBreak;
        state->AddSpanToCurrent(std::move(brSpan));
        state->currentLine++;
        return 0;
    }

    if (type == MD_TEXT_SOFTBR) {
        std::string space = " ";
        if (!state->spanStack.empty()) {
            Span* top = state->spanStack.back();
            if (top->type == SpanType::Code) {
                top->text.append(space);
            } else if (!top->children.empty() && top->children.back().type == SpanType::Text) {
                top->children.back().text.append(space);
            } else {
                Span txt;
                txt.type = SpanType::Text;
                txt.text = space;
                top->children.push_back(std::move(txt));
            }
        } else if (!state->blockStack.empty()) {
            Block* topBlock = state->blockStack.back();
            if (topBlock->type == BlockType::CodeBlock) {
                topBlock->inlineContent.push_back(Span{SpanType::Code, "\n"});
            } else {
                if (!topBlock->inlineContent.empty() && topBlock->inlineContent.back().type == SpanType::Text) {
                    topBlock->inlineContent.back().text.append(space);
                } else {
                    Span txt;
                    txt.type = SpanType::Text;
                    txt.text = space;
                    topBlock->inlineContent.push_back(std::move(txt));
                }
            }
        }
        state->currentLine++;
        return 0;
    }

    std::string textStr;
    if (type == MD_TEXT_ENTITY) {
        textStr = DecodeEntity(std::string_view(text, size));
    } else {
        textStr.assign(text, size);
    }

    // Append text to active span or block
    if (!state->spanStack.empty()) {
        Span* top = state->spanStack.back();
        if (top->type == SpanType::Code) {
            top->text.append(textStr);
        } else if (!top->children.empty() && top->children.back().type == SpanType::Text) {
            top->children.back().text.append(textStr);
        } else {
            Span txt;
            txt.type = SpanType::Text;
            txt.text = std::move(textStr);
            top->children.push_back(std::move(txt));
        }
    } else if (!state->blockStack.empty()) {
        Block* topBlock = state->blockStack.back();
        if (topBlock->type == BlockType::CodeBlock) {
            if (!topBlock->inlineContent.empty() && topBlock->inlineContent.back().type == SpanType::Code) {
                topBlock->inlineContent.back().text.append(textStr);
            } else {
                Span codeSpan;
                codeSpan.type = SpanType::Code;
                codeSpan.text = std::move(textStr);
                topBlock->inlineContent.push_back(std::move(codeSpan));
            }
        } else {
            if (!topBlock->inlineContent.empty() && topBlock->inlineContent.back().type == SpanType::Text) {
                topBlock->inlineContent.back().text.append(textStr);
            } else {
                Span txt;
                txt.type = SpanType::Text;
                txt.text = std::move(textStr);
                topBlock->inlineContent.push_back(std::move(txt));
            }
        }
    }

    return 0;
}

} // namespace

std::unique_ptr<BlockTree> Md4cAdapter::Parse(std::string_view markdown, uint64_t version) {
    ParserState state;
    state.markdown = markdown;
    state.tree = std::make_unique<BlockTree>();
    state.tree->version = version;

    // Index line starts in original markdown
    state.lineStarts.push_back(0);
    for (size_t i = 0; i < markdown.size(); ++i) {
        if (markdown[i] == '\n') {
            state.lineStarts.push_back(i + 1);
        }
    }

    MD_PARSER parser{};
    parser.abi_version = 0;
    // CommonMark 0.31 + GFM tables, tasklists, strikethrough, autolinks
    parser.flags = MD_DIALECT_GITHUB;
    parser.enter_block = OnEnterBlock;
    parser.leave_block = OnLeaveBlock;
    parser.enter_span = OnEnterSpan;
    parser.leave_span = OnLeaveSpan;
    parser.text = OnText;

    md_parse(markdown.data(), static_cast<MD_SIZE>(markdown.size()), &parser, &state);

    return std::move(state.tree);
}

} // namespace Pluma::Markdown
