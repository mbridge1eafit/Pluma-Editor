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
