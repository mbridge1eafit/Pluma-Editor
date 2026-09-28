#include "markdown_format.h"

#include <utility>

namespace Pluma::Editor::MarkdownFormat {

namespace {

bool IsSpace(char c) {
    return c == ' ' || c == '\t';
}

bool IsBlank(std::string_view line) {
    for (char c : line) {
        if (!IsSpace(c)) return false;
    }
    return true;
}

size_t LeadingWhitespace(std::string_view line) {
    size_t i = 0;
    while (i < line.size() && IsSpace(line[i])) ++i;
    return i;
}

// Length of a single "> " marker (up to 3 spaces of indentation), or 0.
size_t QuoteMarkerLength(std::string_view line) {
    size_t i = 0;
    while (i < line.size() && i < 3 && line[i] == ' ') ++i;
    if (i >= line.size() || line[i] != '>') return 0;
    ++i;
    if (i < line.size() && IsSpace(line[i])) ++i;
    return i;
}

// Length of every nested quote marker ("> > "), so list and heading rules apply inside quotes.
size_t QuotePrefixLength(std::string_view line) {
    size_t total = 0;
    while (const size_t len = QuoteMarkerLength(line.substr(total))) total += len;
    return total;
}

struct ListMarker {
    bool found = false;
    bool ordered = false;
    bool task = false;
    size_t indent = 0;       // Leading whitespace
    size_t contentStart = 0; // After "- ", "1. " and the task box, if any
};

ListMarker ParseListMarker(std::string_view line) {
    ListMarker m;
    m.indent = LeadingWhitespace(line);
    size_t j = m.indent;
    if (j >= line.size()) return m;
    if (line[j] == '-' || line[j] == '*' || line[j] == '+') {
        ++j;
    } else {
        const size_t digits = j;
        while (j < line.size() && j - digits < 9 && line[j] >= '0' && line[j] <= '9') ++j;
        if (j == digits || j >= line.size() || (line[j] != '.' && line[j] != ')')) return m;
        ++j;
        m.ordered = true;
    }
    // "**bold**", "-->" or "1.5" are text, not list items.
    if (j < line.size() && !IsSpace(line[j])) return m;
    if (j < line.size()) ++j;
    m.found = true;
    m.contentStart = j;

    const std::string_view rest = line.substr(j);
    if (rest.size() >= 3 && rest[0] == '[' && (rest[1] == ' ' || rest[1] == 'x' || rest[1] == 'X') &&
        rest[2] == ']' && (rest.size() == 3 || IsSpace(rest[3]))) {
        m.task = true;
        m.contentStart = j + (rest.size() > 3 ? 4 : 3);
    }
    return m;
}

bool HasListKind(std::string_view body, LineKind kind) {
    const ListMarker m = ParseListMarker(body);
    switch (kind) {
    case LineKind::Bullet: return m.found && !m.ordered && !m.task;
    case LineKind::Numbered: return m.found && m.ordered && !m.task;
    case LineKind::Task: return m.found && m.task;
    default: return false;
    }
}

std::vector<std::string> ToggleQuote(const std::vector<std::string>& lines, const std::vector<bool>& target,
                                     bool remove, size_t first, size_t last) {
    std::vector<std::string> out = lines;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (remove) {
            if (target[i]) out[i] = lines[i].substr(QuoteMarkerLength(lines[i]));
        } else if (target[i]) {
            out[i] = (IsBlank(lines[i]) && lines.size() > 1 ? ">" : "> ") + lines[i];
        } else if (i > first && i < last) {
            out[i] = ">"; // Blank lines inside the selection keep it a single blockquote
        }
    }
    return out;
}

} // namespace

int HeadingLevel(std::string_view line) {
    size_t i = 0;
    while (i < line.size() && i < 3 && line[i] == ' ') ++i;
    int level = 0;
    while (i < line.size() && line[i] == '#') {
        ++level;
        ++i;
    }
    if (level == 0 || level > 6) return 0;
    return (i == line.size() || IsSpace(line[i])) ? level : 0;
}

std::string SetHeadingLevel(std::string_view line, int level) {
    const size_t quote = QuotePrefixLength(line);
    const std::string_view prefix = line.substr(0, quote);
    std::string_view body = line.substr(quote);

    if (HeadingLevel(body) > 0) {
        body.remove_prefix(body.find('#'));
        while (!body.empty() && body.front() == '#') body.remove_prefix(1);
    }
    body.remove_prefix(LeadingWhitespace(body));

    std::string out(prefix);
    if (level > 0) {
        out.append(static_cast<size_t>(level < 6 ? level : 6), '#');
        out += ' ';
    }
    out += body;
    return out;
}

bool HasLineKind(std::string_view line, LineKind kind) {
    if (kind == LineKind::Quote) return QuoteMarkerLength(line) > 0;
    return HasListKind(line.substr(QuotePrefixLength(line)), kind);
}

std::vector<std::string> ToggleLineKind(const std::vector<std::string>& lines, LineKind kind) {
    // A lone line is a target even when blank, so the marker can be typed after.
    std::vector<bool> target(lines.size());
    size_t first = lines.size();
    size_t last = 0;
    bool allHave = true;
    for (size_t i = 0; i < lines.size(); ++i) {
        target[i] = lines.size() == 1 || !IsBlank(lines[i]);
        if (!target[i]) continue;
        if (first == lines.size()) first = i;
        last = i;
        allHave = allHave && HasLineKind(lines[i], kind);
    }
    if (first == lines.size()) return lines;

    if (kind == LineKind::Quote) return ToggleQuote(lines, target, allHave, first, last);

    std::vector<std::string> out = lines;
    std::vector<std::pair<size_t, int>> numbering; // (indent, last number) per nesting level
    for (size_t i = 0; i < lines.size(); ++i) {
        if (!target[i]) continue;
        const std::string_view line = lines[i];
        const size_t quote = QuotePrefixLength(line);
        const std::string_view body = line.substr(quote);
        const ListMarker m = ParseListMarker(body);
        const std::string_view head = line.substr(0, quote + m.indent);
        const std::string_view content = body.substr(m.found ? m.contentStart : m.indent);

        if (allHave) {
            out[i] = std::string(head) + std::string(content);
            continue;
        }
        if (kind != LineKind::Numbered && HasListKind(body, kind)) continue; // Keeps "*" bullets and "[x]"

        std::string marker;
        switch (kind) {
        case LineKind::Bullet:
            marker = "- ";
            break;
        case LineKind::Task:
            marker = "- [ ] ";
            break;
        default: {
            while (!numbering.empty() && numbering.back().first > m.indent) numbering.pop_back();
            if (!numbering.empty() && numbering.back().first == m.indent) {
                ++numbering.back().second;
            } else {
                numbering.emplace_back(m.indent, 1);
            }
            marker = std::to_string(numbering.back().second) + ". ";
            break;
        }
        }
        out[i] = std::string(head) + marker + std::string(content);
    }
    return out;
}

LineState GetLineState(std::string_view line) {
    LineState state;
    const std::string_view body = line.substr(QuotePrefixLength(line));
    state.headingLevel = HeadingLevel(body);
    state.quote = QuoteMarkerLength(line) > 0;
    state.bullet = HasListKind(body, LineKind::Bullet);
    state.numbered = HasListKind(body, LineKind::Numbered);
    state.task = HasListKind(body, LineKind::Task);
    return state;
}

std::string_view MermaidTemplate(DiagramKind kind) {
    switch (kind) {
    case DiagramKind::Sequence:
        return "sequenceDiagram\n"
               "    Usuario->>Sistema: Solicitud\n"
               "    Sistema-->>Usuario: Respuesta";
    case DiagramKind::State:
        return "stateDiagram-v2\n"
               "    [*] --> Inactivo\n"
               "    Inactivo --> Activo: iniciar\n"
               "    Activo --> Inactivo: detener\n"
               "    Activo --> [*]";
    case DiagramKind::Pie:
        return "pie title Distribución\n"
               "    \"A\" : 45\n"
               "    \"B\" : 30\n"
               "    \"C\" : 25";
    default:
        return "flowchart TD\n"
               "    A[Inicio] --> B{¿Condición?}\n"
               "    B -->|Sí| C[Acción]\n"
               "    B -->|No| D[Fin]\n"
               "    C --> D";
    }
}

} // namespace Pluma::Editor::MarkdownFormat
