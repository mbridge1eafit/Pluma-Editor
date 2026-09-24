#include "mermaid.h"

#include <algorithm>
#include <exception>

#include "mermaid_internal.h"

namespace Pluma::Diagram {

namespace {

// Guards the UI thread against pathological input (layout cost grows quadratically).
constexpr size_t kMaxSourceBytes = 64 * 1024;
constexpr size_t kMaxNodes = 400;
constexpr size_t kMaxEdges = 800;

// Strips a leading YAML front matter block (---\n...\n---) and returns its `title:`.
std::string_view StripFrontMatter(std::string_view source, std::string& title) {
    std::string_view rest = source;
    while (!rest.empty() && (rest.front() == '\n' || rest.front() == '\r' || rest.front() == ' ')) rest.remove_prefix(1);
    if (!detail::StartsWith(rest, "---")) return source;
    const size_t firstEnd = rest.find('\n');
    if (firstEnd == std::string_view::npos) return source;
    size_t pos = firstEnd + 1;
    while (pos < rest.size()) {
        size_t end = rest.find('\n', pos);
        if (end == std::string_view::npos) end = rest.size();
        const std::string_view line = detail::Trim(rest.substr(pos, end - pos));
        if (line == "---") return rest.substr((std::min)(rest.size(), end + 1));
        if (detail::StartsWith(line, "title:")) title = detail::CleanLabel(line.substr(6));
        pos = end + 1;
    }
    return source;
}

} // namespace

bool IsMermaidLanguage(std::string_view info) {
    info = detail::Trim(info);
    const size_t space = info.find_first_of(" \t{");
    return detail::ToLower(info.substr(0, space)) == "mermaid";
}

MermaidResult RenderMermaid(std::string_view source, const MeasureText& measure) {
    MermaidResult result;
    if (source.size() > kMaxSourceBytes) {
        result.error = "El diagrama es demasiado grande para la vista previa";
        return result;
    }

    try {
        std::string frontTitle;
        const std::vector<std::string_view> lines = detail::SourceLines(StripFrontMatter(source, frontTitle));
        if (lines.empty()) {
            result.error = "Diagrama vacío";
            return result;
        }

        const std::string_view header = lines[0];
        auto scene = std::make_shared<Scene>();
        if (detail::StartsWithKeyword(header, "graph") || detail::StartsWithKeyword(header, "flowchart") ||
            detail::StartsWithKeyword(header, "flowchart-elk") || detail::StartsWithKeyword(header, "stateDiagram") ||
            detail::StartsWithKeyword(header, "stateDiagram-v2")) {
            detail::FlowGraph graph;
            const bool isState = detail::StartsWith(header, "stateDiagram");
            const bool ok = isState ? detail::ParseStateDiagram(lines, graph, result.error)
                                    : detail::ParseFlowchart(lines, graph, result.error);
            if (!ok) {
                if (result.error.empty()) result.error = "Sintaxis no válida";
                return result;
            }
            if (graph.nodes.empty()) {
                result.error = "El diagrama no tiene nodos";
                return result;
            }
            if (graph.nodes.size() > kMaxNodes || graph.edges.size() > kMaxEdges) {
                result.error = "El diagrama tiene demasiados nodos para la vista previa";
                return result;
            }
            *scene = detail::BuildFlowScene(graph, measure);
            detail::AddSceneTitle(*scene, frontTitle, measure, 8.0f);
        } else if (detail::StartsWithKeyword(header, "sequenceDiagram")) {
            if (!detail::BuildSequenceScene(lines, measure, *scene, result.error)) return result;
        } else if (detail::StartsWithKeyword(header, "pie")) {
            if (!detail::BuildPieScene(lines, measure, *scene, result.error)) return result;
        } else {
            const std::string_view type = header.substr(0, header.find_first_of(" \t:;"));
            result.error = "Tipo de diagrama no compatible: " + std::string(type);
            return result;
        }

        if (scene->items.empty() || scene->width <= 0.0f || scene->height <= 0.0f) {
            result.error = "El diagrama está vacío";
            return result;
        }
        result.scene = std::move(scene);
    } catch (const std::exception&) {
        result.scene.reset();
        result.error = "No se pudo generar el diagrama";
    }
    return result;
}

} // namespace Pluma::Diagram
