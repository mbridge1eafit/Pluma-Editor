#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <string_view>

#include "Scintilla.h"
#include "SciLexer.h"

namespace Pluma::Editor {

using SciFnDirect = sptr_t (*)(sptr_t ptr, unsigned int iMessage, uptr_t wParam, sptr_t lParam);

class EditorView {
public:
    EditorView() = default;
    ~EditorView();

    // Disallow copy, allow move
    EditorView(const EditorView&) = delete;
    EditorView& operator=(const EditorView&) = delete;
    EditorView(EditorView&& other) noexcept;
    EditorView& operator=(EditorView&& other) noexcept;

    // Registers the Scintilla window class if not already registered
    static bool InitializeScintilla(HINSTANCE hInstance);

    // Creates the Scintilla child window control (M1.2)
    bool Create(HWND parent, HINSTANCE hInstance, int controlId,
                int x, int y, int width, int height);

    // Repositions the editor window within parent
    void SetBounds(int x, int y, int width, int height);

    // Sets keyboard focus to the editor
    void SetFocus();

    // Returns the Scintilla window handle
    HWND GetHwnd() const noexcept { return m_hwndScintilla; }

    // Direct invocation avoiding SendMessage overhead (M1.2)
    sptr_t Call(unsigned int msg, uptr_t wParam = 0, sptr_t lParam = 0) const {
        if (m_fnDirect && m_ptrDirect) {
            return m_fnDirect(m_ptrDirect, msg, wParam, lParam);
        }
        return SendMessageW(m_hwndScintilla, msg, wParam, lParam);
    }

    // Document content access (UTF-8)
    std::string GetText() const;
    void SetText(std::string_view text);

    // Modification state
    bool IsModified() const;
    void SetSavePoint();

    // Undo / Redo (F-04)
    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;

    // Clipboard operations
    void Cut();
    void Copy();
    void Paste();
    void SelectAll();

    // Word wrap (F-04)
    void SetWordWrap(bool wrap);
    bool GetWordWrap() const noexcept { return m_wordWrap; }
    void ToggleWordWrap();

    // Navigation and Search (F-04)
    void GotoLine(int line);
    int GetFirstVisibleDocLine() const;
    void ScrollToDocLine(int docLine);
    bool FindNext(std::string_view text, bool matchCase, bool wholeWord, bool regex, bool forward = true);
    int ReplaceAll(std::string_view findText, std::string_view replaceText,
                   bool matchCase, bool wholeWord, bool regex);

    // Markdown formatting helpers (F-12)
    void WrapSelection(std::string_view prefix, std::string_view suffix);
    void InsertBold();
    void InsertItalic();
    void InsertCode();
    void InsertStrikethrough();
    void InsertLink();

    // Editor metrics and status info (F-13)
    struct CursorPos {
        int line = 1;
        int column = 1;
    };
    CursorPos GetCursorPosition() const;

    struct DocumentStats {
        size_t words = 0;
        size_t characters = 0;
    };
    DocumentStats GetDocumentStats() const;

    // Markdown lexer styling for Light / Dark mode (M1.3, F-05, F-09)
    void ApplyTheme(bool darkMode);

private:
    void SetupStyles(bool darkMode);
    void UpdateLineNumberMargin();

    HWND m_hwndScintilla = nullptr;
    SciFnDirect m_fnDirect = nullptr;
    sptr_t m_ptrDirect = 0;
    bool m_isDarkMode = false;
    bool m_wordWrap = true;
};

} // namespace Pluma::Editor
