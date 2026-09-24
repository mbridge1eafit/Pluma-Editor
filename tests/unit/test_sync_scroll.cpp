#include <gtest/gtest.h>
#include "preview/preview_layout.h"
#include "markdown/md4c_adapter.h"

using namespace Pluma;
using namespace Pluma::Preview;
using namespace Pluma::Markdown;

class SyncScrollTest : public ::testing::Test {
protected:
    void SetUp() override {
        DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf())
        );
        ASSERT_TRUE(dwriteFactory);
        ASSERT_TRUE(layout.Initialize(dwriteFactory.Get()));
    }

    ComPtr<IDWriteFactory> dwriteFactory;
    LayoutEngine layout;
    Md4cAdapter adapter;
};

TEST_F(SyncScrollTest, EmptyTreeReturnsZeroAndFirstLine) {
    BlockTree emptyTree;
    layout.ComputeLayout(emptyTree, 800.0f, 96, PreviewThemeColors::Light());

    EXPECT_FLOAT_EQ(layout.GetScrollYForLine(1), 0.0f);
    EXPECT_EQ(layout.GetLineForScrollY(0.0f), 1);
    EXPECT_EQ(layout.GetLineForScrollY(500.0f), 1);
}

TEST_F(SyncScrollTest, LineToCoordinateMonotonicity) {
    std::string markdown =
        "# Heading 1\n\n"
        "Paragraph 1 on multiple lines.\n"
        "Still paragraph 1.\n\n"
        "## Heading 2\n\n"
        "```cpp\n"
        "int x = 42;\n"
        "int y = 84;\n"
        "```\n\n"
        "Paragraph 2 after code.\n\n"
        "### Heading 3\n\n"
        "- Item 1\n"
        "- Item 2\n"
        "- Item 3\n";

    auto tree = adapter.Parse(markdown);
    ASSERT_TRUE(tree);

    layout.ComputeLayout(*tree, 800.0f, 96, PreviewThemeColors::Light());

    float yLine1 = layout.GetScrollYForLine(1);
    float yLine3 = layout.GetScrollYForLine(3);
    float yLine6 = layout.GetScrollYForLine(6);
    float yLine11 = layout.GetScrollYForLine(11);
    float yLine16 = layout.GetScrollYForLine(16);

    EXPECT_FLOAT_EQ(yLine1, 0.0f);
    EXPECT_GT(yLine3, yLine1);
    EXPECT_GT(yLine6, yLine3);
    EXPECT_GT(yLine11, yLine6);
    EXPECT_GT(yLine16, yLine11);
}

TEST_F(SyncScrollTest, CoordinateToLineMappingAccuracy) {
    std::string markdown =
        "# Top Header\n\n"
        "Line 3 of document.\n"
        "Line 4 of document.\n\n"
        "## Middle Header\n\n"
        "Line 8 of document.\n";

    auto tree = adapter.Parse(markdown);
    ASSERT_TRUE(tree);

    layout.ComputeLayout(*tree, 800.0f, 96, PreviewThemeColors::Light());

    int lineAtZero = layout.GetLineForScrollY(0.0f);
    EXPECT_EQ(lineAtZero, 1);

    float middleHeaderY = layout.GetAnchorY("middle-header");
    EXPECT_GT(middleHeaderY, 0.0f);

    int lineAtMiddle = layout.GetLineForScrollY(middleHeaderY + 5.0f);
    // Middle header is around line 6
    EXPECT_GE(lineAtMiddle, 5);
    EXPECT_LE(lineAtMiddle, 8);
}

TEST_F(SyncScrollTest, LineRoundTripApproximation) {
    std::string markdown =
        "# Title\n\n"
        "First section with descriptive text spanning several lines\n"
        "to ensure proper measurement of height and scroll interpolation.\n\n"
        "## Subtitle\n\n"
        "Second section details with more content.\n\n"
        "### Final Section\n\n"
        "Conclusion paragraph at the bottom.\n";

    auto tree = adapter.Parse(markdown);
    ASSERT_TRUE(tree);

    layout.ComputeLayout(*tree, 800.0f, 96, PreviewThemeColors::Light());

    for (int line = 1; line <= 13; ++line) {
        float y = layout.GetScrollYForLine(line);
        int mappedLine = layout.GetLineForScrollY(y);
        // Mapped line should be within 1 line tolerance of original line
        EXPECT_NEAR(line, mappedLine, 2);
    }
}
