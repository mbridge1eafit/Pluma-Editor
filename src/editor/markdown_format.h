#pragma once

#include <string>
#include <string_view>
#include <vector>

// Line-level Markdown transforms behind the format bar (headings, quotes, lists). Pure string
// functions, independent of Scintilla, so the editing rules can be unit tested in isolation.
namespace Pluma::Editor::MarkdownFormat {

enum class LineKind { Quote, Bullet, Numbered, Task };

// ATX heading level of `line` (1-6), or 0 when it is not a heading.
int HeadingLevel(std::string_view line);

// Rewrites `line` as a heading of `level` (0 = plain paragraph), replacing any existing "#" marker.
std::string SetHeadingLevel(std::string_view line, int level);

// Bullet and Numbered exclude task items ("- [ ] ..."), which count only as Task.
bool HasLineKind(std::string_view line, LineKind kind);

// Toggles `kind` on a block of lines: removes it when every non-blank line already has it,
// otherwise applies it to all of them (converting other list markers). Numbered items are
// renumbered from 1 per indentation level. The result has the same number of lines.
std::vector<std::string> ToggleLineKind(const std::vector<std::string>& lines, LineKind kind);

// Block-level formatting of the line holding the caret, reflected in the format bar.
struct LineState {
    int headingLevel = 0;
    bool quote = false;
    bool bullet = false;
    bool numbered = false;
    bool task = false;
};
LineState GetLineState(std::string_view line);

// Starter source for each Mermaid diagram type the preview renders.
enum class DiagramKind { Flowchart, Sequence, State, Pie };
std::string_view MermaidTemplate(DiagramKind kind);

} // namespace Pluma::Editor::MarkdownFormat
