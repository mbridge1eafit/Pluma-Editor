#include <gtest/gtest.h>

#include "markdown/md4c_adapter.h"
#include "outline/outline_panel.h"

using Pluma::Outline::CollectHeadings;
using Pluma::Outline::FindSectionIndex;
using Pluma::Outline::Heading;

TEST(OutlineTest, CollectsTopLevelHeadingsWithLinesAndPlainTitles) {
    const std::string md =
        "# Título **principal**\n"
        "\n"
        "Texto.\n"
        "\n"
        "## Sección `código`\n"
        "\n"
        "> # No es del esquema (dentro de una cita)\n"
        "\n"
        "### [Enlace](https://example.com)\n";
    const auto tree = Pluma::Markdown::Md4cAdapter::Parse(md);
    ASSERT_TRUE(tree);

    const auto headings = CollectHeadings(*tree);
    ASSERT_EQ(headings.size(), 3u);
    EXPECT_EQ(headings[0].level, 1);
    EXPECT_EQ(headings[0].line, 1);
    EXPECT_EQ(headings[0].title, L"Título principal");
    EXPECT_EQ(headings[1].level, 2);
    EXPECT_EQ(headings[1].line, 5);
    EXPECT_EQ(headings[1].title, L"Sección código");
    EXPECT_EQ(headings[2].level, 3);
    EXPECT_EQ(headings[2].line, 9);
    EXPECT_EQ(headings[2].title, L"Enlace");
}

TEST(OutlineTest, RespectsMaximumAndEmptyTitles) {
    const auto tree = Pluma::Markdown::Md4cAdapter::Parse("#\n\n## a\n\n## b\n");
    ASSERT_TRUE(tree);
    const auto headings = CollectHeadings(*tree, 2);
    ASSERT_EQ(headings.size(), 2u);
    EXPECT_EQ(headings[0].title, L"(Sin título)");
}

TEST(OutlineTest, FindSectionIndexUsesLastHeadingAtOrAboveLine) {
    const std::vector<Heading> headings = {{3, 1, L"a"}, {10, 2, L"b"}, {20, 2, L"c"}};
    EXPECT_EQ(FindSectionIndex(headings, 1), -1); // Before the first heading
    EXPECT_EQ(FindSectionIndex(headings, 3), 0);
    EXPECT_EQ(FindSectionIndex(headings, 9), 0);
    EXPECT_EQ(FindSectionIndex(headings, 10), 1);
    EXPECT_EQ(FindSectionIndex(headings, 500), 2);
    EXPECT_EQ(FindSectionIndex({}, 5), -1);
}
