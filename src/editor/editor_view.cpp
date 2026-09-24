#include "editor_view.h"

#include <vector>
#include <algorithm>

#include "ILexer.h"
#include "LexerModule.h"

extern const Lexilla::LexerModule lmMarkdown;

namespace Pluma::Editor {

namespace {

inline COLORREF MakeSciColor(uint8_t r, uint8_t g, uint8_t b) {
    return RGB(r, g, b);
}

} // namespace

bool EditorView::InitializeScintilla(HINSTANCE hInstance) {
    static bool s_registered = false;
    if (!s_registered) {
        if (Scintilla_RegisterClasses(hInstance) == 0) {
            return false;
        }
        s_registered = true;
    }
    return true;
}

EditorView::~EditorView() {
    if (m_hwndScintilla && IsWindow(m_hwndScintilla)) {
        DestroyWindow(m_hwndScintilla);
        m_hwndScintilla = nullptr;
    }
}

EditorView::EditorView(EditorView&& other) noexcept
    : m_hwndScintilla(other.m_hwndScintilla),
      m_fnDirect(other.m_fnDirect),
      m_ptrDirect(other.m_ptrDirect),
      m_isDarkMode(other.m_isDarkMode),
      m_wordWrap(other.m_wordWrap) {
    other.m_hwndScintilla = nullptr;
    other.m_fnDirect = nullptr;
    other.m_ptrDirect = 0;
}

EditorView& EditorView::operator=(EditorView&& other) noexcept {
    if (this != &other) {
        if (m_hwndScintilla && IsWindow(m_hwndScintilla)) {
            DestroyWindow(m_hwndScintilla);
        }
        m_hwndScintilla = other.m_hwndScintilla;
        m_fnDirect = other.m_fnDirect;
        m_ptrDirect = other.m_ptrDirect;
        m_isDarkMode = other.m_isDarkMode;
        m_wordWrap = other.m_wordWrap;

        other.m_hwndScintilla = nullptr;
        other.m_fnDirect = nullptr;
        other.m_ptrDirect = 0;
    }
    return *this;
}

bool EditorView::Create(HWND parent, HINSTANCE hInstance, int controlId,
                        int x, int y, int width, int height) {
    if (!InitializeScintilla(hInstance)) {
        return false;
    }

    m_hwndScintilla = CreateWindowExW(
        0,
        L"Scintilla",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN,
        x, y, width, height,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
        hInstance,
        nullptr
    );

    if (!m_hwndScintilla) {
        return false;
    }

    // Direct function call setup to avoid SendMessage overhead (M1.2)
    m_fnDirect = reinterpret_cast<SciFnDirect>(
        SendMessageW(m_hwndScintilla, SCI_GETDIRECTFUNCTION, 0, 0));
    m_ptrDirect = SendMessageW(m_hwndScintilla, SCI_GETDIRECTPOINTER, 0, 0);

    // Modern text rendering: DirectWrite (hardware accelerated)
    Call(SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE);

    // UTF-8 document codepage (F-03)
    Call(SCI_SETCODEPAGE, SC_CP_UTF8);

    // Initial word wrap
    Call(SCI_SETWRAPMODE, m_wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE);

    // Margin 0: Line numbers
    Call(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    Call(SCI_SETMARGINWIDTHN, 0, 48);

    // Margin 1: Symbol margin (none for now)
    Call(SCI_SETMARGINWIDTHN, 1, 0);

    // Tab size: 4 spaces
    Call(SCI_SETTABWIDTH, 4);
    Call(SCI_SETUSETABS, false);

    // Attach Lexilla Markdown lexer (M1.3, F-05)
    Call(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(lmMarkdown.Create()));

    // Apply initial theme styles
    SetupStyles(m_isDarkMode);

    return true;
}

void EditorView::SetBounds(int x, int y, int width, int height) {
    if (m_hwndScintilla) {
        SetWindowPos(m_hwndScintilla, nullptr, x, y, width, height,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void EditorView::SetFocus() {
    if (m_hwndScintilla) {
        ::SetFocus(m_hwndScintilla);
    }
}

std::string EditorView::GetText() const {
    auto length = Call(SCI_GETLENGTH);
    if (length <= 0) {
        return {};
    }
    std::string buffer(length, '\0');
    Call(SCI_GETTEXT, length + 1, reinterpret_cast<sptr_t>(buffer.data()));
    return buffer;
}

void EditorView::SetText(std::string_view text) {
    Call(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(text.data()));
    Call(SCI_EMPTYUNDOBUFFER);
    Call(SCI_SETSAVEPOINT);
    UpdateLineNumberMargin();
}

bool EditorView::IsModified() const {
    return Call(SCI_GETMODIFY) != 0;
}

void EditorView::SetSavePoint() {
    Call(SCI_SETSAVEPOINT);
}

void EditorView::Undo() {
    Call(SCI_UNDO);
}

void EditorView::Redo() {
    Call(SCI_REDO);
}

bool EditorView::CanUndo() const {
    return Call(SCI_CANUNDO) != 0;
}

bool EditorView::CanRedo() const {
    return Call(SCI_CANREDO) != 0;
}

void EditorView::Cut() {
    Call(SCI_CUT);
}

void EditorView::Copy() {
    Call(SCI_COPY);
}

void EditorView::Paste() {
    Call(SCI_PASTE);
}

void EditorView::SelectAll() {
    Call(SCI_SELECTALL);
}

void EditorView::SetWordWrap(bool wrap) {
    m_wordWrap = wrap;
    Call(SCI_SETWRAPMODE, m_wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE);
}

void EditorView::ToggleWordWrap() {
    SetWordWrap(!m_wordWrap);
}

void EditorView::GotoLine(int line) {
    if (line < 1) line = 1;
    Call(SCI_GOTOLINE, line - 1);
    Call(SCI_ENSUREVISIBLE, line - 1);
}

bool EditorView::FindNext(std::string_view text, bool matchCase, bool wholeWord, bool regex) {
    if (text.empty()) {
        return false;
    }

    int flags = 0;
    if (matchCase) flags |= SCFIND_MATCHCASE;
    if (wholeWord) flags |= SCFIND_WHOLEWORD;
    if (regex) flags |= (SCFIND_REGEXP | SCFIND_POSIX);

    Call(SCI_SETSEARCHFLAGS, flags);

    auto currentPos = Call(SCI_GETCURRENTPOS);
    auto docLength = Call(SCI_GETLENGTH);

    // Search forward from current position to end
    Call(SCI_SETTARGETSTART, currentPos);
    Call(SCI_SETTARGETEND, docLength);

    auto pos = Call(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<sptr_t>(text.data()));
    if (pos == -1 && currentPos > 0) {
        // Wrap around search from start to current position
        Call(SCI_SETTARGETSTART, 0);
        Call(SCI_SETTARGETEND, currentPos);
        pos = Call(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<sptr_t>(text.data()));
    }

    if (pos != -1) {
        auto matchStart = Call(SCI_GETTARGETSTART);
        auto matchEnd = Call(SCI_GETTARGETEND);
        Call(SCI_SETSEL, matchStart, matchEnd);
        Call(SCI_SCROLLCARET);
        return true;
    }

    return false;
}

int EditorView::ReplaceAll(std::string_view findText, std::string_view replaceText,
                           bool matchCase, bool wholeWord, bool regex) {
    if (findText.empty()) {
        return 0;
    }

    int flags = 0;
    if (matchCase) flags |= SCFIND_MATCHCASE;
    if (wholeWord) flags |= SCFIND_WHOLEWORD;
    if (regex) flags |= (SCFIND_REGEXP | SCFIND_POSIX);

    Call(SCI_SETSEARCHFLAGS, flags);
    Call(SCI_BEGINUNDOACTION);

    int count = 0;
    auto docLength = Call(SCI_GETLENGTH);
    Call(SCI_SETTARGETSTART, 0);
    Call(SCI_SETTARGETEND, docLength);

    while (Call(SCI_SEARCHINTARGET, findText.size(), reinterpret_cast<sptr_t>(findText.data())) != -1) {
        Call(SCI_REPLACETARGET, replaceText.size(), reinterpret_cast<sptr_t>(replaceText.data()));
        auto replacedEnd = Call(SCI_GETTARGETEND);
        count++;

        docLength = Call(SCI_GETLENGTH);
        Call(SCI_SETTARGETSTART, replacedEnd);
        Call(SCI_SETTARGETEND, docLength);
    }

    Call(SCI_ENDUNDOACTION);
    return count;
}

void EditorView::ApplyTheme(bool darkMode) {
    m_isDarkMode = darkMode;
    SetupStyles(darkMode);
}

void EditorView::SetupStyles(bool darkMode) {
    const char* fontName = "Cascadia Code";
    int fontSize = 11;

    COLORREF bgColor;
    COLORREF fgColor;
    COLORREF marginBgColor;
    COLORREF marginFgColor;
    COLORREF caretColor;
    COLORREF selBgColor;
    COLORREF headerColor;
    COLORREF codeColor;
    COLORREF linkColor;
    COLORREF quoteColor;
    COLORREF listColor;

    if (darkMode) {
        bgColor = MakeSciColor(30, 30, 30);
        fgColor = MakeSciColor(220, 220, 220);
        marginBgColor = MakeSciColor(37, 37, 38);
        marginFgColor = MakeSciColor(133, 133, 133);
        caretColor = MakeSciColor(255, 255, 255);
        selBgColor = MakeSciColor(38, 79, 120);
        headerColor = MakeSciColor(86, 156, 214);
        codeColor = MakeSciColor(206, 145, 120);
        linkColor = MakeSciColor(78, 201, 176);
        quoteColor = MakeSciColor(106, 153, 85);
        listColor = MakeSciColor(197, 134, 192);
    } else {
        bgColor = MakeSciColor(255, 255, 255);
        fgColor = MakeSciColor(36, 41, 47);
        marginBgColor = MakeSciColor(246, 248, 250);
        marginFgColor = MakeSciColor(140, 149, 159);
        caretColor = MakeSciColor(0, 0, 0);
        selBgColor = MakeSciColor(179, 215, 255);
        headerColor = MakeSciColor(5, 80, 174);
        codeColor = MakeSciColor(149, 56, 0);
        linkColor = MakeSciColor(9, 105, 218);
        quoteColor = MakeSciColor(87, 96, 106);
        listColor = MakeSciColor(130, 80, 223);
    }

    // Default style
    Call(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>(fontName));
    Call(SCI_STYLESETSIZE, STYLE_DEFAULT, fontSize);
    Call(SCI_STYLESETFORE, STYLE_DEFAULT, fgColor);
    Call(SCI_STYLESETBACK, STYLE_DEFAULT, bgColor);
    Call(SCI_STYLECLEARALL);

    // Caret and selection
    Call(SCI_SETCARETFORE, caretColor);
    Call(SCI_SETSELBACK, true, selBgColor);

    // Line number margin styling
    Call(SCI_STYLESETFORE, STYLE_LINENUMBER, marginFgColor);
    Call(SCI_STYLESETBACK, STYLE_LINENUMBER, marginBgColor);

    // Markdown syntax highlight styles (M1.3, F-05)
    auto setStyle = [this, fontName, fontSize](int styleId, COLORREF fore, COLORREF back,
                                               bool bold, bool italic, bool underline) {
        Call(SCI_STYLESETFONT, styleId, reinterpret_cast<sptr_t>(fontName));
        Call(SCI_STYLESETSIZE, styleId, fontSize);
        Call(SCI_STYLESETFORE, styleId, fore);
        Call(SCI_STYLESETBACK, styleId, back);
        Call(SCI_STYLESETBOLD, styleId, bold);
        Call(SCI_STYLESETITALIC, styleId, italic);
        Call(SCI_STYLESETUNDERLINE, styleId, underline);
    };

    // Headers H1-H6
    for (int h = SCE_MARKDOWN_HEADER1; h <= SCE_MARKDOWN_HEADER6; ++h) {
        setStyle(h, headerColor, bgColor, true, false, false);
    }

    // Emphasis
    setStyle(SCE_MARKDOWN_STRONG1, fgColor, bgColor, true, false, false);
    setStyle(SCE_MARKDOWN_STRONG2, fgColor, bgColor, true, false, false);
    setStyle(SCE_MARKDOWN_EM1, fgColor, bgColor, false, true, false);
    setStyle(SCE_MARKDOWN_EM2, fgColor, bgColor, false, true, false);

    // Code inline and blocks
    COLORREF codeBg = darkMode ? MakeSciColor(40, 40, 40) : MakeSciColor(246, 248, 250);
    setStyle(SCE_MARKDOWN_CODE, codeColor, codeBg, false, false, false);
    setStyle(SCE_MARKDOWN_CODE2, codeColor, codeBg, false, false, false);
    setStyle(SCE_MARKDOWN_CODEBK, codeColor, codeBg, false, false, false);

    // Links
    setStyle(SCE_MARKDOWN_LINK, linkColor, bgColor, false, false, true);

    // Blockquotes
    setStyle(SCE_MARKDOWN_BLOCKQUOTE, quoteColor, bgColor, false, false, false);

    // Lists
    setStyle(SCE_MARKDOWN_ULIST_ITEM, listColor, bgColor, false, false, false);
    setStyle(SCE_MARKDOWN_OLIST_ITEM, listColor, bgColor, false, false, false);

    // Horizontal rules & strikeouts
    setStyle(SCE_MARKDOWN_HRULE, marginFgColor, bgColor, false, false, false);
    setStyle(SCE_MARKDOWN_STRIKEOUT, marginFgColor, bgColor, false, false, false);
}

void EditorView::UpdateLineNumberMargin() {
    auto lines = Call(SCI_GETLINECOUNT);
    int digits = 1;
    while (lines >= 10) {
        lines /= 10;
        digits++;
    }
    int width = std::max(4, digits) * 10 + 12;
    Call(SCI_SETMARGINWIDTHN, 0, width);
}

} // namespace Pluma::Editor
