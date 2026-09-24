#include "pdf_exporter.h"
#include "../diagram/mermaid.h"
#include "../markdown/md4c_adapter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Pluma::Export {

namespace {

// ---------------------------------------------------------------------------
// Fonts (standard 14 Type 1 fonts, WinAnsiEncoding) and their metrics
// ---------------------------------------------------------------------------

enum Font : int {
    kHelvetica = 1,
    kHelveticaBold = 2,
    kHelveticaOblique = 3,
    kHelveticaBoldOblique = 4,
    kCourier = 5,
    kCourierBold = 6,
};

// Glyph widths (1/1000 em) for WinAnsi 32..126, from the Adobe AFM files.
constexpr std::array<short, 95> kHelveticaWidths = {
    278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278, // ' '..'/'
    556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556, // '0'..'?'
    1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778, // '@'..'O'
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556,  // 'P'..'_'
    333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,  // '`'..'o'
    556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584};      // 'p'..'~'

constexpr std::array<short, 95> kHelveticaBoldWidths = {
    278, 333, 474, 556, 556, 889, 722, 238, 333, 333, 389, 584, 278, 333, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 333, 333, 584, 584, 584, 611,
    975, 722, 722, 722, 722, 667, 611, 778, 722, 278, 556, 722, 611, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 333, 278, 333, 584, 556,
    333, 556, 611, 556, 611, 556, 333, 611, 611, 278, 278, 556, 278, 889, 611, 611,
    611, 611, 389, 556, 333, 611, 556, 778, 556, 556, 500, 389, 280, 389, 584};

bool IsBoldFont(int font) {
    return font == kHelveticaBold || font == kHelveticaBoldOblique || font == kCourierBold;
}

bool IsMonoFont(int font) {
    return font == kCourier || font == kCourierBold;
}

// Width in points of WinAnsi-encoded bytes.
float MeasureAnsi(std::string_view ansi, int font, float size) {
    if (IsMonoFont(font)) {
        return static_cast<float>(ansi.size()) * 0.6f * size;
    }
    const auto& table = IsBoldFont(font) ? kHelveticaBoldWidths : kHelveticaWidths;
    int units = 0;
    for (char ch : ansi) {
        const auto c = static_cast<unsigned char>(ch);
        if (c >= 32 && c <= 126) {
            units += table[c - 32];
        } else {
            units += 556; // Accented letters and symbols: average glyph width
        }
    }
    return static_cast<float>(units) * size / 1000.0f;
}

// ---------------------------------------------------------------------------
// Text encoding
// ---------------------------------------------------------------------------

bool IsInvisibleCodepoint(uint32_t cp) {
    return cp == 0xFE0F || cp == 0xFE0E || cp == 0x200D || cp == 0x200B ||
           (cp >= 0x1F3FB && cp <= 0x1F3FF); // Variation selectors, ZWJ, skin tones
}

bool IsEmojiCodepoint(uint32_t cp) {
    return (cp >= 0x1F000 && cp <= 0x1FAFF) || (cp >= 0x2600 && cp <= 0x27BF) ||
           (cp >= 0x2B00 && cp <= 0x2BFF) || (cp >= 0x1F1E6 && cp <= 0x1F1FF);
}

// Maps a code point to a WinAnsiEncoding byte; returns 0 when it has no equivalent.
unsigned char ToWinAnsiByte(uint32_t cp) {
    if (cp >= 0x20 && cp <= 0x7E) return static_cast<unsigned char>(cp);
    if (cp >= 0xA0 && cp <= 0xFF) return static_cast<unsigned char>(cp);
    switch (cp) {
    case 0x20AC: return 0x80; // €
    case 0x201A: return 0x82;
    case 0x0192: return 0x83;
    case 0x201E: return 0x84;
    case 0x2026: return 0x85; // …
    case 0x2020: return 0x86;
    case 0x2021: return 0x87;
    case 0x02C6: return 0x88;
    case 0x2030: return 0x89;
    case 0x0160: return 0x8A;
    case 0x2039: return 0x8B;
    case 0x0152: return 0x8C;
    case 0x017D: return 0x8E;
    case 0x2018: return 0x91;
    case 0x2019: return 0x92;
    case 0x201C: return 0x93;
    case 0x201D: return 0x94;
    case 0x2022: return 0x95; // •
    case 0x2013: return 0x96; // –
    case 0x2014: return 0x97; // —
    case 0x02DC: return 0x98;
    case 0x2122: return 0x99; // ™
    case 0x0161: return 0x9A;
    case 0x203A: return 0x9B;
    case 0x0153: return 0x9C;
    case 0x017E: return 0x9E;
    case 0x0178: return 0x9F;
    case 0x2192: return '>';  // → (no arrow in WinAnsi)
    case 0x2190: return '<';
    case 0x2212: return '-';  // Minus sign
    case 0x2713:
    case 0x2714: return 'x';  // Check marks
    default: return 0;
    }
}

// Converts UTF-8 text to WinAnsi bytes (emoji are dropped, other unknown characters become '?').
std::string ToWinAnsi(std::string_view utf8) {
    std::string out;
    out.reserve(utf8.size());
    for (size_t i = 0; i < utf8.size();) {
        const auto c = static_cast<unsigned char>(utf8[i]);
        uint32_t cp = c;
        size_t len = 1;
        if (c >= 0xF0 && i + 3 < utf8.size()) {
            cp = (c & 0x07u) << 18 | (static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu) << 12 |
                 (static_cast<unsigned char>(utf8[i + 2]) & 0x3Fu) << 6 | (static_cast<unsigned char>(utf8[i + 3]) & 0x3Fu);
            len = 4;
        } else if (c >= 0xE0 && i + 2 < utf8.size()) {
            cp = (c & 0x0Fu) << 12 | (static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu) << 6 |
                 (static_cast<unsigned char>(utf8[i + 2]) & 0x3Fu);
            len = 3;
        } else if (c >= 0xC0 && i + 1 < utf8.size()) {
            cp = (c & 0x1Fu) << 6 | (static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu);
            len = 2;
        } else if (c >= 0x80) {
            cp = 0xFFFD;
        }
        i += len;

        if (cp == '\t') {
            out += "    ";
        } else if (cp == '\n' || cp == '\r') {
            out.push_back(static_cast<char>(cp));
        } else if (IsInvisibleCodepoint(cp)) {
            continue;
        } else if (IsEmojiCodepoint(cp)) {
            // Standard PDF fonts have no emoji glyphs: skip them (and the space that follows).
            if (i < utf8.size() && utf8[i] == ' ' && (out.empty() || out.back() == ' ')) {
                ++i;
            }
        } else if (cp < 0x20) {
            continue;
        } else if (const unsigned char b = ToWinAnsiByte(cp)) {
            out.push_back(static_cast<char>(b));
        } else {
            out.push_back('?');
        }
    }
    return out;
}

// Escapes WinAnsi bytes as the body of a PDF literal string.
std::string EscapePdf(std::string_view ansi) {
    std::string out;
    out.reserve(ansi.size() + 8);
    for (char ch : ansi) {
        const auto c = static_cast<unsigned char>(ch);
        if (c == '\\' || c == '(' || c == ')') {
            out.push_back('\\');
            out.push_back(static_cast<char>(c));
        } else if (c >= 32 && c <= 126) {
            out.push_back(static_cast<char>(c));
        } else {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\%03o", c);
            out += buf;
        }
    }
    return out;
}

// Kept for callers that pass UTF-8 directly (footer, metadata).
std::string ToPdfString(std::string_view utf8) {
    return EscapePdf(ToWinAnsi(utf8));
}

// ---------------------------------------------------------------------------
// Rich text runs and line breaking
// ---------------------------------------------------------------------------

struct Rgb {
    float r = 0.0f, g = 0.0f, b = 0.0f;
    bool operator==(const Rgb&) const = default;
};

constexpr Rgb kTextColor{0.12f, 0.14f, 0.16f};
constexpr Rgb kHeadingColor{0.07f, 0.08f, 0.10f};
constexpr Rgb kMutedColor{0.36f, 0.40f, 0.44f};
constexpr Rgb kLinkColor{0.03f, 0.41f, 0.85f};
constexpr Rgb kCodeColor{0.15f, 0.16f, 0.18f};

struct Piece {
    std::string text; // WinAnsi bytes
    int font = kHelvetica;
    float size = 10.0f;
    Rgb color = kTextColor;
    bool code = false;
    bool strike = false;
    bool isSpace = false;
    bool isBreak = false;
    float width = 0.0f;
};

struct Line {
    std::vector<Piece> pieces;
    float width = 0.0f;
};

struct TextStyle {
    int font = kHelvetica;
    float size = 10.0f;
    float leading = 14.5f;
    Rgb color = kTextColor;
};

int ComposeFont(bool bold, bool italic, bool code) {
    if (code) return bold ? kCourierBold : kCourier;
    if (bold && italic) return kHelveticaBoldOblique;
    if (bold) return kHelveticaBold;
    if (italic) return kHelveticaOblique;
    return kHelvetica;
}

void AppendPieces(std::vector<Piece>& out, std::string_view ansi, const Piece& proto) {
    // Split into words and single spaces so the line breaker can work on them.
    size_t i = 0;
    while (i < ansi.size()) {
        if (ansi[i] == '\n') {
            Piece br = proto;
            br.text.clear();
            br.isBreak = true;
            out.push_back(std::move(br));
            ++i;
            continue;
        }
        const bool space = ansi[i] == ' ';
        size_t j = i;
        while (j < ansi.size() && ansi[j] != '\n' && (ansi[j] == ' ') == space) ++j;
        Piece p = proto;
        p.isSpace = space;
        p.text.assign(space ? std::string_view(" ") : ansi.substr(i, j - i));
        p.width = MeasureAnsi(p.text, p.font, p.size);
        out.push_back(std::move(p));
        i = j;
    }
}

void FlattenSpans(const std::vector<Markdown::Span>& spans, const TextStyle& base, bool bold, bool italic,
                  bool code, bool link, bool strike, std::vector<Piece>& out) {
    for (const auto& span : spans) {
        bool b = bold, it = italic, cd = code, ln = link, st = strike;
        switch (span.type) {
        case Markdown::SpanType::Strong:        b = true; break;
        case Markdown::SpanType::Emphasis:      it = true; break;
        case Markdown::SpanType::Code:          cd = true; break;
        case Markdown::SpanType::Link:          ln = true; break;
        case Markdown::SpanType::Strikethrough: st = true; break;
        default: break;
        }

        Piece proto;
        proto.font = ComposeFont(b || IsBoldFont(base.font), it, cd);
        proto.size = cd ? base.size * 0.92f : base.size;
        proto.color = ln ? kLinkColor : (cd ? kCodeColor : base.color);
        proto.code = cd;
        proto.strike = st;

        switch (span.type) {
        case Markdown::SpanType::Text:
        case Markdown::SpanType::Code:
            AppendPieces(out, ToWinAnsi(span.text), proto);
            break;
        case Markdown::SpanType::LineBreak:
            AppendPieces(out, "\n", proto);
            break;
        case Markdown::SpanType::Image: {
            std::string alt = Markdown::ExtractPlainText(span.children);
            proto.font = ComposeFont(false, true, false);
            proto.color = kMutedColor;
            AppendPieces(out, ToWinAnsi("[imagen: " + (alt.empty() ? span.url : alt) + "]"), proto);
            continue;
        }
        default:
            break;
        }
        FlattenSpans(span.children, base, b, it, cd, ln, st, out);
    }
}

std::vector<Piece> BuildPieces(const std::vector<Markdown::Span>& spans, const TextStyle& style) {
    std::vector<Piece> pieces;
    FlattenSpans(spans, style, false, false, false, false, false, pieces);
    return pieces;
}

// Greedy line breaking; words wider than the line are split by characters.
std::vector<Line> WrapPieces(const std::vector<Piece>& pieces, float maxWidth) {
    std::vector<Line> lines(1);
    maxWidth = (std::max)(maxWidth, 12.0f);

    size_t i = 0;
    while (i < pieces.size()) {
        const Piece& p = pieces[i];
        Line& line = lines.back();

        if (p.isBreak) {
            lines.emplace_back();
            ++i;
            continue;
        }
        if (p.isSpace) {
            if (!line.pieces.empty()) {
                line.pieces.push_back(p);
                line.width += p.width;
            }
            ++i;
            continue;
        }

        // A word may be made of several styled pieces ("**neg**rita").
        size_t j = i;
        float wordWidth = 0.0f;
        while (j < pieces.size() && !pieces[j].isSpace && !pieces[j].isBreak) {
            wordWidth += pieces[j].width;
            ++j;
        }

        if (line.width + wordWidth <= maxWidth || line.pieces.empty()) {
            if (wordWidth <= maxWidth) {
                for (size_t k = i; k < j; ++k) {
                    line.pieces.push_back(pieces[k]);
                }
                line.width += wordWidth;
            } else {
                // Emergency break of an over-long word (URLs, identifiers).
                for (size_t k = i; k < j; ++k) {
                    const Piece& src = pieces[k];
                    Piece chunk = src;
                    chunk.text.clear();
                    chunk.width = 0.0f;
                    for (char ch : src.text) {
                        const float cw = MeasureAnsi(std::string_view(&ch, 1), src.font, src.size);
                        if (lines.back().width + chunk.width + cw > maxWidth && (!chunk.text.empty() || !lines.back().pieces.empty())) {
                            if (!chunk.text.empty()) {
                                lines.back().pieces.push_back(chunk);
                                lines.back().width += chunk.width;
                            }
                            lines.emplace_back();
                            chunk.text.clear();
                            chunk.width = 0.0f;
                        }
                        chunk.text.push_back(ch);
                        chunk.width += cw;
                    }
                    if (!chunk.text.empty()) {
                        lines.back().pieces.push_back(chunk);
                        lines.back().width += chunk.width;
                    }
                }
            }
            i = j;
            continue;
        }

        // Start a new line: drop the trailing space of the current one.
        if (!line.pieces.empty() && line.pieces.back().isSpace) {
            line.width -= line.pieces.back().width;
            line.pieces.pop_back();
        }
        lines.emplace_back();
    }

    for (auto& line : lines) {
        while (!line.pieces.empty() && line.pieces.back().isSpace) {
            line.width -= line.pieces.back().width;
            line.pieces.pop_back();
        }
    }
    return lines;
}

// Widest single word (for table column minimum widths).
float LongestWord(const std::vector<Piece>& pieces) {
    float best = 0.0f;
    float current = 0.0f;
    for (const auto& p : pieces) {
        if (p.isSpace || p.isBreak) {
            best = (std::max)(best, current);
            current = 0.0f;
        } else {
            current += p.width;
        }
    }
    return (std::max)(best, current);
}

float NaturalWidth(const std::vector<Piece>& pieces) {
    float best = 0.0f;
    float current = 0.0f;
    for (const auto& p : pieces) {
        if (p.isBreak) {
            best = (std::max)(best, current);
            current = 0.0f;
        } else {
            current += p.width;
        }
    }
    return (std::max)(best, current);
}

// ---------------------------------------------------------------------------
// Page writer
// ---------------------------------------------------------------------------

class PdfWriter {
public:
    PdfWriter(const PdfExportOptions& options) : m_options(options) {
        m_pageWidth = (options.pageSize == PageSize::Letter) ? 612.0f : 595.28f;
        m_pageHeight = (options.pageSize == PageSize::Letter) ? 792.0f : 841.89f;
        m_margin = options.marginMm * 2.83464567f;
        m_contentWidth = m_pageWidth - 2.0f * m_margin;
        m_top = m_pageHeight - m_margin;
        m_bottom = m_margin + (options.showPageNumbers ? 24.0f : 0.0f);
        NewPage();
    }

    struct Ctx {
        float x = 0.0f;
        float width = 0.0f;
        int listDepth = 0;
        std::vector<float> quoteBars; // x positions of active blockquote bars
        Rgb color = kTextColor;
    };

    Ctx RootContext() const {
        Ctx ctx;
        ctx.x = m_margin;
        ctx.width = m_contentWidth;
        return ctx;
    }

    void RenderBlocks(const std::vector<std::unique_ptr<Markdown::Block>>& blocks, const Ctx& ctx) {
        bool first = true;
        for (const auto& block : blocks) {
            if (!block) continue;
            RenderBlock(*block, ctx, first);
            first = false;
        }
    }

    std::vector<uint8_t> Finish() {
        AddFooters();
        return Assemble();
    }

private:
    // --- page / cursor management -------------------------------------------------

    std::ostringstream& Out() { return m_pages.back(); }

    void NewPage() {
        m_pages.emplace_back();
        m_pages.back() << std::fixed << std::setprecision(2);
        m_y = m_top;
    }

    // Moves down by `h`, drawing the bars of enclosing blockquotes alongside.
    void Advance(float h, const Ctx& ctx) {
        DrawQuoteBars(ctx, m_y, m_y - h);
        m_y -= h;
    }

    void EnsureSpace(float h) {
        if (m_y - h < m_bottom && m_y < m_top - 0.5f) {
            NewPage();
        }
    }

    void Gap(float h, const Ctx& ctx) {
        if (m_y - h < m_bottom) {
            NewPage();
            return;
        }
        Advance(h, ctx);
    }

    void DrawQuoteBars(const Ctx& ctx, float yTop, float yBottom) {
        for (float bx : ctx.quoteBars) {
            Out() << "q 0.82 0.85 0.88 rg " << bx << ' ' << yBottom << " 2.5 " << (yTop - yBottom) << " re f Q\n";
        }
    }

    void FillRect(float x, float y, float w, float h, Rgb c) {
        Out() << "q " << c.r << ' ' << c.g << ' ' << c.b << " rg " << x << ' ' << y << ' ' << w << ' ' << h << " re f Q\n";
    }

    // Circle approximated with four Bézier curves.
    void CirclePath(float cx, float cy, float r) {
        const float k = 0.5523f * r;
        Out() << (cx + r) << ' ' << cy << " m "
              << (cx + r) << ' ' << (cy + k) << ' ' << (cx + k) << ' ' << (cy + r) << ' ' << cx << ' ' << (cy + r) << " c "
              << (cx - k) << ' ' << (cy + r) << ' ' << (cx - r) << ' ' << (cy + k) << ' ' << (cx - r) << ' ' << cy << " c "
              << (cx - r) << ' ' << (cy - k) << ' ' << (cx - k) << ' ' << (cy - r) << ' ' << cx << ' ' << (cy - r) << " c "
              << (cx + k) << ' ' << (cy - r) << ' ' << (cx + r) << ' ' << (cy - k) << ' ' << (cx + r) << ' ' << cy << " c";
    }

    void StrokeLine(float x1, float y1, float x2, float y2, float width, Rgb c) {
        Out() << "q " << c.r << ' ' << c.g << ' ' << c.b << " RG " << width << " w " << x1 << ' ' << y1 << " m "
              << x2 << ' ' << y2 << " l S Q\n";
    }

    // Draws one wrapped line with its baseline at `baseline`.
    void DrawLine(const Line& line, float x, float baseline) {
        // Backgrounds for inline code first.
        float px = x;
        for (const auto& p : line.pieces) {
            if (p.code && !p.isSpace) {
                FillRect(px - 1.0f, baseline - p.size * 0.25f, p.width + 2.0f, p.size * 1.05f, {0.93f, 0.94f, 0.95f});
            }
            px += p.width;
        }

        // Text, merging consecutive pieces that share a style.
        Out() << "BT " << x << ' ' << baseline << " Td\n";
        int font = -1;
        float size = -1.0f;
        Rgb color{-1.0f, -1.0f, -1.0f};
        std::string pending;
        auto flush = [&]() {
            if (!pending.empty()) {
                Out() << '(' << EscapePdf(pending) << ") Tj\n";
                pending.clear();
            }
        };
        for (const auto& p : line.pieces) {
            if (p.font != font || p.size != size) {
                flush();
                font = p.font;
                size = p.size;
                Out() << "/F" << font << ' ' << size << " Tf\n";
            }
            if (!(p.color == color)) {
                flush();
                color = p.color;
                Out() << color.r << ' ' << color.g << ' ' << color.b << " rg\n";
            }
            pending += p.text;
        }
        flush();
        Out() << "ET\n";

        // Strikethrough decorations.
        px = x;
        for (const auto& p : line.pieces) {
            if (p.strike && !p.isSpace) {
                StrokeLine(px, baseline + p.size * 0.3f, px + p.width, baseline + p.size * 0.3f, 0.6f, p.color);
            }
            px += p.width;
        }
    }

    void RenderParagraphLines(const std::vector<Line>& lines, const Ctx& ctx, float x, const TextStyle& style,
                              int alignment = 0, float alignWidth = 0.0f) {
        for (const auto& line : lines) {
            EnsureSpace(style.leading);
            float lx = x;
            if (alignment == 1) lx += (alignWidth - line.width) * 0.5f;
            if (alignment == 2) lx += alignWidth - line.width;
            DrawLine(line, lx, m_y - style.size);
            Advance(style.leading, ctx);
        }
    }

    // --- blocks ------------------------------------------------------------------

    void RenderBlock(const Markdown::Block& block, const Ctx& ctx, bool first) {
        switch (block.type) {
        case Markdown::BlockType::Heading:       RenderHeading(block, ctx, first); break;
        case Markdown::BlockType::Paragraph:     RenderParagraph(block, ctx, first); break;
        case Markdown::BlockType::Blockquote:    RenderBlockquote(block, ctx, first); break;
        case Markdown::BlockType::List:          RenderList(block, ctx, first); break;
        case Markdown::BlockType::CodeBlock:     RenderCode(block, ctx, first); break;
        case Markdown::BlockType::ThematicBreak: RenderRule(ctx, first); break;
        case Markdown::BlockType::Table:         RenderTable(block, ctx, first); break;
        default: break;
        }
    }

    void RenderHeading(const Markdown::Block& block, const Ctx& ctx, bool first) {
        static constexpr float kSizes[6] = {20.0f, 16.0f, 13.5f, 12.0f, 10.5f, 9.5f};
        const int level = (std::clamp)(block.level, 1, 6);
        TextStyle style;
        style.font = kHelveticaBold;
        style.size = kSizes[level - 1];
        style.leading = style.size * 1.3f;
        style.color = level == 6 ? kMutedColor : kHeadingColor;

        if (!first) Gap(level <= 2 ? 14.0f : 10.0f, ctx);

        const auto lines = WrapPieces(BuildPieces(block.inlineContent, style), ctx.width);
        // Orphan prevention (F-09): the heading must be followed by some content on the same page.
        const float needed = static_cast<float>(lines.size()) * style.leading + 40.0f;
        if (m_y - needed < m_bottom && m_y < m_top - 0.5f) {
            NewPage();
        }
        RenderParagraphLines(lines, ctx, ctx.x, style);

        if (level <= 2) {
            Advance(2.0f, ctx);
            StrokeLine(ctx.x, m_y, ctx.x + ctx.width, m_y, 0.75f, {0.82f, 0.85f, 0.88f});
            Advance(4.0f, ctx);
        }
    }

    TextStyle BodyStyle(const Ctx& ctx) const {
        TextStyle style;
        style.size = 10.0f;
        style.leading = 14.5f;
        style.color = ctx.color;
        return style;
    }

    void RenderParagraph(const Markdown::Block& block, const Ctx& ctx, bool first) {
        if (block.inlineContent.empty()) return;
        if (!first) Gap(8.0f, ctx);
        const TextStyle style = BodyStyle(ctx);
        RenderParagraphLines(WrapPieces(BuildPieces(block.inlineContent, style), ctx.width), ctx, ctx.x, style);
    }

    void RenderBlockquote(const Markdown::Block& block, const Ctx& ctx, bool first) {
        if (!first) Gap(8.0f, ctx);
        Ctx inner = ctx;
        inner.quoteBars.push_back(ctx.x + 1.0f);
        inner.x = ctx.x + 14.0f;
        inner.width = ctx.width - 14.0f;
        inner.color = kMutedColor;
        RenderBlocks(block.children, inner);
    }

    void RenderList(const Markdown::Block& block, const Ctx& ctx, bool first) {
        if (!first) Gap(ctx.listDepth > 0 ? 3.0f : 8.0f, ctx);
        const TextStyle style = BodyStyle(ctx);

        float markerWidth = 16.0f;
        if (block.isOrdered) {
            const int last = block.startNumber + static_cast<int>(block.children.size()) - 1;
            markerWidth = (std::max)(markerWidth, MeasureAnsi(std::to_string(last) + ".", kHelvetica, style.size) + 6.0f);
        }

        int number = block.startNumber;
        bool firstItem = true;
        for (const auto& item : block.children) {
            if (!item || item->type != Markdown::BlockType::ListItem) continue;
            if (!firstItem) Gap(3.0f, ctx);
            firstItem = false;

            Ctx inner = ctx;
            inner.x = ctx.x + markerWidth;
            inner.width = ctx.width - markerWidth;
            inner.listDepth = ctx.listDepth + 1;

            const std::vector<Markdown::Span>* firstSpans = &item->inlineContent;
            size_t rest = 0;
            if (firstSpans->empty() && !item->children.empty() && item->children[0] &&
                item->children[0]->type == Markdown::BlockType::Paragraph) {
                firstSpans = &item->children[0]->inlineContent;
                rest = 1;
            }

            const auto lines = WrapPieces(BuildPieces(*firstSpans, style), inner.width);
            EnsureSpace(style.leading);
            const float baseline = m_y - style.size;
            DrawMarker(*item, block.isOrdered, number++, ctx, markerWidth, baseline, style);
            RenderParagraphLines(lines, ctx, inner.x, style);

            for (size_t i = rest; i < item->children.size(); ++i) {
                if (item->children[i]) {
                    RenderBlock(*item->children[i], inner, false);
                }
            }
        }
    }

    void DrawMarker(const Markdown::Block& item, bool ordered, int number, const Ctx& ctx, float markerWidth,
                    float baseline, const TextStyle& style) {
        const float midY = baseline + style.size * 0.33f;
        if (item.isTask) {
            const float box = 8.0f;
            const float bx = ctx.x + 1.0f;
            const float by = midY - box / 2.0f;
            if (item.isTaskChecked) {
                FillRect(bx, by, box, box, {0.04f, 0.41f, 0.85f});
                Out() << "q 1 1 1 RG 1.2 w 1 J 1 j " << (bx + 1.8f) << ' ' << (by + 4.2f) << " m " << (bx + 3.4f) << ' '
                      << (by + 2.2f) << " l " << (bx + 6.4f) << ' ' << (by + 6.2f) << " l S Q\n";
            } else {
                Out() << "q 0.45 0.48 0.52 RG 0.8 w " << bx << ' ' << by << ' ' << box << ' ' << box << " re S Q\n";
            }
        } else if (ordered) {
            const std::string label = std::to_string(number) + ".";
            const float w = MeasureAnsi(label, kHelvetica, style.size);
            Line line;
            Piece p;
            p.text = label;
            p.size = style.size;
            p.color = ctx.color;
            p.width = w;
            line.pieces.push_back(p);
            DrawLine(line, ctx.x + markerWidth - 5.0f - w, baseline);
        } else {
            const float cx = ctx.x + 5.0f;
            const float r = 1.8f;
            switch (ctx.listDepth % 3) {
            case 0:
                Out() << "q 0.2 0.22 0.25 rg ";
                CirclePath(cx, midY, r);
                Out() << " f Q\n";
                break;
            case 1:
                Out() << "q 0.2 0.22 0.25 RG 0.6 w ";
                CirclePath(cx, midY, r - 0.3f);
                Out() << " S Q\n";
                break;
            default:
                Out() << "q 0.2 0.22 0.25 rg " << (cx - r * 0.8f) << ' ' << (midY - r * 0.8f) << ' ' << 1.6f * r << ' '
                      << 1.6f * r << " re f Q\n";
                break;
            }
        }
    }

    // --- Mermaid diagrams (vector) --------------------------------------------------

    static Rgb ToRgb(uint32_t rgb) {
        return Rgb{static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                   static_cast<float>(rgb & 0xFF) / 255.0f};
    }

    // Closed rounded rectangle path; (x, y) is the bottom-left corner in PDF space.
    void RoundedRectPath(float x, float y, float w, float h, float r) {
        r = (std::min)({r, w * 0.5f, h * 0.5f});
        const float k = 0.5523f * r;
        auto& out = Out();
        out << (x + r) << ' ' << y << " m " << (x + w - r) << ' ' << y << " l "
            << (x + w - r + k) << ' ' << y << ' ' << (x + w) << ' ' << (y + r - k) << ' ' << (x + w) << ' ' << (y + r) << " c "
            << (x + w) << ' ' << (y + h - r) << " l "
            << (x + w) << ' ' << (y + h - r + k) << ' ' << (x + w - r + k) << ' ' << (y + h) << ' ' << (x + w - r) << ' ' << (y + h) << " c "
            << (x + r) << ' ' << (y + h) << " l "
            << (x + r - k) << ' ' << (y + h) << ' ' << x << ' ' << (y + h - r + k) << ' ' << x << ' ' << (y + h - r) << " c "
            << x << ' ' << (y + r) << " l "
            << x << ' ' << (y + r - k) << ' ' << (x + r - k) << ' ' << y << ' ' << (x + r) << ' ' << y << " c h\n";
    }

    void EllipsePath(float cx, float cy, float rx, float ry) {
        const float kx = 0.5523f * rx;
        const float ky = 0.5523f * ry;
        Out() << (cx + rx) << ' ' << cy << " m "
              << (cx + rx) << ' ' << (cy + ky) << ' ' << (cx + kx) << ' ' << (cy + ry) << ' ' << cx << ' ' << (cy + ry) << " c "
              << (cx - kx) << ' ' << (cy + ry) << ' ' << (cx - rx) << ' ' << (cy + ky) << ' ' << (cx - rx) << ' ' << cy << " c "
              << (cx - rx) << ' ' << (cy - ky) << ' ' << (cx - kx) << ' ' << (cy - ry) << ' ' << cx << ' ' << (cy - ry) << " c "
              << (cx + kx) << ' ' << (cy - ry) << ' ' << (cx + rx) << ' ' << (cy - ky) << ' ' << (cx + rx) << ' ' << cy << " c h\n";
    }

    // Draws a ```mermaid block as a vector diagram; false when the source cannot be rendered.
    bool RenderDiagram(const Markdown::Block& block, const Ctx& ctx, bool first) {
        std::string source;
        for (const auto& s : block.inlineContent) source += s.text;
        const Diagram::MermaidResult result = Diagram::RenderMermaid(
            source, [](std::string_view text, float size, bool bold) {
                return MeasureAnsi(ToWinAnsi(text), bold ? kHelveticaBold : kHelvetica, size);
            });
        if (!result.scene) return false;
        const Diagram::Scene& scene = *result.scene;

        // Diagram units are DIPs (1/96 in); 0.72 keeps labels close to the 10 pt body text.
        const float scale = (std::min)({0.72f, ctx.width / scene.width, (m_top - m_bottom) / scene.height});
        const float width = scene.width * scale;
        const float height = scene.height * scale;
        if (!first) Gap(8.0f, ctx);
        EnsureSpace(height);

        const float ox = ctx.x + (ctx.width - width) * 0.5f;
        const float oy = m_y;
        auto px = [&](float x) { return ox + x * scale; };
        auto py = [&](float y) { return oy - y * scale; };
        const Diagram::Palette palette = Diagram::Palette::Light();
        auto& out = Out();

        for (const auto& p : scene.items) {
            const bool fill = !p.fill.IsNone();
            const bool stroke = !p.stroke.IsNone();
            if (p.kind == Diagram::PrimitiveKind::Text) {
                const int font = p.bold ? kHelveticaBold : kHelvetica;
                const float size = p.fontSize * scale;
                const std::string ansi = ToWinAnsi(p.text);
                const float textWidth = MeasureAnsi(ansi, font, size);
                float x = px(p.x) - textWidth * 0.5f;
                if (p.align == Diagram::TextAlign::Left) x = px(p.x);
                if (p.align == Diagram::TextAlign::Right) x = px(p.x) - textWidth;
                const Rgb c = ToRgb(palette.Resolve(p.fill));
                out << "BT /F" << font << ' ' << size << " Tf " << c.r << ' ' << c.g << ' ' << c.b << " rg " << x << ' '
                    << (py(p.y) - size * 0.35f) << " Td (" << EscapePdf(ansi) << ") Tj ET\n";
                continue;
            }
            if (!fill && !stroke) continue;

            out << "q ";
            if (fill) {
                const Rgb c = ToRgb(palette.Resolve(p.fill));
                out << c.r << ' ' << c.g << ' ' << c.b << " rg ";
            }
            if (stroke) {
                const Rgb c = ToRgb(palette.Resolve(p.stroke));
                out << c.r << ' ' << c.g << ' ' << c.b << " RG " << (p.strokeWidth * scale) << " w 1 j ";
                if (p.lineStyle == Diagram::LineStyle::Dashed) out << "[3.5 2.8] 0 d ";
                if (p.lineStyle == Diagram::LineStyle::Dotted) out << "[1 2] 0 d ";
            }
            out << '\n';

            switch (p.kind) {
            case Diagram::PrimitiveKind::Rect:
                if (p.radius > 0.0f) {
                    RoundedRectPath(px(p.x), py(p.y + p.h), p.w * scale, p.h * scale, p.radius * scale);
                } else {
                    out << px(p.x) << ' ' << py(p.y + p.h) << ' ' << (p.w * scale) << ' ' << (p.h * scale) << " re\n";
                }
                break;
            case Diagram::PrimitiveKind::Ellipse:
                EllipsePath(px(p.x), py(p.y), p.w * scale, p.h * scale);
                break;
            case Diagram::PrimitiveKind::Path:
                out << px(p.start.x) << ' ' << py(p.start.y) << " m";
                for (const auto& seg : p.segments) {
                    out << ' ' << px(seg.c1.x) << ' ' << py(seg.c1.y) << ' ' << px(seg.c2.x) << ' ' << py(seg.c2.y) << ' '
                        << px(seg.end.x) << ' ' << py(seg.end.y) << " c";
                }
                out << (p.closed ? " h\n" : "\n");
                break;
            default:
                break;
            }
            out << (fill && stroke ? "B" : (fill ? "f" : "S")) << " Q\n";
        }

        Advance(height, ctx);
        return true;
    }

    void RenderCode(const Markdown::Block& block, const Ctx& ctx, bool first) {
        if (Diagram::IsMermaidLanguage(block.info) && RenderDiagram(block, ctx, first)) return;
        if (!first) Gap(8.0f, ctx);
        const float size = 8.8f;
        const float leading = 12.0f;
        const float pad = 7.0f;
        const float innerWidth = ctx.width - 2.0f * pad;
        const size_t maxChars = (std::max)(static_cast<size_t>(8), static_cast<size_t>(innerWidth / (0.6f * size)));

        std::string code;
        for (const auto& s : block.inlineContent) code += s.text;
        while (!code.empty() && (code.back() == '\n' || code.back() == '\r')) code.pop_back();
        const std::string ansi = ToWinAnsi(code);

        // Split into lines and hard-wrap long ones at the box width.
        std::vector<std::string> lines;
        size_t start = 0;
        while (start <= ansi.size()) {
            size_t end = ansi.find('\n', start);
            if (end == std::string::npos) end = ansi.size();
            std::string raw = ansi.substr(start, end - start);
            if (!raw.empty() && raw.back() == '\r') raw.pop_back();
            do {
                lines.push_back(raw.substr(0, maxChars));
                raw.erase(0, (std::min)(maxChars, raw.size()));
            } while (!raw.empty());
            start = end + 1;
        }

        // Draw page by page so the background follows the code across page breaks.
        size_t index = 0;
        while (index < lines.size()) {
            EnsureSpace(leading + 2.0f * pad);
            const float available = m_y - m_bottom - 2.0f * pad;
            const size_t fit = (std::max)(static_cast<size_t>(1), static_cast<size_t>(available / leading));
            const size_t count = (std::min)(fit, lines.size() - index);
            const float boxHeight = static_cast<float>(count) * leading + 2.0f * pad;

            FillRect(ctx.x, m_y - boxHeight, ctx.width, boxHeight, {0.965f, 0.97f, 0.975f});
            Out() << "q 0.84 0.86 0.89 RG 0.5 w " << ctx.x << ' ' << (m_y - boxHeight) << ' ' << ctx.width << ' '
                  << boxHeight << " re S Q\n";

            float baseline = m_y - pad - size;
            for (size_t i = 0; i < count; ++i) {
                Out() << "BT /F" << kCourier << ' ' << size << " Tf 0.15 0.16 0.18 rg " << (ctx.x + pad) << ' '
                      << baseline << " Td (" << EscapePdf(lines[index + i]) << ") Tj ET\n";
                baseline -= leading;
            }
            Advance(boxHeight, ctx);
            index += count;
            if (index < lines.size()) {
                NewPage();
            }
        }
    }

    void RenderRule(const Ctx& ctx, bool first) {
        if (!first) Gap(8.0f, ctx);
        EnsureSpace(4.0f);
        StrokeLine(ctx.x, m_y - 2.0f, ctx.x + ctx.width, m_y - 2.0f, 1.0f, {0.8f, 0.82f, 0.85f});
        Advance(4.0f, ctx);
    }

    void RenderTable(const Markdown::Block& block, const Ctx& ctx, bool first) {
        struct Row {
            const Markdown::Block* block;
            bool header;
        };
        std::vector<Row> rows;
        for (const auto& section : block.children) {
            if (!section) continue;
            if (section->type == Markdown::BlockType::TableRow) {
                rows.push_back({section.get(), false});
            } else {
                for (const auto& r : section->children) {
                    if (r && r->type == Markdown::BlockType::TableRow) {
                        rows.push_back({r.get(), section->type == Markdown::BlockType::TableHead});
                    }
                }
            }
        }
        if (rows.empty()) return;
        size_t cols = 0;
        for (const auto& r : rows) cols = (std::max)(cols, r.block->children.size());
        if (cols == 0) return;

        if (!first) Gap(8.0f, ctx);

        TextStyle body = BodyStyle(ctx);
        body.size = 9.0f;
        body.leading = 12.0f;
        TextStyle head = body;
        head.font = kHelveticaBold;
        const float padX = 5.0f;
        const float padY = 4.0f;

        // Measure cells: natural width and longest word.
        std::vector<std::vector<std::vector<Piece>>> cells(rows.size(), std::vector<std::vector<Piece>>(cols));
        std::vector<float> colMin(cols, 12.0f), colMax(cols, 12.0f);
        for (size_t r = 0; r < rows.size(); ++r) {
            for (size_t c = 0; c < cols && c < rows[r].block->children.size(); ++c) {
                const auto& cell = rows[r].block->children[c];
                if (!cell) continue;
                cells[r][c] = BuildPieces(cell->inlineContent, rows[r].header ? head : body);
                colMax[c] = (std::max)(colMax[c], NaturalWidth(cells[r][c]) + 1.0f);
                colMin[c] = (std::max)(colMin[c], (std::min)(90.0f, LongestWord(cells[r][c]) + 1.0f));
            }
        }

        // Auto column widths (same strategy as the preview): no text ever leaves its cell.
        const float available = ctx.width - static_cast<float>(cols) * 2.0f * padX;
        float sumMin = 0.0f, sumMax = 0.0f;
        for (size_t c = 0; c < cols; ++c) {
            colMax[c] = (std::max)(colMax[c], colMin[c]);
            sumMin += colMin[c];
            sumMax += colMax[c];
        }
        std::vector<float> widths(cols);
        for (size_t c = 0; c < cols; ++c) {
            if (sumMax <= available) {
                widths[c] = colMax[c];
            } else if (sumMin <= available) {
                widths[c] = colMin[c] + (colMax[c] - colMin[c]) * (available - sumMin) / (sumMax - sumMin);
            } else {
                widths[c] = available * colMin[c] / sumMin;
            }
        }
        float tableWidth = 0.0f;
        for (float w : widths) tableWidth += w + 2.0f * padX;

        const Rgb border{0.82f, 0.85f, 0.88f};
        auto drawRow = [&](size_t r, int bodyIndex) {
            std::vector<std::vector<Line>> wrapped(cols);
            size_t maxLines = 1;
            for (size_t c = 0; c < cols; ++c) {
                wrapped[c] = WrapPieces(cells[r][c], widths[c]);
                maxLines = (std::max)(maxLines, wrapped[c].size());
            }
            const TextStyle& style = rows[r].header ? head : body;
            const float rowHeight = static_cast<float>(maxLines) * style.leading + 2.0f * padY;
            const float top = m_y;

            if (rows[r].header) {
                FillRect(ctx.x, top - rowHeight, tableWidth, rowHeight, {0.95f, 0.96f, 0.97f});
            } else if (bodyIndex % 2 == 1) {
                FillRect(ctx.x, top - rowHeight, tableWidth, rowHeight, {0.98f, 0.985f, 0.99f});
            }

            float cx = ctx.x;
            for (size_t c = 0; c < cols; ++c) {
                int alignment = 0;
                if (c < rows[r].block->children.size() && rows[r].block->children[c]) {
                    const auto align = rows[r].block->children[c]->align;
                    alignment = align == Markdown::Alignment::Center ? 1 : align == Markdown::Alignment::Right ? 2 : 0;
                }
                float baseline = top - padY - style.size;
                for (const auto& line : wrapped[c]) {
                    float lx = cx + padX;
                    if (alignment == 1) lx += (widths[c] - line.width) * 0.5f;
                    if (alignment == 2) lx += widths[c] - line.width;
                    DrawLine(line, lx, baseline);
                    baseline -= style.leading;
                }
                Out() << "q " << border.r << ' ' << border.g << ' ' << border.b << " RG 0.5 w " << cx << ' '
                      << (top - rowHeight) << ' ' << (widths[c] + 2.0f * padX) << ' ' << rowHeight << " re S Q\n";
                cx += widths[c] + 2.0f * padX;
            }
            Advance(rowHeight, ctx);
        };

        auto rowHeightOf = [&](size_t r) {
            size_t maxLines = 1;
            for (size_t c = 0; c < cols; ++c) maxLines = (std::max)(maxLines, WrapPieces(cells[r][c], widths[c]).size());
            const TextStyle& style = rows[r].header ? head : body;
            return static_cast<float>(maxLines) * style.leading + 2.0f * padY;
        };

        int bodyIndex = 0;
        for (size_t r = 0; r < rows.size(); ++r) {
            if (m_y - rowHeightOf(r) < m_bottom && m_y < m_top - 0.5f) {
                NewPage();
                // Repeat the header rows on the new page.
                if (!rows[r].header) {
                    for (size_t h = 0; h < rows.size() && rows[h].header; ++h) drawRow(h, 0);
                }
            }
            drawRow(r, rows[r].header ? 0 : bodyIndex);
            if (!rows[r].header) ++bodyIndex;
        }
    }

    // --- document assembly -------------------------------------------------------

    void AddFooters() {
        const size_t totalPages = m_pages.size();
        if (!m_options.showPageNumbers) return;
        for (size_t i = 0; i < totalPages; ++i) {
            const std::string pageLabel = "Página " + std::to_string(i + 1) + " de " + std::to_string(totalPages);
            const float numWidth = MeasureAnsi(ToWinAnsi(pageLabel), kHelvetica, 8.5f);
            const float footerY = m_margin * 0.6f;
            auto& out = m_pages[i];
            out << "q 0.85 0.85 0.85 RG 0.5 w " << m_margin << ' ' << (footerY + 12.0f) << " m "
                << (m_margin + m_contentWidth) << ' ' << (footerY + 12.0f) << " l S Q\n";
            if (!m_options.title.empty()) {
                // Keep the title clear of the page number.
                std::string title = ToWinAnsi(m_options.title);
                while (!title.empty() && MeasureAnsi(title, kHelvetica, 8.5f) > m_contentWidth - numWidth - 20.0f) {
                    title.pop_back();
                }
                out << "BT /F1 8.5 Tf 0.5 0.5 0.5 rg " << m_margin << ' ' << footerY << " Td (" << EscapePdf(title)
                    << ") Tj ET\n";
            }
            out << "BT /F1 8.5 Tf 0.5 0.5 0.5 rg " << (m_margin + m_contentWidth - numWidth) << ' ' << footerY
                << " Td (" << ToPdfString(pageLabel) << ") Tj ET\n";
        }
    }

    std::vector<uint8_t> Assemble() {
        const size_t totalPages = m_pages.size();
        std::ostringstream pdf;
        pdf << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";

        std::vector<size_t> offsets(1, 0);
        auto startObject = [&](int id) {
            if (offsets.size() <= static_cast<size_t>(id)) offsets.resize(static_cast<size_t>(id) + 1, 0);
            offsets[static_cast<size_t>(id)] = static_cast<size_t>(pdf.tellp());
            pdf << id << " 0 obj\n";
        };

        startObject(1);
        pdf << "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

        startObject(2);
        pdf << "<< /Type /Pages /Kids [";
        for (size_t i = 0; i < totalPages; ++i) pdf << ' ' << (3 + static_cast<int>(i) * 2) << " 0 R";
        pdf << " ] /Count " << totalPages << " /MediaBox [ 0 0 " << std::fixed << std::setprecision(2) << m_pageWidth
            << ' ' << m_pageHeight << " ] >>\nendobj\n";

        const int fontBase = 3 + static_cast<int>(totalPages) * 2;
        for (size_t i = 0; i < totalPages; ++i) {
            const int pageId = 3 + static_cast<int>(i) * 2;
            startObject(pageId);
            pdf << "<< /Type /Page /Parent 2 0 R /Contents " << (pageId + 1) << " 0 R /Resources << /Font << ";
            for (int f = 1; f <= 6; ++f) pdf << "/F" << f << ' ' << (fontBase + f - 1) << " 0 R ";
            pdf << ">> >> >>\nendobj\n";

            const std::string stream = m_pages[i].str();
            startObject(pageId + 1);
            pdf << "<< /Length " << stream.size() << " >>\nstream\n" << stream << "\nendstream\nendobj\n";
        }

        static constexpr const char* kFontNames[6] = {"Helvetica", "Helvetica-Bold", "Helvetica-Oblique",
                                                      "Helvetica-BoldOblique", "Courier", "Courier-Bold"};
        for (int f = 0; f < 6; ++f) {
            startObject(fontBase + f);
            pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /" << kFontNames[f] << " /Encoding /WinAnsiEncoding >>\nendobj\n";
        }

        const int infoId = fontBase + 6;
        startObject(infoId);
        pdf << "<< /Title (" << ToPdfString(m_options.title) << ") /Producer (Pluma) /Creator (Pluma) >>\nendobj\n";

        const size_t xref = static_cast<size_t>(pdf.tellp());
        pdf << "xref\n0 " << offsets.size() << "\n0000000000 65535 f \n";
        for (size_t i = 1; i < offsets.size(); ++i) {
            pdf << std::setw(10) << std::setfill('0') << offsets[i] << " 00000 n \n";
        }
        pdf << "trailer\n<< /Size " << offsets.size() << " /Root 1 0 R /Info " << infoId << " 0 R >>\n";
        pdf << "startxref\n" << xref << "\n%%EOF\n";

        const std::string bytes = pdf.str();
        return std::vector<uint8_t>(bytes.begin(), bytes.end());
    }

    const PdfExportOptions& m_options;
    float m_pageWidth = 595.28f;
    float m_pageHeight = 841.89f;
    float m_margin = 56.7f;
    float m_contentWidth = 0.0f;
    float m_top = 0.0f;
    float m_bottom = 0.0f;
    float m_y = 0.0f;
    std::vector<std::ostringstream> m_pages;
};

} // namespace

std::vector<uint8_t> PdfExporter::ExportToBytes(const Markdown::BlockTree& tree, const PdfExportOptions& options) {
    if (!tree.root) {
        return {};
    }
    PdfWriter writer(options);
    writer.RenderBlocks(tree.root->children, writer.RootContext());
    return writer.Finish();
}

std::vector<uint8_t> PdfExporter::ExportMarkdownToBytes(std::string_view markdown, const PdfExportOptions& options) {
    auto tree = Markdown::Md4cAdapter::Parse(markdown);
    if (!tree) {
        return {};
    }
    return ExportToBytes(*tree, options);
}

bool PdfExporter::ExportToFile(const std::filesystem::path& path, const Markdown::BlockTree& tree, const PdfExportOptions& options) {
    auto bytes = ExportToBytes(tree, options);
    if (bytes.empty()) {
        return false;
    }

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return ofs.good();
}

bool PdfExporter::ExportMarkdownToFile(const std::filesystem::path& path, std::string_view markdown, const PdfExportOptions& options) {
    auto tree = Markdown::Md4cAdapter::Parse(markdown);
    if (!tree) {
        return false;
    }
    return ExportToFile(path, *tree, options);
}

} // namespace Pluma::Export
