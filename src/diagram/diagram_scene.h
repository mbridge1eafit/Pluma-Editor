#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// Backend-independent vector scene produced by the diagram engines (Mermaid).
// The preview draws it with Direct2D, the HTML exporter writes it as inline SVG
// and the PDF exporter emits it as vector operators. Coordinates are in DIPs,
// y grows downwards, origin at the top-left corner of the diagram.
namespace Pluma::Diagram {

// Semantic colour of a primitive; each backend resolves it for its theme, so a
// scene can be rendered light or dark without being rebuilt.
enum class Role : uint8_t {
    None,           // Not painted
    Custom,         // Explicit RGB from the diagram source (style / classDef)
    Background,     // Page background
    Text,
    MutedText,
    Line,           // Edges, messages, lifelines
    NodeFill,
    NodeStroke,
    ClusterFill,
    ClusterStroke,
    LabelBackground,// Behind edge labels
    NoteFill,
    NoteStroke,
    ActorFill,
    ActorStroke,
    ActivationFill,
    BlockStroke,    // Sequence loop/alt frames
    BlockLabelFill,
    Marker,         // Solid markers (state start, fork bars)
    PieText,
    Pie0, Pie1, Pie2, Pie3, Pie4, Pie5, Pie6, Pie7, Pie8, Pie9, Pie10, Pie11,
    Count
};

constexpr int kPieColorCount = 12;
constexpr int kRoleCount = static_cast<int>(Role::Count);

inline Role PieRole(size_t index) {
    return static_cast<Role>(static_cast<int>(Role::Pie0) + static_cast<int>(index % kPieColorCount));
}

struct Paint {
    Role role = Role::None;
    uint32_t rgb = 0; // 0xRRGGBB, only meaningful when role == Role::Custom

    static Paint Of(Role r) { return Paint{r, 0}; }
    static Paint Rgb(uint32_t value) { return Paint{Role::Custom, value}; }
    bool IsNone() const noexcept { return role == Role::None; }
};

// Resolved colours for every role of one theme.
struct Palette {
    uint32_t colors[kRoleCount] = {};

    uint32_t Resolve(const Paint& paint) const noexcept {
        return paint.role == Role::Custom ? paint.rgb : colors[static_cast<int>(paint.role)];
    }

    static Palette Light();
    static Palette Dark();
};

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

// Cubic Bézier segment starting at the previous end point. Straight lines use
// c1 == previous point and c2 == end.
struct Segment {
    Point c1;
    Point c2;
    Point end;
};

enum class PrimitiveKind : uint8_t { Rect, Ellipse, Path, Text };
enum class LineStyle : uint8_t { Solid, Dashed, Dotted };
enum class TextAlign : uint8_t { Left, Center, Right };

struct Primitive {
    PrimitiveKind kind = PrimitiveKind::Rect;
    Paint fill;               // Text colour for Text primitives
    Paint stroke;
    float strokeWidth = 1.0f;
    LineStyle lineStyle = LineStyle::Solid;

    // Rect: top-left (x, y), size (w, h), corner radius.
    // Ellipse: centre (x, y), radii (w, h).
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float radius = 0.0f;

    // Path
    Point start;
    std::vector<Segment> segments;
    bool closed = false;

    // Text (single line): x is the anchor given by `align`, y the vertical centre of the line.
    std::string text; // UTF-8
    float fontSize = 14.0f;
    bool bold = false;
    TextAlign align = TextAlign::Center;
};

struct Scene {
    float width = 0.0f;
    float height = 0.0f;
    std::vector<Primitive> items; // Painter's order
};

// Width in DIPs of a single line of UTF-8 text at `fontSize`. Each backend
// supplies the metrics of the font it will draw with, so labels always fit.
using MeasureText = std::function<float(std::string_view utf8, float fontSize, bool bold)>;

// Font-independent estimate based on Helvetica/Arial metrics (used for SVG and tests).
float EstimateTextWidth(std::string_view utf8, float fontSize, bool bold);

// Height of one text line at `fontSize` used by every diagram layout.
constexpr float LineHeight(float fontSize) { return fontSize * 1.3f; }

// --- Builders shared by the engines and backends ---------------------------------

class SceneBuilder {
public:
    explicit SceneBuilder(Scene& scene) : m_scene(scene) {}

    Primitive& Rect(float x, float y, float w, float h, Paint fill, Paint stroke, float strokeWidth = 1.0f,
                    float radius = 0.0f);
    Primitive& Ellipse(float cx, float cy, float rx, float ry, Paint fill, Paint stroke, float strokeWidth = 1.0f);
    Primitive& Line(Point a, Point b, Paint stroke, float strokeWidth = 1.0f, LineStyle style = LineStyle::Solid);
    Primitive& Polygon(const std::vector<Point>& points, Paint fill, Paint stroke, float strokeWidth = 1.0f);
    Primitive& Path(Point start, std::vector<Segment> segments, Paint fill, Paint stroke, float strokeWidth = 1.0f,
                    bool closed = false);
    Primitive& Text(std::string text, float x, float y, float fontSize, Paint color, bool bold = false,
                    TextAlign align = TextAlign::Center);

    // Filled arrowhead whose tip is at `tip`, pointing along `from` -> `tip`.
    void ArrowHead(Point from, Point tip, float size, Paint paint);

private:
    Scene& m_scene;
};

Segment StraightSegment(Point from, Point to);

// Appends Bézier segments approximating an elliptical arc (angles in radians, y down).
void AppendArc(std::vector<Segment>& out, Point center, float rx, float ry, float startAngle, float endAngle);

} // namespace Pluma::Diagram
