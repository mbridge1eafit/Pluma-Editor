#include "md4c_adapter.h"

#include <vector>
#include <algorithm>
#include <string_view>
#include "md4c.h"
#include "emoji.h"

extern "C" {
#include "entity.h"
}

namespace Pluma::Markdown {

namespace {

constexpr std::string_view kReplacementChar = "\xEF\xBF\xBD"; // U+FFFD

void AppendUtf8(std::string& out, unsigned cp) {
    if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        out.append(kReplacementChar);
    } else if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Decodes named ("&copy;"), decimal ("&#169;") and hexadecimal ("&#xA9;") entities to UTF-8.
std::string DecodeEntity(std::string_view entity) {
    std::string out;
    if (entity.size() >= 4 && entity[1] == '#') {
        const bool hex = (entity[2] == 'x' || entity[2] == 'X');
        unsigned cp = 0;
        for (size_t i = hex ? 3 : 2; i + 1 < entity.size(); ++i) {
            const char c = entity[i];
            unsigned digit = 0;
            if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
            else if (hex && c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
            else if (hex && c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
            else return std::string(entity);
            cp = cp * (hex ? 16u : 10u) + digit;
            if (cp > 0x10FFFF) {
                cp = 0x110000; // Clamp: rendered as U+FFFD
            }
        }
        AppendUtf8(out, cp);
        return out;
    }

    if (const ENTITY* ent = entity_lookup(entity.data(), entity.size())) {
        AppendUtf8(out, ent->codepoints[0]);
        if (ent->codepoints[1]) {
            AppendUtf8(out, ent->codepoints[1]);
        }
        return out;
    }
    return std::string(entity);
}

// Decodes every "&...;" entity found in a run of raw HTML text.
std::string DecodeEntitiesInText(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        const size_t amp = text.find('&', i);
        if (amp == std::string_view::npos) {
            out.append(text.substr(i));
            break;
        }
        out.append(text.substr(i, amp - i));
        const size_t semi = text.find(';', amp);
        if (semi == std::string_view::npos || semi - amp > 32) {
            out.push_back('&');
            i = amp + 1;
            continue;
        }
        out.append(DecodeEntity(text.substr(amp, semi - amp + 1)));
        i = semi + 1;
    }
    return out;
}

// Extracts the value of an attribute (e.g. alt="Logo") from a raw HTML tag.
std::string ExtractHtmlAttribute(std::string_view tag, std::string_view name) {
    size_t pos = 0;
    while ((pos = tag.find(name, pos)) != std::string_view::npos) {
        const bool boundary = pos > 0 && (tag[pos - 1] == ' ' || tag[pos - 1] == '\t' || tag[pos - 1] == '\n');
        size_t eq = pos + name.size();
        while (eq < tag.size() && tag[eq] == ' ') ++eq;
        if (!boundary || eq >= tag.size() || tag[eq] != '=') {
            pos += name.size();
            continue;
        }
        size_t valueStart = eq + 1;
        while (valueStart < tag.size() && tag[valueStart] == ' ') ++valueStart;
        if (valueStart >= tag.size()) break;
        const char quote = tag[valueStart];
        if (quote == '"' || quote == '\'') {
            const size_t valueEnd = tag.find(quote, valueStart + 1);
            if (valueEnd == std::string_view::npos) break;
            return std::string(tag.substr(valueStart + 1, valueEnd - valueStart - 1));
        }
        size_t valueEnd = valueStart;
        while (valueEnd < tag.size() && tag[valueEnd] != ' ' && tag[valueEnd] != '>' && tag[valueEnd] != '/') ++valueEnd;
        return std::string(tag.substr(valueStart, valueEnd - valueStart));
    }
    return {};
}

std::string_view HtmlTagName(std::string_view tag) {
    size_t start = 1;
    if (start < tag.size() && tag[start] == '/') ++start;
    size_t end = start;
    while (end < tag.size() && ((tag[end] >= 'a' && tag[end] <= 'z') || (tag[end] >= 'A' && tag[end] <= 'Z') ||
                                (tag[end] >= '0' && tag[end] <= '9'))) {
        ++end;
    }
    return tag.substr(start, end - start);
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
               return lower(x) == lower(y);
           });
}

struct ParserState {
    std::string_view markdown;
    std::vector<size_t> lineStarts;
    std::unique_ptr<BlockTree> tree;
    std::vector<Block*> blockStack;
    std::vector<Span*> spanStack;
    int currentLine = 1;
    size_t lastOffset = 0;
    bool inHtmlComment = false;
    std::string pendingHtmlTag; // Raw HTML tag split across several callbacks

    size_t cachedLineIndex = 0;

    int OffsetToLine(size_t offset) {
        if (lineStarts.empty()) return 1;
        // Offsets normally grow monotonically; restart the scan if md4c reports an earlier one.
        if (cachedLineIndex < lineStarts.size() && lineStarts[cachedLineIndex] > offset) {
            cachedLineIndex = static_cast<size_t>(
                std::upper_bound(lineStarts.begin(), lineStarts.end(), offset) - lineStarts.begin() - 1);
        }
        while (cachedLineIndex + 1 < lineStarts.size() && lineStarts[cachedLineIndex + 1] <= offset) {
            cachedLineIndex++;
        }
        return static_cast<int>(cachedLineIndex + 1);
    }

    size_t FindNextNonWhitespace(size_t from) const {
        while (from < markdown.size()) {
            const char c = markdown[from];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
                return from;
            }
            from++;
        }
        return markdown.size();
    }

    void AddSpanToCurrent(Span span) {
        if (!spanStack.empty()) {
            spanStack.back()->children.push_back(std::move(span));
        } else if (!blockStack.empty()) {
            blockStack.back()->inlineContent.push_back(std::move(span));
        }
    }

    // Appends plain text to the innermost span/block, merging adjacent text runs.
    void AppendText(std::string_view text) {
        if (text.empty()) return;
        if (!spanStack.empty()) {
            Span* top = spanStack.back();
            if (top->type == SpanType::Code) {
                top->text.append(text);
                return;
            }
            AppendToList(top->children, text, SpanType::Text);
            return;
        }
        if (!blockStack.empty()) {
            Block* topBlock = blockStack.back();
            const SpanType type = (topBlock->type == BlockType::CodeBlock) ? SpanType::Code : SpanType::Text;
            AppendToList(topBlock->inlineContent, text, type);
        }
    }

    static void AppendToList(std::vector<Span>& list, std::string_view text, SpanType type) {
        if (!list.empty() && list.back().type == type && list.back().children.empty()) {
            list.back().text.append(text);
        } else {
            Span span;
            span.type = type;
            span.text.assign(text);
            list.push_back(std::move(span));
        }
    }

    void HandleHtmlTag(std::string_view tag) {
        const std::string_view name = HtmlTagName(tag);
        if (EqualsIgnoreCase(name, "br")) {
            Span brSpan;
            brSpan.type = SpanType::LineBreak;
            AddSpanToCurrent(std::move(brSpan));
        } else if (EqualsIgnoreCase(name, "img")) {
            Span img;
            img.type = SpanType::Image;
            img.url = DecodeEntitiesInText(ExtractHtmlAttribute(tag, "src"));
            img.title = DecodeEntitiesInText(ExtractHtmlAttribute(tag, "title"));
            std::string alt = DecodeEntitiesInText(ExtractHtmlAttribute(tag, "alt"));
            if (!alt.empty()) {
                Span altText;
                altText.type = SpanType::Text;
                altText.text = std::move(alt);
                img.children.push_back(std::move(altText));
            }
            AddSpanToCurrent(std::move(img));
        }
        // Any other tag is presentational markup: drop it, keep its text content.
    }

    // Raw HTML (inline or block): renders the text content, honours <br> and <img>, hides comments.
    void HandleHtml(std::string_view html) {
        size_t i = 0;
        while (i < html.size()) {
            if (inHtmlComment) {
                const size_t end = html.find("-->", i);
                if (end == std::string_view::npos) return;
                inHtmlComment = false;
                i = end + 3;
                continue;
            }
            if (!pendingHtmlTag.empty()) {
                const size_t close = html.find('>', i);
                if (close == std::string_view::npos) {
                    pendingHtmlTag.append(html.substr(i));
                    return;
                }
                pendingHtmlTag.append(html.substr(i, close - i + 1));
                std::string tag = std::move(pendingHtmlTag);
                pendingHtmlTag.clear();
                HandleHtmlTag(tag);
                i = close + 1;
                continue;
            }
            if (html.compare(i, 4, "<!--") == 0) {
                inHtmlComment = true;
                i += 4;
                continue;
            }
            if (html[i] == '<') {
                const size_t close = html.find('>', i);
                if (close == std::string_view::npos) {
                    pendingHtmlTag.assign(html.substr(i));
                    return;
                }
                HandleHtmlTag(html.substr(i, close - i + 1));
                i = close + 1;
                continue;
            }
            size_t next = html.find('<', i);
            if (next == std::string_view::npos) next = html.size();
            std::string text = DecodeEntitiesInText(html.substr(i, next - i));
            // HTML collapses whitespace: newlines inside raw HTML are plain spaces.
            std::replace_if(text.begin(), text.end(), [](char c) { return c == '\n' || c == '\r' || c == '\t'; }, ' ');
            AppendText(text);
            i = next;
        }
    }
};

std::string AttrToString(const MD_ATTRIBUTE& attr) {
    if (!attr.text || attr.size == 0) {
        return {};
    }
    // Attributes may contain entities (e.g. "a&amp;b" in link destinations).
    std::string out;
    for (unsigned i = 0; attr.substr_offsets[i] < attr.size; ++i) {
        const MD_OFFSET begin = attr.substr_offsets[i];
        const MD_OFFSET end = attr.substr_offsets[i + 1];
        std::string_view part(attr.text + begin, end - begin);
        switch (attr.substr_types[i]) {
        case MD_TEXT_ENTITY:   out += DecodeEntity(part); break;
        case MD_TEXT_NULLCHAR: out += kReplacementChar; break;
        default:               out += part; break;
        }
    }
    return out;
}

Alignment ToAlignment(MD_ALIGN align) {
    switch (align) {
    case MD_ALIGN_LEFT:   return Alignment::Left;
    case MD_ALIGN_CENTER: return Alignment::Center;
    case MD_ALIGN_RIGHT:  return Alignment::Right;
    default:              return Alignment::Default;
    }
}

int OnEnterBlock(MD_BLOCKTYPE type, void* detail, void* userdata) {
    auto* state = static_cast<ParserState*>(userdata);

    auto block = std::make_unique<Block>();
    const size_t nextPos = state->FindNextNonWhitespace(state->lastOffset);
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
        if (auto* olDetail = static_cast<MD_BLOCK_OL_DETAIL*>(detail)) {
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
        if (auto* hDetail = static_cast<MD_BLOCK_H_DETAIL*>(detail)) {
            block->level = static_cast<int>(hDetail->level);
        }
        break;
    }
    case MD_BLOCK_P:
        block->type = BlockType::Paragraph;
        break;
    case MD_BLOCK_HTML:
        block->type = BlockType::Paragraph;
        block->isRawHtml = true;
        break;
    case MD_BLOCK_CODE: {
        block->type = BlockType::CodeBlock;
        if (auto* codeDetail = static_cast<MD_BLOCK_CODE_DETAIL*>(detail)) {
            block->info = AttrToString(codeDetail->lang);
            if (block->info.empty()) {
                block->info = AttrToString(codeDetail->info);
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
    case MD_BLOCK_TH:
    case MD_BLOCK_TD: {
        block->type = BlockType::TableCell;
        block->isHeaderCell = (type == MD_BLOCK_TH);
        if (auto* tdDetail = static_cast<MD_BLOCK_TD_DETAIL*>(detail)) {
            block->align = ToAlignment(tdDetail->align);
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
    } else {
        state->blockStack.back()->AddChild(std::move(block));
    }
    state->blockStack.push_back(rawBlock);
    state->inHtmlComment = false;
    state->pendingHtmlTag.clear();

    return 0;
}

int OnLeaveBlock(MD_BLOCKTYPE /*type*/, void* /*detail*/, void* userdata) {
    auto* state = static_cast<ParserState*>(userdata);
    if (!state->blockStack.empty()) {
        Block* top = state->blockStack.back();
        top->endLine = (std::max)(top->startLine, (std::max)(top->endLine, state->currentLine));
        state->blockStack.pop_back();
        // Propagate the end line so containers (lists, quotes) cover their children.
        if (!state->blockStack.empty()) {
            Block* parent = state->blockStack.back();
            parent->endLine = (std::max)(parent->endLine, top->endLine);
        }
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
        if (auto* aDetail = static_cast<MD_SPAN_A_DETAIL*>(detail)) {
            span.url = AttrToString(aDetail->href);
            span.title = AttrToString(aDetail->title);
        }
        break;
    }
    case MD_SPAN_IMG: {
        span.type = SpanType::Image;
        if (auto* imgDetail = static_cast<MD_SPAN_IMG_DETAIL*>(detail)) {
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

    // Pointers into the vectors stay valid: a parent's children are only appended to while
    // it is the innermost open span, and closed spans are never touched again.
    if (!state->spanStack.empty()) {
        auto& siblings = state->spanStack.back()->children;
        siblings.push_back(std::move(span));
        state->spanStack.push_back(&siblings.back());
    } else if (!state->blockStack.empty()) {
        auto& siblings = state->blockStack.back()->inlineContent;
        siblings.push_back(std::move(span));
        state->spanStack.push_back(&siblings.back());
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
        const size_t offset = static_cast<size_t>(text - state->markdown.data());
        state->lastOffset = offset + size;
        state->currentLine = state->OffsetToLine(offset + (size > 0 ? size - 1 : 0));
        if (!state->blockStack.empty()) {
            Block* top = state->blockStack.back();
            top->endLine = (std::max)(top->endLine, state->currentLine);
        }
    }

    switch (type) {
    case MD_TEXT_BR: {
        Span brSpan;
        brSpan.type = SpanType::LineBreak;
        state->AddSpanToCurrent(std::move(brSpan));
        return 0;
    }
    case MD_TEXT_SOFTBR:
        state->AppendText(" ");
        return 0;
    case MD_TEXT_NULLCHAR:
        state->AppendText(kReplacementChar);
        return 0;
    case MD_TEXT_ENTITY:
        state->AppendText(DecodeEntity(std::string_view(text, size)));
        return 0;
    case MD_TEXT_HTML:
        state->HandleHtml(std::string_view(text, size));
        return 0;
    default:
        state->AppendText(std::string_view(text, size));
        return 0;
    }
}

void ReplaceShortcodesInSpans(std::vector<Span>& spans) {
    for (auto& span : spans) {
        if (span.type == SpanType::Text && span.text.find(':') != std::string::npos) {
            span.text = ReplaceEmojiShortcodes(span.text);
        }
        if (span.type != SpanType::Code) {
            ReplaceShortcodesInSpans(span.children);
        }
    }
}

// Post-processing pass: emoji shortcodes and whitespace clean-up of raw HTML blocks.
void FinalizeBlock(Block& block) {
    if (block.type != BlockType::CodeBlock) {
        ReplaceShortcodesInSpans(block.inlineContent);
    }
    if (block.isRawHtml) {
        // Trim the whitespace left behind by stripped tags.
        while (!block.inlineContent.empty() && block.inlineContent.front().type == SpanType::Text) {
            auto& t = block.inlineContent.front().text;
            t.erase(0, t.find_first_not_of(' '));
            if (!t.empty()) break;
            block.inlineContent.erase(block.inlineContent.begin());
        }
        while (!block.inlineContent.empty() && block.inlineContent.back().type == SpanType::Text) {
            auto& t = block.inlineContent.back().text;
            const size_t last = t.find_last_not_of(' ');
            t.erase(last == std::string::npos ? 0 : last + 1);
            if (!t.empty()) break;
            block.inlineContent.pop_back();
        }
    }
    for (auto& child : block.children) {
        if (child) FinalizeBlock(*child);
    }
}

} // namespace

std::unique_ptr<BlockTree> Md4cAdapter::Parse(std::string_view markdown, uint64_t version) {
    ParserState state;
    state.markdown = markdown;
    state.tree = std::make_unique<BlockTree>();
    state.tree->version = version;

    // Index line starts in original markdown
    state.lineStarts.reserve(markdown.size() / 32 + 1);
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

    if (state.tree->root) {
        FinalizeBlock(*state.tree->root);
    }
    return std::move(state.tree);
}

} // namespace Pluma::Markdown
