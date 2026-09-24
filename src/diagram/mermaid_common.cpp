#include "mermaid_internal.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>

namespace Pluma::Diagram::detail {

namespace {

bool IsIdentifierChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
}

std::string ReplaceAll(std::string text, std::string_view from, std::string_view to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x110000) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Decodes `&name;` (HTML) and `#name;` (Mermaid) entities.
std::string DecodeEntities(std::string_view text) {
    static constexpr std::pair<std::string_view, uint32_t> kNamed[] = {
        {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''}, {"nbsp", 0xA0},
        {"copy", 0xA9}, {"reg", 0xAE}, {"deg", 0xB0}, {"hellip", 0x2026}, {"mdash", 0x2014},
        {"ndash", 0x2013}, {"larr", 0x2190}, {"rarr", 0x2192}, {"uarr", 0x2191}, {"darr", 0x2193},
        {"hearts", 0x2665}, {"check", 0x2713}, {"infin", 0x221E}};

    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if ((c == '&' || c == '#') && i + 1 < text.size()) {
            const size_t semi = text.find(';', i + 1);
            if (semi != std::string_view::npos && semi - i <= 10) {
                std::string_view name = text.substr(i + 1, semi - i - 1);
                uint32_t cp = 0;
                bool ok = false;
                if (!name.empty() && name[0] == '#') name.remove_prefix(1); // &#65; / #35;
                if (!name.empty() && std::isdigit(static_cast<unsigned char>(name[0]))) {
                    ok = std::from_chars(name.data(), name.data() + name.size(), cp).ec == std::errc{};
                } else if (name.size() > 1 && (name[0] == 'x' || name[0] == 'X')) {
                    ok = std::from_chars(name.data() + 1, name.data() + name.size(), cp, 16).ec == std::errc{};
                } else {
                    for (const auto& [n, value] : kNamed) {
                        if (n == name) {
                            cp = value;
                            ok = true;
                            break;
                        }
                    }
                }
                if (ok && cp > 0) {
                    AppendUtf8(out, cp);
                    i = semi;
                    continue;
                }
            }
        }
        out.push_back(c);
    }
    return out;
}

} // namespace

std::string_view Trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

bool StartsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool StartsWithKeyword(std::string_view s, std::string_view word) {
    if (!StartsWith(s, word)) return false;
    return s.size() == word.size() || !IsIdentifierChar(s[word.size()]);
}

std::string ToLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::vector<std::string_view> SourceLines(std::string_view source) {
    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start <= source.size()) {
        size_t end = source.find('\n', start);
        if (end == std::string_view::npos) end = source.size();
        std::string_view line = Trim(source.substr(start, end - start));
        if (!line.empty() && !StartsWith(line, "%%")) {
            lines.push_back(line);
        }
        start = end + 1;
    }
    return lines;
}

std::string CleanLabel(std::string_view raw) {
    std::string_view s = Trim(raw);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = Trim(s.substr(1, s.size() - 2));
    }
    if (s.size() >= 2 && s.front() == '`' && s.back() == '`') { // Markdown strings
        s = Trim(s.substr(1, s.size() - 2));
    }
    std::string text(s);
    for (std::string_view br : {"<br/>", "<br />", "<br>", "<BR>", "<BR/>"}) {
        text = ReplaceAll(std::move(text), br, "\n");
    }
    text = ReplaceAll(std::move(text), "**", "");
    return DecodeEntities(text);
}

std::vector<std::string> WrapLabel(const std::string& text, const MeasureText& measure, float fontSize, bool bold,
                                   float maxWidth) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        const std::string_view paragraph = Trim(std::string_view(text).substr(start, end - start));

        std::string current;
        size_t pos = 0;
        while (pos < paragraph.size()) {
            size_t space = paragraph.find(' ', pos);
            if (space == std::string_view::npos) space = paragraph.size();
            const std::string_view word = paragraph.substr(pos, space - pos);
            pos = space + 1;
            if (word.empty()) continue;
            std::string candidate = current.empty() ? std::string(word) : current + " " + std::string(word);
            if (!current.empty() && measure(candidate, fontSize, bold) > maxWidth) {
                lines.push_back(std::move(current));
                current = std::string(word);
            } else {
                current = std::move(candidate);
            }
        }
        lines.push_back(std::move(current));
        start = end + 1;
    }
    // Drop trailing empty lines but keep at least one.
    while (lines.size() > 1 && lines.back().empty()) lines.pop_back();
    return lines;
}

float WidestLine(const std::vector<std::string>& lines, const MeasureText& measure, float fontSize, bool bold) {
    float width = 0.0f;
    for (const auto& line : lines) {
        width = (std::max)(width, measure(line, fontSize, bold));
    }
    return width;
}

std::optional<uint32_t> ParseColor(std::string_view text) {
    text = Trim(text);
    if (text.empty()) return std::nullopt;
    if (text.front() == '#') {
        std::string_view hex = text.substr(1);
        uint32_t value = 0;
        if (std::from_chars(hex.data(), hex.data() + hex.size(), value, 16).ec != std::errc{}) return std::nullopt;
        if (hex.size() == 3) {
            const uint32_t r = (value >> 8) & 0xF;
            const uint32_t g = (value >> 4) & 0xF;
            const uint32_t b = value & 0xF;
            return (r * 17u << 16) | (g * 17u << 8) | (b * 17u);
        }
        if (hex.size() == 6) return value;
        if (hex.size() == 8) return value >> 8; // #rrggbbaa: alpha ignored
        return std::nullopt;
    }
    const std::string lower = ToLower(text);
    if (StartsWith(lower, "rgb")) {
        const size_t open = lower.find('(');
        const size_t close = lower.find(')');
        if (open == std::string::npos || close == std::string::npos || close < open) return std::nullopt;
        uint32_t channels[3] = {0, 0, 0};
        size_t pos = open + 1;
        for (int i = 0; i < 3; ++i) {
            while (pos < close && (lower[pos] == ' ' || lower[pos] == ',')) ++pos;
            unsigned value = 0;
            const auto res = std::from_chars(lower.data() + pos, lower.data() + close, value);
            if (res.ec != std::errc{}) return std::nullopt;
            channels[i] = (std::min)(255u, value);
            pos = static_cast<size_t>(res.ptr - lower.data());
        }
        return (channels[0] << 16) | (channels[1] << 8) | channels[2];
    }
    static constexpr std::pair<std::string_view, uint32_t> kNames[] = {
        {"black", 0x000000}, {"white", 0xFFFFFF}, {"red", 0xFF0000}, {"green", 0x008000}, {"blue", 0x0000FF},
        {"yellow", 0xFFFF00}, {"orange", 0xFFA500}, {"purple", 0x800080}, {"pink", 0xFFC0CB},
        {"gray", 0x808080}, {"grey", 0x808080}, {"lightgray", 0xD3D3D3}, {"lightgrey", 0xD3D3D3},
        {"darkgray", 0xA9A9A9}, {"lightblue", 0xADD8E6}, {"lightgreen", 0x90EE90}, {"cyan", 0x00FFFF},
        {"magenta", 0xFF00FF}, {"brown", 0xA52A2A}, {"navy", 0x000080}, {"teal", 0x008080},
        {"lime", 0x00FF00}, {"gold", 0xFFD700}, {"salmon", 0xFA8072}, {"coral", 0xFF7F50},
        {"tomato", 0xFF6347}, {"violet", 0xEE82EE}, {"indigo", 0x4B0082}, {"silver", 0xC0C0C0},
        {"beige", 0xF5F5DC}, {"lavender", 0xE6E6FA}, {"khaki", 0xF0E68C}, {"olive", 0x808000},
        {"maroon", 0x800000}, {"aqua", 0x00FFFF}, {"crimson", 0xDC143C}, {"darkgreen", 0x006400},
        {"darkblue", 0x00008B}, {"darkred", 0x8B0000}, {"lightyellow", 0xFFFFE0}, {"orchid", 0xDA70D6}};
    for (const auto& [name, value] : kNames) {
        if (lower == name) return value;
    }
    return std::nullopt;
}

namespace {

void TranslateScene(Scene& scene, float dx, float dy) {
    for (auto& item : scene.items) {
        item.x += dx;
        item.y += dy;
        item.start.x += dx;
        item.start.y += dy;
        for (auto& s : item.segments) {
            s.c1.x += dx;
            s.c1.y += dy;
            s.c2.x += dx;
            s.c2.y += dy;
            s.end.x += dx;
            s.end.y += dy;
        }
    }
}

} // namespace

void FitSceneToContent(Scene& scene, const MeasureText& measure, float margin) {
    float minX = 1.0e9f;
    float minY = 1.0e9f;
    float maxX = -1.0e9f;
    float maxY = -1.0e9f;
    auto include = [&](float x0, float y0, float x1, float y1) {
        minX = (std::min)(minX, x0);
        minY = (std::min)(minY, y0);
        maxX = (std::max)(maxX, x1);
        maxY = (std::max)(maxY, y1);
    };
    for (const auto& item : scene.items) {
        const float half = item.stroke.IsNone() ? 0.0f : item.strokeWidth * 0.5f;
        switch (item.kind) {
        case PrimitiveKind::Rect:
            include(item.x - half, item.y - half, item.x + item.w + half, item.y + item.h + half);
            break;
        case PrimitiveKind::Ellipse:
            include(item.x - item.w - half, item.y - item.h - half, item.x + item.w + half, item.y + item.h + half);
            break;
        case PrimitiveKind::Path:
            include(item.start.x - half, item.start.y - half, item.start.x + half, item.start.y + half);
            for (const auto& s : item.segments) {
                for (const Point& p : {s.c1, s.c2, s.end}) include(p.x - half, p.y - half, p.x + half, p.y + half);
            }
            break;
        case PrimitiveKind::Text: {
            const float w = measure(item.text, item.fontSize, item.bold);
            const float x0 = item.align == TextAlign::Left ? item.x
                             : item.align == TextAlign::Right ? item.x - w
                                                              : item.x - w * 0.5f;
            const float h = LineHeight(item.fontSize) * 0.5f;
            include(x0, item.y - h, x0 + w, item.y + h);
            break;
        }
        }
    }
    if (minX > maxX) {
        scene.width = scene.height = 0.0f;
        return;
    }
    TranslateScene(scene, margin - minX, margin - minY);
    scene.width = maxX - minX + 2.0f * margin;
    scene.height = maxY - minY + 2.0f * margin;
}

void AddSceneTitle(Scene& scene, const std::string& title, const MeasureText& measure, float margin) {
    if (title.empty()) return;
    constexpr float kTitleFont = 16.0f;
    const float titleWidth = measure(title, kTitleFont, true);
    const float titleHeight = LineHeight(kTitleFont) + 8.0f;
    const float width = (std::max)(scene.width, titleWidth + 2.0f * margin);
    TranslateScene(scene, (width - scene.width) * 0.5f, titleHeight);
    scene.width = width;
    scene.height += titleHeight;
    SceneBuilder(scene).Text(title, width * 0.5f, margin + LineHeight(kTitleFont) * 0.5f, kTitleFont,
                             Paint::Of(Role::Text), true);
}

void StyleSpec::Merge(const StyleSpec& other) {
    if (other.fill) fill = other.fill;
    if (other.stroke) stroke = other.stroke;
    if (other.color) color = other.color;
    if (other.strokeWidth) strokeWidth = other.strokeWidth;
    dashed = dashed || other.dashed;
}

StyleSpec ParseStyleSpec(std::string_view spec) {
    StyleSpec style;
    size_t pos = 0;
    while (pos < spec.size()) {
        // Commas inside rgb(...) do not separate declarations.
        size_t end = pos;
        int depth = 0;
        while (end < spec.size() && (spec[end] != ',' || depth > 0)) {
            if (spec[end] == '(') ++depth;
            if (spec[end] == ')') --depth;
            ++end;
        }
        const std::string_view decl = Trim(spec.substr(pos, end - pos));
        pos = end + 1;
        const size_t colon = decl.find(':');
        if (colon == std::string_view::npos) continue;
        const std::string key = ToLower(Trim(decl.substr(0, colon)));
        std::string_view value = Trim(decl.substr(colon + 1));
        if (!value.empty() && value.back() == ';') value.remove_suffix(1);
        if (key == "fill" || key == "background" || key == "background-color") {
            style.fill = ParseColor(value);
        } else if (key == "stroke" || key == "border-color") {
            style.stroke = ParseColor(value);
        } else if (key == "color") {
            style.color = ParseColor(value);
        } else if (key == "stroke-width") {
            style.strokeWidth = std::clamp(static_cast<float>(std::atof(std::string(value).c_str())), 0.0f, 12.0f);
        } else if (key == "stroke-dasharray") {
            style.dashed = true;
        }
    }
    return style;
}

} // namespace Pluma::Diagram::detail
