#include <gtest/gtest.h>

#include "diagram/mermaid.h"
#include "diagram/svg_writer.h"
#include "editor/markdown_format.h"

using namespace Pluma::Editor::MarkdownFormat;
using Lines = std::vector<std::string>;

TEST(MarkdownFormatTest, HeadingLevelRecognizesAtxHeadings) {
    EXPECT_EQ(HeadingLevel("# Title"), 1);
    EXPECT_EQ(HeadingLevel("###### Six"), 6);
    EXPECT_EQ(HeadingLevel("   ## Indented"), 2);
    EXPECT_EQ(HeadingLevel("##"), 2);
    EXPECT_EQ(HeadingLevel("####### Seven"), 0);
    EXPECT_EQ(HeadingLevel("#hashtag"), 0);
    EXPECT_EQ(HeadingLevel("Plain"), 0);
}

TEST(MarkdownFormatTest, SetHeadingLevelReplacesTheMarker) {
    EXPECT_EQ(SetHeadingLevel("Title", 2), "## Title");
    EXPECT_EQ(SetHeadingLevel("### Title", 1), "# Title");
    EXPECT_EQ(SetHeadingLevel("## Title", 0), "Title");
    EXPECT_EQ(SetHeadingLevel("", 3), "### ");
    EXPECT_EQ(SetHeadingLevel("#hashtag", 1), "# #hashtag");
    EXPECT_EQ(SetHeadingLevel("> Quoted", 2), "> ## Quoted");
}

TEST(MarkdownFormatTest, BulletListTogglesOnAndOff) {
    const Lines on = ToggleLineKind({"one", "two"}, LineKind::Bullet);
    EXPECT_EQ(on, (Lines{"- one", "- two"}));
    EXPECT_EQ(ToggleLineKind(on, LineKind::Bullet), (Lines{"one", "two"}));
}

TEST(MarkdownFormatTest, MixedSelectionAppliesInsteadOfRemoving) {
    EXPECT_EQ(ToggleLineKind({"* one", "two"}, LineKind::Bullet), (Lines{"* one", "- two"}));
    EXPECT_EQ(ToggleLineKind({"1. one", "2) two"}, LineKind::Bullet), (Lines{"- one", "- two"}));
}

TEST(MarkdownFormatTest, BlankLinesAreSkippedButALoneLineIsNot) {
    EXPECT_EQ(ToggleLineKind({"one", "", "two"}, LineKind::Bullet), (Lines{"- one", "", "- two"}));
    EXPECT_EQ(ToggleLineKind({""}, LineKind::Bullet), (Lines{"- "}));
    EXPECT_EQ(ToggleLineKind({"", "  "}, LineKind::Numbered), (Lines{"", "  "}));
}

TEST(MarkdownFormatTest, NumberedListRenumbersPerIndentLevel) {
    EXPECT_EQ(ToggleLineKind({"a", "  b", "  c", "d"}, LineKind::Numbered),
              (Lines{"1. a", "  1. b", "  2. c", "2. d"}));
    // Existing numbers are rewritten so the sequence stays correct.
    EXPECT_EQ(ToggleLineKind({"3. a", "b"}, LineKind::Numbered), (Lines{"1. a", "2. b"}));
    EXPECT_EQ(ToggleLineKind({"1. a", "2. b"}, LineKind::Numbered), (Lines{"a", "b"}));
}

TEST(MarkdownFormatTest, TaskListKeepsCheckedItems) {
    EXPECT_EQ(ToggleLineKind({"- [x] done", "todo"}, LineKind::Task), (Lines{"- [x] done", "- [ ] todo"}));
    EXPECT_EQ(ToggleLineKind({"- item"}, LineKind::Task), (Lines{"- [ ] item"}));
    EXPECT_EQ(ToggleLineKind({"- [x] done", "- [ ] todo"}, LineKind::Task), (Lines{"done", "todo"}));
    // A task item is not a plain bullet: the bullet button turns it into one.
    EXPECT_EQ(ToggleLineKind({"- [ ] todo"}, LineKind::Bullet), (Lines{"- todo"}));
}

TEST(MarkdownFormatTest, QuoteKeepsBlankLinesInsideTheBlock) {
    const Lines on = ToggleLineKind({"one", "", "two"}, LineKind::Quote);
    EXPECT_EQ(on, (Lines{"> one", ">", "> two"}));
    EXPECT_EQ(ToggleLineKind(on, LineKind::Quote), (Lines{"one", "", "two"}));
    EXPECT_EQ(ToggleLineKind({"> > nested"}, LineKind::Quote), (Lines{"> nested"}));
}

TEST(MarkdownFormatTest, ListsApplyInsideQuotes) {
    EXPECT_EQ(ToggleLineKind({"> item"}, LineKind::Bullet), (Lines{"> - item"}));
    EXPECT_EQ(ToggleLineKind({"> - item"}, LineKind::Bullet), (Lines{"> item"}));
}

TEST(MarkdownFormatTest, EmphasisAndRulesAreNotListItems) {
    EXPECT_FALSE(HasLineKind("**bold**", LineKind::Bullet));
    EXPECT_FALSE(HasLineKind("---", LineKind::Bullet));
    EXPECT_FALSE(HasLineKind("1.5 metros", LineKind::Numbered));
    EXPECT_TRUE(HasLineKind("-", LineKind::Bullet));
    EXPECT_TRUE(HasLineKind("10) diez", LineKind::Numbered));
}

TEST(MarkdownFormatTest, LineStateReportsBlockFormatting) {
    const LineState heading = GetLineState("> ## Title");
    EXPECT_EQ(heading.headingLevel, 2);
    EXPECT_TRUE(heading.quote);
    EXPECT_FALSE(heading.bullet);

    const LineState task = GetLineState("  - [X] done");
    EXPECT_TRUE(task.task);
    EXPECT_FALSE(task.bullet);
    EXPECT_FALSE(task.numbered);

    EXPECT_TRUE(GetLineState("2. two").numbered);
    EXPECT_EQ(GetLineState("text").headingLevel, 0);
}

TEST(MarkdownFormatTest, MermaidTemplatesRender) {
    const Pluma::Diagram::MeasureText measure = [](std::string_view text, float size, bool bold) {
        return Pluma::Diagram::EstimateTextWidth(text, size, bold);
    };
    for (DiagramKind kind : {DiagramKind::Flowchart, DiagramKind::Sequence, DiagramKind::State, DiagramKind::Pie}) {
        const auto result = Pluma::Diagram::RenderMermaid(MermaidTemplate(kind), measure);
        EXPECT_TRUE(result.scene) << MermaidTemplate(kind) << "\n" << result.error;
    }
}
