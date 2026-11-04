#include "graph_layout.h"

// Layered layout ported from dagre (https://github.com/dagrejs/dagre, lib/*.ts).
//
//   Copyright (c) 2012-2014 Chris Pettitt
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
//   associated documentation files (the "Software"), to deal in the Software without restriction,
//   including without limitation the rights to use, copy, modify, merge, publish, distribute,
//   sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
//   furnished to do so, subject to the following conditions: The above copyright notice and this
//   permission notice shall be included in all copies or substantial portions of the Software.
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.
//
// The pipeline follows dagre's runLayout: acyclic (DFS feedback arc set) -> nesting graph ->
// network simplex ranking -> normalize (dummy chains) -> parent dummy chains -> border segments ->
// crossing minimisation (layer sweeps with subgraph constraints) -> Brandes-Köpf positioning.
// Differences: no dynamic/incremental layout, edge labels are always centred (Mermaid's
// labelpos "c"), self loops keep Pluma's own shape, and cluster titles get room through the size
// of the cluster's border nodes.

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <set>

namespace Pluma::Diagram::detail {

namespace {

constexpr float kLoopReach = 34.0f; // How far a self loop bulges out of its node
constexpr float kMargin = 8.0f;
constexpr float kEdgeSep = 20.0f;   // dagre's default edgesep

enum class Dummy : unsigned char { None, Edge, EdgeLabel, EdgeProxy, Border, SelfEdge, Root };
enum class BorderType : unsigned char { None, Left, Right };

struct LNode {
    float width = 0.0f;
    float height = 0.0f;
    bool hasRank = false;
    int rank = 0;
    int order = 0;
    float x = 0.0f;
    float y = 0.0f;
    Dummy dummy = Dummy::None;
    BorderType borderType = BorderType::None;
    int parent = -1;
    std::vector<int> children;
    bool alive = true;
    // Cluster nodes
    int cluster = -1;
    int borderTop = -1;
    int borderBottom = -1;
    std::vector<int> borderLeft;  // Indexed by rank, -1 when absent
    std::vector<int> borderRight;
    bool hasMinRank = false;
    int minRank = 0;
    int maxRank = 0;
    // Dummies
    int edgeRef = -1;           // Edge this chain dummy / label proxy stands for
    std::vector<int> selfEdges; // Input edge indices of self loops on this node
};

struct LEdge {
    int v = 0;
    int w = 0;
    float weight = 1.0f;
    int minlen = 1;
    float width = 0.0f;
    float height = 0.0f;
    bool reversed = false;
    bool nesting = false;
    bool hasLabelRank = false;
    int labelRank = 0;
    int input = -1; // Index in GraphLayoutInput::edges, -1 for internal edges
    bool alive = true;
};

// A small compound multigraph with the graphlib operations dagre relies on. Iteration orders
// (nodes, edges, children, in/out edges) follow insertion order like graphlib's.
class LGraph {
public:
    std::vector<LNode> nodes;
    std::vector<LEdge> edges;
    std::vector<std::vector<int>> out;
    std::vector<std::vector<int>> in;
    std::vector<int> rootChildren;

    int AddNode(LNode node) {
        node.parent = -1;
        nodes.push_back(std::move(node));
        out.emplace_back();
        in.emplace_back();
        const int id = static_cast<int>(nodes.size()) - 1;
        rootChildren.push_back(id);
        return id;
    }

    int AddDummy(Dummy type, LNode node) {
        node.dummy = type;
        return AddNode(std::move(node));
    }

    std::vector<int>& ChildList(int p) { return p < 0 ? rootChildren : nodes[static_cast<size_t>(p)].children; }

    const std::vector<int>& Children(int p) const {
        return p < 0 ? rootChildren : nodes[static_cast<size_t>(p)].children;
    }

    bool HasChildren(int v) const { return !nodes[static_cast<size_t>(v)].children.empty(); }

    int Parent(int v) const { return nodes[static_cast<size_t>(v)].parent; }

    void SetParent(int v, int p) {
        auto& old = ChildList(nodes[static_cast<size_t>(v)].parent);
        old.erase(std::remove(old.begin(), old.end(), v), old.end());
        nodes[static_cast<size_t>(v)].parent = p;
        ChildList(p).push_back(v);
    }

    int AddEdge(int v, int w, LEdge e) {
        e.v = v;
        e.w = w;
        e.alive = true;
        edges.push_back(e);
        const int id = static_cast<int>(edges.size()) - 1;
        out[static_cast<size_t>(v)].push_back(id);
        in[static_cast<size_t>(w)].push_back(id);
        return id;
    }

    void RemoveEdge(int id) {
        LEdge& e = edges[static_cast<size_t>(id)];
        if (!e.alive) return;
        e.alive = false;
        auto& o = out[static_cast<size_t>(e.v)];
        o.erase(std::remove(o.begin(), o.end(), id), o.end());
        auto& i = in[static_cast<size_t>(e.w)];
        i.erase(std::remove(i.begin(), i.end(), id), i.end());
    }

    void RemoveNode(int v) {
        LNode& n = nodes[static_cast<size_t>(v)];
        if (!n.alive) return;
        for (int e : std::vector<int>(out[static_cast<size_t>(v)])) RemoveEdge(e);
        for (int e : std::vector<int>(in[static_cast<size_t>(v)])) RemoveEdge(e);
        for (int c : std::vector<int>(n.children)) SetParent(c, -1);
        auto& siblings = ChildList(n.parent);
        siblings.erase(std::remove(siblings.begin(), siblings.end(), v), siblings.end());
        n.alive = false;
    }

    std::vector<int> AliveNodes() const {
        std::vector<int> r;
        for (size_t v = 0; v < nodes.size(); ++v) {
            if (nodes[v].alive) r.push_back(static_cast<int>(v));
        }
        return r;
    }

    std::vector<int> AliveEdges() const {
        std::vector<int> r;
        for (size_t e = 0; e < edges.size(); ++e) {
            if (edges[e].alive) r.push_back(static_cast<int>(e));
        }
        return r;
    }

    std::vector<int> Successors(int v) const {
        std::vector<int> r;
        for (int e : out[static_cast<size_t>(v)]) {
            const int w = edges[static_cast<size_t>(e)].w;
            if (std::find(r.begin(), r.end(), w) == r.end()) r.push_back(w);
        }
        return r;
    }

    std::vector<int> Predecessors(int v) const {
        std::vector<int> r;
        for (int e : in[static_cast<size_t>(v)]) {
            const int u = edges[static_cast<size_t>(e)].v;
            if (std::find(r.begin(), r.end(), u) == r.end()) r.push_back(u);
        }
        return r;
    }

    LNode& N(int v) { return nodes[static_cast<size_t>(v)]; }
    const LNode& N(int v) const { return nodes[static_cast<size_t>(v)]; }
    LEdge& E(int e) { return edges[static_cast<size_t>(e)]; }
    const LEdge& E(int e) const { return edges[static_cast<size_t>(e)]; }
};

bool IsDummy(const LNode& n) {
    return n.dummy != Dummy::None;
}

// ---------------------------------------------------------------------------------------------
// rank/network-simplex.ts, rank/feasible-tree.ts, rank/util.ts
// Works on the simplified, non-compound graph (leaf nodes, multi-edges merged).
class NetworkSimplex {
public:
    explicit NetworkSimplex(LGraph& g) : m_g(g) {
        for (int v : g.AliveNodes()) {
            if (g.HasChildren(v)) continue;
            m_local.emplace(v, static_cast<int>(m_nodes.size()));
            m_nodes.push_back(v);
        }
        const size_t n = m_nodes.size();
        m_rank.assign(n, 0);
        m_in.assign(n, {});
        m_out.assign(n, {});
        for (int id : g.AliveEdges()) {
            const LEdge& e = g.E(id);
            const auto iv = m_local.find(e.v);
            const auto iw = m_local.find(e.w);
            if (iv == m_local.end() || iw == m_local.end()) continue;
            const std::pair<int, int> key{iv->second, iw->second};
            const auto it = m_edgeIndex.find(key);
            if (it == m_edgeIndex.end()) {
                const int sid = static_cast<int>(m_edges.size());
                m_edgeIndex.emplace(key, sid);
                m_edges.push_back({key.first, key.second, e.weight, e.minlen});
                m_out[static_cast<size_t>(key.first)].push_back(sid);
                m_in[static_cast<size_t>(key.second)].push_back(sid);
            } else {
                SEdge& s = m_edges[static_cast<size_t>(it->second)];
                s.weight += e.weight;
                s.minlen = (std::max)(s.minlen, e.minlen);
            }
        }
    }

    void Run() {
        if (m_nodes.empty()) return;
        LongestPath();
        FeasibleTree();
        InitLowLimValues();
        InitCutValues();
        // The algorithm terminates; the bound only protects the UI thread from surprises.
        const int maxIterations = 50 + static_cast<int>(m_edges.size()) * 10;
        for (int iter = 0; iter < maxIterations; ++iter) {
            const int e = LeaveEdge();
            if (e < 0) break;
            const int f = EnterEdge(e);
            if (f < 0) break;
            ExchangeEdges(e, f);
        }
        for (size_t i = 0; i < m_nodes.size(); ++i) {
            LNode& node = m_g.N(m_nodes[i]);
            node.rank = m_rank[i];
            node.hasRank = true;
        }
    }

private:
    struct SEdge {
        int v;
        int w;
        float weight;
        int minlen;
    };

    struct TreeEdge {
        int a;
        int b;
        float cutvalue = 0.0f;
        bool alive = true;
    };

    int Slack(int e) const {
        const SEdge& s = m_edges[static_cast<size_t>(e)];
        return m_rank[static_cast<size_t>(s.w)] - m_rank[static_cast<size_t>(s.v)] - s.minlen;
    }

    int FindEdge(int v, int w) const {
        const auto it = m_edgeIndex.find({v, w});
        return it == m_edgeIndex.end() ? -1 : it->second;
    }

    std::vector<int> NodeEdges(int v) const {
        std::vector<int> r = m_in[static_cast<size_t>(v)];
        r.insert(r.end(), m_out[static_cast<size_t>(v)].begin(), m_out[static_cast<size_t>(v)].end());
        return r;
    }

    void LongestPath() {
        std::vector<char> visited(m_nodes.size(), 0);
        std::function<int(int)> dfs = [&](int v) -> int {
            if (visited[static_cast<size_t>(v)]) return m_rank[static_cast<size_t>(v)];
            visited[static_cast<size_t>(v)] = 1;
            int rank = std::numeric_limits<int>::max();
            for (int e : m_out[static_cast<size_t>(v)]) {
                const SEdge& s = m_edges[static_cast<size_t>(e)];
                rank = (std::min)(rank, dfs(s.w) - s.minlen);
            }
            if (rank == std::numeric_limits<int>::max()) rank = 0;
            m_rank[static_cast<size_t>(v)] = rank;
            return rank;
        };
        for (size_t v = 0; v < m_nodes.size(); ++v) {
            if (m_in[v].empty()) dfs(static_cast<int>(v));
        }
    }

    bool InTree(int v) const { return m_inTree[static_cast<size_t>(v)] != 0; }

    void AddTreeNode(int v) {
        m_inTree[static_cast<size_t>(v)] = 1;
        m_treeNodes.push_back(v);
    }

    void AddTreeEdge(int a, int b) {
        m_tree.push_back({a, b});
        const int id = static_cast<int>(m_tree.size()) - 1;
        m_adj[static_cast<size_t>(a)].push_back(id);
        m_adj[static_cast<size_t>(b)].push_back(id);
    }

    void RemoveTreeEdge(int id) {
        TreeEdge& t = m_tree[static_cast<size_t>(id)];
        t.alive = false;
        for (int x : {t.a, t.b}) {
            auto& adj = m_adj[static_cast<size_t>(x)];
            adj.erase(std::remove(adj.begin(), adj.end(), id), adj.end());
        }
    }

    int TreeEdgeBetween(int a, int b) const {
        for (int id : m_adj[static_cast<size_t>(a)]) {
            const TreeEdge& t = m_tree[static_cast<size_t>(id)];
            if ((t.a == a && t.b == b) || (t.a == b && t.b == a)) return id;
        }
        return -1;
    }

    int Other(int id, int v) const {
        const TreeEdge& t = m_tree[static_cast<size_t>(id)];
        return t.a == v ? t.b : t.a;
    }

    void FeasibleTree() {
        const size_t n = m_nodes.size();
        m_inTree.assign(n, 0);
        m_adj.assign(n, {});
        AddTreeNode(0);
        std::function<void(int)> dfs = [&](int v) {
            for (int e : NodeEdges(v)) {
                const SEdge& s = m_edges[static_cast<size_t>(e)];
                const int w = v == s.v ? s.w : s.v;
                if (!InTree(w) && Slack(e) == 0) {
                    AddTreeNode(w);
                    AddTreeEdge(v, w);
                    dfs(w);
                }
            }
        };
        auto tightTree = [&]() {
            const std::vector<int> snapshot = m_treeNodes;
            for (int v : snapshot) dfs(v);
            return m_treeNodes.size();
        };
        while (tightTree() < n) {
            int best = -1;
            int bestSlack = std::numeric_limits<int>::max();
            for (size_t e = 0; e < m_edges.size(); ++e) {
                const SEdge& s = m_edges[e];
                if (InTree(s.v) == InTree(s.w)) continue;
                const int sl = Slack(static_cast<int>(e));
                if (sl < bestSlack) {
                    bestSlack = sl;
                    best = static_cast<int>(e);
                }
            }
            if (best < 0) break; // Disconnected (cannot happen after the nesting graph)
            const int delta = InTree(m_edges[static_cast<size_t>(best)].v) ? Slack(best) : -Slack(best);
            for (int v : m_treeNodes) m_rank[static_cast<size_t>(v)] += delta;
        }
    }

    void InitLowLimValues() {
        const size_t n = m_nodes.size();
        m_low.assign(n, 0);
        m_lim.assign(n, 0);
        m_parent.assign(n, -1);
        std::vector<char> visited(n, 0);
        const int root = m_treeNodes.front();
        // Iterative DFS: lim is the post-order number, low the smallest lim in the subtree.
        struct Frame {
            int v;
            int parent;
            size_t next;
            int low;
        };
        int nextLim = 1;
        std::vector<Frame> stack{{root, -1, 0, nextLim}};
        visited[static_cast<size_t>(root)] = 1;
        while (!stack.empty()) {
            Frame& f = stack.back();
            const auto& adj = m_adj[static_cast<size_t>(f.v)];
            if (f.next < adj.size()) {
                const int w = Other(adj[f.next++], f.v);
                if (!visited[static_cast<size_t>(w)]) {
                    visited[static_cast<size_t>(w)] = 1;
                    const int parent = f.v;
                    stack.push_back({w, parent, 0, nextLim});
                }
                continue;
            }
            m_low[static_cast<size_t>(f.v)] = f.low;
            m_lim[static_cast<size_t>(f.v)] = nextLim++;
            m_parent[static_cast<size_t>(f.v)] = f.parent;
            stack.pop_back();
        }
        m_postorder.clear();
        std::vector<int> byLim(n + 2, -1);
        for (int v : m_treeNodes) byLim[static_cast<size_t>(m_lim[static_cast<size_t>(v)])] = v;
        for (int v : byLim) {
            if (v >= 0) m_postorder.push_back(v);
        }
    }

    float CalcCutValue(int child) const {
        const int parent = m_parent[static_cast<size_t>(child)];
        bool childIsTail = true;
        int ge = FindEdge(child, parent);
        if (ge < 0) {
            childIsTail = false;
            ge = FindEdge(parent, child);
        }
        float cutValue = m_edges[static_cast<size_t>(ge)].weight;
        for (int e : NodeEdges(child)) {
            const SEdge& s = m_edges[static_cast<size_t>(e)];
            const bool isOutEdge = s.v == child;
            const int other = isOutEdge ? s.w : s.v;
            if (other == parent) continue;
            const bool pointsToHead = isOutEdge == childIsTail;
            cutValue += pointsToHead ? s.weight : -s.weight;
            const int te = TreeEdgeBetween(child, other);
            if (te >= 0) {
                const float otherCut = m_tree[static_cast<size_t>(te)].cutvalue;
                cutValue += pointsToHead ? -otherCut : otherCut;
            }
        }
        return cutValue;
    }

    void InitCutValues() {
        for (size_t i = 0; i + 1 < m_postorder.size(); ++i) { // Every node but the root (last)
            const int child = m_postorder[i];
            const int te = TreeEdgeBetween(child, m_parent[static_cast<size_t>(child)]);
            if (te >= 0) m_tree[static_cast<size_t>(te)].cutvalue = CalcCutValue(child);
        }
    }

    int LeaveEdge() const {
        for (size_t id = 0; id < m_tree.size(); ++id) {
            if (m_tree[id].alive && m_tree[id].cutvalue < 0.0f) return static_cast<int>(id);
        }
        return -1;
    }

    bool IsDescendant(int v, int root) const {
        return m_low[static_cast<size_t>(root)] <= m_lim[static_cast<size_t>(v)] &&
               m_lim[static_cast<size_t>(v)] <= m_lim[static_cast<size_t>(root)];
    }

    int EnterEdge(int treeEdge) const {
        int v = m_tree[static_cast<size_t>(treeEdge)].a;
        int w = m_tree[static_cast<size_t>(treeEdge)].b;
        if (FindEdge(v, w) < 0) std::swap(v, w);
        int tail = v;
        bool flip = false;
        if (m_lim[static_cast<size_t>(v)] > m_lim[static_cast<size_t>(w)]) {
            tail = w;
            flip = true;
        }
        int best = -1;
        for (size_t e = 0; e < m_edges.size(); ++e) {
            const SEdge& s = m_edges[e];
            if (flip == IsDescendant(s.v, tail) && flip != IsDescendant(s.w, tail)) {
                if (best < 0 || Slack(static_cast<int>(e)) < Slack(best)) best = static_cast<int>(e);
            }
        }
        return best;
    }

    void ExchangeEdges(int treeEdge, int graphEdge) {
        RemoveTreeEdge(treeEdge);
        AddTreeEdge(m_edges[static_cast<size_t>(graphEdge)].v, m_edges[static_cast<size_t>(graphEdge)].w);
        InitLowLimValues();
        InitCutValues();
        UpdateRanks();
    }

    void UpdateRanks() {
        int root = -1;
        for (int v : m_treeNodes) {
            if (m_parent[static_cast<size_t>(v)] < 0) {
                root = v;
                break;
            }
        }
        if (root < 0) return;
        std::vector<int> stack{root};
        std::vector<char> seen(m_nodes.size(), 0);
        seen[static_cast<size_t>(root)] = 1;
        while (!stack.empty()) {
            const int v = stack.back();
            stack.pop_back();
            if (v != root) {
                const int parent = m_parent[static_cast<size_t>(v)];
                int ge = FindEdge(v, parent);
                bool flipped = false;
                if (ge < 0) {
                    ge = FindEdge(parent, v);
                    flipped = true;
                }
                const int minlen = m_edges[static_cast<size_t>(ge)].minlen;
                m_rank[static_cast<size_t>(v)] = m_rank[static_cast<size_t>(parent)] + (flipped ? minlen : -minlen);
            }
            const auto& adj = m_adj[static_cast<size_t>(v)];
            for (auto it = adj.rbegin(); it != adj.rend(); ++it) {
                const int w = Other(*it, v);
                if (!seen[static_cast<size_t>(w)]) {
                    seen[static_cast<size_t>(w)] = 1;
                    stack.push_back(w);
                }
            }
        }
    }

    LGraph& m_g;
    std::vector<int> m_nodes; // local -> graph id
    std::map<int, int> m_local;
    std::vector<SEdge> m_edges;
    std::map<std::pair<int, int>, int> m_edgeIndex;
    std::vector<std::vector<int>> m_in;
    std::vector<std::vector<int>> m_out;
    std::vector<int> m_rank;
    std::vector<char> m_inTree;
    std::vector<int> m_treeNodes;
    std::vector<TreeEdge> m_tree;
    std::vector<std::vector<int>> m_adj;
    std::vector<int> m_low;
    std::vector<int> m_lim;
    std::vector<int> m_parent;
    std::vector<int> m_postorder;
};

// ---------------------------------------------------------------------------------------------

struct Constraint {
    int to;
    float dist;
};

class Layouter {
public:
    explicit Layouter(const GraphLayoutInput& input) : m_in(input) {}

    GraphLayoutResult Run() {
        m_horizontal = IsHorizontal(m_in.direction);
        m_nodeSep = m_in.nodeSep;
        m_rankSep = m_in.rankSep;
        BuildLayoutGraph();
        MakeSpaceForEdgeLabels();
        Acyclic();
        NestingGraphRun();
        NetworkSimplex(m_g).Run();
        InjectEdgeLabelProxies();
        RemoveEmptyRanks();
        NestingGraphCleanup();
        NormalizeRanks();
        AssignRankMinMax();
        RemoveEdgeLabelProxies();
        NormalizeRun();
        ParentDummyChains();
        AddBorderSegments();
        Order();
        InsertSelfEdges();
        PositionY();
        PositionX();
        return Finish();
    }

private:
    float LabelAlong(const GraphEdge& e) const { return m_horizontal ? e.labelHeight : e.labelWidth; }

    float LoopExtent(const GraphEdge& e) const {
        const float label = e.labelWidth > 0.0f ? LabelAlong(e) + 6.0f : 0.0f;
        return kLoopReach * 0.75f + 6.0f + label;
    }

    // --- layout graph (layout.ts buildLayoutGraph / removeSelfEdges) -------------------------
    // Sizes are converted to dagre's coordinate system (ranks along y) up front, which is what
    // coordinateSystem.adjust does for LR/RL.
    void BuildLayoutGraph() {
        const size_t n = m_in.nodes.size();
        const size_t clusterCount = m_in.clusters.size();
        for (size_t i = 0; i < n; ++i) {
            const GraphNode& gn = m_in.nodes[i];
            LNode node;
            node.width = m_horizontal ? gn.height : gn.width;
            node.height = m_horizontal ? gn.width : gn.height;
            m_g.AddNode(std::move(node));
        }

        // Cluster nodes only for clusters that (transitively) contain a node.
        std::vector<char> used(clusterCount, 0);
        for (const GraphNode& gn : m_in.nodes) {
            int c = gn.cluster;
            int guard = 0;
            while (c >= 0 && c < static_cast<int>(clusterCount) && guard++ < 64) {
                used[static_cast<size_t>(c)] = 1;
                c = m_in.clusters[static_cast<size_t>(c)].parent;
            }
        }
        m_clusterNode.assign(clusterCount, -1);
        for (size_t c = 0; c < clusterCount; ++c) {
            if (!used[c]) continue;
            LNode node;
            node.cluster = static_cast<int>(c);
            m_clusterNode[c] = m_g.AddNode(std::move(node));
        }
        for (size_t c = 0; c < clusterCount; ++c) {
            if (m_clusterNode[c] < 0) continue;
            const int p = m_in.clusters[c].parent;
            if (p >= 0 && p < static_cast<int>(clusterCount) && m_clusterNode[static_cast<size_t>(p)] >= 0) {
                m_g.SetParent(m_clusterNode[c], m_clusterNode[static_cast<size_t>(p)]);
            }
        }
        for (size_t i = 0; i < n; ++i) {
            const int c = m_in.nodes[i].cluster;
            if (c >= 0 && c < static_cast<int>(clusterCount) && m_clusterNode[static_cast<size_t>(c)] >= 0) {
                m_g.SetParent(static_cast<int>(i), m_clusterNode[static_cast<size_t>(c)]);
            }
        }

        for (size_t i = 0; i < m_in.edges.size(); ++i) {
            const GraphEdge& ge = m_in.edges[i];
            if (ge.from < 0 || ge.to < 0 || ge.from >= static_cast<int>(n) || ge.to >= static_cast<int>(n)) continue;
            if (ge.from == ge.to) {
                m_g.N(ge.from).selfEdges.push_back(static_cast<int>(i));
                continue;
            }
            LEdge e;
            e.minlen = (std::max)(1, ge.minLen);
            if (ge.labelWidth > 0.0f) {
                e.width = m_horizontal ? ge.labelHeight : ge.labelWidth;
                e.height = m_horizontal ? ge.labelWidth : ge.labelHeight;
            }
            e.input = static_cast<int>(i);
            m_g.AddEdge(ge.from, ge.to, e);
        }
    }

    void MakeSpaceForEdgeLabels() {
        m_rankSep *= 0.5f;
        for (int e : m_g.AliveEdges()) m_g.E(e).minlen *= 2;
    }

    // --- acyclic.ts (dfsFAS) ----------------------------------------------------------------

    void Acyclic() {
        const size_t n = m_g.nodes.size();
        std::vector<char> visited(n, 0);
        std::vector<char> onStack(n, 0);
        std::vector<int> fas;
        std::function<void(int)> dfs = [&](int v) {
            if (visited[static_cast<size_t>(v)]) return;
            visited[static_cast<size_t>(v)] = 1;
            onStack[static_cast<size_t>(v)] = 1;
            for (int e : std::vector<int>(m_g.out[static_cast<size_t>(v)])) {
                const int w = m_g.E(e).w;
                if (onStack[static_cast<size_t>(w)]) {
                    fas.push_back(e);
                } else {
                    dfs(w);
                }
            }
            onStack[static_cast<size_t>(v)] = 0;
        };
        const std::vector<int> nodes = m_g.AliveNodes();
        for (int v : nodes) {
            if (m_g.in[static_cast<size_t>(v)].empty()) dfs(v); // sources first
        }
        for (int v : nodes) dfs(v);
        for (int e : fas) {
            LEdge label = m_g.E(e);
            m_g.RemoveEdge(e);
            label.reversed = true;
            m_g.AddEdge(label.w, label.v, label);
        }
    }

    // --- nesting-graph.ts ------------------------------------------------------------------

    void TreeDepths(int v, int depth) {
        for (int c : std::vector<int>(m_g.Children(v))) TreeDepths(c, depth + 1);
        m_depth[static_cast<size_t>(v)] = depth;
    }

    void NestingDfs(int v) {
        const std::vector<int> children = m_g.Children(v);
        if (children.empty()) {
            if (v != m_nestingRoot) {
                LEdge e;
                e.weight = 0.0f;
                e.minlen = m_nodeRankFactor;
                m_g.AddEdge(m_nestingRoot, v, e);
            }
            return;
        }
        const GraphCluster& gc = m_in.clusters[static_cast<size_t>(m_g.N(v).cluster)];
        LNode topNode;
        LNode bottomNode;
        // Frame padding, plus room for the title where the title goes along the rank axis.
        topNode.height = kClusterPadding + (m_in.direction == Direction::TB ? gc.titleHeight : 0.0f);
        bottomNode.height = kClusterPadding + (m_in.direction == Direction::BT ? gc.titleHeight : 0.0f);
        const int top = m_g.AddDummy(Dummy::Border, topNode);
        const int bottom = m_g.AddDummy(Dummy::Border, bottomNode);
        m_depth.resize(m_g.nodes.size(), 0);
        m_g.SetParent(top, v);
        m_g.N(v).borderTop = top;
        m_g.SetParent(bottom, v);
        m_g.N(v).borderBottom = bottom;

        for (int child : children) {
            NestingDfs(child);
            const LNode& cn = m_g.N(child);
            const int childTop = cn.borderTop >= 0 ? cn.borderTop : child;
            const int childBottom = cn.borderBottom >= 0 ? cn.borderBottom : child;
            const float thisWeight = cn.borderTop >= 0 ? m_nestingWeight : 2.0f * m_nestingWeight;
            const int minlen = childTop != childBottom ? 1 : m_nestingHeight - m_depth[static_cast<size_t>(v)] + 1;
            LEdge e;
            e.weight = thisWeight;
            e.minlen = minlen;
            e.nesting = true;
            m_g.AddEdge(top, childTop, e);
            m_g.AddEdge(childBottom, bottom, e);
        }
        if (m_g.Parent(v) < 0) {
            LEdge e;
            e.weight = 0.0f;
            e.minlen = m_nestingHeight + m_depth[static_cast<size_t>(v)];
            m_g.AddEdge(m_nestingRoot, top, e);
        }
    }

    void NestingGraphRun() {
        m_nestingRoot = m_g.AddDummy(Dummy::Root, LNode{});
        m_depth.assign(m_g.nodes.size(), 0);
        for (int v : std::vector<int>(m_g.rootChildren)) TreeDepths(v, 1);
        int maxDepth = 0;
        for (int v : m_g.AliveNodes()) maxDepth = (std::max)(maxDepth, m_depth[static_cast<size_t>(v)]);
        m_nestingHeight = maxDepth - 1;
        m_nodeRankFactor = 2 * m_nestingHeight + 1;
        for (int e : m_g.AliveEdges()) m_g.E(e).minlen *= m_nodeRankFactor;
        float weights = 0.0f;
        for (int e : m_g.AliveEdges()) weights += m_g.E(e).weight;
        m_nestingWeight = weights + 1.0f;
        for (int v : std::vector<int>(m_g.rootChildren)) NestingDfs(v);
    }

    void NestingGraphCleanup() {
        m_g.RemoveNode(m_nestingRoot);
        for (int e : m_g.AliveEdges()) {
            if (m_g.E(e).nesting) m_g.RemoveEdge(e);
        }
    }

    // --- layout.ts helpers / util.ts --------------------------------------------------------

    void InjectEdgeLabelProxies() {
        for (int e : m_g.AliveEdges()) {
            const LEdge& edge = m_g.E(e);
            if (edge.width > 0.0f && edge.height > 0.0f) {
                const int vRank = m_g.N(edge.v).rank;
                const int wRank = m_g.N(edge.w).rank;
                LNode proxy;
                proxy.rank = vRank + (wRank - vRank) / 2; // dagre can produce half ranks; they are floored
                proxy.hasRank = true;
                proxy.edgeRef = e;
                m_g.AddDummy(Dummy::EdgeProxy, proxy);
            }
        }
    }

    void RemoveEmptyRanks() {
        int offset = std::numeric_limits<int>::max();
        int maxRank = std::numeric_limits<int>::min();
        for (int v : m_g.AliveNodes()) {
            if (!m_g.N(v).hasRank) continue;
            offset = (std::min)(offset, m_g.N(v).rank);
            maxRank = (std::max)(maxRank, m_g.N(v).rank);
        }
        if (offset > maxRank) return;
        std::vector<std::vector<int>> layers(static_cast<size_t>(maxRank - offset) + 1);
        for (int v : m_g.AliveNodes()) {
            if (m_g.N(v).hasRank) layers[static_cast<size_t>(m_g.N(v).rank - offset)].push_back(v);
        }
        int delta = 0;
        for (size_t i = 0; i < layers.size(); ++i) {
            if (layers[i].empty() && static_cast<int>(i) % m_nodeRankFactor != 0) {
                --delta;
            } else if (!layers[i].empty() && delta != 0) {
                for (int v : layers[i]) m_g.N(v).rank += delta;
            }
        }
    }

    void NormalizeRanks() {
        int minRank = std::numeric_limits<int>::max();
        for (int v : m_g.AliveNodes()) {
            if (m_g.N(v).hasRank) minRank = (std::min)(minRank, m_g.N(v).rank);
        }
        if (minRank == std::numeric_limits<int>::max()) return;
        for (int v : m_g.AliveNodes()) {
            if (m_g.N(v).hasRank) m_g.N(v).rank -= minRank;
        }
    }

    void AssignRankMinMax() {
        for (int v : m_g.AliveNodes()) {
            LNode& node = m_g.N(v);
            if (node.borderTop >= 0) {
                node.minRank = m_g.N(node.borderTop).rank;
                node.maxRank = m_g.N(node.borderBottom).rank;
                node.hasMinRank = true;
            }
        }
    }

    void RemoveEdgeLabelProxies() {
        for (int v : m_g.AliveNodes()) {
            const LNode& node = m_g.N(v);
            if (node.dummy != Dummy::EdgeProxy) continue;
            LEdge& e = m_g.E(node.edgeRef);
            e.labelRank = node.rank;
            e.hasLabelRank = true;
            m_g.RemoveNode(v);
        }
    }

    // --- normalize.ts -------------------------------------------------------------------------

    void NormalizeRun() {
        for (int id : m_g.AliveEdges()) {
            const LEdge edge = m_g.E(id);
            int v = edge.v;
            const int vRank = m_g.N(v).rank;
            const int w = edge.w;
            const int wRank = m_g.N(w).rank;
            if (wRank == vRank + 1) continue;
            m_g.RemoveEdge(id);
            for (int i = 0, r = vRank + 1; r < wRank; ++i, ++r) {
                LNode attrs;
                attrs.rank = r;
                attrs.hasRank = true;
                attrs.edgeRef = id;
                Dummy type = Dummy::Edge;
                if (edge.hasLabelRank && r == edge.labelRank) {
                    attrs.width = edge.width;
                    attrs.height = edge.height;
                    type = Dummy::EdgeLabel;
                }
                const int dummy = m_g.AddDummy(type, attrs);
                LEdge link;
                link.weight = edge.weight;
                m_g.AddEdge(v, dummy, link);
                if (i == 0) m_dummyChains.push_back(dummy);
                v = dummy;
            }
            LEdge link;
            link.weight = edge.weight;
            m_g.AddEdge(v, w, link);
        }
    }

    // --- parent-dummy-chains.ts ---------------------------------------------------------------

    void ParentDummyChains() {
        const size_t n = m_g.nodes.size();
        std::vector<int> low(n, 0);
        std::vector<int> lim(n, 0);
        int counter = 0;
        std::function<void(int)> post = [&](int v) {
            const int l = counter;
            for (int c : std::vector<int>(m_g.Children(v))) post(c);
            low[static_cast<size_t>(v)] = l;
            lim[static_cast<size_t>(v)] = counter++;
        };
        for (int v : std::vector<int>(m_g.rootChildren)) post(v);

        for (int start : m_dummyChains) {
            int v = start;
            const LEdge& edgeObj = m_g.E(m_g.N(v).edgeRef);
            const int ev = edgeObj.v;
            const int ew = edgeObj.w;

            // findPath: ancestors of v up to the lowest common ancestor, then down to w.
            std::vector<int> path;
            std::vector<int> wPath;
            const int lowest = (std::min)(low[static_cast<size_t>(ev)], low[static_cast<size_t>(ew)]);
            const int highest = (std::max)(lim[static_cast<size_t>(ev)], lim[static_cast<size_t>(ew)]);
            int parent = ev;
            do {
                parent = m_g.Parent(parent);
                path.push_back(parent);
            } while (parent >= 0 &&
                     (low[static_cast<size_t>(parent)] > lowest || highest > lim[static_cast<size_t>(parent)]));
            const int lca = parent;
            int wParent = ew;
            while ((wParent = m_g.Parent(wParent)) != lca && wParent >= 0) wPath.push_back(wParent);
            path.insert(path.end(), wPath.rbegin(), wPath.rend());

            size_t pathIdx = 0;
            int pathV = path[pathIdx];
            bool ascending = true;
            int guard = 0;
            while (v != ew && guard++ < 100000) {
                const int rank = m_g.N(v).rank;
                if (ascending) {
                    while ((pathV = path[pathIdx]) != lca && m_g.N(pathV).maxRank < rank) ++pathIdx;
                    if (pathV == lca) ascending = false;
                }
                if (!ascending) {
                    while (pathIdx + 1 < path.size() && m_g.N(path[pathIdx + 1]).minRank <= rank) ++pathIdx;
                    pathV = path[pathIdx];
                }
                if (pathV >= 0) m_g.SetParent(v, pathV);
                const std::vector<int> succ = m_g.Successors(v);
                if (succ.empty()) break;
                v = succ[0];
            }
        }
    }

    // --- add-border-segments.ts ---------------------------------------------------------------

    void AddBorderNode(BorderType type, int sg, int rank) {
        LNode label;
        label.rank = rank;
        label.hasRank = true;
        label.borderType = type;
        // In LR/RL layouts the in-layer axis is the final vertical one: the left border is the
        // top of the frame and needs room for the title.
        if (m_horizontal && type == BorderType::Left) {
            label.width = m_in.clusters[static_cast<size_t>(m_g.N(sg).cluster)].titleHeight;
        }
        const int curr = m_g.AddDummy(Dummy::Border, label);
        auto& list = type == BorderType::Left ? m_g.N(sg).borderLeft : m_g.N(sg).borderRight;
        const int prev = rank > 0 ? list[static_cast<size_t>(rank - 1)] : -1;
        list[static_cast<size_t>(rank)] = curr;
        m_g.SetParent(curr, sg);
        if (prev >= 0) {
            LEdge e;
            e.weight = 1.0f;
            m_g.AddEdge(prev, curr, e);
        }
    }

    void AddBorderSegments() {
        std::function<void(int)> dfs = [&](int v) {
            for (int c : std::vector<int>(m_g.Children(v))) dfs(c);
            if (!m_g.N(v).hasMinRank) return;
            const int minRank = m_g.N(v).minRank;
            const int maxRank = m_g.N(v).maxRank;
            m_g.N(v).borderLeft.assign(static_cast<size_t>(maxRank) + 1, -1);
            m_g.N(v).borderRight.assign(static_cast<size_t>(maxRank) + 1, -1);
            for (int r = minRank; r <= maxRank; ++r) {
                AddBorderNode(BorderType::Left, v, r);
                AddBorderNode(BorderType::Right, v, r);
            }
        };
        for (int v : std::vector<int>(m_g.rootChildren)) dfs(v);
    }

    // --- order/*.ts ---------------------------------------------------------------------------

    int MaxRank() const {
        int r = 0;
        for (int v : m_g.AliveNodes()) {
            if (m_g.N(v).hasRank) r = (std::max)(r, m_g.N(v).rank);
        }
        return r;
    }

    std::vector<std::vector<int>> BuildLayerMatrix() const {
        std::vector<std::vector<std::pair<int, int>>> tmp(static_cast<size_t>(MaxRank()) + 1);
        for (int v : m_g.AliveNodes()) {
            const LNode& node = m_g.N(v);
            if (!node.hasRank || m_g.HasChildren(v)) continue;
            tmp[static_cast<size_t>(node.rank)].emplace_back(node.order, v);
        }
        std::vector<std::vector<int>> layering(tmp.size());
        for (size_t r = 0; r < tmp.size(); ++r) {
            std::stable_sort(tmp[r].begin(), tmp[r].end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            for (const auto& entry : tmp[r]) layering[r].push_back(entry.second);
        }
        return layering;
    }

    void AssignOrder(const std::vector<std::vector<int>>& layering) {
        for (const auto& layer : layering) {
            for (size_t i = 0; i < layer.size(); ++i) m_g.N(layer[i]).order = static_cast<int>(i);
        }
    }

    // order/init-order.ts
    std::vector<std::vector<int>> InitOrder() const {
        std::vector<int> simple;
        for (int v : m_g.AliveNodes()) {
            if (!m_g.HasChildren(v) && m_g.N(v).hasRank) simple.push_back(v);
        }
        int maxRank = 0;
        for (int v : simple) maxRank = (std::max)(maxRank, m_g.N(v).rank);
        std::vector<std::vector<int>> layers(static_cast<size_t>(maxRank) + 1);
        std::vector<char> visited(m_g.nodes.size(), 0);
        std::function<void(int)> dfs = [&](int v) {
            if (visited[static_cast<size_t>(v)]) return;
            visited[static_cast<size_t>(v)] = 1;
            layers[static_cast<size_t>(m_g.N(v).rank)].push_back(v);
            for (int w : m_g.Successors(v)) dfs(w);
        };
        std::stable_sort(simple.begin(), simple.end(), [&](int a, int b) { return m_g.N(a).rank < m_g.N(b).rank; });
        for (int v : simple) dfs(v);
        return layers;
    }

    // order/build-layer-graph.ts: the nodes of one rank (plus the clusters spanning it) and the
    // weighted edges from the adjacent rank.
    static constexpr int kLayerRoot = -2;

    struct LayerGraph {
        std::map<int, int> parent;                                 // node -> parent (cluster or kLayerRoot)
        std::map<int, std::vector<int>> children;                  // parent -> children
        std::map<int, std::vector<std::pair<int, float>>> inEdges; // v -> (u, weight)
        std::map<int, std::pair<int, int>> borders;                // cluster -> (borderLeft, borderRight)

        int Parent(int v) const {
            if (v == kLayerRoot) return -1;
            const auto it = parent.find(v);
            return it == parent.end() ? -1 : it->second;
        }

        const std::vector<int>& Children(int v) const {
            static const std::vector<int> empty;
            const auto it = children.find(v);
            return it == children.end() ? empty : it->second;
        }
    };

    LayerGraph BuildLayerGraph(int rank, bool useInEdges) const {
        LayerGraph lg;
        for (int v : m_g.AliveNodes()) {
            const LNode& node = m_g.N(v);
            const bool inRank = (node.hasRank && node.rank == rank) ||
                                (node.hasMinRank && node.minRank <= rank && rank <= node.maxRank);
            if (!inRank) continue;
            const int p = m_g.Parent(v) >= 0 ? m_g.Parent(v) : kLayerRoot;
            lg.parent[v] = p;
            lg.children[p].push_back(v);
            const auto& list = useInEdges ? m_g.in[static_cast<size_t>(v)] : m_g.out[static_cast<size_t>(v)];
            for (int e : list) {
                const LEdge& edge = m_g.E(e);
                const int u = edge.v == v ? edge.w : edge.v;
                auto& ins = lg.inEdges[v];
                auto it = std::find_if(ins.begin(), ins.end(), [&](const auto& entry) { return entry.first == u; });
                if (it == ins.end()) {
                    ins.emplace_back(u, edge.weight);
                } else {
                    it->second += edge.weight;
                }
            }
            if (node.hasMinRank) {
                lg.borders[v] = {node.borderLeft[static_cast<size_t>(rank)], node.borderRight[static_cast<size_t>(rank)]};
            }
        }
        return lg;
    }

    struct Entry {
        std::vector<int> vs;
        int i = 0;
        bool hasBary = false;
        float barycenter = 0.0f;
        float weight = 0.0f;
        int indegree = 0;
        std::vector<int> ins;
        std::vector<int> outs;
        bool merged = false;
    };

    struct SortResult {
        std::vector<int> vs;
        bool hasBary = false;
        float barycenter = 0.0f;
        float weight = 0.0f;
    };

    using ConstraintGraph = std::vector<std::pair<int, int>>;

    // order/resolve-conflicts.ts
    static std::vector<Entry> ResolveConflicts(std::vector<Entry> entries, const std::vector<int>& ids,
                                               const ConstraintGraph& cg) {
        std::map<int, int> byId;
        for (size_t k = 0; k < ids.size(); ++k) byId[ids[k]] = static_cast<int>(k);
        for (const auto& constraint : cg) {
            const auto ia = byId.find(constraint.first);
            const auto ib = byId.find(constraint.second);
            if (ia == byId.end() || ib == byId.end()) continue;
            entries[static_cast<size_t>(ib->second)].indegree++;
            entries[static_cast<size_t>(ia->second)].outs.push_back(ib->second);
        }
        std::vector<int> sourceSet;
        for (size_t k = 0; k < entries.size(); ++k) {
            if (entries[k].indegree == 0) sourceSet.push_back(static_cast<int>(k));
        }
        auto mergeEntries = [](Entry& target, Entry& source) {
            float sum = 0.0f;
            float weight = 0.0f;
            if (target.weight != 0.0f) {
                sum += target.barycenter * target.weight;
                weight += target.weight;
            }
            if (source.weight != 0.0f) {
                sum += source.barycenter * source.weight;
                weight += source.weight;
            }
            std::vector<int> vs = source.vs;
            vs.insert(vs.end(), target.vs.begin(), target.vs.end());
            target.vs = std::move(vs);
            target.hasBary = weight != 0.0f;
            target.barycenter = weight != 0.0f ? sum / weight : 0.0f;
            target.weight = weight;
            target.i = (std::min)(source.i, target.i);
            source.merged = true;
        };
        std::vector<int> processed;
        while (!sourceSet.empty()) {
            const int k = sourceSet.back();
            sourceSet.pop_back();
            processed.push_back(k);
            std::vector<int> ins = entries[static_cast<size_t>(k)].ins;
            for (auto it = ins.rbegin(); it != ins.rend(); ++it) {
                Entry& u = entries[static_cast<size_t>(*it)];
                Entry& entry = entries[static_cast<size_t>(k)];
                if (u.merged) continue;
                if (!u.hasBary || !entry.hasBary || u.barycenter >= entry.barycenter) mergeEntries(entry, u);
            }
            for (int w : std::vector<int>(entries[static_cast<size_t>(k)].outs)) {
                Entry& we = entries[static_cast<size_t>(w)];
                we.ins.push_back(k);
                if (--we.indegree == 0) sourceSet.push_back(w);
            }
        }
        std::vector<Entry> result;
        for (int k : processed) {
            if (!entries[static_cast<size_t>(k)].merged) result.push_back(entries[static_cast<size_t>(k)]);
        }
        return result;
    }

    // order/sort.ts
    static SortResult SortEntries(std::vector<Entry> entries, bool biasRight) {
        std::vector<Entry> sortable;
        std::vector<Entry> unsortable;
        for (auto& e : entries) (e.hasBary ? sortable : unsortable).push_back(std::move(e));
        std::stable_sort(unsortable.begin(), unsortable.end(), [](const Entry& a, const Entry& b) { return b.i < a.i; });
        std::stable_sort(sortable.begin(), sortable.end(), [&](const Entry& v, const Entry& w) {
            if (v.barycenter < w.barycenter) return true;
            if (v.barycenter > w.barycenter) return false;
            return !biasRight ? v.i < w.i : w.i < v.i;
        });
        SortResult result;
        float sum = 0.0f;
        float weight = 0.0f;
        int vsIndex = 0;
        auto consume = [&]() {
            while (!unsortable.empty() && unsortable.back().i <= vsIndex) {
                const Entry last = unsortable.back();
                unsortable.pop_back();
                result.vs.insert(result.vs.end(), last.vs.begin(), last.vs.end());
                ++vsIndex;
            }
        };
        consume();
        for (const Entry& entry : sortable) {
            vsIndex += static_cast<int>(entry.vs.size());
            result.vs.insert(result.vs.end(), entry.vs.begin(), entry.vs.end());
            sum += entry.barycenter * entry.weight;
            weight += entry.weight;
            consume();
        }
        while (!unsortable.empty()) { // Indices past the end keep their relative order
            const Entry last = unsortable.back();
            unsortable.pop_back();
            result.vs.insert(result.vs.end(), last.vs.begin(), last.vs.end());
        }
        if (weight != 0.0f) {
            result.hasBary = true;
            result.barycenter = sum / weight;
            result.weight = weight;
        }
        return result;
    }

    // order/sort-subgraph.ts
    SortResult SortSubgraph(const LayerGraph& lg, int v, const ConstraintGraph& cg, bool biasRight) const {
        std::vector<int> movable = lg.Children(v);
        int bl = -1;
        int br = -1;
        const auto bit = lg.borders.find(v);
        if (bit != lg.borders.end()) {
            bl = bit->second.first;
            br = bit->second.second;
            movable.erase(std::remove_if(movable.begin(), movable.end(), [&](int w) { return w == bl || w == br; }),
                          movable.end());
        }

        // order/barycenter.ts
        std::vector<Entry> entries(movable.size());
        for (size_t k = 0; k < movable.size(); ++k) {
            Entry& entry = entries[k];
            entry.vs = {movable[k]};
            entry.i = static_cast<int>(k);
            const auto it = lg.inEdges.find(movable[k]);
            if (it == lg.inEdges.end() || it->second.empty()) continue;
            float sum = 0.0f;
            float weight = 0.0f;
            for (const auto& [u, w] : it->second) {
                sum += w * static_cast<float>(m_g.N(u).order);
                weight += w;
            }
            entry.hasBary = true;
            entry.barycenter = weight != 0.0f ? sum / weight : 0.0f;
            entry.weight = weight;
        }

        std::map<int, SortResult> subgraphs;
        for (Entry& entry : entries) {
            const int w = entry.vs[0];
            if (lg.Children(w).empty()) continue;
            SortResult sub = SortSubgraph(lg, w, cg, biasRight);
            if (sub.hasBary) {
                if (entry.hasBary) {
                    entry.barycenter = (entry.barycenter * entry.weight + sub.barycenter * sub.weight) /
                                       (entry.weight + sub.weight);
                    entry.weight += sub.weight;
                } else {
                    entry.hasBary = true;
                    entry.barycenter = sub.barycenter;
                    entry.weight = sub.weight;
                }
            }
            subgraphs.emplace(w, std::move(sub));
        }

        std::vector<Entry> resolved = ResolveConflicts(std::move(entries), movable, cg);
        for (Entry& entry : resolved) { // expandSubgraphs
            std::vector<int> vs;
            for (int w : entry.vs) {
                const auto it = subgraphs.find(w);
                if (it != subgraphs.end()) {
                    vs.insert(vs.end(), it->second.vs.begin(), it->second.vs.end());
                } else {
                    vs.push_back(w);
                }
            }
            entry.vs = std::move(vs);
        }

        SortResult result = SortEntries(std::move(resolved), biasRight);
        if (bl >= 0 && br >= 0) {
            std::vector<int> vs{bl};
            vs.insert(vs.end(), result.vs.begin(), result.vs.end());
            vs.push_back(br);
            result.vs = std::move(vs);
            const auto blIn = lg.inEdges.find(bl);
            const auto brIn = lg.inEdges.find(br);
            if (blIn != lg.inEdges.end() && !blIn->second.empty() && brIn != lg.inEdges.end() && !brIn->second.empty()) {
                const float blPred = static_cast<float>(m_g.N(blIn->second[0].first).order);
                const float brPred = static_cast<float>(m_g.N(brIn->second[0].first).order);
                if (!result.hasBary) {
                    result.barycenter = 0.0f;
                    result.weight = 0.0f;
                }
                result.barycenter = (result.barycenter * result.weight + blPred + brPred) / (result.weight + 2.0f);
                result.weight += 2.0f;
                result.hasBary = true;
            }
        }
        return result;
    }

    // order/add-subgraph-constraints.ts
    static void AddSubgraphConstraints(const LayerGraph& lg, ConstraintGraph& cg, const std::vector<int>& vs) {
        constexpr int kNone = std::numeric_limits<int>::min();
        std::map<int, int> prev;
        int rootPrev = kNone;
        for (int v : vs) {
            int child = lg.Parent(v);
            while (child != -1) {
                const int parent = lg.Parent(child);
                int prevChild = kNone;
                if (parent != -1) {
                    const auto it = prev.find(parent);
                    if (it != prev.end()) prevChild = it->second;
                    prev[parent] = child;
                } else {
                    prevChild = rootPrev;
                    rootPrev = child;
                }
                if (prevChild != kNone && prevChild != child) {
                    const std::pair<int, int> edge{prevChild, child};
                    if (std::find(cg.begin(), cg.end(), edge) == cg.end()) cg.push_back(edge);
                    break;
                }
                child = parent;
            }
        }
    }

    // order/cross-count.ts
    float CrossCount(const std::vector<std::vector<int>>& layering) const {
        float cc = 0.0f;
        for (size_t i = 1; i < layering.size(); ++i) {
            const auto& north = layering[i - 1];
            const auto& south = layering[i];
            std::map<int, int> southPos;
            for (size_t k = 0; k < south.size(); ++k) southPos[south[k]] = static_cast<int>(k);
            std::vector<std::pair<int, float>> entries;
            for (int v : north) {
                std::vector<std::pair<int, float>> local;
                for (int e : m_g.out[static_cast<size_t>(v)]) {
                    const auto it = southPos.find(m_g.E(e).w);
                    if (it != southPos.end()) local.emplace_back(it->second, m_g.E(e).weight);
                }
                std::stable_sort(local.begin(), local.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
                entries.insert(entries.end(), local.begin(), local.end());
            }
            size_t firstIndex = 1;
            while (firstIndex < south.size()) firstIndex <<= 1;
            const size_t treeSize = 2 * firstIndex - 1;
            firstIndex -= 1;
            std::vector<float> tree(treeSize, 0.0f);
            for (const auto& [pos, weight] : entries) {
                size_t index = static_cast<size_t>(pos) + firstIndex;
                tree[index] += weight;
                float weightSum = 0.0f;
                while (index > 0) {
                    if (index % 2) weightSum += tree[index + 1];
                    index = (index - 1) >> 1;
                    tree[index] += weight;
                }
                cc += weight * weightSum;
            }
        }
        return cc;
    }

    // order/index.ts
    void Order() {
        const int maxRank = MaxRank();
        std::vector<LayerGraph> down;
        std::vector<LayerGraph> up;
        for (int r = 1; r <= maxRank; ++r) down.push_back(BuildLayerGraph(r, true));
        for (int r = maxRank - 1; r >= 0; --r) up.push_back(BuildLayerGraph(r, false));

        AssignOrder(InitOrder());
        std::vector<std::vector<int>> best = BuildLayerMatrix();
        float bestCC = std::numeric_limits<float>::max();
        for (int i = 0, lastBest = 0; lastBest < 4 && i < 64; ++i, ++lastBest) {
            const auto& layerGraphs = (i % 2) ? down : up;
            const bool biasRight = (i % 4) >= 2;
            ConstraintGraph cg;
            for (const LayerGraph& lg : layerGraphs) {
                const SortResult sorted = SortSubgraph(lg, kLayerRoot, cg, biasRight);
                for (size_t k = 0; k < sorted.vs.size(); ++k) m_g.N(sorted.vs[k]).order = static_cast<int>(k);
                AddSubgraphConstraints(lg, cg, sorted.vs);
            }
            std::vector<std::vector<int>> layering = BuildLayerMatrix();
            const float cc = CrossCount(layering);
            if (cc < bestCC) {
                lastBest = 0;
                best = std::move(layering);
                bestCC = cc;
            }
        }
        AssignOrder(best);
    }

    // --- self edges (layout.ts insertSelfEdges) ------------------------------------------------
    // A dummy as wide as the loop sits right after its node so that neighbours keep clear of it.
    void InsertSelfEdges() {
        for (const auto& layer : BuildLayerMatrix()) {
            int orderShift = 0;
            for (size_t i = 0; i < layer.size(); ++i) {
                const int v = layer[i];
                m_g.N(v).order = static_cast<int>(i) + orderShift;
                const std::vector<int> selfEdges = m_g.N(v).selfEdges;
                for (int input : selfEdges) {
                    LNode dummy;
                    dummy.width = LoopExtent(m_in.edges[static_cast<size_t>(input)]);
                    dummy.rank = m_g.N(v).rank;
                    dummy.hasRank = true;
                    dummy.order = static_cast<int>(i) + (++orderShift);
                    m_g.AddDummy(Dummy::SelfEdge, dummy);
                }
            }
        }
    }

    // --- position/index.ts ---------------------------------------------------------------------

    void PositionY() {
        m_layering = BuildLayerMatrix();
        float prevY = 0.0f;
        for (const auto& layer : m_layering) {
            float maxHeight = 0.0f;
            for (int v : layer) maxHeight = (std::max)(maxHeight, m_g.N(v).height);
            for (int v : layer) m_g.N(v).y = prevY + maxHeight * 0.5f;
            prevY += maxHeight + m_rankSep;
        }
    }

    // --- position/bk.ts --------------------------------------------------------------------------

    using ConflictSet = std::set<std::pair<int, int>>;

    static void AddConflict(ConflictSet& conflicts, int v, int w) {
        if (v > w) std::swap(v, w);
        conflicts.emplace(v, w);
    }

    static bool HasConflict(const ConflictSet& conflicts, int v, int w) {
        if (v > w) std::swap(v, w);
        return conflicts.count({v, w}) != 0;
    }

    int OtherInnerSegmentNode(int v) const {
        if (!IsDummy(m_g.N(v))) return -1;
        for (int u : m_g.Predecessors(v)) {
            if (IsDummy(m_g.N(u))) return u;
        }
        return -1;
    }

    void FindType1Conflicts(ConflictSet& conflicts) const {
        for (size_t r = 1; r < m_layering.size(); ++r) {
            const auto& prev = m_layering[r - 1];
            const auto& layer = m_layering[r];
            if (layer.empty()) continue;
            int k0 = 0;
            size_t scanPos = 0;
            const int lastNode = layer.back();
            for (size_t i = 0; i < layer.size(); ++i) {
                const int v = layer[i];
                const int w = OtherInnerSegmentNode(v);
                const int k1 = w >= 0 ? m_g.N(w).order : static_cast<int>(prev.size());
                if (w < 0 && v != lastNode) continue;
                for (size_t s = scanPos; s <= i; ++s) {
                    const int scanNode = layer[s];
                    for (int u : m_g.Predecessors(scanNode)) {
                        const LNode& uLabel = m_g.N(u);
                        const int uPos = uLabel.order;
                        if ((uPos < k0 || k1 < uPos) && !(IsDummy(uLabel) && IsDummy(m_g.N(scanNode)))) {
                            AddConflict(conflicts, u, scanNode);
                        }
                    }
                }
                scanPos = i + 1;
                k0 = k1;
            }
        }
    }

    void FindType2Conflicts(ConflictSet& conflicts) const {
        auto scan = [&](const std::vector<int>& south, size_t from, size_t to, int prevNorthBorder, int nextNorthBorder) {
            for (size_t i = from; i < to && i < south.size(); ++i) {
                const int v = south[i];
                if (!IsDummy(m_g.N(v))) continue;
                for (int u : m_g.Predecessors(v)) {
                    const LNode& uNode = m_g.N(u);
                    if (IsDummy(uNode) && (uNode.order < prevNorthBorder || uNode.order > nextNorthBorder)) {
                        AddConflict(conflicts, u, v);
                    }
                }
            }
        };
        for (size_t r = 1; r < m_layering.size(); ++r) {
            const auto& north = m_layering[r - 1];
            const auto& south = m_layering[r];
            int prevNorthPos = -1;
            int nextNorthPos = -1;
            size_t southPos = 0;
            for (size_t lookahead = 0; lookahead < south.size(); ++lookahead) {
                const int v = south[lookahead];
                if (m_g.N(v).dummy == Dummy::Border) {
                    const std::vector<int> preds = m_g.Predecessors(v);
                    if (!preds.empty()) {
                        nextNorthPos = m_g.N(preds[0]).order;
                        scan(south, southPos, lookahead, prevNorthPos, nextNorthPos);
                        southPos = lookahead;
                        prevNorthPos = nextNorthPos;
                    }
                }
                scan(south, southPos, south.size(), nextNorthPos, static_cast<int>(north.size()));
            }
        }
    }

    void VerticalAlignment(const std::vector<std::vector<int>>& layering, const ConflictSet& conflicts, bool usePreds,
                           std::vector<int>& root) const {
        const size_t n = m_g.nodes.size();
        root.assign(n, -1);
        std::vector<int> align(n, -1);
        std::vector<int> pos(n, 0);
        for (const auto& layer : layering) {
            for (size_t i = 0; i < layer.size(); ++i) {
                const auto v = static_cast<size_t>(layer[i]);
                root[v] = align[v] = layer[i];
                pos[v] = static_cast<int>(i);
            }
        }
        for (const auto& layer : layering) {
            int prevIdx = -1;
            for (int v : layer) {
                std::vector<int> ws = usePreds ? m_g.Predecessors(v) : m_g.Successors(v);
                if (ws.empty()) continue;
                std::stable_sort(ws.begin(), ws.end(),
                                 [&](int a, int b) { return pos[static_cast<size_t>(a)] < pos[static_cast<size_t>(b)]; });
                const size_t lo = (ws.size() - 1) / 2;
                const size_t hi = ws.size() / 2;
                for (size_t i = lo; i <= hi; ++i) {
                    const int w = ws[i];
                    if (root[static_cast<size_t>(w)] < 0) continue;
                    const int posW = pos[static_cast<size_t>(w)];
                    if (align[static_cast<size_t>(v)] == v && prevIdx < posW && !HasConflict(conflicts, v, w)) {
                        align[static_cast<size_t>(w)] = v;
                        align[static_cast<size_t>(v)] = root[static_cast<size_t>(v)] = root[static_cast<size_t>(w)];
                        prevIdx = posW;
                    }
                }
            }
        }
    }

    float Sep(int v, int w) const {
        const LNode& vl = m_g.N(v);
        const LNode& wl = m_g.N(w);
        return vl.width * 0.5f + (IsDummy(vl) ? kEdgeSep : m_nodeSep) * 0.5f + (IsDummy(wl) ? kEdgeSep : m_nodeSep) * 0.5f +
               wl.width * 0.5f;
    }

    bool HorizontalCompaction(const std::vector<std::vector<int>>& layering, const std::vector<int>& root,
                              bool reverseSep, std::vector<float>& xs) const {
        const size_t n = m_g.nodes.size();
        std::vector<std::vector<Constraint>> outC(n);
        std::vector<std::vector<Constraint>> inC(n);
        std::vector<char> isBlock(n, 0);
        std::vector<int> blocks;
        for (const auto& layer : layering) { // buildBlockGraph
            int u = -1;
            for (int v : layer) {
                const int vRoot = root[static_cast<size_t>(v)];
                if (!isBlock[static_cast<size_t>(vRoot)]) {
                    isBlock[static_cast<size_t>(vRoot)] = 1;
                    blocks.push_back(vRoot);
                }
                if (u >= 0) {
                    const int uRoot = root[static_cast<size_t>(u)];
                    const float s = Sep(v, u);
                    auto& edges = outC[static_cast<size_t>(uRoot)];
                    auto it = std::find_if(edges.begin(), edges.end(), [&](const Constraint& c) { return c.to == vRoot; });
                    if (it != edges.end()) {
                        it->dist = (std::max)(it->dist, s);
                        for (auto& c : inC[static_cast<size_t>(vRoot)]) {
                            if (c.to == uRoot) c.dist = it->dist;
                        }
                    } else {
                        edges.push_back({vRoot, s});
                        inC[static_cast<size_t>(vRoot)].push_back({uRoot, s});
                    }
                }
                u = v;
            }
        }

        std::vector<int> indegree(n, 0);
        for (int b : blocks) indegree[static_cast<size_t>(b)] = static_cast<int>(inC[static_cast<size_t>(b)].size());
        std::vector<int> topo;
        for (int b : blocks) {
            if (indegree[static_cast<size_t>(b)] == 0) topo.push_back(b);
        }
        for (size_t k = 0; k < topo.size(); ++k) {
            for (const Constraint& c : outC[static_cast<size_t>(topo[k])]) {
                if (--indegree[static_cast<size_t>(c.to)] == 0) topo.push_back(c.to);
            }
        }
        if (topo.size() != blocks.size()) return false;

        xs.assign(n, 0.0f);
        for (int b : topo) { // Pass 1: smallest coordinates
            float x = 0.0f;
            for (const Constraint& c : inC[static_cast<size_t>(b)]) x = (std::max)(x, xs[static_cast<size_t>(c.to)] + c.dist);
            xs[static_cast<size_t>(b)] = x;
        }
        const BorderType borderType = reverseSep ? BorderType::Left : BorderType::Right;
        for (size_t k = topo.size(); k-- > 0;) { // Pass 2: remove unused space
            const auto b = static_cast<size_t>(topo[k]);
            if (outC[b].empty()) continue;
            float minX = std::numeric_limits<float>::max();
            for (const Constraint& c : outC[b]) minX = (std::min)(minX, xs[static_cast<size_t>(c.to)] - c.dist);
            if (m_g.nodes[b].borderType != borderType) xs[b] = (std::max)(xs[b], minX);
        }
        for (const auto& layer : layering) {
            for (int v : layer) xs[static_cast<size_t>(v)] = xs[static_cast<size_t>(root[static_cast<size_t>(v)])];
        }
        return true;
    }

    void PositionX() {
        ConflictSet conflicts;
        FindType1Conflicts(conflicts);
        FindType2Conflicts(conflicts);

        std::vector<int> all;
        for (const auto& layer : m_layering) all.insert(all.end(), layer.begin(), layer.end());
        if (all.empty()) return;

        // 0 = up-left, 1 = up-right, 2 = down-left, 3 = down-right
        std::array<std::vector<float>, 4> xss;
        for (size_t a = 0; a < 4; ++a) {
            const bool down = a >= 2;
            const bool right = (a % 2) == 1;
            std::vector<std::vector<int>> layering = m_layering;
            if (down) std::reverse(layering.begin(), layering.end());
            if (right) {
                for (auto& layer : layering) std::reverse(layer.begin(), layer.end());
            }
            std::vector<int> root;
            VerticalAlignment(layering, conflicts, !down, root);
            if (!HorizontalCompaction(layering, root, right, xss[a])) {
                // Defensive (the block graph is acyclic): pack every layer.
                xss[a].assign(m_g.nodes.size(), 0.0f);
                for (const auto& layer : layering) {
                    float x = 0.0f;
                    for (size_t i = 0; i < layer.size(); ++i) {
                        if (i > 0) x += Sep(layer[i - 1], layer[i]);
                        xss[a][static_cast<size_t>(layer[i])] = x;
                    }
                }
            }
            if (right) {
                for (int v : all) xss[a][static_cast<size_t>(v)] = -xss[a][static_cast<size_t>(v)];
            }
        }

        // findSmallestWidthAlignment
        size_t best = 0;
        float bestWidth = std::numeric_limits<float>::max();
        for (size_t a = 0; a < 4; ++a) {
            float lo = std::numeric_limits<float>::max();
            float hi = std::numeric_limits<float>::lowest();
            for (int v : all) {
                const float half = m_g.N(v).width * 0.5f;
                lo = (std::min)(lo, xss[a][static_cast<size_t>(v)] - half);
                hi = (std::max)(hi, xss[a][static_cast<size_t>(v)] + half);
            }
            if (hi - lo < bestWidth) {
                bestWidth = hi - lo;
                best = a;
            }
        }
        // alignCoordinates
        auto range = [&](const std::vector<float>& xs, float& lo, float& hi) {
            lo = std::numeric_limits<float>::max();
            hi = std::numeric_limits<float>::lowest();
            for (int v : all) {
                lo = (std::min)(lo, xs[static_cast<size_t>(v)]);
                hi = (std::max)(hi, xs[static_cast<size_t>(v)]);
            }
        };
        float alignMin = 0.0f;
        float alignMax = 0.0f;
        range(xss[best], alignMin, alignMax);
        for (size_t a = 0; a < 4; ++a) {
            if (a == best) continue;
            float lo = 0.0f;
            float hi = 0.0f;
            range(xss[a], lo, hi);
            const float delta = (a % 2 == 0) ? alignMin - lo : alignMax - hi;
            for (int v : all) xss[a][static_cast<size_t>(v)] += delta;
        }
        // balance
        for (int v : all) {
            std::array<float, 4> c{xss[0][static_cast<size_t>(v)], xss[1][static_cast<size_t>(v)],
                                   xss[2][static_cast<size_t>(v)], xss[3][static_cast<size_t>(v)]};
            std::sort(c.begin(), c.end());
            m_g.N(v).x = (c[1] + c[2]) * 0.5f;
        }
    }

    // --- results (normalize.undo, removeBorderNodes, coordinateSystem.undo, translateGraph) -------

    Point ToFinal(float x, float y) const {
        switch (m_in.direction) {
        case Direction::TB:
            return Point{x, y};
        case Direction::BT:
            return Point{x, -y};
        case Direction::LR:
            return Point{y, x};
        case Direction::RL:
            return Point{-y, x};
        }
        return Point{x, y};
    }

    GraphLayoutResult Finish() {
        GraphLayoutResult result;
        const size_t n = m_in.nodes.size();
        float minX = std::numeric_limits<float>::max();
        float minY = minX;
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = maxX;
        auto include = [&](float x0, float y0, float x1, float y1) {
            minX = (std::min)(minX, x0);
            minY = (std::min)(minY, y0);
            maxX = (std::max)(maxX, x1);
            maxY = (std::max)(maxY, y1);
        };

        result.nodeCenters.resize(n);
        for (size_t i = 0; i < n; ++i) {
            const LNode& node = m_g.nodes[i];
            const Point c = ToFinal(node.x, node.y);
            result.nodeCenters[i] = c;
            const GraphNode& gn = m_in.nodes[i];
            include(c.x - gn.width * 0.5f, c.y - gn.height * 0.5f, c.x + gn.width * 0.5f, c.y + gn.height * 0.5f);
        }

        // Bend points of every dummy chain, in the working (acyclic) direction.
        result.edgePoints.resize(m_in.edges.size());
        result.labelCenters.resize(m_in.edges.size());
        std::vector<char> hasLabel(m_in.edges.size(), 0);
        std::vector<std::vector<Point>> bends(m_in.edges.size());
        std::vector<char> reversed(m_in.edges.size(), 0);
        for (const LEdge& e : m_g.edges) {
            if (e.input >= 0 && e.reversed) reversed[static_cast<size_t>(e.input)] = 1;
        }
        for (int start : m_dummyChains) {
            const int input = m_g.E(m_g.N(start).edgeRef).input;
            if (input < 0) continue;
            int v = start;
            int guard = 0;
            while (v >= 0 && (m_g.N(v).dummy == Dummy::Edge || m_g.N(v).dummy == Dummy::EdgeLabel) && guard++ < 100000) {
                const LNode& node = m_g.N(v);
                const Point p = ToFinal(node.x, node.y);
                bends[static_cast<size_t>(input)].push_back(p);
                if (node.dummy == Dummy::EdgeLabel) {
                    result.labelCenters[static_cast<size_t>(input)] = p;
                    hasLabel[static_cast<size_t>(input)] = 1;
                }
                const std::vector<int> succ = m_g.Successors(v);
                v = succ.empty() ? -1 : succ[0];
            }
        }

        const Point axis = m_horizontal ? Point{0.0f, 1.0f} : Point{1.0f, 0.0f};  // In-layer direction
        const Point cross = m_horizontal ? Point{1.0f, 0.0f} : Point{0.0f, 1.0f}; // Rank direction
        for (size_t i = 0; i < m_in.edges.size(); ++i) {
            const GraphEdge& e = m_in.edges[i];
            if (e.from < 0 || e.to < 0 || e.from >= static_cast<int>(n) || e.to >= static_cast<int>(n)) continue;
            if (e.from == e.to) {
                const GraphNode& node = m_in.nodes[static_cast<size_t>(e.from)];
                const Point c = result.nodeCenters[static_cast<size_t>(e.from)];
                const float ha = m_horizontal ? node.height * 0.5f : node.width * 0.5f;
                const float hp = m_horizontal ? node.width * 0.5f : node.height * 0.5f;
                auto at = [&](float a, float p) { return Point{c.x + axis.x * a + cross.x * p, c.y + axis.y * a + cross.y * p}; };
                const float spread = (std::min)(hp * 0.5f, 14.0f);
                result.edgePoints[i] = {at(ha, -spread), at(ha + kLoopReach, -spread - 12.0f),
                                        at(ha + kLoopReach, spread + 12.0f), at(ha, spread)};
                result.labelCenters[i] = at(ha + kLoopReach * 0.75f + 6.0f + LabelAlong(e) * 0.5f, 0.0f);
                const Point far = at(ha + LoopExtent(e), 0.0f);
                include(far.x - 1.0f, far.y - 1.0f, far.x + 1.0f, far.y + 1.0f);
                if (e.labelWidth > 0.0f) {
                    const Point l = result.labelCenters[i];
                    include(l.x - e.labelWidth * 0.5f, l.y - e.labelHeight * 0.5f, l.x + e.labelWidth * 0.5f,
                            l.y + e.labelHeight * 0.5f);
                }
                continue;
            }
            std::vector<Point> mid = bends[i];
            if (reversed[i]) std::reverse(mid.begin(), mid.end());
            auto& points = result.edgePoints[i];
            points.push_back(result.nodeCenters[static_cast<size_t>(e.from)]);
            points.insert(points.end(), mid.begin(), mid.end());
            points.push_back(result.nodeCenters[static_cast<size_t>(e.to)]);
            if (!hasLabel[i]) {
                const Point a = points[points.size() / 2 - (points.size() % 2 == 0 ? 1 : 0)];
                const Point b = points[points.size() / 2];
                result.labelCenters[i] = Point{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
            }
            if (e.labelWidth > 0.0f) {
                const Point l = result.labelCenters[i];
                include(l.x - e.labelWidth * 0.5f, l.y - e.labelHeight * 0.5f, l.x + e.labelWidth * 0.5f,
                        l.y + e.labelHeight * 0.5f);
            }
            for (const Point& p : points) include(p.x, p.y, p.x, p.y);
        }

        // Cluster frames from their border nodes.
        result.clusterRects.resize(m_in.clusters.size());
        for (size_t c = 0; c < m_in.clusters.size(); ++c) {
            const int v = m_clusterNode[c];
            if (v < 0) continue;
            const LNode& node = m_g.N(v);
            if (node.borderTop < 0 || node.borderLeft.empty()) continue;
            float lx = std::numeric_limits<float>::max();
            float rx = std::numeric_limits<float>::lowest();
            for (int bl : node.borderLeft) {
                if (bl >= 0) lx = (std::min)(lx, m_g.N(bl).x - m_g.N(bl).width * 0.5f);
            }
            for (int br : node.borderRight) {
                if (br >= 0) rx = (std::max)(rx, m_g.N(br).x + m_g.N(br).width * 0.5f);
            }
            if (lx > rx) continue;
            const LNode& top = m_g.N(node.borderTop);
            const LNode& bottom = m_g.N(node.borderBottom);
            const Point p0 = ToFinal(lx, top.y - top.height * 0.5f);
            const Point p1 = ToFinal(rx, bottom.y + bottom.height * 0.5f);
            float x0 = (std::min)(p0.x, p1.x);
            float x1 = (std::max)(p0.x, p1.x);
            const float y0 = (std::min)(p0.y, p1.y);
            const float y1 = (std::max)(p0.y, p1.y);
            const float minWidth = m_in.clusters[c].titleWidth + 2.0f * kClusterPadding;
            if (x1 - x0 < minWidth) {
                const float grow = (minWidth - (x1 - x0)) * 0.5f;
                x0 -= grow;
                x1 += grow;
            }
            result.clusterRects[c] = LayoutRect{x0, y0, x1 - x0, y1 - y0, false};
            include(x0, y0, x1, y1);
        }

        if (minX > maxX) {
            minX = minY = 0.0f;
            maxX = maxY = 0.0f;
        }
        const float dx = kMargin - minX;
        const float dy = kMargin - minY;
        auto shift = [&](Point& p) {
            p.x += dx;
            p.y += dy;
        };
        for (auto& p : result.nodeCenters) shift(p);
        for (auto& pts : result.edgePoints) {
            for (auto& p : pts) shift(p);
        }
        for (auto& p : result.labelCenters) shift(p);
        for (auto& r : result.clusterRects) {
            r.x += dx;
            r.y += dy;
        }
        result.width = maxX - minX + 2.0f * kMargin;
        result.height = maxY - minY + 2.0f * kMargin;
        return result;
    }

    const GraphLayoutInput& m_in;
    bool m_horizontal = false;
    float m_nodeSep = 50.0f;
    float m_rankSep = 50.0f;
    LGraph m_g;
    std::vector<int> m_clusterNode; // Input cluster -> graph node (-1 when it has no nodes)
    int m_nestingRoot = -1;
    int m_nestingHeight = 0;
    int m_nodeRankFactor = 1;
    float m_nestingWeight = 1.0f;
    std::vector<int> m_depth;
    std::vector<int> m_dummyChains;
    std::vector<std::vector<int>> m_layering;
};

} // namespace

GraphLayoutResult LayoutGraph(const GraphLayoutInput& input) {
    return Layouter(input).Run();
}

} // namespace Pluma::Diagram::detail
