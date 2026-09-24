#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "diagram_scene.h"

// Native Mermaid renderer: parses the diagram source and lays it out as a vector Scene.
// Supported: flowchart/graph, sequenceDiagram, stateDiagram(-v2) and pie.
namespace Pluma::Diagram {

// True for fenced code blocks whose info string is "mermaid" (case-insensitive).
bool IsMermaidLanguage(std::string_view info);

struct MermaidResult {
    std::shared_ptr<const Scene> scene; // Null on error
    std::string error;                  // Human readable reason (Spanish), empty on success
};

MermaidResult RenderMermaid(std::string_view source, const MeasureText& measure);

} // namespace Pluma::Diagram
