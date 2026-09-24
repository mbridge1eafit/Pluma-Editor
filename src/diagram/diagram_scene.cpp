#include "diagram_scene.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Pluma::Diagram {

namespace {

// Glyph widths (1/1000 em) for ASCII 32..126, from the Adobe Helvetica AFM files
// (Arial shares the same advance widths).
constexpr std::array<short, 95> kHelveticaWidths = {
    278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556,
    1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556,
    333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,
    556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584};

constexpr std::array<short, 95> kHelveticaBoldWidths = {
    278, 333, 474, 556, 556, 889, 722, 238, 333, 333, 389, 584, 278, 333, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 333, 333, 584, 584, 584, 611,
    975, 722, 722, 722, 722, 667, 611, 778, 722, 278, 556, 722, 611, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 333, 278, 333, 584, 556,
    333, 556, 611, 556, 611, 556, 333, 611, 611, 278, 278, 556, 278, 889, 611, 611,
    611, 611, 389, 556, 333, 611, 556, 778, 556, 556, 500, 389, 280, 389, 584};

void Set(Palette& p, Role role, uint32_t rgb) {
    p.colors[static_cast<int>(role)] = rgb;
}

// Categorical slice colours, dark enough for white percentage labels.
constexpr uint32_t kPieColors[kPieColorCount] = {
    0x4E79A7, 0xE8702A, 0xD1495B, 0x3E9C98, 0x59A14F, 0xC9A227,
    0x9A6FB0, 0xE0698A, 0x8C6D52, 0x7F8C8D, 0x2E86AB, 0xB5577E};

} // namespace

Palette Palette::Light() {
    Palette p;
    Set(p, Role::Background, 0xFFFFFF);
    Set(p, Role::Text, 0x1F2328);
    Set(p, Role::MutedText, 0x59636E);
    Set(p, Role::Line, 0x57606A);
    Set(p, Role::NodeFill, 0xEEF4FE);
    Set(p, Role::NodeStroke, 0x5B8DEF);
    Set(p, Role::ClusterFill, 0xF6F8FA);
    Set(p, Role::ClusterStroke, 0xC5CED8);
    Set(p, Role::LabelBackground, 0xFFFFFF);
    Set(p, Role::NoteFill, 0xFFF8C5);
    Set(p, Role::NoteStroke, 0xD4A72C);
    Set(p, Role::ActorFill, 0xEEF4FE);
    Set(p, Role::ActorStroke, 0x5B8DEF);
    Set(p, Role::ActivationFill, 0xEAEEF2);
    Set(p, Role::BlockStroke, 0x8C959F);
    Set(p, Role::BlockLabelFill, 0xEAEEF2);
    Set(p, Role::Marker, 0x1F2328);
    Set(p, Role::PieText, 0xFFFFFF);
    for (int i = 0; i < kPieColorCount; ++i) {
        Set(p, PieRole(static_cast<size_t>(i)), kPieColors[i]);
    }
    return p;
}

Palette Palette::Dark() {
    Palette p;
    Set(p, Role::Background, 0x1E1E1E);
    Set(p, Role::Text, 0xD4D4D4);
    Set(p, Role::MutedText, 0x9198A1);
    Set(p, Role::Line, 0x9198A1);
    Set(p, Role::NodeFill, 0x1C2A40);
    Set(p, Role::NodeStroke, 0x4F7FD6);
    Set(p, Role::ClusterFill, 0x252526);
    Set(p, Role::ClusterStroke, 0x484F58);
    Set(p, Role::LabelBackground, 0x1E1E1E);
    Set(p, Role::NoteFill, 0x3A3423);
    Set(p, Role::NoteStroke, 0x9E8A3F);
    Set(p, Role::ActorFill, 0x1C2A40);
    Set(p, Role::ActorStroke, 0x4F7FD6);
    Set(p, Role::ActivationFill, 0x2D333B);
    Set(p, Role::BlockStroke, 0x6E7681);
    Set(p, Role::BlockLabelFill, 0x2D333B);
    Set(p, Role::Marker, 0xD4D4D4);
    Set(p, Role::PieText, 0xFFFFFF);
    for (int i = 0; i < kPieColorCount; ++i) {
        Set(p, PieRole(static_cast<size_t>(i)), kPieColors[i]);
    }
    return p;
}

float EstimateTextWidth(std::string_view utf8, float fontSize, bool bold) {
    const auto& table = bold ? kHelveticaBoldWidths : kHelveticaWidths;
    int units = 0;
    for (size_t i = 0; i < utf8.size();) {
        const auto c = static_cast<unsigned char>(utf8[i]);
        uint32_t cp = c;
        size_t len = 1;
        if (c >= 0xF0 && i + 3 < utf8.size()) {
            cp = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu) << 12) |
                 ((static_cast<unsigned char>(utf8[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(utf8[i + 3]) & 0x3Fu);
            len = 4;
        } else if (c >= 0xE0 && i + 2 < utf8.size()) {
            cp = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu) << 6) |
                 (static_cast<unsigned char>(utf8[i + 2]) & 0x3Fu);
            len = 3;
        } else if (c >= 0xC0 && i + 1 < utf8.size()) {
            cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu);
            len = 2;
        }
        i += len;

        if (cp >= 32 && cp <= 126) {
            units += table[cp - 32];
        } else if (cp == 0x200D || (cp >= 0xFE00 && cp <= 0xFE0F) || cp < 32) {
            // Zero-width joiners, variation selectors and controls take no room.
        } else if (cp >= 0x2E80) {
            units += 1000; // CJK and emoji are full width
        } else {
            units += bold ? 611 : 556; // Accented Latin letters and symbols
        }
    }
    return static_cast<float>(units) * fontSize / 1000.0f;
}

Primitive& SceneBuilder::Rect(float x, float y, float w, float h, Paint fill, Paint stroke, float strokeWidth,
                              float radius) {
    Primitive p;
    p.kind = PrimitiveKind::Rect;
    p.x = x;
    p.y = y;
    p.w = w;
    p.h = h;
    p.radius = radius;
    p.fill = fill;
    p.stroke = stroke;
    p.strokeWidth = strokeWidth;
    m_scene.items.push_back(std::move(p));
    return m_scene.items.back();
}

Primitive& SceneBuilder::Ellipse(float cx, float cy, float rx, float ry, Paint fill, Paint stroke, float strokeWidth) {
    Primitive p;
    p.kind = PrimitiveKind::Ellipse;
    p.x = cx;
    p.y = cy;
    p.w = rx;
    p.h = ry;
    p.fill = fill;
    p.stroke = stroke;
    p.strokeWidth = strokeWidth;
    m_scene.items.push_back(std::move(p));
    return m_scene.items.back();
}

Primitive& SceneBuilder::Line(Point a, Point b, Paint stroke, float strokeWidth, LineStyle style) {
    Primitive& p = Path(a, {StraightSegment(a, b)}, Paint{}, stroke, strokeWidth, false);
    p.lineStyle = style;
    return p;
}

Primitive& SceneBuilder::Polygon(const std::vector<Point>& points, Paint fill, Paint stroke, float strokeWidth) {
    std::vector<Segment> segments;
    for (size_t i = 1; i < points.size(); ++i) {
        segments.push_back(StraightSegment(points[i - 1], points[i]));
    }
    return Path(points.empty() ? Point{} : points.front(), std::move(segments), fill, stroke, strokeWidth, true);
}

Primitive& SceneBuilder::Path(Point start, std::vector<Segment> segments, Paint fill, Paint stroke, float strokeWidth,
                              bool closed) {
    Primitive p;
    p.kind = PrimitiveKind::Path;
    p.start = start;
    p.segments = std::move(segments);
    p.closed = closed;
    p.fill = fill;
    p.stroke = stroke;
    p.strokeWidth = strokeWidth;
    m_scene.items.push_back(std::move(p));
    return m_scene.items.back();
}

Primitive& SceneBuilder::Text(std::string text, float x, float y, float fontSize, Paint color, bool bold,
                              TextAlign align) {
    Primitive p;
    p.kind = PrimitiveKind::Text;
    p.text = std::move(text);
    p.x = x;
    p.y = y;
    p.fontSize = fontSize;
    p.fill = color;
    p.bold = bold;
    p.align = align;
    m_scene.items.push_back(std::move(p));
    return m_scene.items.back();
}

void SceneBuilder::ArrowHead(Point from, Point tip, float size, Paint paint) {
    float dx = tip.x - from.x;
    float dy = tip.y - from.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0e-3f) {
        dx = 0.0f;
        dy = 1.0f;
    } else {
        dx /= len;
        dy /= len;
    }
    const float half = size * 0.45f;
    const Point base{tip.x - dx * size, tip.y - dy * size};
    Polygon({tip, {base.x - dy * half, base.y + dx * half}, {base.x + dy * half, base.y - dx * half}, tip}, paint,
            paint, 1.0f);
}

Segment StraightSegment(Point from, Point to) {
    return Segment{from, to, to};
}

void AppendArc(std::vector<Segment>& out, Point center, float rx, float ry, float startAngle, float endAngle) {
    constexpr float kQuarter = 1.5707963f;
    const float sweep = endAngle - startAngle;
    const int pieces = (std::max)(1, static_cast<int>(std::ceil(std::fabs(sweep) / kQuarter - 1.0e-4f)));
    const float step = sweep / static_cast<float>(pieces);
    const float k = 4.0f / 3.0f * std::tan(step / 4.0f);
    for (int i = 0; i < pieces; ++i) {
        const float a0 = startAngle + step * static_cast<float>(i);
        const float a1 = a0 + step;
        const Point p0{center.x + rx * std::cos(a0), center.y + ry * std::sin(a0)};
        const Point p3{center.x + rx * std::cos(a1), center.y + ry * std::sin(a1)};
        const Point c1{p0.x - k * rx * std::sin(a0), p0.y + k * ry * std::cos(a0)};
        const Point c2{p3.x + k * rx * std::sin(a1), p3.y - k * ry * std::cos(a1)};
        out.push_back(Segment{c1, c2, p3});
    }
}

} // namespace Pluma::Diagram
