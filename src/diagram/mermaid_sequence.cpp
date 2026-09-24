#include "mermaid_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

namespace Pluma::Diagram::detail {

namespace {

constexpr float kFont = 14.0f;
constexpr float kNoteFont = 13.0f;
constexpr float kBlockFont = 12.5f;
constexpr float kActorGap = 44.0f;
constexpr float kMargin = 10.0f;
constexpr float kActivationWidth = 10.0f;
constexpr float kSelfLoopWidth = 34.0f;
constexpr float kMaxNoteWidth = 240.0f;

enum class Head { None, Arrow, Open, Cross };

struct Participant {
    std::string id;
    std::vector<std::string> lines;
    bool actor = false;
    float width = 0.0f;
    float height = 0.0f;
    float x = 0.0f;
};

enum class EventKind { Message, Note, BlockStart, BlockElse, BlockEnd, Activate, Deactivate };

struct Event {
    EventKind kind = EventKind::Message;
    int from = -1;
    int to = -1;
    std::vector<std::string> lines;
    bool dotted = false;
    Head head = Head::Arrow;
    bool bidirectional = false;
    bool activateTarget = false;
    bool deactivateSource = false;
    // Notes
    enum class Placement { Left, Right, Over } placement = Placement::Over;
    // Blocks
    std::string keyword;
    std::optional<uint32_t> rectColor;
};

struct ArrowToken {
    std::string_view text;
    bool dotted;
    Head head;
    bool bidirectional;
};

// Longest first so that "-->>" is not read as "-->".
constexpr ArrowToken kArrows[] = {
    {"<<-->>", true, Head::Arrow, true}, {"<<->>", false, Head::Arrow, true},
    {"-->>", true, Head::Arrow, false},  {"->>", false, Head::Arrow, false},
    {"--x", true, Head::Cross, false},   {"-x", false, Head::Cross, false},
    {"--)", true, Head::Open, false},    {"-)", false, Head::Open, false},
    {"-->", true, Head::None, false},    {"->", false, Head::None, false},
};

class SequenceBuilder {
public:
    SequenceBuilder(const MeasureText& measure) : m_measure(measure) {}

    bool Parse(const std::vector<std::string_view>& lines, std::string& error) {
        int depth = 0;
        std::vector<bool> blockIsBox; // `box` groups are accepted but not drawn
        for (size_t l = 1; l < lines.size(); ++l) {
            std::string_view line = lines[l];
            const std::string lower = ToLower(line);

            if (StartsWithKeyword(line, "participant") || StartsWithKeyword(line, "actor")) {
                const bool actor = StartsWithKeyword(line, "actor");
                DeclareParticipant(Trim(line.substr(actor ? 5 : 11)), actor);
                continue;
            }
            if (StartsWithKeyword(line, "create")) {
                line = Trim(line.substr(6));
                const bool actor = StartsWithKeyword(line, "actor");
                DeclareParticipant(Trim(line.substr(actor ? 5 : 11)), actor);
                continue;
            }
            if (StartsWithKeyword(line, "destroy") || StartsWithKeyword(line, "link") ||
                StartsWithKeyword(line, "links") || StartsWithKeyword(line, "properties") ||
                StartsWithKeyword(line, "details") || StartsWithKeyword(line, "accTitle") ||
                StartsWithKeyword(line, "accDescr")) {
                continue;
            }
            if (StartsWithKeyword(line, "autonumber")) {
                m_autonumber = !StartsWithKeyword(Trim(line.substr(10)), "off");
                continue;
            }
            if (StartsWithKeyword(line, "title")) {
                std::string_view rest = Trim(line.substr(5));
                if (!rest.empty() && rest.front() == ':') rest = Trim(rest.substr(1));
                m_title = CleanLabel(rest);
                continue;
            }
            if (StartsWithKeyword(line, "activate") || StartsWithKeyword(line, "deactivate")) {
                const bool activate = StartsWithKeyword(line, "activate");
                Event e;
                e.kind = activate ? EventKind::Activate : EventKind::Deactivate;
                e.from = ParticipantIndex(std::string(Trim(line.substr(activate ? 8 : 10))));
                m_events.push_back(std::move(e));
                continue;
            }
            if (StartsWithKeyword(lower, "note")) {
                if (!ParseNote(line, error)) return false;
                continue;
            }
            if (StartsWithKeyword(line, "box")) {
                blockIsBox.push_back(true);
                continue;
            }
            static constexpr std::string_view kBlocks[] = {"loop", "alt", "opt", "par", "critical", "break", "rect"};
            bool isBlock = false;
            for (std::string_view kw : kBlocks) {
                if (StartsWithKeyword(line, kw)) {
                    Event e;
                    e.kind = EventKind::BlockStart;
                    e.keyword = std::string(kw);
                    const std::string_view rest = Trim(line.substr(kw.size()));
                    if (kw == "rect") {
                        e.rectColor = ParseColor(rest);
                    } else if (!rest.empty()) {
                        e.lines = {CleanLabel(rest)};
                    }
                    m_events.push_back(std::move(e));
                    blockIsBox.push_back(false);
                    ++depth;
                    isBlock = true;
                    break;
                }
            }
            if (isBlock) continue;
            if (StartsWithKeyword(line, "else") || StartsWithKeyword(line, "and") || StartsWithKeyword(line, "option")) {
                const size_t kwLen = StartsWithKeyword(line, "else") ? 4 : (StartsWithKeyword(line, "and") ? 3 : 6);
                Event e;
                e.kind = EventKind::BlockElse;
                const std::string_view rest = Trim(line.substr(kwLen));
                if (!rest.empty()) e.lines = {CleanLabel(rest)};
                m_events.push_back(std::move(e));
                continue;
            }
            if (line == "end") {
                if (blockIsBox.empty()) continue;
                const bool box = blockIsBox.back();
                blockIsBox.pop_back();
                if (!box) {
                    Event e;
                    e.kind = EventKind::BlockEnd;
                    m_events.push_back(std::move(e));
                    --depth;
                }
                continue;
            }
            if (!ParseMessage(line, error)) return false;
        }
        while (depth-- > 0) { // Unclosed blocks end with the diagram
            Event e;
            e.kind = EventKind::BlockEnd;
            m_events.push_back(std::move(e));
        }
        if (m_participants.empty()) {
            error = "El diagrama de secuencia no tiene participantes";
            return false;
        }
        return true;
    }

    Scene Build();

private:
    int ParticipantIndex(const std::string& id, bool actor = false) {
        auto it = m_index.find(id);
        if (it != m_index.end()) return it->second;
        Participant p;
        p.id = id;
        p.actor = actor;
        p.lines = {id};
        m_participants.push_back(std::move(p));
        const int index = static_cast<int>(m_participants.size()) - 1;
        m_index.emplace(id, index);
        return index;
    }

    void DeclareParticipant(std::string_view rest, bool actor) {
        std::string id;
        std::string label;
        const size_t as = rest.find(" as ");
        if (as != std::string_view::npos) {
            id = std::string(Trim(rest.substr(0, as)));
            label = CleanLabel(rest.substr(as + 4));
        } else {
            id = CleanLabel(rest);
        }
        if (id.size() >= 2 && id.front() == '"' && id.back() == '"') id = id.substr(1, id.size() - 2);
        const int index = ParticipantIndex(id, actor);
        auto& p = m_participants[static_cast<size_t>(index)];
        p.actor = actor;
        if (!label.empty()) {
            p.lines = WrapLabel(label, m_measure, kFont, false, 1000.0f);
        }
    }

    bool ParseNote(std::string_view line, std::string& error) {
        std::string_view rest = Trim(line.substr(4));
        const size_t colon = rest.find(':');
        if (colon == std::string_view::npos) {
            error = "Nota sin texto (falta ':'): " + std::string(line);
            return false;
        }
        std::string_view head = Trim(rest.substr(0, colon));
        Event e;
        e.kind = EventKind::Note;
        const std::string lowerHead = ToLower(head);
        std::string_view targets;
        if (StartsWith(lowerHead, "left of")) {
            e.placement = Event::Placement::Left;
            targets = Trim(head.substr(7));
        } else if (StartsWith(lowerHead, "right of")) {
            e.placement = Event::Placement::Right;
            targets = Trim(head.substr(8));
        } else if (StartsWith(lowerHead, "over")) {
            e.placement = Event::Placement::Over;
            targets = Trim(head.substr(4));
        } else {
            error = "Posición de nota desconocida: " + std::string(line);
            return false;
        }
        const size_t comma = targets.find(',');
        e.from = ParticipantIndex(std::string(Trim(targets.substr(0, comma))));
        e.to = comma == std::string_view::npos ? e.from : ParticipantIndex(std::string(Trim(targets.substr(comma + 1))));
        if (e.to < e.from) std::swap(e.from, e.to);
        e.lines = WrapLabel(CleanLabel(rest.substr(colon + 1)), m_measure, kNoteFont, false, kMaxNoteWidth);
        m_events.push_back(std::move(e));
        return true;
    }

    bool ParseMessage(std::string_view line, std::string& error) {
        // Participant ids cannot contain '-', '<', '+' or ':' (as in Mermaid), so the arrow starts at the first of them.
        size_t pos = std::string_view::npos;
        const ArrowToken* token = nullptr;
        for (size_t i = 1; i < line.size() && !token; ++i) {
            if (line[i] != '-' && line[i] != '<') continue;
            for (const auto& t : kArrows) {
                if (line.substr(i, t.text.size()) == t.text) {
                    token = &t;
                    pos = i;
                    break;
                }
            }
        }
        if (!token) {
            error = "Sintaxis no reconocida: " + std::string(line);
            return false;
        }
        Event e;
        e.kind = EventKind::Message;
        e.dotted = token->dotted;
        e.head = token->head;
        e.bidirectional = token->bidirectional;

        std::string_view rest = line.substr(pos + token->text.size());
        const size_t colon = rest.find(':');
        std::string_view target = Trim(colon == std::string_view::npos ? rest : rest.substr(0, colon));
        if (!target.empty() && target.front() == '+') {
            e.activateTarget = true;
            target = Trim(target.substr(1));
        } else if (!target.empty() && target.front() == '-') {
            e.deactivateSource = true;
            target = Trim(target.substr(1));
        }
        const std::string_view source = Trim(line.substr(0, pos));
        if (source.empty() || target.empty()) {
            error = "Mensaje incompleto: " + std::string(line);
            return false;
        }
        e.from = ParticipantIndex(std::string(source));
        e.to = ParticipantIndex(std::string(target));
        if (colon != std::string_view::npos) {
            e.lines = WrapLabel(CleanLabel(rest.substr(colon + 1)), m_measure, kFont, false, 1000.0f);
        }
        m_events.push_back(std::move(e));
        return true;
    }

    const MeasureText& m_measure;
    std::vector<Participant> m_participants;
    std::map<std::string, int> m_index;
    std::vector<Event> m_events;
    bool m_autonumber = false;
    std::string m_title;
};

struct Constraint {
    int left;
    int right;
    float distance;
};

Scene SequenceBuilder::Build() {
    const float lineH = LineHeight(kFont);
    const float noteLineH = LineHeight(kNoteFont);
    const size_t n = m_participants.size();

    // Participant boxes.
    float boxHeight = 0.0f;
    for (auto& p : m_participants) {
        const float tw = WidestLine(p.lines, m_measure, kFont, false);
        p.width = (std::max)(tw + 24.0f, 84.0f);
        p.height = static_cast<float>(p.lines.size()) * lineH + 18.0f;
        if (p.actor) p.height += 38.0f; // Stick figure above the name
        boxHeight = (std::max)(boxHeight, p.height);
    }

    // Horizontal spacing constraints.
    std::vector<Constraint> constraints;
    auto widthOf = [&](int i) { return m_participants[static_cast<size_t>(i)].width; };
    for (size_t i = 1; i < n; ++i) {
        constraints.push_back({static_cast<int>(i) - 1, static_cast<int>(i),
                               (widthOf(static_cast<int>(i) - 1) + widthOf(static_cast<int>(i))) * 0.5f + kActorGap});
    }
    for (const auto& e : m_events) {
        const float tw = WidestLine(e.lines, m_measure, e.kind == EventKind::Note ? kNoteFont : kFont, false);
        if (e.kind == EventKind::Message) {
            const float numberRoom = m_autonumber ? 20.0f : 0.0f;
            if (e.from == e.to) {
                const float need = kSelfLoopWidth + tw + 16.0f + numberRoom;
                if (e.from + 1 < static_cast<int>(n)) {
                    constraints.push_back({e.from, e.from + 1, need + widthOf(e.from + 1) * 0.5f});
                }
            } else {
                constraints.push_back({(std::min)(e.from, e.to), (std::max)(e.from, e.to), tw + 28.0f + numberRoom});
            }
        } else if (e.kind == EventKind::Note) {
            const float w = tw + 20.0f;
            if (e.placement == Event::Placement::Right) {
                if (e.from + 1 < static_cast<int>(n)) {
                    constraints.push_back({e.from, e.from + 1, w + 18.0f + widthOf(e.from + 1) * 0.5f});
                }
            } else if (e.placement == Event::Placement::Left) {
                if (e.from > 0) {
                    constraints.push_back({e.from - 1, e.from, w + 18.0f + widthOf(e.from - 1) * 0.5f});
                }
            } else if (e.from != e.to) {
                constraints.push_back({e.from, e.to, w - 40.0f});
            } else {
                const float half = w * 0.5f;
                if (e.from > 0) constraints.push_back({e.from - 1, e.from, half + widthOf(e.from - 1) * 0.5f + 10.0f});
                if (e.from + 1 < static_cast<int>(n)) {
                    constraints.push_back({e.from, e.from + 1, half + widthOf(e.from + 1) * 0.5f + 10.0f});
                }
            }
        }
    }

    m_participants[0].x = widthOf(0) * 0.5f; // Final placement comes from FitSceneToContent
    for (size_t k = 1; k < n; ++k) {
        float x = m_participants[k - 1].x;
        for (const auto& c : constraints) {
            if (c.right == static_cast<int>(k)) x = (std::max)(x, m_participants[static_cast<size_t>(c.left)].x + c.distance);
        }
        m_participants[k].x = x;
    }

    Scene back;       // Lifelines, frames
    Scene middle;     // Activation boxes
    Scene front;      // Messages, notes, labels
    SceneBuilder bb(back);
    SceneBuilder mb(middle);
    SceneBuilder fb(front);

    float y = kMargin;
    const float actorTop = y;
    y += boxHeight + 22.0f;

    struct Activation {
        float start = 0.0f;
        int level = 0;
    };
    std::vector<std::vector<Activation>> active(n);
    auto activate = [&](int p, float at) {
        auto& stack = active[static_cast<size_t>(p)];
        stack.push_back({at, static_cast<int>(stack.size())});
    };
    auto deactivate = [&](int p, float at) {
        auto& stack = active[static_cast<size_t>(p)];
        if (stack.empty()) return;
        const Activation a = stack.back();
        stack.pop_back();
        const float x = m_participants[static_cast<size_t>(p)].x - kActivationWidth * 0.5f + static_cast<float>(a.level) * 4.0f;
        mb.Rect(x, a.start, kActivationWidth, (std::max)(8.0f, at - a.start), Paint::Of(Role::ActivationFill),
                Paint::Of(Role::ActorStroke), 1.0f);
    };
    auto edgeX = [&](int p, bool towardsRight) {
        const auto& stack = active[static_cast<size_t>(p)];
        const float x = m_participants[static_cast<size_t>(p)].x;
        if (stack.empty()) return x;
        const float offset = static_cast<float>(stack.size() - 1) * 4.0f + kActivationWidth * 0.5f;
        return towardsRight ? x + offset : x - kActivationWidth * 0.5f;
    };

    struct Block {
        size_t event = 0;
        float top = 0.0f;
        float bottom = 0.0f;
        float minX = 1.0e9f;
        float maxX = -1.0e9f;
        int parent = -1;
        int innerDepth = 0;
        std::vector<std::pair<float, const std::vector<std::string>*>> sections; // "else" separators
    };
    std::vector<Block> blocks;
    std::vector<int> openBlocks;
    auto touchX = [&](float x0, float x1) {
        for (int b : openBlocks) {
            blocks[static_cast<size_t>(b)].minX = (std::min)(blocks[static_cast<size_t>(b)].minX, x0);
            blocks[static_cast<size_t>(b)].maxX = (std::max)(blocks[static_cast<size_t>(b)].maxX, x1);
        }
    };

    float lastArrowY = y;
    int number = 0;
    for (size_t i = 0; i < m_events.size(); ++i) {
        const Event& e = m_events[i];
        switch (e.kind) {
        case EventKind::Message: {
            const Paint line = Paint::Of(Role::Line);
            const LineStyle style = e.dotted ? LineStyle::Dashed : LineStyle::Solid;
            const float textH = static_cast<float>(e.lines.size()) * lineH;
            const float textW = WidestLine(e.lines, m_measure, kFont, false);
            const float px = m_participants[static_cast<size_t>(e.from)].x;
            if (e.from == e.to) {
                y += textH;
                const float top = y + 4.0f;
                const float x0 = edgeX(e.from, true);
                const float loopH = 24.0f;
                float ty = top - textH + lineH * 0.5f - 4.0f;
                for (const auto& l : e.lines) {
                    fb.Text(l, x0 + 6.0f, ty, kFont, Paint::Of(Role::Text), false, TextAlign::Left);
                    ty += lineH;
                }
                const Point p0{x0, top};
                const Point p3{x0 + 1.0f, top + loopH};
                fb.Path(p0, {Segment{{x0 + kSelfLoopWidth, top - 4.0f}, {x0 + kSelfLoopWidth, top + loopH + 4.0f}, p3}},
                        Paint{}, line, 1.4f).lineStyle = style;
                if (e.head == Head::Arrow) fb.ArrowHead({x0 + 12.0f, top + loopH + 2.0f}, p3, 8.0f, line);
                touchX(px, x0 + (std::max)(kSelfLoopWidth, textW + 6.0f));
                lastArrowY = top + loopH;
                if (e.activateTarget) activate(e.to, lastArrowY);
                if (e.deactivateSource) deactivate(e.from, lastArrowY);
                if (m_autonumber) {
                    ++number;
                    fb.Ellipse(x0, top, 8.0f, 8.0f, Paint::Of(Role::Marker), Paint{}, 1.0f);
                    fb.Text(std::to_string(number), x0, top, 10.0f, Paint::Of(Role::Background), true);
                }
                y = top + loopH + 18.0f;
                break;
            }

            y += textH;
            const float arrowY = y + 6.0f;
            const bool rightwards = m_participants[static_cast<size_t>(e.to)].x > px;
            if (e.activateTarget) activate(e.to, arrowY);
            const float x0 = edgeX(e.from, rightwards);
            const float x1 = edgeX(e.to, !rightwards);
            const float mid = (x0 + x1) * 0.5f;
            float ty = arrowY - 4.0f - textH + lineH * 0.5f;
            for (const auto& l : e.lines) {
                fb.Text(l, mid, ty, kFont, Paint::Of(Role::Text));
                ty += lineH;
            }
            const float dir = rightwards ? 1.0f : -1.0f;
            const float headLen = (e.head == Head::Arrow) ? 8.0f : 0.0f;
            const float startInset = e.bidirectional ? headLen : 0.0f;
            fb.Line({x0 + dir * startInset, arrowY}, {x1 - dir * headLen, arrowY}, line, 1.4f, style);
            auto head = [&](float tipX, float d) {
                const Point tip{tipX, arrowY};
                switch (e.head) {
                case Head::Arrow:
                    fb.ArrowHead({tipX - d * 8.0f, arrowY}, tip, 9.0f, line);
                    break;
                case Head::Open:
                    fb.Line({tipX - d * 8.0f, arrowY - 5.0f}, tip, line, 1.4f);
                    fb.Line({tipX - d * 8.0f, arrowY + 5.0f}, tip, line, 1.4f);
                    break;
                case Head::Cross: {
                    const float cx = tipX - d * 5.0f;
                    fb.Line({cx - 4.5f, arrowY - 4.5f}, {cx + 4.5f, arrowY + 4.5f}, line, 1.8f);
                    fb.Line({cx - 4.5f, arrowY + 4.5f}, {cx + 4.5f, arrowY - 4.5f}, line, 1.8f);
                    break;
                }
                default:
                    break;
                }
            };
            head(x1, dir);
            if (e.bidirectional) head(x0, -dir);
            if (m_autonumber) {
                ++number;
                fb.Ellipse(x0, arrowY, 8.0f, 8.0f, Paint::Of(Role::Marker), Paint{}, 1.0f);
                fb.Text(std::to_string(number), x0, arrowY, 10.0f, Paint::Of(Role::Background), true);
            }
            touchX((std::min)(px, m_participants[static_cast<size_t>(e.to)].x), (std::max)(px, m_participants[static_cast<size_t>(e.to)].x));
            touchX(mid - textW * 0.5f, mid + textW * 0.5f);
            if (e.deactivateSource) deactivate(e.from, arrowY);
            lastArrowY = arrowY;
            y = arrowY + 20.0f;
            break;
        }
        case EventKind::Note: {
            const float tw = WidestLine(e.lines, m_measure, kNoteFont, false);
            const float h = static_cast<float>(e.lines.size()) * noteLineH + 12.0f;
            float w = tw + 20.0f;
            const float xa = m_participants[static_cast<size_t>(e.from)].x;
            const float xb = m_participants[static_cast<size_t>(e.to)].x;
            float left = 0.0f;
            if (e.placement == Event::Placement::Right) {
                left = xa + 12.0f;
            } else if (e.placement == Event::Placement::Left) {
                left = xa - 12.0f - w;
            } else if (e.from != e.to) {
                w = (std::max)(w, xb - xa + 50.0f);
                left = (xa + xb) * 0.5f - w * 0.5f;
            } else {
                left = xa - w * 0.5f;
            }
            y += 4.0f;
            fb.Rect(left, y, w, h, Paint::Of(Role::NoteFill), Paint::Of(Role::NoteStroke), 1.0f, 2.0f);
            float ty = y + 6.0f + noteLineH * 0.5f;
            for (const auto& l : e.lines) {
                fb.Text(l, left + w * 0.5f, ty, kNoteFont, Paint::Of(Role::Text));
                ty += noteLineH;
            }
            touchX(left, left + w);
            y += h + 14.0f;
            break;
        }
        case EventKind::BlockStart: {
            Block b;
            b.event = i;
            b.parent = openBlocks.empty() ? -1 : openBlocks.back();
            y += 6.0f;
            b.top = y;
            blocks.push_back(std::move(b));
            openBlocks.push_back(static_cast<int>(blocks.size()) - 1);
            y += (e.keyword == "rect") ? 8.0f : LineHeight(kBlockFont) + 16.0f;
            break;
        }
        case EventKind::BlockElse:
            if (!openBlocks.empty()) {
                y += 6.0f;
                blocks[static_cast<size_t>(openBlocks.back())].sections.emplace_back(y, &e.lines);
                y += LineHeight(kBlockFont) + 12.0f;
            }
            break;
        case EventKind::BlockEnd:
            if (!openBlocks.empty()) {
                y += 4.0f;
                blocks[static_cast<size_t>(openBlocks.back())].bottom = y;
                openBlocks.pop_back();
                y += 10.0f;
            }
            break;
        case EventKind::Activate:
            activate(e.from, (std::max)(lastArrowY, y - 20.0f));
            break;
        case EventKind::Deactivate:
            deactivate(e.from, (std::max)(lastArrowY, y - 20.0f));
            break;
        }
    }

    const float lifelineEnd = y + 6.0f;
    for (size_t p = 0; p < n; ++p) {
        while (!active[p].empty()) deactivate(static_cast<int>(p), lifelineEnd);
    }

    // Frames of loop/alt/opt... blocks: nested frames get inset margins.
    for (size_t b = blocks.size(); b-- > 0;) {
        const int parent = blocks[b].parent;
        if (parent >= 0) {
            Block& pb = blocks[static_cast<size_t>(parent)];
            pb.innerDepth = (std::max)(pb.innerDepth, blocks[b].innerDepth + 1);
            pb.minX = (std::min)(pb.minX, blocks[b].minX);
            pb.maxX = (std::max)(pb.maxX, blocks[b].maxX);
        }
    }
    const float allLeft = m_participants.front().x - widthOf(0) * 0.5f;
    const float allRight = m_participants.back().x + widthOf(static_cast<int>(n) - 1) * 0.5f;
    for (const Block& b : blocks) {
        const Event& start = m_events[b.event];
        // Wide enough for the label tab to clear the activation bars on the lifeline.
        const float margin = 36.0f + 8.0f * static_cast<float>(b.innerDepth);
        float left = b.minX <= b.maxX ? b.minX - margin : allLeft;
        float right = b.minX <= b.maxX ? b.maxX + margin : allRight;
        if (start.keyword == "rect") {
            const Paint fill = start.rectColor ? Paint::Rgb(*start.rectColor) : Paint::Of(Role::ClusterFill);
            bb.Rect(left, b.top, right - left, b.bottom - b.top, fill, Paint{}, 1.0f, 0.0f);
            continue;
        }
        const float kwWidth = m_measure(start.keyword, kBlockFont, true) + 18.0f;
        const float condWidth = start.lines.empty() ? 0.0f : m_measure("[" + start.lines[0] + "]", kBlockFont, false);
        right = (std::max)(right, left + kwWidth + condWidth + 24.0f);
        const Paint stroke = Paint::Of(Role::BlockStroke);
        bb.Rect(left, b.top, right - left, b.bottom - b.top, Paint{}, stroke, 1.0f, 0.0f);
        const float tabH = LineHeight(kBlockFont) + 6.0f;
        bb.Polygon({{left, b.top}, {left + kwWidth, b.top}, {left + kwWidth, b.top + tabH - 6.0f},
                    {left + kwWidth - 6.0f, b.top + tabH}, {left, b.top + tabH}, {left, b.top}},
                   Paint::Of(Role::BlockLabelFill), stroke, 1.0f);
        bb.Text(start.keyword, left + kwWidth * 0.5f - 3.0f, b.top + tabH * 0.5f, kBlockFont, Paint::Of(Role::Text), true);
        if (!start.lines.empty()) {
            bb.Text("[" + start.lines[0] + "]", left + kwWidth + 10.0f, b.top + tabH * 0.5f, kBlockFont,
                    Paint::Of(Role::MutedText), false, TextAlign::Left);
        }
        for (const auto& [sy, lines] : b.sections) {
            bb.Line({left, sy}, {right, sy}, stroke, 1.0f, LineStyle::Dashed);
            if (lines && !lines->empty()) {
                bb.Text("[" + lines->front() + "]", (left + right) * 0.5f, sy + 6.0f + LineHeight(kBlockFont) * 0.5f,
                        kBlockFont, Paint::Of(Role::MutedText));
            }
        }
    }

    // Lifelines and participants (top and bottom).
    const float bottomTop = lifelineEnd + 4.0f;
    Scene actors;
    SceneBuilder ab(actors);
    for (const auto& p : m_participants) {
        bb.Line({p.x, actorTop + p.height}, {p.x, bottomTop}, Paint::Of(Role::BlockStroke), 1.0f, LineStyle::Dashed);
        for (float top : {actorTop, bottomTop}) {
            const float textBlock = static_cast<float>(p.lines.size()) * lineH;
            if (p.actor) {
                const float cx = p.x;
                const float headY = top + 8.0f;
                const Paint stroke = Paint::Of(Role::ActorStroke);
                ab.Ellipse(cx, headY, 7.0f, 7.0f, Paint::Of(Role::ActorFill), stroke, 1.5f);
                ab.Line({cx, headY + 7.0f}, {cx, headY + 22.0f}, stroke, 1.5f);
                ab.Line({cx - 12.0f, headY + 12.0f}, {cx + 12.0f, headY + 12.0f}, stroke, 1.5f);
                ab.Line({cx, headY + 22.0f}, {cx - 10.0f, headY + 34.0f}, stroke, 1.5f);
                ab.Line({cx, headY + 22.0f}, {cx + 10.0f, headY + 34.0f}, stroke, 1.5f);
                float ty = top + 44.0f + lineH * 0.5f;
                for (const auto& l : p.lines) {
                    ab.Text(l, cx, ty, kFont, Paint::Of(Role::Text));
                    ty += lineH;
                }
            } else {
                ab.Rect(p.x - p.width * 0.5f, top, p.width, p.height, Paint::Of(Role::ActorFill),
                        Paint::Of(Role::ActorStroke), 1.3f, 3.0f);
                float ty = top + (p.height - textBlock) * 0.5f + lineH * 0.5f;
                for (const auto& l : p.lines) {
                    ab.Text(l, p.x, ty, kFont, Paint::Of(Role::Text));
                    ty += lineH;
                }
            }
        }
    }

    Scene scene;
    for (Scene* part : {&back, &middle, &front, &actors}) {
        for (auto& item : part->items) scene.items.push_back(std::move(item));
    }
    FitSceneToContent(scene, m_measure, kMargin);
    AddSceneTitle(scene, m_title, m_measure, kMargin);
    return scene;
}

} // namespace

bool BuildSequenceScene(const std::vector<std::string_view>& lines, const MeasureText& measure, Scene& scene,
                        std::string& error) {
    SequenceBuilder builder(measure);
    if (!builder.Parse(lines, error)) return false;
    scene = builder.Build();
    return true;
}

} // namespace Pluma::Diagram::detail
