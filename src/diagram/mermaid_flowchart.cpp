#include "mermaid_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <set>

namespace Pluma::Diagram::detail {

namespace {

bool IsIdChar(char c) {
    const auto u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '_' || u >= 0x80;
}

bool IsLinkBodyChar(char c) {
    return c == '-' || c == '=' || c == '.' || c == '~';
}

std::vector<std::string_view> SplitList(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(separator, start);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view part = Trim(text.substr(start, end - start));
        if (!part.empty()) parts.push_back(part);
        start = end + 1;
    }
    return parts;
}

// First whitespace-separated token and the remainder.
std::pair<std::string_view, std::string_view> SplitFirstToken(std::string_view text) {
    text = Trim(text);
    size_t end = 0;
    while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end]))) ++end;
    return {text.substr(0, end), Trim(text.substr(end))};
}

// Splits a line into statements at ';' outside quotes and brackets.
std::vector<std::string_view> SplitStatements(std::string_view line) {
    std::vector<std::string_view> out;
    int depth = 0;
    bool quoted = false;
    size_t start = 0;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') quoted = !quoted;
        if (quoted) continue;
        if (c == '[' || c == '(' || c == '{') ++depth;
        if ((c == ']' || c == ')' || c == '}') && depth > 0) --depth;
        if (c == ';' && depth == 0) {
            const std::string_view part = Trim(line.substr(start, i - start));
            if (!part.empty()) out.push_back(part);
            start = i + 1;
        }
    }
    const std::string_view part = Trim(line.substr(start));
    if (!part.empty()) out.push_back(part);
    return out;
}

std::optional<Direction> ParseDirection(std::string_view token) {
    const std::string d = ToLower(Trim(token));
    if (d == "td" || d == "tb" || d == "v") return Direction::TB;
    if (d == "bt" || d == "^") return Direction::BT;
    if (d == "lr" || d == ">") return Direction::LR;
    if (d == "rl" || d == "<") return Direction::RL;
    return std::nullopt;
}

struct Cursor {
    std::string_view s;
    size_t p = 0;

    bool AtEnd() const { return p >= s.size(); }
    char Peek(size_t k = 0) const { return p + k < s.size() ? s[p + k] : '\0'; }
    void SkipSpaces() {
        while (!AtEnd() && (s[p] == ' ' || s[p] == '\t')) ++p;
    }
    bool Consume(std::string_view token) {
        if (s.substr(p, token.size()) == token) {
            p += token.size();
            return true;
        }
        return false;
    }
};

struct LinkInfo {
    EdgeStroke stroke = EdgeStroke::Normal;
    ArrowKind startArrow = ArrowKind::None;
    ArrowKind endArrow = ArrowKind::None;
    int minLen = 1;
    std::string label;
};

struct ShapeOpener {
    std::string_view open;
    std::string_view close;
    NodeShape shape;
};

constexpr ShapeOpener kOpeners[] = {
    {"(((", ")))", NodeShape::DoubleCircle},
    {"((", "))", NodeShape::Circle},
    {"([", "])", NodeShape::Stadium},
    {"(", ")", NodeShape::Round},
    {"[[", "]]", NodeShape::Subroutine},
    {"[(", ")]", NodeShape::Cylinder},
    {"[/", "", NodeShape::Parallelogram},     // Closing decides: /] or \]
    {"[\\", "", NodeShape::ParallelogramAlt}, // Closing decides: \] or /]
    {"[", "]", NodeShape::Rect},
    {"{{", "}}", NodeShape::Hexagon},
    {"{", "}", NodeShape::Diamond},
    {">", "]", NodeShape::Asymmetric},
};

std::optional<NodeShape> ShapeFromName(std::string_view name) {
    static constexpr std::pair<std::string_view, NodeShape> kNames[] = {
        {"rect", NodeShape::Rect}, {"proc", NodeShape::Rect}, {"process", NodeShape::Rect},
        {"rounded", NodeShape::Round}, {"event", NodeShape::Round}, {"stadium", NodeShape::Stadium},
        {"pill", NodeShape::Stadium}, {"terminal", NodeShape::Stadium}, {"subroutine", NodeShape::Subroutine},
        {"fr-rect", NodeShape::Subroutine}, {"subproc", NodeShape::Subroutine}, {"cyl", NodeShape::Cylinder},
        {"cylinder", NodeShape::Cylinder}, {"db", NodeShape::Cylinder}, {"database", NodeShape::Cylinder},
        {"circle", NodeShape::Circle}, {"circ", NodeShape::Circle}, {"dbl-circ", NodeShape::DoubleCircle},
        {"double-circle", NodeShape::DoubleCircle}, {"diamond", NodeShape::Diamond}, {"diam", NodeShape::Diamond},
        {"decision", NodeShape::Diamond}, {"question", NodeShape::Diamond}, {"hex", NodeShape::Hexagon},
        {"hexagon", NodeShape::Hexagon}, {"prepare", NodeShape::Hexagon}, {"lean-r", NodeShape::Parallelogram},
        {"lean-right", NodeShape::Parallelogram}, {"in-out", NodeShape::Parallelogram},
        {"lean-l", NodeShape::ParallelogramAlt}, {"lean-left", NodeShape::ParallelogramAlt},
        {"out-in", NodeShape::ParallelogramAlt}, {"trap-b", NodeShape::Trapezoid},
        {"trapezoid", NodeShape::Trapezoid}, {"priority", NodeShape::Trapezoid},
        {"trap-t", NodeShape::TrapezoidAlt}, {"manual", NodeShape::TrapezoidAlt},
        {"odd", NodeShape::Asymmetric}, {"sm-circ", NodeShape::StateStart}, {"start", NodeShape::StateStart},
        {"fr-circ", NodeShape::StateEnd}, {"stop", NodeShape::StateEnd}, {"fork", NodeShape::ForkBar},
        {"join", NodeShape::ForkBar}};
    const std::string lower = ToLower(Trim(name));
    for (const auto& [n, shape] : kNames) {
        if (lower == n) return shape;
    }
    return std::nullopt;
}

// Replaces nodes that stand for a cluster (edges to a subgraph / composite state) by a member node.
void RetargetClusterNodes(FlowGraph& g, const std::function<int(int cluster, bool incoming)>& representative) {
    std::map<std::string, int> clusterById;
    for (size_t c = 0; c < g.clusters.size(); ++c) clusterById.emplace(g.clusters[c].id, static_cast<int>(c));

    std::vector<int> remap(g.nodes.size());
    std::vector<bool> removed(g.nodes.size(), false);
    for (size_t i = 0; i < g.nodes.size(); ++i) remap[i] = static_cast<int>(i);

    std::vector<std::pair<int, int>> placeholders; // node -> cluster
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        auto it = clusterById.find(g.nodes[i].id);
        if (it != clusterById.end() && !g.nodes[i].explicitShape) {
            placeholders.emplace_back(static_cast<int>(i), it->second);
        }
    }
    if (placeholders.empty()) return;

    for (const auto& [node, cluster] : placeholders) {
        const int in = representative(cluster, true);
        const int out = representative(cluster, false);
        if (in < 0 || out < 0 || in == node || out == node) continue;
        for (auto& e : g.edges) {
            if (e.to == node) e.to = in;
            if (e.from == node) e.from = out;
        }
        removed[static_cast<size_t>(node)] = true;
    }

    // Compact the node list.
    std::vector<FlowNode> nodes;
    int next = 0;
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        if (removed[i]) {
            remap[i] = -1;
            continue;
        }
        remap[i] = next++;
        nodes.push_back(std::move(g.nodes[i]));
    }
    g.nodes = std::move(nodes);
    g.nodeIndex.clear();
    for (size_t i = 0; i < g.nodes.size(); ++i) g.nodeIndex.emplace(g.nodes[i].id, static_cast<int>(i));
    std::vector<FlowEdge> edges;
    for (auto& e : g.edges) {
        if (remap[static_cast<size_t>(e.from)] < 0 || remap[static_cast<size_t>(e.to)] < 0) continue;
        e.from = remap[static_cast<size_t>(e.from)];
        e.to = remap[static_cast<size_t>(e.to)];
        edges.push_back(std::move(e));
    }
    g.edges = std::move(edges);
}

bool NodeInCluster(const FlowGraph& g, const FlowNode& node, int cluster) {
    int c = node.cluster;
    int guard = 0;
    while (c >= 0 && guard++ < 64) {
        if (c == cluster) return true;
        c = g.clusters[static_cast<size_t>(c)].parent;
    }
    return false;
}

int FirstMember(const FlowGraph& g, int cluster, const std::string& excludeId) {
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        if (g.nodes[i].id != excludeId && NodeInCluster(g, g.nodes[i], cluster)) return static_cast<int>(i);
    }
    return -1;
}

// --- flowchart parser -----------------------------------------------------------

class FlowchartParser {
public:
    explicit FlowchartParser(FlowGraph& graph) : m_g(graph) {}

    bool Parse(const std::vector<std::string_view>& lines, std::string& error) {
        if (lines.empty()) return false;
        // Header: "graph TD" / "flowchart LR", possibly followed by statements after ';'.
        auto statements = SplitStatements(lines[0]);
        if (statements.empty()) return false;
        const auto [keyword, rest] = SplitFirstToken(statements[0]);
        (void)keyword;
        if (!rest.empty()) {
            if (auto dir = ParseDirection(SplitFirstToken(rest).first)) m_g.direction = *dir;
        }
        for (size_t i = 1; i < statements.size(); ++i) {
            if (!ParseStatement(statements[i], error)) return false;
        }
        for (size_t l = 1; l < lines.size(); ++l) {
            for (std::string_view stmt : SplitStatements(lines[l])) {
                if (!ParseStatement(stmt, error)) return false;
            }
        }
        ApplyLinkStyles();
        RetargetClusterNodes(m_g, [this](int cluster, bool) {
            return FirstMember(m_g, cluster, m_g.clusters[static_cast<size_t>(cluster)].id);
        });
        return true;
    }

private:
    bool ParseStatement(std::string_view stmt, std::string& error) {
        stmt = Trim(stmt);
        if (stmt.empty()) return true;

        if (StartsWithKeyword(stmt, "subgraph")) {
            OpenSubgraph(Trim(stmt.substr(8)));
            return true;
        }
        if (stmt == "end") {
            if (!m_clusterStack.empty()) m_clusterStack.pop_back();
            return true;
        }
        if (StartsWithKeyword(stmt, "classDef")) {
            const auto [names, spec] = SplitFirstToken(stmt.substr(8));
            const StyleSpec style = ParseStyleSpec(spec);
            for (std::string_view name : SplitList(names, ',')) m_g.classDefs[std::string(name)].Merge(style);
            return true;
        }
        if (StartsWithKeyword(stmt, "class")) {
            const auto [ids, cls] = SplitFirstToken(stmt.substr(5));
            for (std::string_view id : SplitList(ids, ',')) {
                const int node = TouchNode(std::string(id));
                m_g.nodes[static_cast<size_t>(node)].classes.emplace_back(Trim(cls));
            }
            return true;
        }
        if (StartsWithKeyword(stmt, "style")) {
            const auto [id, spec] = SplitFirstToken(stmt.substr(5));
            const int node = TouchNode(std::string(id));
            m_g.nodes[static_cast<size_t>(node)].style.Merge(ParseStyleSpec(spec));
            return true;
        }
        if (StartsWithKeyword(stmt, "linkStyle")) {
            const auto [ids, spec] = SplitFirstToken(stmt.substr(9));
            m_linkStyles.emplace_back(std::string(ids), ParseStyleSpec(spec));
            return true;
        }
        if (StartsWithKeyword(stmt, "click") || StartsWithKeyword(stmt, "direction") ||
            StartsWithKeyword(stmt, "accTitle") || StartsWithKeyword(stmt, "accDescr") ||
            StartsWithKeyword(stmt, "title")) {
            return true;
        }
        return ParseChain(stmt, error);
    }

    void OpenSubgraph(std::string_view rest) {
        FlowCluster cluster;
        const size_t bracket = rest.find('[');
        if (rest.empty()) {
            cluster.id = "subgraph_" + std::to_string(m_g.clusters.size());
        } else if (rest.front() == '"') {
            cluster.title = CleanLabel(rest);
            cluster.id = cluster.title;
        } else if (bracket != std::string_view::npos) {
            cluster.id = std::string(Trim(rest.substr(0, bracket)));
            const size_t close = rest.rfind(']');
            cluster.title = CleanLabel(rest.substr(bracket + 1, (close == std::string_view::npos || close < bracket)
                                                                     ? std::string_view::npos
                                                                     : close - bracket - 1));
        } else {
            cluster.id = std::string(rest);
            cluster.title = CleanLabel(rest);
        }
        cluster.parent = m_clusterStack.empty() ? -1 : m_clusterStack.back();
        m_g.clusters.push_back(std::move(cluster));
        m_clusterStack.push_back(static_cast<int>(m_g.clusters.size()) - 1);
    }

    // Creates the node if needed and moves it into the innermost open subgraph.
    int TouchNode(const std::string& id) {
        const int index = m_g.AddNode(id, id, NodeShape::Rect);
        if (!m_clusterStack.empty()) m_g.nodes[static_cast<size_t>(index)].cluster = m_clusterStack.back();
        return index;
    }

    std::optional<int> ParseNode(Cursor& c, std::string& error) {
        c.SkipSpaces();
        const size_t start = c.p;
        while (!c.AtEnd()) {
            const char ch = c.Peek();
            if (IsIdChar(ch)) {
                ++c.p;
            } else if ((ch == '-' || ch == '.') && c.p > start && IsIdChar(c.Peek(1))) {
                ++c.p;
            } else {
                break;
            }
        }
        if (c.p == start) return std::nullopt;
        const std::string id(c.s.substr(start, c.p - start));
        const int index = TouchNode(id);
        FlowNode& node = m_g.nodes[static_cast<size_t>(index)];

        // Extended syntax: A@{ shape: diamond, label: "Text" }
        if (c.Peek() == '@' && c.Peek(1) == '{') {
            const size_t close = c.s.find('}', c.p);
            if (close == std::string_view::npos) {
                error = "Falta '}' en la definición del nodo " + id;
                return std::nullopt;
            }
            const std::string_view body = c.s.substr(c.p + 2, close - c.p - 2);
            c.p = close + 1;
            for (std::string_view decl : SplitList(body, ',')) {
                const size_t colon = decl.find(':');
                if (colon == std::string_view::npos) continue;
                const std::string key = ToLower(Trim(decl.substr(0, colon)));
                const std::string_view value = Trim(decl.substr(colon + 1));
                if (key == "shape") {
                    if (auto shape = ShapeFromName(value)) {
                        node.shape = *shape;
                        node.explicitShape = true;
                    }
                } else if (key == "label") {
                    node.label = CleanLabel(value);
                    node.explicitShape = true;
                }
            }
        } else {
            const size_t beforeSpaces = c.p;
            c.SkipSpaces();
            bool matched = false;
            for (const auto& opener : kOpeners) {
                if (!c.Consume(opener.open)) continue;
                matched = true;
                std::string_view label;
                NodeShape shape = opener.shape;
                c.SkipSpaces();
                size_t labelEnd = std::string_view::npos;
                if (c.Peek() == '"') {
                    const size_t quoteEnd = c.s.find('"', c.p + 1);
                    if (quoteEnd != std::string_view::npos) labelEnd = quoteEnd + 1;
                }
                const size_t searchFrom = labelEnd != std::string_view::npos ? labelEnd : c.p;
                size_t closePos = std::string_view::npos;
                std::string_view closeToken = opener.close;
                if (opener.close.empty()) {
                    const size_t slash = c.s.find("/]", searchFrom);
                    const size_t back = c.s.find("\\]", searchFrom);
                    const bool slashFirst = slash != std::string_view::npos && (back == std::string_view::npos || slash < back);
                    closePos = slashFirst ? slash : back;
                    closeToken = slashFirst ? "/]" : "\\]";
                    if (opener.shape == NodeShape::Parallelogram) {
                        shape = slashFirst ? NodeShape::Parallelogram : NodeShape::Trapezoid;
                    } else {
                        shape = slashFirst ? NodeShape::TrapezoidAlt : NodeShape::ParallelogramAlt;
                    }
                } else {
                    closePos = c.s.find(opener.close, searchFrom);
                }
                if (closePos == std::string_view::npos) {
                    error = "Falta el cierre '" + std::string(opener.close.empty() ? "/]" : opener.close) +
                            "' en el nodo " + id;
                    return std::nullopt;
                }
                label = c.s.substr(c.p, closePos - c.p);
                c.p = closePos + closeToken.size();
                node.label = CleanLabel(label);
                node.shape = shape;
                node.explicitShape = true;
                break;
            }
            if (!matched) c.p = beforeSpaces;
        }

        if (c.Consume(":::")) {
            const size_t start2 = c.p;
            while (!c.AtEnd() && (IsIdChar(c.Peek()) || c.Peek() == '-')) ++c.p;
            node.classes.emplace_back(c.s.substr(start2, c.p - start2));
        }
        return index;
    }

    std::optional<std::vector<int>> ParseGroup(Cursor& c, std::string& error) {
        std::vector<int> group;
        while (true) {
            auto node = ParseNode(c, error);
            if (!node) return std::nullopt;
            group.push_back(*node);
            const size_t save = c.p;
            c.SkipSpaces();
            if (c.Peek() == '&') {
                ++c.p;
                continue;
            }
            c.p = save;
            return group;
        }
    }

    static ArrowKind ParseEndArrow(Cursor& c) {
        const char ch = c.Peek();
        if (ch == '>') {
            ++c.p;
            return ArrowKind::Arrow;
        }
        if ((ch == 'o' || ch == 'x') && !IsIdChar(c.Peek(1))) {
            ++c.p;
            return ch == 'o' ? ArrowKind::Circle : ArrowKind::Cross;
        }
        return ArrowKind::None;
    }

    static std::optional<LinkInfo> ParseLink(Cursor& c) {
        const size_t save = c.p;
        c.SkipSpaces();
        LinkInfo link;
        const char first = c.Peek();
        if (first == '<' && IsLinkBodyChar(c.Peek(1))) {
            link.startArrow = ArrowKind::Arrow;
            ++c.p;
        } else if ((first == 'o' || first == 'x') && IsLinkBodyChar(c.Peek(1)) && IsLinkBodyChar(c.Peek(2))) {
            link.startArrow = first == 'o' ? ArrowKind::Circle : ArrowKind::Cross;
            ++c.p;
        }

        size_t bodyStart = c.p;
        while (IsLinkBodyChar(c.Peek())) ++c.p;
        std::string_view body = c.s.substr(bodyStart, c.p - bodyStart);
        if (body.size() < 2) {
            c.p = save;
            return std::nullopt;
        }
        link.endArrow = ParseEndArrow(c);
        const bool complete = link.endArrow != ArrowKind::None || body.size() >= 3;
        if (!complete) {
            // Text form: "-- text -->", "== text ==>", "-. text .->"
            const bool thick = body.find('=') != std::string_view::npos;
            const bool dotted = body.find('.') != std::string_view::npos;
            const std::string_view closer = thick ? "==" : (dotted ? ".-" : "--");
            const size_t closePos = c.s.find(closer, c.p);
            if (closePos == std::string_view::npos) {
                c.p = save;
                return std::nullopt;
            }
            link.label = CleanLabel(c.s.substr(c.p, closePos - c.p));
            c.p = closePos;
            bodyStart = c.p;
            while (IsLinkBodyChar(c.Peek())) ++c.p;
            body = c.s.substr(bodyStart, c.p - bodyStart);
            link.endArrow = ParseEndArrow(c);
            if (thick) {
                link.stroke = EdgeStroke::Thick;
            } else if (dotted) {
                link.stroke = EdgeStroke::Dotted;
            }
        } else if (body.find('~') != std::string_view::npos) {
            link.stroke = EdgeStroke::Invisible;
        } else if (body.find('=') != std::string_view::npos) {
            link.stroke = EdgeStroke::Thick;
        } else if (body.find('.') != std::string_view::npos) {
            link.stroke = EdgeStroke::Dotted;
        }

        if (link.stroke == EdgeStroke::Dotted) {
            link.minLen = static_cast<int>(std::count(body.begin(), body.end(), '.'));
        } else {
            link.minLen = static_cast<int>(body.size()) - (link.endArrow != ArrowKind::None ? 1 : 2);
        }
        link.minLen = std::clamp(link.minLen, 1, 8);

        // Pipe label: -->|text|
        const size_t beforeLabel = c.p;
        c.SkipSpaces();
        if (c.Peek() == '|') {
            const size_t close = c.s.find('|', c.p + 1);
            if (close != std::string_view::npos) {
                link.label = CleanLabel(c.s.substr(c.p + 1, close - c.p - 1));
                c.p = close + 1;
            } else {
                c.p = beforeLabel;
            }
        } else {
            c.p = beforeLabel;
        }
        return link;
    }

    bool ParseChain(std::string_view stmt, std::string& error) {
        Cursor c{stmt};
        auto left = ParseGroup(c, error);
        if (!left) {
            if (error.empty()) error = "Sintaxis no reconocida: " + std::string(stmt);
            return false;
        }
        while (true) {
            c.SkipSpaces();
            if (c.AtEnd()) break;
            auto link = ParseLink(c);
            if (!link) {
                error = "Sintaxis no reconocida: " + std::string(stmt);
                return false;
            }
            auto right = ParseGroup(c, error);
            if (!right) {
                if (error.empty()) error = "Falta el nodo de destino en: " + std::string(stmt);
                return false;
            }
            for (int a : *left) {
                for (int b : *right) {
                    FlowEdge edge;
                    edge.from = a;
                    edge.to = b;
                    edge.label = link->label;
                    edge.stroke = link->stroke;
                    edge.startArrow = link->startArrow;
                    edge.endArrow = link->endArrow;
                    edge.minLen = link->minLen;
                    m_g.edges.push_back(std::move(edge));
                }
            }
            left = std::move(right);
        }
        return true;
    }

    void ApplyLinkStyles() {
        for (const auto& [ids, style] : m_linkStyles) {
            if (ids == "default") {
                for (auto& e : m_g.edges) e.style.Merge(style);
                continue;
            }
            for (std::string_view id : SplitList(ids, ',')) {
                const int index = std::atoi(std::string(id).c_str());
                if (index >= 0 && index < static_cast<int>(m_g.edges.size())) {
                    m_g.edges[static_cast<size_t>(index)].style.Merge(style);
                }
            }
        }
    }

    FlowGraph& m_g;
    std::vector<int> m_clusterStack;
    std::vector<std::pair<std::string, StyleSpec>> m_linkStyles;
};

// --- state diagram parser -------------------------------------------------------

class StateParser {
public:
    explicit StateParser(FlowGraph& graph) : m_g(graph) {}

    bool Parse(const std::vector<std::string_view>& lines, std::string& error) {
        m_g.roundedDefault = true;
        for (size_t l = 1; l < lines.size(); ++l) {
            std::string_view line = lines[l];
            if (m_inNote) {
                if (ToLower(line) == "end note") {
                    FinishNote();
                } else {
                    if (!m_noteText.empty()) m_noteText += "\n";
                    m_noteText += CleanLabel(line);
                }
                continue;
            }
            if (!ParseLine(line, error)) return false;
        }
        RetargetClusterNodes(m_g, [this](int cluster, bool incoming) {
            const std::string& id = m_g.clusters[static_cast<size_t>(cluster)].id;
            const int pseudo = m_g.FindNode((incoming ? "[*]start:" : "[*]end:") + id);
            return pseudo >= 0 ? pseudo : FirstMember(m_g, cluster, id);
        });
        return true;
    }

private:
    std::string ScopeKey() const {
        return m_clusterStack.empty() ? std::string() : m_g.clusters[static_cast<size_t>(m_clusterStack.back())].id;
    }

    int StateRef(std::string_view name, bool asSource) {
        name = Trim(name);
        const int cluster = m_clusterStack.empty() ? -1 : m_clusterStack.back();
        if (name == "[*]") {
            const std::string id = (asSource ? "[*]start:" : "[*]end:") + ScopeKey();
            const bool isNew = m_g.FindNode(id) < 0;
            const int index = m_g.AddNode(id, "", asSource ? NodeShape::StateStart : NodeShape::StateEnd);
            if (isNew) {
                m_g.nodes[static_cast<size_t>(index)].cluster = cluster;
                m_g.nodes[static_cast<size_t>(index)].explicitShape = true;
            }
            return index;
        }
        std::string id(name);
        std::string cls;
        if (const size_t colons = id.find(":::"); colons != std::string::npos) {
            cls = id.substr(colons + 3);
            id = std::string(Trim(std::string_view(id).substr(0, colons)));
        }
        const bool isNew = m_g.FindNode(id) < 0;
        const int index = m_g.AddNode(id, id, NodeShape::StateBox);
        if (isNew) m_g.nodes[static_cast<size_t>(index)].cluster = cluster;
        if (!cls.empty()) m_g.nodes[static_cast<size_t>(index)].classes.push_back(cls);
        return index;
    }

    void FinishNote() {
        m_inNote = false;
        if (m_noteTarget < 0) return;
        const std::string id = "note:" + std::to_string(m_noteCount++);
        const int note = m_g.AddNode(id, m_noteText, NodeShape::Note);
        m_g.nodes[static_cast<size_t>(note)].explicitShape = true;
        m_g.nodes[static_cast<size_t>(note)].cluster = m_g.nodes[static_cast<size_t>(m_noteTarget)].cluster;
        FlowEdge edge;
        edge.from = m_noteTarget;
        edge.to = note;
        edge.stroke = EdgeStroke::Dotted;
        edge.endArrow = ArrowKind::None;
        m_g.edges.push_back(std::move(edge));
        m_noteText.clear();
        m_noteTarget = -1;
    }

    bool ParseLine(std::string_view line, std::string& error) {
        if (line == "}") {
            if (!m_clusterStack.empty()) m_clusterStack.pop_back();
            return true;
        }
        if (line == "--" || StartsWithKeyword(line, "hide") || StartsWithKeyword(line, "scale") ||
            StartsWithKeyword(line, "accTitle") || StartsWithKeyword(line, "accDescr") ||
            StartsWithKeyword(line, "title")) {
            return true;
        }
        if (StartsWithKeyword(line, "direction")) {
            if (m_clusterStack.empty()) {
                if (auto dir = ParseDirection(line.substr(9))) m_g.direction = *dir;
            }
            return true;
        }
        if (StartsWithKeyword(line, "classDef")) {
            const auto [names, spec] = SplitFirstToken(line.substr(8));
            const StyleSpec style = ParseStyleSpec(spec);
            for (std::string_view name : SplitList(names, ',')) m_g.classDefs[std::string(name)].Merge(style);
            return true;
        }
        if (StartsWithKeyword(line, "class")) {
            const auto [ids, cls] = SplitFirstToken(line.substr(5));
            for (std::string_view id : SplitList(ids, ',')) {
                m_g.nodes[static_cast<size_t>(StateRef(id, true))].classes.emplace_back(Trim(cls));
            }
            return true;
        }
        if (StartsWithKeyword(line, "style")) {
            const auto [id, spec] = SplitFirstToken(line.substr(5));
            m_g.nodes[static_cast<size_t>(StateRef(id, true))].style.Merge(ParseStyleSpec(spec));
            return true;
        }
        if (StartsWithKeyword(ToLower(line), "note")) {
            return ParseNote(line);
        }
        if (StartsWithKeyword(line, "state")) {
            return ParseStateDeclaration(Trim(line.substr(5)), error);
        }

        const size_t arrow = line.find("-->");
        if (arrow != std::string_view::npos) {
            std::string_view rest = line.substr(arrow + 3);
            std::string label;
            const size_t colon = rest.find(':');
            if (colon != std::string_view::npos) {
                label = CleanLabel(rest.substr(colon + 1));
                rest = rest.substr(0, colon);
            }
            const std::string_view fromName = Trim(line.substr(0, arrow));
            const std::string_view toName = Trim(rest);
            if (fromName.empty() || toName.empty()) {
                error = "Transición incompleta: " + std::string(line);
                return false;
            }
            FlowEdge edge;
            edge.from = StateRef(fromName, true);
            edge.to = StateRef(toName, false);
            edge.label = std::move(label);
            m_g.edges.push_back(std::move(edge));
            return true;
        }

        const size_t colon = line.find(':');
        if (colon != std::string_view::npos) {
            const int state = StateRef(line.substr(0, colon), true);
            AppendDescription(state, CleanLabel(line.substr(colon + 1)));
            return true;
        }
        StateRef(line, true);
        return true;
    }

    void AppendDescription(int state, const std::string& text) {
        FlowNode& node = m_g.nodes[static_cast<size_t>(state)];
        if (!text.empty()) node.label += "\n" + text;
    }

    bool ParseNote(std::string_view line) {
        // note right of A : text   |   note left of A (multi-line, until "end note")
        std::string_view rest = Trim(line.substr(4));
        const size_t colon = rest.find(':');
        std::string_view head = colon == std::string_view::npos ? rest : rest.substr(0, colon);
        const size_t of = head.find(" of ");
        if (of == std::string_view::npos) return true; // Floating notes are ignored
        m_noteTarget = StateRef(head.substr(of + 4), true);
        m_noteText.clear();
        if (colon != std::string_view::npos) {
            m_noteText = CleanLabel(rest.substr(colon + 1));
            FinishNote();
        } else {
            m_inNote = true;
        }
        return true;
    }

    bool ParseStateDeclaration(std::string_view rest, std::string& error) {
        bool opensBlock = false;
        if (!rest.empty() && rest.back() == '{') {
            opensBlock = true;
            rest = Trim(rest.substr(0, rest.size() - 1));
        }

        std::string label;
        std::string id;
        if (!rest.empty() && rest.front() == '"') {
            const size_t close = rest.find('"', 1);
            if (close == std::string_view::npos) {
                error = "Falta la comilla de cierre: state " + std::string(rest);
                return false;
            }
            label = CleanLabel(rest.substr(1, close - 1));
            std::string_view after = Trim(rest.substr(close + 1));
            if (StartsWithKeyword(after, "as")) after = Trim(after.substr(2));
            id = std::string(SplitFirstToken(after).first);
        } else {
            const auto [first, tail] = SplitFirstToken(rest);
            id = std::string(first);
            if (StartsWithKeyword(tail, "as")) {
                // state Id as "Label" is not valid Mermaid, but "state Label as Id" is.
                label = id;
                id = std::string(SplitFirstToken(Trim(tail.substr(2))).first);
            }
            const size_t colon = id.find(':');
            if (colon != std::string::npos && id.find(":::") == std::string::npos) {
                label = CleanLabel(std::string_view(rest).substr(rest.find(':') + 1));
                id = std::string(Trim(std::string_view(id).substr(0, colon)));
            }
        }
        if (id.empty()) {
            error = "Declaración de estado sin identificador";
            return false;
        }

        const std::string lowerRest = ToLower(rest);
        if (lowerRest.find("<<fork>>") != std::string::npos || lowerRest.find("<<join>>") != std::string::npos ||
            lowerRest.find("<<choice>>") != std::string::npos) {
            const int node = StateRef(id, true);
            FlowNode& n = m_g.nodes[static_cast<size_t>(node)];
            n.shape = lowerRest.find("<<choice>>") != std::string::npos ? NodeShape::Choice : NodeShape::ForkBar;
            n.label.clear();
            n.explicitShape = true;
            return true;
        }

        if (opensBlock) {
            FlowCluster cluster;
            cluster.id = id;
            cluster.title = label.empty() ? id : label;
            cluster.parent = m_clusterStack.empty() ? -1 : m_clusterStack.back();
            m_g.clusters.push_back(std::move(cluster));
            m_clusterStack.push_back(static_cast<int>(m_g.clusters.size()) - 1);
            return true;
        }

        const int node = StateRef(id, true);
        if (!label.empty()) m_g.nodes[static_cast<size_t>(node)].label = label;
        return true;
    }

    FlowGraph& m_g;
    std::vector<int> m_clusterStack;
    bool m_inNote = false;
    int m_noteTarget = -1;
    int m_noteCount = 0;
    std::string m_noteText;
};

// --- scene building -------------------------------------------------------------

constexpr float kNodeFont = 14.0f;
constexpr float kEdgeFont = 12.5f;
constexpr float kTitleFont = 13.0f;
constexpr float kPadX = 14.0f;
constexpr float kPadY = 9.0f;
constexpr float kMaxLabelWidth = 200.0f;
constexpr float kMaxEdgeLabelWidth = 160.0f;

bool HasLabel(NodeShape shape) {
    switch (shape) {
    case NodeShape::StateStart:
    case NodeShape::StateEnd:
    case NodeShape::ForkBar:
    case NodeShape::Choice:
        return false;
    default:
        return true;
    }
}

float CylinderRy(float w) {
    return std::clamp(w * 0.08f, 5.0f, 12.0f);
}

// Node size (final orientation) for a text block of tw x th.
std::pair<float, float> NodeSize(NodeShape shape, float tw, float th, bool horizontal) {
    const float boxH = th + 2.0f * kPadY;
    switch (shape) {
    case NodeShape::Stadium:
        return {tw + 2.0f * kPadX + boxH * 0.5f, boxH};
    case NodeShape::Subroutine:
        return {tw + 2.0f * kPadX + 16.0f, boxH};
    case NodeShape::Cylinder: {
        const float w = (std::max)(tw + 2.0f * kPadX, 50.0f);
        return {w, boxH + 2.0f * CylinderRy(w)};
    }
    case NodeShape::Circle:
    case NodeShape::DoubleCircle: {
        const float d = (std::max)(tw, th) + 2.0f * kPadY + 6.0f + (shape == NodeShape::DoubleCircle ? 10.0f : 0.0f);
        return {d, d};
    }
    case NodeShape::Diamond: {
        const float a = tw;
        const float b = th;
        const float w = a + b + 2.0f * kPadX;
        const float h = (std::max)({b * w / (std::max)(1.0f, w - a), w * 0.5f, boxH});
        return {w, h};
    }
    case NodeShape::Hexagon:
        return {tw + 2.0f * kPadX + boxH * 0.5f, boxH};
    case NodeShape::Parallelogram:
    case NodeShape::ParallelogramAlt:
    case NodeShape::Trapezoid:
    case NodeShape::TrapezoidAlt:
        return {tw + 2.0f * kPadX + boxH * 0.66f, boxH};
    case NodeShape::Asymmetric:
        return {tw + 2.0f * kPadX + boxH * 0.3f, boxH};
    case NodeShape::StateStart:
    case NodeShape::StateEnd:
        return {16.0f, 16.0f};
    case NodeShape::ForkBar:
        return horizontal ? std::make_pair(8.0f, 64.0f) : std::make_pair(64.0f, 8.0f);
    case NodeShape::Choice:
        return {28.0f, 28.0f};
    default:
        return {(std::max)(tw + 2.0f * kPadX, 40.0f), boxH};
    }
}

// Outline polygon (centred on the origin) used both to draw and to clip edges.
std::vector<Point> Outline(NodeShape shape, float w, float h) {
    const float x0 = -w * 0.5f;
    const float x1 = w * 0.5f;
    const float y0 = -h * 0.5f;
    const float y1 = h * 0.5f;
    switch (shape) {
    case NodeShape::Diamond:
    case NodeShape::Choice:
        return {{0, y0}, {x1, 0}, {0, y1}, {x0, 0}};
    case NodeShape::Hexagon: {
        const float s = h * 0.25f;
        return {{x0 + s, y0}, {x1 - s, y0}, {x1, 0}, {x1 - s, y1}, {x0 + s, y1}, {x0, 0}};
    }
    case NodeShape::Parallelogram: {
        const float s = h * 0.33f;
        return {{x0 + s, y0}, {x1, y0}, {x1 - s, y1}, {x0, y1}};
    }
    case NodeShape::ParallelogramAlt: {
        const float s = h * 0.33f;
        return {{x0, y0}, {x1 - s, y0}, {x1, y1}, {x0 + s, y1}};
    }
    case NodeShape::Trapezoid: {
        const float s = h * 0.33f;
        return {{x0 + s, y0}, {x1 - s, y0}, {x1, y1}, {x0, y1}};
    }
    case NodeShape::TrapezoidAlt: {
        const float s = h * 0.33f;
        return {{x0, y0}, {x1, y0}, {x1 - s, y1}, {x0 + s, y1}};
    }
    case NodeShape::Asymmetric: {
        const float s = h * 0.3f;
        return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0 + s, 0}};
    }
    default:
        return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    }
}

bool IsRound(NodeShape shape) {
    return shape == NodeShape::Circle || shape == NodeShape::DoubleCircle || shape == NodeShape::StateStart ||
           shape == NodeShape::StateEnd;
}

// Point where the ray centre -> toward leaves the node outline.
Point ClipToNode(NodeShape shape, Point center, float w, float h, Point toward) {
    const float dx = toward.x - center.x;
    const float dy = toward.y - center.y;
    if (std::fabs(dx) < 1.0e-3f && std::fabs(dy) < 1.0e-3f) return center;
    if (IsRound(shape)) {
        const float rx = w * 0.5f;
        const float ry = h * 0.5f;
        const float t = 1.0f / std::sqrt((dx * dx) / (rx * rx) + (dy * dy) / (ry * ry));
        return Point{center.x + dx * t, center.y + dy * t};
    }
    const std::vector<Point> poly = Outline(shape, w, h);
    float best = std::numeric_limits<float>::max();
    for (size_t i = 0; i < poly.size(); ++i) {
        const Point a = poly[i];
        const Point b = poly[(i + 1) % poly.size()];
        const float ex = b.x - a.x;
        const float ey = b.y - a.y;
        const float denom = dx * ey - dy * ex;
        if (std::fabs(denom) < 1.0e-6f) continue;
        const float t = (a.x * ey - a.y * ex) / denom;
        const float u = (a.x * dy - a.y * dx) / denom;
        if (t > 0.0f && u >= -1.0e-4f && u <= 1.0f + 1.0e-4f) best = (std::min)(best, t);
    }
    if (best == std::numeric_limits<float>::max()) return center;
    return Point{center.x + dx * best, center.y + dy * best};
}

Point Normalized(Point v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y);
    return len < 1.0e-4f ? Point{0.0f, 1.0f} : Point{v.x / len, v.y / len};
}

// Direction in which an edge arrives at `end` coming from `prev`: along the rank axis when it dominates.
Point ArrivalDirection(Point prev, Point end, bool horizontal) {
    const float along = horizontal ? end.x - prev.x : end.y - prev.y;
    if (std::fabs(along) >= 1.0f) {
        const float sign = along > 0.0f ? 1.0f : -1.0f;
        return horizontal ? Point{sign, 0.0f} : Point{0.0f, sign};
    }
    return Normalized(Point{end.x - prev.x, end.y - prev.y});
}

void DrawEndMarker(SceneBuilder& b, ArrowKind kind, Point tip, Point dir, Paint paint, float size) {
    switch (kind) {
    case ArrowKind::Arrow:
        b.ArrowHead(Point{tip.x - dir.x * size, tip.y - dir.y * size}, tip, size, paint);
        break;
    case ArrowKind::Circle:
        b.Ellipse(tip.x - dir.x * 4.5f, tip.y - dir.y * 4.5f, 4.0f, 4.0f, paint, paint, 1.0f);
        break;
    case ArrowKind::Cross: {
        const Point c{tip.x - dir.x * 5.0f, tip.y - dir.y * 5.0f};
        const float r = 4.5f;
        b.Line({c.x - r, c.y - r}, {c.x + r, c.y + r}, paint, 1.8f);
        b.Line({c.x - r, c.y + r}, {c.x + r, c.y - r}, paint, 1.8f);
        break;
    }
    default:
        break;
    }
}

float MarkerLength(ArrowKind kind, float arrowSize) {
    switch (kind) {
    case ArrowKind::Arrow:  return arrowSize * 0.85f;
    case ArrowKind::Circle: return 8.5f;
    case ArrowKind::Cross:  return 5.0f;
    default:                return 0.0f;
    }
}

StyleSpec ResolveNodeStyle(const FlowGraph& g, const FlowNode& node) {
    StyleSpec style;
    if (auto it = g.classDefs.find("default"); it != g.classDefs.end()) style.Merge(it->second);
    for (const auto& cls : node.classes) {
        if (auto it = g.classDefs.find(cls); it != g.classDefs.end()) style.Merge(it->second);
    }
    style.Merge(node.style);
    return style;
}

void DrawNodeShape(SceneBuilder& b, NodeShape shape, Point c, float w, float h, Paint fill, Paint stroke,
                   float strokeWidth, LineStyle lineStyle) {
    const float x = c.x - w * 0.5f;
    const float y = c.y - h * 0.5f;
    auto styled = [&](Primitive& p) { p.lineStyle = lineStyle; };
    switch (shape) {
    case NodeShape::Rect:
        styled(b.Rect(x, y, w, h, fill, stroke, strokeWidth, 2.0f));
        break;
    case NodeShape::Round:
    case NodeShape::StateBox:
        styled(b.Rect(x, y, w, h, fill, stroke, strokeWidth, (std::min)(9.0f, h * 0.3f)));
        break;
    case NodeShape::Note:
        styled(b.Rect(x, y, w, h, fill, stroke, strokeWidth, 0.0f));
        break;
    case NodeShape::Stadium:
        styled(b.Rect(x, y, w, h, fill, stroke, strokeWidth, h * 0.5f));
        break;
    case NodeShape::Subroutine:
        styled(b.Rect(x, y, w, h, fill, stroke, strokeWidth, 0.0f));
        b.Line({x + 8.0f, y}, {x + 8.0f, y + h}, stroke, strokeWidth);
        b.Line({x + w - 8.0f, y}, {x + w - 8.0f, y + h}, stroke, strokeWidth);
        break;
    case NodeShape::Cylinder: {
        const float ry = CylinderRy(w);
        const float rx = w * 0.5f;
        std::vector<Segment> body;
        body.push_back(StraightSegment({x, y + ry}, {x, y + h - ry}));
        AppendArc(body, {c.x, y + h - ry}, rx, ry, 3.14159265f, 0.0f);
        body.push_back(StraightSegment({x + w, y + h - ry}, {x + w, y + ry}));
        AppendArc(body, {c.x, y + ry}, rx, ry, 0.0f, -3.14159265f);
        styled(b.Path({x, y + ry}, std::move(body), fill, stroke, strokeWidth, true));
        std::vector<Segment> rim;
        AppendArc(rim, {c.x, y + ry}, rx, ry, 3.14159265f, 0.0f);
        b.Path({x, y + ry}, std::move(rim), Paint{}, stroke, strokeWidth, false);
        break;
    }
    case NodeShape::Circle:
        styled(b.Ellipse(c.x, c.y, w * 0.5f, h * 0.5f, fill, stroke, strokeWidth));
        break;
    case NodeShape::DoubleCircle:
        styled(b.Ellipse(c.x, c.y, w * 0.5f, h * 0.5f, fill, stroke, strokeWidth));
        b.Ellipse(c.x, c.y, w * 0.5f - 5.0f, h * 0.5f - 5.0f, Paint{}, stroke, strokeWidth);
        break;
    case NodeShape::StateStart:
        b.Ellipse(c.x, c.y, w * 0.5f, h * 0.5f, Paint::Of(Role::Marker), Paint::Of(Role::Marker), 1.0f);
        break;
    case NodeShape::StateEnd:
        b.Ellipse(c.x, c.y, w * 0.5f, h * 0.5f, Paint::Of(Role::Background), Paint::Of(Role::Marker), 1.5f);
        b.Ellipse(c.x, c.y, w * 0.5f - 4.0f, h * 0.5f - 4.0f, Paint::Of(Role::Marker), Paint{}, 1.0f);
        break;
    case NodeShape::ForkBar:
        b.Rect(x, y, w, h, Paint::Of(Role::Marker), Paint{}, 1.0f, 2.0f);
        break;
    default: {
        std::vector<Point> poly = Outline(shape, w, h);
        for (auto& p : poly) {
            p.x += c.x;
            p.y += c.y;
        }
        poly.push_back(poly.front());
        styled(b.Polygon(poly, fill, stroke, strokeWidth));
        break;
    }
    }
}

void DrawTextBlock(SceneBuilder& b, const std::vector<std::string>& lines, Point center, float fontSize, Paint color,
                   bool bold = false) {
    const float lineH = LineHeight(fontSize);
    float y = center.y - lineH * static_cast<float>(lines.size()) * 0.5f + lineH * 0.5f;
    for (const auto& line : lines) {
        if (!line.empty()) b.Text(line, center.x, y, fontSize, color, bold);
        y += lineH;
    }
}

} // namespace

bool ParseFlowchart(const std::vector<std::string_view>& lines, FlowGraph& graph, std::string& error) {
    return FlowchartParser(graph).Parse(lines, error);
}

bool ParseStateDiagram(const std::vector<std::string_view>& lines, FlowGraph& graph, std::string& error) {
    return StateParser(graph).Parse(lines, error);
}

Scene BuildFlowScene(const FlowGraph& g, const MeasureText& measure) {
    const bool horizontal = IsHorizontal(g.direction);

    struct NodeGeometry {
        std::vector<std::string> lines;
        float w = 0.0f;
        float h = 0.0f;
        StyleSpec style;
    };
    std::vector<NodeGeometry> geometry(g.nodes.size());

    GraphLayoutInput input;
    input.direction = g.direction;
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        const FlowNode& node = g.nodes[i];
        NodeGeometry& geo = geometry[i];
        geo.style = ResolveNodeStyle(g, node);
        float tw = 0.0f;
        float th = 0.0f;
        if (HasLabel(node.shape)) {
            geo.lines = WrapLabel(node.label, measure, kNodeFont, false, kMaxLabelWidth);
            tw = WidestLine(geo.lines, measure, kNodeFont, false);
            th = LineHeight(kNodeFont) * static_cast<float>(geo.lines.size());
        }
        std::tie(geo.w, geo.h) = NodeSize(node.shape, tw, th, horizontal);
        input.nodes.push_back(GraphNode{geo.w, geo.h, node.cluster});
    }

    std::vector<std::vector<std::string>> edgeLines(g.edges.size());
    for (size_t i = 0; i < g.edges.size(); ++i) {
        const FlowEdge& e = g.edges[i];
        GraphEdge ge;
        ge.from = e.from;
        ge.to = e.to;
        ge.minLen = e.minLen;
        if (!e.label.empty() && e.stroke != EdgeStroke::Invisible) {
            edgeLines[i] = WrapLabel(e.label, measure, kEdgeFont, false, kMaxEdgeLabelWidth);
            ge.labelWidth = WidestLine(edgeLines[i], measure, kEdgeFont, false) + 10.0f;
            ge.labelHeight = LineHeight(kEdgeFont) * static_cast<float>(edgeLines[i].size()) + 4.0f;
        }
        input.edges.push_back(ge);
    }

    for (const auto& cluster : g.clusters) {
        GraphCluster gc;
        gc.parent = cluster.parent;
        if (!cluster.title.empty()) {
            gc.titleWidth = measure(cluster.title, kTitleFont, true);
            gc.titleHeight = LineHeight(kTitleFont) + 2.0f;
        }
        input.clusters.push_back(gc);
    }

    const GraphLayoutResult layout = LayoutGraph(input);

    Scene scene;
    scene.width = layout.width;
    scene.height = layout.height;
    SceneBuilder b(scene);

    // Cluster frames, outermost first.
    std::vector<int> depth(g.clusters.size(), 0);
    for (size_t c = 0; c < g.clusters.size(); ++c) {
        int p = g.clusters[c].parent;
        int guard = 0;
        while (p >= 0 && guard++ < 64) {
            ++depth[c];
            p = g.clusters[static_cast<size_t>(p)].parent;
        }
    }
    std::vector<size_t> clusterOrder(g.clusters.size());
    for (size_t c = 0; c < clusterOrder.size(); ++c) clusterOrder[c] = c;
    std::stable_sort(clusterOrder.begin(), clusterOrder.end(), [&](size_t a, size_t b2) { return depth[a] < depth[b2]; });
    for (size_t c : clusterOrder) {
        const LayoutRect& r = layout.clusterRects[c];
        if (r.empty) continue;
        b.Rect(r.x, r.y, r.w, r.h, Paint::Of(Role::ClusterFill), Paint::Of(Role::ClusterStroke), 1.0f, 6.0f);
        if (!g.clusters[c].title.empty()) {
            const float titleH = LineHeight(kTitleFont);
            b.Text(g.clusters[c].title, r.x + r.w * 0.5f, r.y + 7.0f + titleH * 0.5f, kTitleFont,
                   Paint::Of(Role::Text), true);
        }
    }

    // Edges.
    for (size_t i = 0; i < g.edges.size(); ++i) {
        const FlowEdge& e = g.edges[i];
        if (e.stroke == EdgeStroke::Invisible) continue;
        const std::vector<Point>& raw = layout.edgePoints[i];
        if (raw.size() < 2) continue;

        const Paint paint = e.style.stroke ? Paint::Rgb(*e.style.stroke) : Paint::Of(Role::Line);
        const float width = e.style.strokeWidth ? *e.style.strokeWidth : (e.stroke == EdgeStroke::Thick ? 3.0f : 1.4f);
        const LineStyle lineStyle = (e.stroke == EdgeStroke::Dotted || e.style.dashed) ? LineStyle::Dashed : LineStyle::Solid;
        const float arrowSize = e.stroke == EdgeStroke::Thick ? 11.0f : 9.0f;

        if (e.from == e.to) {
            // Self loop: one cubic Bézier returned by the layout.
            Point start = raw[0];
            Point end = raw[3];
            const Point dirEnd = Normalized(Point{end.x - raw[2].x, end.y - raw[2].y});
            const Point tip = end;
            end = Point{end.x - dirEnd.x * MarkerLength(e.endArrow, arrowSize),
                        end.y - dirEnd.y * MarkerLength(e.endArrow, arrowSize)};
            b.Path(start, {Segment{raw[1], raw[2], end}}, Paint{}, paint, width).lineStyle = lineStyle;
            DrawEndMarker(b, e.endArrow, tip, dirEnd, paint, arrowSize);
            continue;
        }

        std::vector<Point> pts = raw;
        const auto from = static_cast<size_t>(e.from);
        const auto to = static_cast<size_t>(e.to);
        pts.front() = ClipToNode(g.nodes[from].shape, raw.front(), geometry[from].w, geometry[from].h, raw[1]);
        pts.back() = ClipToNode(g.nodes[to].shape, raw.back(), geometry[to].w, geometry[to].h, raw[raw.size() - 2]);

        const Point endTip = pts.back();
        const Point endDir = ArrivalDirection(pts[pts.size() - 2], endTip, horizontal);
        const float endLen = MarkerLength(e.endArrow, arrowSize);
        pts.back() = Point{endTip.x - endDir.x * endLen, endTip.y - endDir.y * endLen};

        const Point startTip = pts.front();
        const Point startDir = ArrivalDirection(pts[1], startTip, horizontal);
        const float startLen = MarkerLength(e.startArrow, arrowSize);
        pts.front() = Point{startTip.x - startDir.x * startLen, startTip.y - startDir.y * startLen};

        // Smooth curve: every piece leaves and enters along the rank axis.
        std::vector<Segment> segments;
        for (size_t k = 1; k < pts.size(); ++k) {
            const Point a = pts[k - 1];
            const Point c2 = pts[k];
            if (horizontal) {
                const float mid = (c2.x - a.x) * 0.5f;
                segments.push_back(Segment{{a.x + mid, a.y}, {c2.x - mid, c2.y}, c2});
            } else {
                const float mid = (c2.y - a.y) * 0.5f;
                segments.push_back(Segment{{a.x, a.y + mid}, {c2.x, c2.y - mid}, c2});
            }
        }
        b.Path(pts.front(), std::move(segments), Paint{}, paint, width).lineStyle = lineStyle;
        DrawEndMarker(b, e.endArrow, endTip, endDir, paint, arrowSize);
        DrawEndMarker(b, e.startArrow, startTip, startDir, paint, arrowSize);
    }

    // Edge labels on top of the lines.
    for (size_t i = 0; i < g.edges.size(); ++i) {
        if (edgeLines[i].empty()) continue;
        const GraphEdge& ge = input.edges[i];
        const Point c = layout.labelCenters[i];
        b.Rect(c.x - ge.labelWidth * 0.5f, c.y - ge.labelHeight * 0.5f, ge.labelWidth, ge.labelHeight,
               Paint::Of(Role::LabelBackground), Paint{}, 1.0f, 3.0f);
        DrawTextBlock(b, edgeLines[i], c, kEdgeFont, Paint::Of(Role::MutedText));
    }

    // Nodes.
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        const FlowNode& node = g.nodes[i];
        const NodeGeometry& geo = geometry[i];
        const Point c = layout.nodeCenters[i];
        const bool isNote = node.shape == NodeShape::Note;
        const Paint fill = geo.style.fill ? Paint::Rgb(*geo.style.fill)
                                          : Paint::Of(isNote ? Role::NoteFill : Role::NodeFill);
        const Paint stroke = geo.style.stroke ? Paint::Rgb(*geo.style.stroke)
                                              : Paint::Of(isNote ? Role::NoteStroke : Role::NodeStroke);
        const float strokeWidth = geo.style.strokeWidth ? *geo.style.strokeWidth : 1.3f;
        DrawNodeShape(b, node.shape, c, geo.w, geo.h, fill, stroke, strokeWidth,
                      geo.style.dashed ? LineStyle::Dashed : LineStyle::Solid);
        if (!geo.lines.empty()) {
            Point textCenter = c;
            if (node.shape == NodeShape::Cylinder) textCenter.y += CylinderRy(geo.w) * 0.5f;
            // A custom fill keeps its colour in both themes, so the text must contrast with it, not the theme.
            Paint color = Paint::Of(Role::Text);
            if (geo.style.color) {
                color = Paint::Rgb(*geo.style.color);
            } else if (geo.style.fill) {
                const uint32_t f = *geo.style.fill;
                const float luminance = 0.299f * static_cast<float>((f >> 16) & 0xFF) +
                                        0.587f * static_cast<float>((f >> 8) & 0xFF) + 0.114f * static_cast<float>(f & 0xFF);
                color = Paint::Rgb(luminance > 150.0f ? 0x1F2328u : 0xFFFFFFu);
            }
            DrawTextBlock(b, geo.lines, textCenter, kNodeFont, color);
        }
    }

    return scene;
}

} // namespace Pluma::Diagram::detail
