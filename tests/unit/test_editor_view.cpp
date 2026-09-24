#include <gtest/gtest.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "editor/editor_view.h"

class EditorViewTest : public ::testing::Test {
protected:
    HWND m_parentHwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;

    void SetUp() override {
        m_hInstance = GetModuleHandleW(nullptr);

        WNDCLASSEXW wcex{};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.lpfnWndProc = DefWindowProcW;
        wcex.hInstance = m_hInstance;
        wcex.lpszClassName = L"PlumaTestHostWindow";
        RegisterClassExW(&wcex);

        m_parentHwnd = CreateWindowExW(
            0,
            L"PlumaTestHostWindow",
            L"Host",
            WS_OVERLAPPEDWINDOW,
            0, 0, 800, 600,
            nullptr, nullptr, m_hInstance, nullptr
        );
    }

    void TearDown() override {
        if (m_parentHwnd && IsWindow(m_parentHwnd)) {
            DestroyWindow(m_parentHwnd);
            m_parentHwnd = nullptr;
        }
    }
};

TEST_F(EditorViewTest, CreateAndDirectDispatch) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    bool created = editor.Create(m_parentHwnd, m_hInstance, 1001, 0, 0, 800, 600);
    ASSERT_TRUE(created);
    ASSERT_NE(editor.GetHwnd(), nullptr);

    // Initial content should be empty
    EXPECT_EQ(editor.GetText(), "");
    EXPECT_FALSE(editor.IsModified());

    // Set text and verify via direct dispatch
    std::string sample = "# Markdown Title\n\nContent paragraph with **bold** text.\n";
    editor.SetText(sample);
    EXPECT_EQ(editor.GetText(), sample);
    EXPECT_FALSE(editor.IsModified()); // SetText resets modify flag and sets save point
}

TEST_F(EditorViewTest, UndoRedoOperations) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1002, 0, 0, 800, 600));

    editor.SetText("Line 1\n");
    EXPECT_FALSE(editor.CanUndo());

    // Append text by inserting at end
    editor.Call(SCI_DOCUMENTEND);
    const char* append = "Line 2\n";
    editor.Call(SCI_ADDTEXT, strlen(append), reinterpret_cast<sptr_t>(append));

    EXPECT_TRUE(editor.CanUndo());
    EXPECT_TRUE(editor.IsModified());

    editor.Undo();
    EXPECT_EQ(editor.GetText(), "Line 1\n");
    EXPECT_TRUE(editor.CanRedo());

    editor.Redo();
    EXPECT_EQ(editor.GetText(), "Line 1\nLine 2\n");
}

TEST_F(EditorViewTest, FindAndReplace) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1003, 0, 0, 800, 600));

    editor.SetText("The quick brown fox jumps over the lazy dog. The fox is fast.");

    // Find next
    bool found = editor.FindNext("fox", false, false, false);
    EXPECT_TRUE(found);

    // Replace all
    int replacedCount = editor.ReplaceAll("fox", "cat", false, false, false);
    EXPECT_EQ(replacedCount, 2);
    EXPECT_EQ(editor.GetText(), "The quick brown cat jumps over the lazy dog. The cat is fast.");
}

TEST_F(EditorViewTest, WordWrapAndThemeToggle) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1004, 0, 0, 800, 600));

    // Word wrap toggle
    EXPECT_TRUE(editor.GetWordWrap());
    editor.ToggleWordWrap();
    EXPECT_FALSE(editor.GetWordWrap());
    editor.ToggleWordWrap();
    EXPECT_TRUE(editor.GetWordWrap());

    // Theme application (Dark and Light)
    editor.ApplyTheme(true);
    editor.ApplyTheme(false);
}

TEST_F(EditorViewTest, MarkdownFormattingShortcuts) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1005, 0, 0, 800, 600));

    // Test Bold with selection
    editor.SetText("Hello world");
    editor.Call(SCI_SETSEL, 0, 5); // Select "Hello"
    editor.InsertBold();
    EXPECT_EQ(editor.GetText(), "**Hello** world");

    // Test Italic with selection
    editor.SetText("Hello world");
    editor.Call(SCI_SETSEL, 6, 11); // Select "world"
    editor.InsertItalic();
    EXPECT_EQ(editor.GetText(), "Hello *world*");

    // Test Strikethrough with selection
    editor.SetText("Hello world");
    editor.Call(SCI_SETSEL, 0, 11); // Select "Hello world"
    editor.InsertStrikethrough();
    EXPECT_EQ(editor.GetText(), "~~Hello world~~");

    // Test Inline Code with single line
    editor.SetText("code here");
    editor.Call(SCI_SETSEL, 0, 4); // Select "code"
    editor.InsertCode();
    EXPECT_EQ(editor.GetText(), "`code` here");

    // Test Fenced Code block with multi line
    editor.SetText("line1\nline2");
    editor.Call(SCI_SETSEL, 0, 11); // Select all
    editor.InsertCode();
    EXPECT_EQ(editor.GetText(), "```\nline1\nline2\n```");

    // Test Link with selection
    editor.SetText("Click here");
    editor.Call(SCI_SETSEL, 0, 5); // Select "Click"
    editor.InsertLink();
    EXPECT_EQ(editor.GetText(), "[Click](url) here");

    // Test Empty selection formatting
    editor.SetText("");
    editor.InsertBold();
    EXPECT_EQ(editor.GetText(), "****");
    sptr_t pos = editor.Call(SCI_GETCURRENTPOS);
    EXPECT_EQ(pos, 2); // Cursor should be between ** and **
}

TEST_F(EditorViewTest, EditorMetricsAndCursorPosition) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1006, 0, 0, 800, 600));

    editor.SetText("First line\nSecond line has more words\nThird");

    auto stats = editor.GetDocumentStats();
    EXPECT_EQ(stats.words, 8); // "First", "line", "Second", "line", "has", "more", "words", "Third"
    EXPECT_EQ(stats.characters, editor.GetText().size());

    // Cursor position at start
    editor.Call(SCI_GOTOPOS, 0);
    auto cpos1 = editor.GetCursorPosition();
    EXPECT_EQ(cpos1.line, 1);
    EXPECT_EQ(cpos1.column, 1);

    // Cursor position on second line
    editor.GotoLine(2); // 1-indexed line 2 = second line
    auto cpos2 = editor.GetCursorPosition();
    EXPECT_EQ(cpos2.line, 2);
    EXPECT_EQ(cpos2.column, 1);
}

TEST_F(EditorViewTest, BidirectionalFind) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1007, 0, 0, 800, 600));

    editor.SetText("alpha beta gamma beta delta");

    // Forward search
    editor.Call(SCI_GOTOPOS, 0);
    bool found1 = editor.FindNext("beta", false, false, false, true);
    EXPECT_TRUE(found1);
    EXPECT_EQ(editor.Call(SCI_GETSELECTIONSTART), 6);
    EXPECT_EQ(editor.Call(SCI_GETSELECTIONEND), 10);

    // Next forward search finds second "beta"
    bool found2 = editor.FindNext("beta", false, false, false, true);
    EXPECT_TRUE(found2);
    EXPECT_EQ(editor.Call(SCI_GETSELECTIONSTART), 17);
    EXPECT_EQ(editor.Call(SCI_GETSELECTIONEND), 21);

    // Backward search from second beta finds first beta
    bool found3 = editor.FindNext("beta", false, false, false, false);
    EXPECT_TRUE(found3);
    EXPECT_EQ(editor.Call(SCI_GETSELECTIONSTART), 6);
    EXPECT_EQ(editor.Call(SCI_GETSELECTIONEND), 10);
}

TEST_F(EditorViewTest, FormattingTogglesOff) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1008, 0, 0, 800, 600));

    // Ctrl+B twice on the same selection restores the original text.
    editor.SetText("Hello world");
    editor.Call(SCI_SETSEL, 0, 5);
    editor.InsertBold();
    EXPECT_EQ(editor.GetText(), "**Hello** world");
    editor.InsertBold();
    EXPECT_EQ(editor.GetText(), "Hello world");

    // Selecting the markers too also unwraps.
    editor.SetText("**Hello** world");
    editor.Call(SCI_SETSEL, 0, 9);
    editor.InsertBold();
    EXPECT_EQ(editor.GetText(), "Hello world");

    // Italic must not eat one star of a bold span.
    editor.SetText("**Hello**");
    editor.Call(SCI_SETSEL, 0, 9);
    editor.InsertItalic();
    EXPECT_EQ(editor.GetText(), "***Hello***");

    // Empty markers are removed by a second press.
    editor.SetText("");
    editor.InsertBold();
    editor.InsertBold();
    EXPECT_EQ(editor.GetText(), "");
}

TEST_F(EditorViewTest, CodeFenceKeepsSelectionLineEndings) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1009, 0, 0, 800, 600));

    editor.SetText("a\r\nb");
    editor.Call(SCI_SETSEL, 0, 4);
    editor.InsertCode();
    EXPECT_EQ(editor.GetText(), "```\r\na\r\nb\r\n```");
}

TEST_F(EditorViewTest, InsertLinkUsesSelectedUrlAsTarget) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1010, 0, 0, 800, 600));

    editor.SetText("https://example.com");
    editor.Call(SCI_SETSEL, 0, 19);
    editor.InsertLink();
    EXPECT_EQ(editor.GetText(), "[](https://example.com)");
    EXPECT_EQ(editor.Call(SCI_GETCURRENTPOS), 1); // Caret ready to type the label
}

TEST_F(EditorViewTest, StatsCountCharactersNotBytes) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1011, 0, 0, 800, 600));

    editor.SetText("Año ñandú 🚀");
    auto stats = editor.GetDocumentStats();
    EXPECT_EQ(stats.words, 3u);
    EXPECT_EQ(stats.characters, 11u);
}

TEST_F(EditorViewTest, SetTextHandlesNonTerminatedViews) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1012, 0, 0, 800, 600));

    const std::string buffer = "Primera línea\nSegunda línea";
    editor.SetText(std::string_view(buffer).substr(0, 5)); // Not NUL-terminated at 5
    EXPECT_EQ(editor.GetText(), "Prime");
    EXPECT_FALSE(editor.CanUndo());
}

TEST_F(EditorViewTest, SelectionMatchesIgnoresCaseOnRequest) {
    ASSERT_NE(m_parentHwnd, nullptr);

    Pluma::Editor::EditorView editor;
    ASSERT_TRUE(editor.Create(m_parentHwnd, m_hInstance, 1013, 0, 0, 800, 600));

    editor.SetText("Árbol verde");
    editor.Call(SCI_SETSEL, 0, 6); // "Árbol" (Á is two bytes)
    EXPECT_TRUE(editor.SelectionMatches("árbol", false));
    EXPECT_FALSE(editor.SelectionMatches("árbol", true));
    EXPECT_FALSE(editor.SelectionMatches("verde", false));
}
