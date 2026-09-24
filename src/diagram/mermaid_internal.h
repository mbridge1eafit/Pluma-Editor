#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "diagram_scene.h"
#include "graph_layout.h"

// Internal pieces of the Mermaid engine shared between its translation units.
namespace Pluma::Diagram::detail {

// --- text utilities -------------------------------------------------------------

std::string_view Trim(std::string_view s);
bool StartsWith(std::string_view s, std::string_view prefix);
// True when `s` starts with `word` followed by the end of the string or a non-identifier character.
bool StartsWithKeyword(std::string_view s, std::string_view word);
std::string ToLower(std::string_view s);

// Splits the source into statements: lines without `%%` comments, blank lines dropped.
std::vector<std::string_view> SourceLines(std::string_view source);

// Removes quotes and backticks, turns <br> into line breaks and decodes entities (&amp;, #quot;...).
std::string CleanLabel(std::string_view raw);

// Splits on explicit line breaks and wraps words so that no line exceeds `maxWidth`.
std::vector<std::string> WrapLabel(const std::string& text, const MeasureText& measure, float fontSize, bool bold,
                                   float maxWidth);
float WidestLine(const std::vector<std::string>& lines, const MeasureText& measure, float fontSize, bool bold);

// #rgb, #rrggbb, rgb(r,g,b) or a basic CSS colour name.
std::optional<uint32_t> ParseColor(std::string_view text);

// Subset of the CSS-like styles accepted by `style`, `classDef` and `linkStyle`.
struct StyleSpec {
    std::optional<uint32_t> fill;
    std::optional<uint32_t> stroke;
    std::optional<uint32_t> color;
    std::optional<float> strokeWidth;
    bool dashed = false;

    void Merge(const StyleSpec& other);
};
StyleSpec ParseStyleSpec(std::string_view spec);

// Translates the scene so its content starts at `margin` and sets width/height from the real extents.
void FitSceneToContent(Scene& scene, const MeasureText& measure, float margin);
// Moves the content down and adds a bold centred title above it (no-op for an empty title).
void AddSceneTitle(Scene& scene, const std::string& title, const MeasureText& measure, float margin);

// --- flowchart / state diagram model --------------------------------------------

enum class NodeShape {
    Rect,
    Round,
    Stadium,
    Subroutine,
    Cylinder,
    Circle,
    DoubleCircle,
    Asymmetric,
    Diamond,
    Hexagon,
    Parallelogram,
    ParallelogramAlt,
    Trapezoid,
    TrapezoidAlt,
    // State diagrams
    StateBox,
    StateStart,
    StateEnd,
    ForkBar,
    Choice,
    Note,
};

enum class EdgeStroke { Normal, Dotted, Thick, Invisible };
enum class ArrowKind { None, Arrow, Circle, Cross };

struct FlowNode {
    std::string id;
    std::string label;
    NodeShape shape = NodeShape::Rect;
    bool explicitShape = false;
    int cluster = -1;
    std::vector<std::string> classes;
    StyleSpec style;
};

struct FlowEdge {
    int from = 0;
    int to = 0;
    std::string label;
    EdgeStroke stroke = EdgeStroke::Normal;
    ArrowKind startArrow = ArrowKind::None;
    ArrowKind endArrow = ArrowKind::Arrow;
    int minLen = 1;
    StyleSpec style;
};

struct FlowCluster {
    std::string id;
    std::string title;
    int parent = -1;
};

struct FlowGraph {
    Direction direction = Direction::TB;
    std::vector<FlowNode> nodes;
    std::vector<FlowEdge> edges;
    std::vector<FlowCluster> clusters;
    std::map<std::string, int> nodeIndex;
    std::map<std::string, StyleSpec> classDefs;
    bool roundedDefault = false; // State boxes

    int FindNode(const std::string& id) const {
        auto it = nodeIndex.find(id);
        return it == nodeIndex.end() ? -1 : it->second;
    }
    int AddNode(const std::string& id, const std::string& label, NodeShape shape) {
        const int existing = FindNode(id);
        if (existing >= 0) return existing;
        FlowNode node;
        node.id = id;
        node.label = label;
        node.shape = shape;
        nodes.push_back(std::move(node));
        const int index = static_cast<int>(nodes.size()) - 1;
        nodeIndex.emplace(id, index);
        return index;
    }
};

bool ParseFlowchart(const std::vector<std::string_view>& lines, FlowGraph& graph, std::string& error);
bool ParseStateDiagram(const std::vector<std::string_view>& lines, FlowGraph& graph, std::string& error);
Scene BuildFlowScene(const FlowGraph& graph, const MeasureText& measure);

// --- other diagram types --------------------------------------------------------

bool BuildSequenceScene(const std::vector<std::string_view>& lines, const MeasureText& measure, Scene& scene,
                        std::string& error);
bool BuildPieScene(const std::vector<std::string_view>& lines, const MeasureText& measure, Scene& scene,
                   std::string& error);

} // namespace Pluma::Diagram::detail
