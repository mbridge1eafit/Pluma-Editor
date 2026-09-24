#pragma once

#include <string>
#include <string_view>

#include "diagram_scene.h"

namespace Pluma::Diagram {

// Serializes a scene as an inline <svg> element. Theme colours are CSS custom properties
// (--pluma-mm-*) with the light palette as fallback, so the host page can restyle them.
std::string WriteSvg(const Scene& scene, std::string_view accessibleTitle = {});

// "--pluma-mm-*: #rrggbb;" declarations for every role of `palette`.
std::string SvgPaletteVariables(const Palette& palette);

} // namespace Pluma::Diagram
