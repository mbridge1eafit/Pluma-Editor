#pragma once

#include <vector>

#include "diagram_scene.h"

// Layered (Sugiyama-style) layout for directed graphs with nested clusters,
// shared by the Mermaid flowchart and state diagram engines.
namespace Pluma::Diagram::detail {

enum class Direction { TB, BT, LR, RL };

inline bool IsHorizontal(Direction d) {
    return d == Direction::LR || d == Direction::RL;
}

struct GraphNode {
    float width = 0.0f;  // Final orientation
    float height = 0.0f;
    int cluster = -1;    // Innermost cluster, -1 = none
};

struct GraphEdge {
    int from = 0;
    int to = 0;
    int minLen = 1;           // Minimum number of ranks spanned
    float labelWidth = 0.0f;  // 0 = no label
    float labelHeight = 0.0f;
};

struct GraphCluster {
    int parent = -1;
    float titleWidth = 0.0f;
    float titleHeight = 0.0f;
};

struct GraphLayoutInput {
    Direction direction = Direction::TB;
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
    std::vector<GraphCluster> clusters;
    float nodeSep = 36.0f;
    float rankSep = 48.0f;
};

struct LayoutRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool empty = true;
};

struct GraphLayoutResult {
    std::vector<Point> nodeCenters;
    // Regular edges: polyline from the source centre, through the bend points, to the target centre.
    // Self loops (from == to): the four control points of one cubic Bézier.
    std::vector<std::vector<Point>> edgePoints;
    std::vector<Point> labelCenters;       // Valid for edges with a label
    std::vector<LayoutRect> clusterRects;  // Empty clusters have empty == true
    float width = 0.0f;
    float height = 0.0f;
};

// Padding between a cluster frame and its content.
constexpr float kClusterPadding = 14.0f;

GraphLayoutResult LayoutGraph(const GraphLayoutInput& input);

} // namespace Pluma::Diagram::detail
