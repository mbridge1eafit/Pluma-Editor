#include "mermaid_internal.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Pluma::Diagram::detail {

namespace {

constexpr float kRadius = 110.0f;
constexpr float kLegendFont = 13.5f;
constexpr float kMargin = 10.0f;
constexpr float kPi = 3.14159265f;

struct Slice {
    std::string label;
    double value = 0.0;
};

std::string FormatNumber(double value) {
    char buffer[32];
    if (std::fabs(value - std::round(value)) < 1.0e-9) {
        std::snprintf(buffer, sizeof(buffer), "%.0f", value);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.2f", value);
        std::string s(buffer);
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
        return s;
    }
    return buffer;
}

} // namespace

bool BuildPieScene(const std::vector<std::string_view>& lines, const MeasureText& measure, Scene& scene,
                   std::string& error) {
    std::string title;
    bool showData = false;
    std::vector<Slice> slices;

    // Header: pie [showData] [title Text]
    std::string_view header = Trim(lines[0].substr(3));
    if (StartsWithKeyword(header, "showData")) {
        showData = true;
        header = Trim(header.substr(8));
    }
    if (StartsWithKeyword(header, "title")) title = CleanLabel(header.substr(5));

    for (size_t l = 1; l < lines.size(); ++l) {
        const std::string_view line = lines[l];
        if (StartsWithKeyword(line, "title")) {
            title = CleanLabel(line.substr(5));
            continue;
        }
        if (StartsWithKeyword(line, "showData")) {
            showData = true;
            continue;
        }
        if (StartsWithKeyword(line, "accTitle") || StartsWithKeyword(line, "accDescr")) continue;
        const size_t colon = line.rfind(':');
        if (colon == std::string_view::npos) {
            error = "Sección sin valor (falta ':'): " + std::string(line);
            return false;
        }
        const std::string valueText(Trim(line.substr(colon + 1)));
        char* end = nullptr;
        const double value = std::strtod(valueText.c_str(), &end);
        if (end == valueText.c_str()) {
            error = "Valor no numérico: " + std::string(line);
            return false;
        }
        if (value > 0.0) slices.push_back(Slice{CleanLabel(line.substr(0, colon)), value});
    }
    if (slices.empty()) {
        error = "El gráfico circular no tiene valores positivos";
        return false;
    }

    double total = 0.0;
    for (const auto& s : slices) total += s.value;

    SceneBuilder b(scene);
    const Point center{kMargin + kRadius, kMargin + kRadius};
    const Paint separator = Paint::Of(Role::Background);

    float angle = -kPi * 0.5f; // 12 o'clock, clockwise
    for (size_t i = 0; i < slices.size(); ++i) {
        const float sweep = static_cast<float>(slices[i].value / total) * 2.0f * kPi;
        const Paint fill = Paint::Of(PieRole(i));
        if (slices.size() == 1) {
            b.Ellipse(center.x, center.y, kRadius, kRadius, fill, separator, 1.5f);
        } else {
            const Point start{center.x + kRadius * std::cos(angle), center.y + kRadius * std::sin(angle)};
            std::vector<Segment> segments{StraightSegment(center, start)};
            AppendArc(segments, center, kRadius, kRadius, angle, angle + sweep);
            segments.push_back(StraightSegment(segments.back().end, center));
            b.Path(center, std::move(segments), fill, separator, 1.5f, true);
        }
        angle += sweep;
    }

    // Percentages inside the slices that are big enough to hold them.
    angle = -kPi * 0.5f;
    for (size_t i = 0; i < slices.size(); ++i) {
        const double fraction = slices[i].value / total;
        const float sweep = static_cast<float>(fraction) * 2.0f * kPi;
        if (fraction >= 0.05) {
            const float mid = angle + sweep * 0.5f;
            const float r = slices.size() == 1 ? 0.0f : kRadius * 0.64f;
            char text[16];
            std::snprintf(text, sizeof(text), "%.0f%%", fraction * 100.0);
            b.Text(text, center.x + r * std::cos(mid), center.y + r * std::sin(mid), 12.5f, Paint::Of(Role::PieText), true);
        }
        angle += sweep;
    }

    // Legend.
    const float lineH = LineHeight(kLegendFont) + 6.0f;
    const float legendX = center.x + kRadius + 28.0f;
    float y = center.y - lineH * static_cast<float>(slices.size()) * 0.5f + lineH * 0.5f;
    for (size_t i = 0; i < slices.size(); ++i) {
        b.Rect(legendX, y - 7.0f, 14.0f, 14.0f, Paint::Of(PieRole(i)), Paint{}, 1.0f, 3.0f);
        std::string label = slices[i].label;
        if (showData) label += " [" + FormatNumber(slices[i].value) + "]";
        b.Text(std::move(label), legendX + 22.0f, y, kLegendFont, Paint::Of(Role::Text), false, TextAlign::Left);
        y += lineH;
    }

    FitSceneToContent(scene, measure, kMargin);
    AddSceneTitle(scene, title, measure, kMargin);
    return true;
}

} // namespace Pluma::Diagram::detail
