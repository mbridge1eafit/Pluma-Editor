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

    // Line ending inserted by Enter (SC_EOL_CRLF / SC_EOL_LF / SC_EOL_CR)
    void SetEolMode(int sciEolMode);
    std::string_view EolString() const;

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

    // True when the main selection is exactly `text` (case-insensitive unless matchCase).
    bool SelectionMatches(std::string_view text, bool matchCase) const;

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

    // User preferences. An empty or missing font name selects the automatic font
    // (Cascadia Code, Cascadia Mono or Consolas).
    void SetFont(std::wstring_view faceName, int sizePoints);
    void SetShowLineNumbers(bool show);
    void SetHighlightCurrentLine(bool highlight);
    void SetTabSettings(int width, bool useTabs);

    // Resizes the line number margin when the number of digits changes
    void UpdateLineNumberMargin();

private:
    void SetupStyles(bool darkMode);
    std::string GetRange(sptr_t start, sptr_t end) const;

    HWND m_hwndScintilla = nullptr;
    SciFnDirect m_fnDirect = nullptr;
    sptr_t m_ptrDirect = 0;
    bool m_isDarkMode = false;
    bool m_wordWrap = true;
    bool m_showLineNumbers = true;
    bool m_highlightCurrentLine = true;
    int m_tabWidth = 4;
    bool m_useTabs = false;
    std::string m_fontName; // UTF-8; empty = automatic
    int m_fontSize = 11;
    int m_marginDigits = 0;
};

} // namespace Pluma::Editor
