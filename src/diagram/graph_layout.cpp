#include "graph_layout.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>

namespace Pluma::Diagram::detail {

namespace {

constexpr float kEdgeSep = 14.0f;      // Between a bend point and its neighbours in a rank
constexpr float kLoopReach = 34.0f;    // How far a self loop bulges out of its node
constexpr float kMargin = 8.0f;
constexpr int kOrderIterations = 24;
constexpr int kPositionRounds = 8;

struct Vertex {
    float lw = 0.0f;           // Extent along the rank (in-layer axis)
    float lh = 0.0f;           // Extent across ranks
    std::vector<int> path;     // Enclosing clusters, outermost first
    bool dummy = false;
    int rank = 0;
    int order = 0;
    float x = 0.0f;            // In-layer coordinate (centre)
    float y = 0.0f;            // Rank coordinate (centre)
    std::vector<std::pair<int, float>> preds; // (vertex, weight)
    std::vector<std::pair<int, float>> succs;
};

struct Chain {
    std::vector<int> vertices; // Working direction (after cycle removal)
    int labelVertex = -1;
    bool reversed = false;
};

std::vector<int> CommonPrefix(const std::vector<int>& a, const std::vector<int>& b) {
    std::vector<int> out;
    for (size_t i = 0; i < a.size() && i < b.size() && a[i] == b[i]; ++i) {
        out.push_back(a[i]);
    }
    return out;
}

// Weighted isotonic regression with minimum separations: returns x[i] as close as possible
// (least squares) to desired[i] while keeping x[i] >= x[i-1] + sep[i].
std::vector<float> PlaceWithSeparation(const std::vector<float>& desired, const std::vector<float>& weights,
                                       const std::vector<float>& sep) {
    const size_t n = desired.size();
    std::vector<float> offset(n, 0.0f);
    for (size_t i = 1; i < n; ++i) {
        offset[i] = offset[i - 1] + sep[i];
    }

    struct Block {
        double sumW = 0.0;
        double sumWT = 0.0;
        size_t count = 0;
        double Mean() const { return sumW > 0.0 ? sumWT / sumW : 0.0; }
    };
    std::vector<Block> blocks;
    for (size_t i = 0; i < n; ++i) {
        const double w = (std::max)(1.0e-3f, weights[i]);
        blocks.push_back(Block{w, w * static_cast<double>(desired[i] - offset[i]), 1});
        while (blocks.size() > 1 && blocks[blocks.size() - 2].Mean() > blocks.back().Mean()) {
            Block last = blocks.back();
            blocks.pop_back();
            blocks.back().sumW += last.sumW;
            blocks.back().sumWT += last.sumWT;
            blocks.back().count += last.count;
        }
    }

    std::vector<float> x(n);
    size_t i = 0;
    for (const auto& b : blocks) {
        const auto mean = static_cast<float>(b.Mean());
        for (size_t k = 0; k < b.count; ++k, ++i) {
            x[i] = mean + offset[i];
        }
    }
    return x;
}

class Layouter {
public:
    explicit Layouter(const GraphLayoutInput& input) : m_in(input) {}

    GraphLayoutResult Run() {
        m_horizontal = IsHorizontal(m_in.direction);
        BuildVertices();
        RemoveCycles();
        AssignRanks();
        BuildChains();
        OrderLayers();
        AssignInLayerPositions();
        SeparateClusters();
        AssignRankPositions();
        return Finish();
    }

private:
    std::vector<int> ClusterPath(int cluster) const {
        std::vector<int> path;
        int guard = 0;
        while (cluster >= 0 && cluster < static_cast<int>(m_in.clusters.size()) && guard++ < 64) {
            path.push_back(cluster);
            cluster = m_in.clusters[static_cast<size_t>(cluster)].parent;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    float LabelAlong(const GraphEdge& e) const {
        return m_horizontal ? e.labelHeight : e.labelWidth;
    }

    float LoopExtent(const GraphEdge& e) const {
        const float label = e.labelWidth > 0.0f ? LabelAlong(e) + 6.0f : 0.0f;
        return kLoopReach * 0.75f + 6.0f + label;
    }

    void BuildVertices() {
        const size_t n = m_in.nodes.size();
        m_vertices.resize(n);
        for (size_t i = 0; i < n; ++i) {
            const GraphNode& node = m_in.nodes[i];
            Vertex& v = m_vertices[i];
            v.lw = m_horizontal ? node.height : node.width;
            v.lh = m_horizontal ? node.width : node.height;
            v.path = ClusterPath(node.cluster);
        }
        for (const auto& e : m_in.edges) {
            if (e.from == e.to && e.from >= 0 && e.from < static_cast<int>(n)) {
                m_vertices[static_cast<size_t>(e.from)].lw += 2.0f * LoopExtent(e);
            }
            if (e.from != e.to && e.labelWidth > 0.0f) {
                m_anyLabel = true;
            }
        }
    }

    bool IsValidEdge(const GraphEdge& e) const {
        const int n = static_cast<int>(m_in.nodes.size());
        return e.from >= 0 && e.to >= 0 && e.from < n && e.to < n && e.from != e.to;
    }

    // Depth-first search in declaration order; edges closing a cycle are reversed.
    void RemoveCycles() {
        const size_t n = m_in.nodes.size();
        m_reversed.assign(m_in.edges.size(), false);
        std::vector<std::vector<size_t>> out(n);
        for (size_t i = 0; i < m_in.edges.size(); ++i) {
            if (IsValidEdge(m_in.edges[i])) {
                out[static_cast<size_t>(m_in.edges[i].from)].push_back(i);
            }
        }

        std::vector<int> state(n, 0); // 0 new, 1 on stack, 2 done
        for (size_t root = 0; root < n; ++root) {
            if (state[root] != 0) continue;
            std::vector<std::pair<size_t, size_t>> stack{{root, 0}};
            state[root] = 1;
            while (!stack.empty()) {
                auto& [node, next] = stack.back();
                if (next < out[node].size()) {
                    const size_t edge = out[node][next++];
                    const auto target = static_cast<size_t>(m_in.edges[edge].to);
                    if (state[target] == 1) {
                        m_reversed[edge] = true;
                    } else if (state[target] == 0) {
                        state[target] = 1;
                        stack.emplace_back(target, 0);
                    }
                } else {
                    state[node] = 2;
                    stack.pop_back();
                }
            }
        }
    }

    std::pair<int, int> WorkingEnds(size_t edge) const {
        const GraphEdge& e = m_in.edges[edge];
        return m_reversed[edge] ? std::make_pair(e.to, e.from) : std::make_pair(e.from, e.to);
    }

    int MinLen(size_t edge) const {
        return (std::max)(1, m_in.edges[edge].minLen) * (m_anyLabel ? 2 : 1);
    }

    // Longest path from the sources, then sources are pulled down next to their successors.
    void AssignRanks() {
        const size_t n = m_in.nodes.size();
        std::vector<std::vector<size_t>> out(n);
        std::vector<int> indegree(n, 0);
        for (size_t i = 0; i < m_in.edges.size(); ++i) {
            if (!IsValidEdge(m_in.edges[i])) continue;
            const auto [u, v] = WorkingEnds(i);
            out[static_cast<size_t>(u)].push_back(i);
            indegree[static_cast<size_t>(v)]++;
        }

        std::vector<int> remaining = indegree;
        std::deque<size_t> queue;
        for (size_t i = 0; i < n; ++i) {
            if (remaining[i] == 0) queue.push_back(i);
        }
        std::vector<size_t> topo;
        std::vector<int> rank(n, 0);
        while (!queue.empty()) {
            const size_t u = queue.front();
            queue.pop_front();
            topo.push_back(u);
            for (size_t edge : out[u]) {
                const auto v = static_cast<size_t>(WorkingEnds(edge).second);
                rank[v] = (std::max)(rank[v], rank[u] + MinLen(edge));
                if (--remaining[v] == 0) queue.push_back(v);
            }
        }

        for (auto it = topo.rbegin(); it != topo.rend(); ++it) {
            const size_t u = *it;
            if (indegree[u] != 0 || out[u].empty()) continue;
            int best = std::numeric_limits<int>::max();
            for (size_t edge : out[u]) {
                best = (std::min)(best, rank[static_cast<size_t>(WorkingEnds(edge).second)] - MinLen(edge));
            }
            rank[u] = best;
        }

        const int minRank = n ? *std::min_element(rank.begin(), rank.end()) : 0;
        for (size_t i = 0; i < n; ++i) {
            m_vertices[i].rank = rank[i] - minRank;
        }
    }

    static float EdgeWeight(const Vertex& a, const Vertex& b) {
        if (a.dummy && b.dummy) return 8.0f;
        if (a.dummy || b.dummy) return 2.0f;
        return 1.0f;
    }

    void Connect(int a, int b) {
        const float w = EdgeWeight(m_vertices[static_cast<size_t>(a)], m_vertices[static_cast<size_t>(b)]);
        m_vertices[static_cast<size_t>(a)].succs.emplace_back(b, w);
        m_vertices[static_cast<size_t>(b)].preds.emplace_back(a, w);
    }

    // Long edges are split with one dummy vertex per crossed rank; the middle one carries the label.
    void BuildChains() {
        m_chains.resize(m_in.edges.size());
        for (size_t i = 0; i < m_in.edges.size(); ++i) {
            if (!IsValidEdge(m_in.edges[i])) continue;
            const GraphEdge& e = m_in.edges[i];
            const auto [u, v] = WorkingEnds(i);
            Chain& chain = m_chains[i];
            chain.reversed = m_reversed[i];
            chain.vertices.push_back(u);

            const int span = m_vertices[static_cast<size_t>(v)].rank - m_vertices[static_cast<size_t>(u)].rank;
            const std::vector<int> path = CommonPrefix(m_vertices[static_cast<size_t>(u)].path,
                                                       m_vertices[static_cast<size_t>(v)].path);
            const int labelStep = (e.labelWidth > 0.0f) ? (std::max)(1, span / 2) : -1;
            int prev = u;
            for (int k = 1; k < span; ++k) {
                Vertex d;
                d.dummy = true;
                d.rank = m_vertices[static_cast<size_t>(u)].rank + k;
                d.path = path;
                if (k == labelStep) {
                    d.lw = m_horizontal ? e.labelHeight : e.labelWidth;
                    d.lh = m_horizontal ? e.labelWidth : e.labelHeight;
                }
                m_vertices.push_back(std::move(d));
                const int id = static_cast<int>(m_vertices.size()) - 1;
                if (k == labelStep) chain.labelVertex = id;
                Connect(prev, id);
                chain.vertices.push_back(id);
                prev = id;
            }
            Connect(prev, v);
            chain.vertices.push_back(v);
        }

        int maxRank = 0;
        for (const auto& v : m_vertices) maxRank = (std::max)(maxRank, v.rank);
        m_layers.assign(static_cast<size_t>(maxRank) + 1, {});
    }

    // --- ordering ---------------------------------------------------------------

    void InitialOrder() {
        std::vector<bool> seen(m_vertices.size(), false);
        std::vector<int> roots(m_in.nodes.size());
        for (size_t i = 0; i < roots.size(); ++i) roots[i] = static_cast<int>(i);
        std::stable_sort(roots.begin(), roots.end(), [this](int a, int b) {
            return m_vertices[static_cast<size_t>(a)].rank < m_vertices[static_cast<size_t>(b)].rank;
        });

        auto visit = [&](int root) {
            std::vector<int> stack{root};
            while (!stack.empty()) {
                const int v = stack.back();
                stack.pop_back();
                if (seen[static_cast<size_t>(v)]) continue;
                seen[static_cast<size_t>(v)] = true;
                m_layers[static_cast<size_t>(m_vertices[static_cast<size_t>(v)].rank)].push_back(v);
                const auto& succs = m_vertices[static_cast<size_t>(v)].succs;
                for (auto it = succs.rbegin(); it != succs.rend(); ++it) {
                    if (!seen[static_cast<size_t>(it->first)]) stack.push_back(it->first);
                }
            }
        };
        for (int r : roots) visit(r);
        for (size_t v = 0; v < m_vertices.size(); ++v) {
            if (!seen[v]) visit(static_cast<int>(v));
        }
        UpdateOrderIndices();
    }

    void UpdateOrderIndices() {
        for (const auto& layer : m_layers) {
            for (size_t i = 0; i < layer.size(); ++i) {
                m_vertices[static_cast<size_t>(layer[i])].order = static_cast<int>(i);
            }
        }
    }

    // Sorts a layer by barycentre, keeping the members of each cluster contiguous.
    void SortLayer(size_t r, bool usePreds) {
        auto& layer = m_layers[r];
        std::vector<float> bary(m_vertices.size(), 0.0f);
        for (int v : layer) {
            const Vertex& vx = m_vertices[static_cast<size_t>(v)];
            const auto& nbs = usePreds ? vx.preds : vx.succs;
            float sum = 0.0f;
            float weight = 0.0f;
            for (const auto& [nb, w] : nbs) {
                sum += w * static_cast<float>(m_vertices[static_cast<size_t>(nb)].order);
                weight += w;
            }
            bary[static_cast<size_t>(v)] = weight > 0.0f ? sum / weight : static_cast<float>(vx.order);
        }

        // Barycentre and first position of every cluster present in this layer.
        std::vector<float> clusterSum(m_in.clusters.size(), 0.0f);
        std::vector<int> clusterCount(m_in.clusters.size(), 0);
        std::vector<int> clusterFirst(m_in.clusters.size(), std::numeric_limits<int>::max());
        for (int v : layer) {
            const Vertex& vx = m_vertices[static_cast<size_t>(v)];
            for (int c : vx.path) {
                clusterSum[static_cast<size_t>(c)] += bary[static_cast<size_t>(v)];
                clusterCount[static_cast<size_t>(c)]++;
                clusterFirst[static_cast<size_t>(c)] = (std::min)(clusterFirst[static_cast<size_t>(c)], vx.order);
            }
        }

        auto groupKey = [&](int v, size_t depth) -> std::pair<float, int> {
            const Vertex& vx = m_vertices[static_cast<size_t>(v)];
            if (depth < vx.path.size()) {
                const auto c = static_cast<size_t>(vx.path[depth]);
                return {clusterSum[c] / static_cast<float>((std::max)(1, clusterCount[c])), clusterFirst[c]};
            }
            return {bary[static_cast<size_t>(v)], vx.order};
        };
        auto groupId = [&](int v, size_t depth) -> long long {
            const Vertex& vx = m_vertices[static_cast<size_t>(v)];
            return depth < vx.path.size() ? -1 - static_cast<long long>(vx.path[depth]) : static_cast<long long>(v);
        };

        std::stable_sort(layer.begin(), layer.end(), [&](int a, int b) {
            if (a == b) return false;
            for (size_t depth = 0;; ++depth) {
                const long long ga = groupId(a, depth);
                const long long gb = groupId(b, depth);
                if (ga != gb) {
                    const auto ka = groupKey(a, depth);
                    const auto kb = groupKey(b, depth);
                    if (ka.first != kb.first) return ka.first < kb.first;
                    return ka.second < kb.second;
                }
            }
        });
        for (size_t i = 0; i < layer.size(); ++i) {
            m_vertices[static_cast<size_t>(layer[i])].order = static_cast<int>(i);
        }
    }

    long long CountCrossings() const {
        long long crossings = 0;
        std::vector<std::pair<int, int>> edges;
        for (size_t r = 0; r + 1 < m_layers.size(); ++r) {
            edges.clear();
            for (int u : m_layers[r]) {
                const Vertex& vu = m_vertices[static_cast<size_t>(u)];
                for (const auto& [v, w] : vu.succs) {
                    edges.emplace_back(vu.order, m_vertices[static_cast<size_t>(v)].order);
                }
            }
            for (size_t i = 0; i < edges.size(); ++i) {
                for (size_t j = i + 1; j < edges.size(); ++j) {
                    const auto& a = edges[i];
                    const auto& b = edges[j];
                    if ((a.first < b.first && a.second > b.second) || (a.first > b.first && a.second < b.second)) {
                        ++crossings;
                    }
                }
            }
        }
        return crossings;
    }

    void OrderLayers() {
        InitialOrder();
        auto best = m_layers;
        long long bestCrossings = CountCrossings();
        for (int iter = 0; iter < kOrderIterations && bestCrossings > 0; ++iter) {
            if (iter % 2 == 0) {
                for (size_t r = 1; r < m_layers.size(); ++r) SortLayer(r, true);
            } else {
                for (size_t r = m_layers.size(); r-- > 1;) SortLayer(r - 1, false);
            }
            const long long crossings = CountCrossings();
            if (crossings < bestCrossings) {
                bestCrossings = crossings;
                best = m_layers;
            }
        }
        m_layers = std::move(best);
        UpdateOrderIndices();
    }

    // --- coordinates ------------------------------------------------------------

    float ClusterPadAlong() const {
        // The title sits on top of the frame: along the rank axis only for horizontal layouts.
        return m_horizontal ? kClusterPadding + MaxTitleHeight() : kClusterPadding;
    }

    float MaxTitleHeight() const {
        float h = 0.0f;
        for (const auto& c : m_in.clusters) h = (std::max)(h, c.titleHeight);
        return h;
    }

    float Separation(int left, int right) const {
        const Vertex& a = m_vertices[static_cast<size_t>(left)];
        const Vertex& b = m_vertices[static_cast<size_t>(right)];
        float sep = (a.lw + b.lw) * 0.5f + ((a.dummy || b.dummy) ? kEdgeSep : m_in.nodeSep);
        size_t common = 0;
        while (common < a.path.size() && common < b.path.size() && a.path[common] == b.path[common]) ++common;
        const size_t boundaries = (a.path.size() - common) + (b.path.size() - common);
        sep += static_cast<float>(boundaries) * ClusterPadAlong();
        return sep;
    }

    void PlaceLayer(size_t r, bool usePreds, bool useSuccs) {
        const auto& layer = m_layers[r];
        if (layer.empty()) return;
        std::vector<float> desired(layer.size());
        std::vector<float> weights(layer.size());
        std::vector<float> sep(layer.size(), 0.0f);
        for (size_t i = 0; i < layer.size(); ++i) {
            const Vertex& v = m_vertices[static_cast<size_t>(layer[i])];
            float sum = 0.0f;
            float weight = 0.0f;
            auto accumulate = [&](const std::vector<std::pair<int, float>>& nbs) {
                for (const auto& [nb, w] : nbs) {
                    sum += w * m_vertices[static_cast<size_t>(nb)].x;
                    weight += w;
                }
            };
            if (usePreds) accumulate(v.preds);
            if (useSuccs) accumulate(v.succs);
            desired[i] = weight > 0.0f ? sum / weight : v.x;
            weights[i] = weight > 0.0f ? weight : 0.25f;
            if (i > 0) sep[i] = Separation(layer[i - 1], layer[i]);
        }
        const std::vector<float> x = PlaceWithSeparation(desired, weights, sep);
        for (size_t i = 0; i < layer.size(); ++i) {
            m_vertices[static_cast<size_t>(layer[i])].x = x[i];
        }
    }

    void AssignInLayerPositions() {
        for (auto& v : m_vertices) v.x = 0.0f;
        for (size_t r = 0; r < m_layers.size(); ++r) PlaceLayer(r, false, false); // Packed and centred
        for (int round = 0; round < kPositionRounds; ++round) {
            for (size_t r = 1; r < m_layers.size(); ++r) PlaceLayer(r, true, false);
            for (size_t r = m_layers.size(); r-- > 1;) PlaceLayer(r - 1, false, true);
        }
        for (size_t r = 0; r < m_layers.size(); ++r) PlaceLayer(r, true, true);
    }

    // Cluster frames span several ranks, but the in-layer placement only separates neighbours of
    // the same rank. Push every non-member out of each frame's horizontal extent on the ranks the
    // frame covers (shifting whole layer prefixes/suffixes keeps the order and separations).
    void SeparateClusters() {
        const size_t clusterCount = m_in.clusters.size();
        if (clusterCount == 0) return;

        std::vector<std::vector<int>> paths(clusterCount);
        std::vector<size_t> order(clusterCount);
        for (size_t c = 0; c < clusterCount; ++c) {
            paths[c] = ClusterPath(static_cast<int>(c));
            order[c] = c;
        }
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return paths[a].size() > paths[b].size(); });

        const float pad = ClusterPadAlong();
        auto shift = [&](const std::vector<int>& layer, size_t from, size_t to, float dx) {
            for (size_t k = from; k < to; ++k) m_vertices[static_cast<size_t>(layer[k])].x += dx;
        };

        for (int iteration = 0; iteration < 8; ++iteration) {
            bool changed = false;
            for (size_t c : order) {
                const int cluster = static_cast<int>(c);
                float left = std::numeric_limits<float>::max();
                float right = std::numeric_limits<float>::lowest();
                int r0 = std::numeric_limits<int>::max();
                int r1 = -1;
                for (const auto& v : m_vertices) {
                    const auto it = std::find(v.path.begin(), v.path.end(), cluster);
                    if (it == v.path.end()) continue;
                    const float depthBelow = static_cast<float>(v.path.end() - it);
                    left = (std::min)(left, v.x - v.lw * 0.5f - depthBelow * pad);
                    right = (std::max)(right, v.x + v.lw * 0.5f + depthBelow * pad);
                    r0 = (std::min)(r0, v.rank);
                    r1 = (std::max)(r1, v.rank);
                }
                if (r1 < 0) continue;
                const float center = (left + right) * 0.5f;

                for (int r = r0; r <= r1; ++r) {
                    const auto& layer = m_layers[static_cast<size_t>(r)];
                    size_t firstMember = layer.size();
                    for (size_t k = 0; k < layer.size(); ++k) {
                        const auto& path = m_vertices[static_cast<size_t>(layer[k])].path;
                        if (std::find(path.begin(), path.end(), cluster) != path.end()) {
                            firstMember = k;
                            break;
                        }
                    }
                    for (size_t k = 0; k < layer.size(); ++k) {
                        const Vertex& v = m_vertices[static_cast<size_t>(layer[k])];
                        if (std::find(v.path.begin(), v.path.end(), cluster) != v.path.end()) continue;
                        // Frames of v's own clusters that do not contain this one also need room.
                        size_t common = 0;
                        while (common < v.path.size() && common < paths[c].size() && v.path[common] == paths[c][common]) {
                            ++common;
                        }
                        const float own = static_cast<float>(v.path.size() - common) * pad;
                        const float gap = v.dummy ? 6.0f : 12.0f;
                        const float vl = v.x - v.lw * 0.5f - own - gap;
                        const float vr = v.x + v.lw * 0.5f + own + gap;
                        if (vr <= left || vl >= right) continue;
                        const bool onLeft = firstMember < layer.size() ? k < firstMember : v.x < center;
                        if (onLeft) {
                            shift(layer, 0, k + 1, left - vr);
                        } else {
                            shift(layer, k, layer.size(), right - vl);
                        }
                        changed = true;
                    }
                }
            }
            if (!changed) break;
        }
    }

    void AssignRankPositions() {
        const size_t layerCount = m_layers.size();
        const size_t clusterCount = m_in.clusters.size();
        std::vector<int> minRank(clusterCount, std::numeric_limits<int>::max());
        std::vector<int> maxRank(clusterCount, -1);
        for (const auto& v : m_vertices) {
            for (int c : v.path) {
                minRank[static_cast<size_t>(c)] = (std::min)(minRank[static_cast<size_t>(c)], v.rank);
                maxRank[static_cast<size_t>(c)] = (std::max)(maxRank[static_cast<size_t>(c)], v.rank);
            }
        }

        const float titlePad = m_horizontal ? kClusterPadding : kClusterPadding + MaxTitleHeight();
        const float gap = m_anyLabel ? m_in.rankSep * 0.5f : m_in.rankSep;
        float y = 0.0f;
        float prevThickness = 0.0f;
        for (size_t r = 0; r < layerCount; ++r) {
            float thickness = 0.0f;
            int opens = 0;
            for (int v : m_layers[r]) {
                const Vertex& vx = m_vertices[static_cast<size_t>(v)];
                thickness = (std::max)(thickness, vx.lh);
                int count = 0;
                for (int c : vx.path) count += (minRank[static_cast<size_t>(c)] == static_cast<int>(r)) ? 1 : 0;
                opens = (std::max)(opens, count);
            }
            int closes = 0;
            if (r > 0) {
                for (int v : m_layers[r - 1]) {
                    const Vertex& vx = m_vertices[static_cast<size_t>(v)];
                    int count = 0;
                    for (int c : vx.path) count += (maxRank[static_cast<size_t>(c)] == static_cast<int>(r) - 1) ? 1 : 0;
                    closes = (std::max)(closes, count);
                }
                y += prevThickness * 0.5f + gap + static_cast<float>(opens) * titlePad +
                     static_cast<float>(closes) * kClusterPadding + thickness * 0.5f;
            } else {
                y = thickness * 0.5f + static_cast<float>(opens) * titlePad;
            }
            for (int v : m_layers[r]) m_vertices[static_cast<size_t>(v)].y = y;
            prevThickness = thickness;
        }
    }

    Point ToFinal(float x, float y) const {
        switch (m_in.direction) {
        case Direction::BT: return Point{x, -y};
        case Direction::LR: return Point{y, x};
        case Direction::RL: return Point{-y, x};
        default:            return Point{x, y};
        }
    }

    GraphLayoutResult Finish() {
        GraphLayoutResult result;
        const size_t n = m_in.nodes.size();
        result.nodeCenters.resize(n);
        for (size_t i = 0; i < n; ++i) {
            result.nodeCenters[i] = ToFinal(m_vertices[i].x, m_vertices[i].y);
        }

        // Bounding boxes that must stay inside the diagram (nodes, labels, loops).
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
        for (size_t i = 0; i < n; ++i) {
            const Point c = result.nodeCenters[i];
            const GraphNode& node = m_in.nodes[i];
            include(c.x - node.width * 0.5f, c.y - node.height * 0.5f, c.x + node.width * 0.5f, c.y + node.height * 0.5f);
        }

        result.edgePoints.resize(m_in.edges.size());
        result.labelCenters.resize(m_in.edges.size());
        const Point axis = m_horizontal ? Point{0.0f, 1.0f} : Point{1.0f, 0.0f};  // In-layer direction
        const Point cross = m_horizontal ? Point{1.0f, 0.0f} : Point{0.0f, 1.0f}; // Rank direction
        for (size_t i = 0; i < m_in.edges.size(); ++i) {
            const GraphEdge& e = m_in.edges[i];
            if (e.from == e.to && e.from >= 0 && e.from < static_cast<int>(n)) {
                const GraphNode& node = m_in.nodes[static_cast<size_t>(e.from)];
                const Point c = result.nodeCenters[static_cast<size_t>(e.from)];
                const float ha = m_horizontal ? node.height * 0.5f : node.width * 0.5f;
                const float hp = m_horizontal ? node.width * 0.5f : node.height * 0.5f;
                auto at = [&](float a, float p) { return Point{c.x + axis.x * a + cross.x * p, c.y + axis.y * a + cross.y * p}; };
                const float spread = (std::min)(hp * 0.5f, 14.0f);
                result.edgePoints[i] = {at(ha, -spread), at(ha + kLoopReach, -spread - 12.0f),
                                        at(ha + kLoopReach, spread + 12.0f), at(ha, spread)};
                const float labelAlong = LabelAlong(e);
                result.labelCenters[i] = at(ha + kLoopReach * 0.75f + 6.0f + labelAlong * 0.5f, 0.0f);
                const Point far = at(ha + LoopExtent(e), 0.0f);
                include(far.x - 1.0f, far.y - 1.0f, far.x + 1.0f, far.y + 1.0f);
                if (e.labelWidth > 0.0f) {
                    const Point l = result.labelCenters[i];
                    include(l.x - e.labelWidth * 0.5f, l.y - e.labelHeight * 0.5f, l.x + e.labelWidth * 0.5f,
                            l.y + e.labelHeight * 0.5f);
                }
                continue;
            }
            const Chain& chain = m_chains[i];
            if (chain.vertices.empty()) continue;
            auto& points = result.edgePoints[i];
            for (int v : chain.vertices) {
                points.push_back(ToFinal(m_vertices[static_cast<size_t>(v)].x, m_vertices[static_cast<size_t>(v)].y));
            }
            if (chain.reversed) std::reverse(points.begin(), points.end());
            if (chain.labelVertex >= 0) {
                const Vertex& lv = m_vertices[static_cast<size_t>(chain.labelVertex)];
                const Point l = ToFinal(lv.x, lv.y);
                result.labelCenters[i] = l;
                include(l.x - e.labelWidth * 0.5f, l.y - e.labelHeight * 0.5f, l.x + e.labelWidth * 0.5f,
                        l.y + e.labelHeight * 0.5f);
            } else if (points.size() >= 2) {
                const Point a = points[points.size() / 2 - (points.size() % 2 == 0 ? 1 : 0)];
                const Point b = points[points.size() / 2];
                result.labelCenters[i] = Point{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
            }
            for (const Point& p : points) include(p.x, p.y, p.x, p.y);
        }

        // Cluster frames, innermost first so parents enclose their children.
        const size_t clusterCount = m_in.clusters.size();
        result.clusterRects.resize(clusterCount);
        std::vector<int> depth(clusterCount, 0);
        for (size_t c = 0; c < clusterCount; ++c) depth[c] = static_cast<int>(ClusterPath(static_cast<int>(c)).size());
        std::vector<size_t> byDepth(clusterCount);
        for (size_t c = 0; c < clusterCount; ++c) byDepth[c] = c;
        std::stable_sort(byDepth.begin(), byDepth.end(), [&](size_t a, size_t b) { return depth[a] > depth[b]; });

        for (size_t c : byDepth) {
            float x0 = std::numeric_limits<float>::max();
            float y0 = x0;
            float x1 = std::numeric_limits<float>::lowest();
            float y1 = x1;
            bool any = false;
            auto add = [&](float ax0, float ay0, float ax1, float ay1) {
                x0 = (std::min)(x0, ax0);
                y0 = (std::min)(y0, ay0);
                x1 = (std::max)(x1, ax1);
                y1 = (std::max)(y1, ay1);
                any = true;
            };
            for (size_t v = 0; v < m_vertices.size(); ++v) {
                const Vertex& vx = m_vertices[v];
                if (vx.path.empty() || vx.path.back() != static_cast<int>(c)) continue;
                const Point p = ToFinal(vx.x, vx.y);
                const float hw = (v < n ? m_in.nodes[v].width : (m_horizontal ? vx.lh : vx.lw)) * 0.5f;
                const float hh = (v < n ? m_in.nodes[v].height : (m_horizontal ? vx.lw : vx.lh)) * 0.5f;
                add(p.x - hw, p.y - hh, p.x + hw, p.y + hh);
            }
            for (size_t child = 0; child < clusterCount; ++child) {
                if (m_in.clusters[child].parent == static_cast<int>(c) && !result.clusterRects[child].empty) {
                    const LayoutRect& r = result.clusterRects[child];
                    add(r.x, r.y, r.x + r.w, r.y + r.h);
                }
            }
            if (!any) continue;

            const GraphCluster& cl = m_in.clusters[c];
            x0 -= kClusterPadding;
            x1 += kClusterPadding;
            y0 -= kClusterPadding + cl.titleHeight;
            y1 += kClusterPadding;
            const float minWidth = cl.titleWidth + 2.0f * kClusterPadding;
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

        // Translate everything to a positive origin with a small margin.
        const float dx = kMargin - minX;
        const float dy = kMargin - minY;
        auto shift = [&](Point& p) {
            p.x += dx;
            p.y += dy;
        };
        for (auto& p : result.nodeCenters) shift(p);
        for (auto& pts : result.edgePoints) for (auto& p : pts) shift(p);
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
    bool m_anyLabel = false;
    std::vector<Vertex> m_vertices;
    std::vector<bool> m_reversed;
    std::vector<Chain> m_chains;
    std::vector<std::vector<int>> m_layers;
};

} // namespace

GraphLayoutResult LayoutGraph(const GraphLayoutInput& input) {
    return Layouter(input).Run();
}

} // namespace Pluma::Diagram::detail
