#include "editor_view.h"

#include <vector>
#include <algorithm>
#include <uxtheme.h>

#include "ILexer.h"
#include "LexerModule.h"

extern const Lexilla::LexerModule lmMarkdown;

namespace Pluma::Editor {

namespace {

inline COLORREF MakeSciColor(uint8_t r, uint8_t g, uint8_t b) {
    return RGB(r, g, b);
}

int CALLBACK FontExistsProc(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lParam) {
    *reinterpret_cast<bool*>(lParam) = true;
    return 0; // Stop enumeration
}

bool FontExists(const wchar_t* faceName) {
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(lf.lfFaceName, faceName, _TRUNCATE);
    bool found = false;
    HDC hdc = GetDC(nullptr);
    EnumFontFamiliesExW(hdc, &lf, FontExistsProc, reinterpret_cast<LPARAM>(&found), 0);
    ReleaseDC(nullptr, hdc);
    return found;
}

// Cascadia Code ships with Windows 11 / Terminal; Windows 10 always has Consolas.
const char* EditorFontName() {
    static const char* s_font = [] {
        if (FontExists(L"Cascadia Code")) return "Cascadia Code";
        if (FontExists(L"Cascadia Mono")) return "Cascadia Mono";
        return "Consolas";
    }();
    return s_font;
}

// True when `text` is `prefix` + content + `suffix`, without mistaking "**bold**" for "*italic*".
bool IsWrappedBy(std::string_view text, std::string_view prefix, std::string_view suffix) {
    if (text.size() < prefix.size() + suffix.size() || !text.starts_with(prefix) || !text.ends_with(suffix)) {
        return false;
    }
    if (prefix.size() == 1 && text.size() > 2) {
        const char m = prefix[0];
        const bool doubledStart = text[1] == m && !(text.size() > 3 && text[2] == m);
        const bool doubledEnd = text[text.size() - 2] == m && !(text.size() > 3 && text[text.size() - 3] == m);
        if (doubledStart || doubledEnd) {
            return false;
        }
    }
    return true;
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

    // Modern text rendering: DirectWrite (hardware accelerated, colour emoji).
    // Direct2D already double-buffers, Scintilla's own buffer is redundant.
    Call(SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DIRECTWRITE);
    Call(SCI_SETBUFFEREDDRAW, 0);
    Call(SCI_SETLAYOUTCACHE, SC_CACHE_PAGE);
    Call(SCI_SETIMEINTERACTION, SC_IME_INLINE);

    // Comfortable editing defaults
    Call(SCI_SETMULTIPLESELECTION, 1);
    Call(SCI_SETADDITIONALSELECTIONTYPING, 1);
    Call(SCI_SETMOUSEWHEELCAPTURES, 0);
    Call(SCI_SETSCROLLWIDTH, 1);
    Call(SCI_SETSCROLLWIDTHTRACKING, 1);
    Call(SCI_SETWRAPINDENTMODE, SC_WRAPINDENT_SAME);
    Call(SCI_SETEXTRAASCENT, 2);
    Call(SCI_SETEXTRADESCENT, 2);
    Call(SCI_SETMARGINLEFT, 0, 4);

    // UTF-8 document codepage (F-03)
    Call(SCI_SETCODEPAGE, SC_CP_UTF8);

    // Initial word wrap
    Call(SCI_SETWRAPMODE, m_wordWrap ? SC_WRAP_WORD : SC_WRAP_NONE);

    // Margin 0: Line numbers (width computed from the font in UpdateLineNumberMargin)
    Call(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);

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
    // Length-based insertion: string_view is not NUL-terminated and documents may contain NULs.
    Call(SCI_SETUNDOCOLLECTION, 0);
    Call(SCI_CLEARALL);
    if (!text.empty()) {
        Call(SCI_APPENDTEXT, text.size(), reinterpret_cast<sptr_t>(text.data()));
    }
    Call(SCI_SETUNDOCOLLECTION, 1);
    Call(SCI_EMPTYUNDOBUFFER);
    Call(SCI_SETSAVEPOINT);
    Call(SCI_GOTOPOS, 0);
    UpdateLineNumberMargin();
}

void EditorView::SetEolMode(int sciEolMode) {
    Call(SCI_SETEOLMODE, sciEolMode);
}

std::string_view EditorView::EolString() const {
    switch (Call(SCI_GETEOLMODE)) {
    case SC_EOL_LF: return "\n";
    case SC_EOL_CR: return "\r";
    default:        return "\r\n";
    }
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

int EditorView::GetFirstVisibleDocLine() const {
    sptr_t visibleLine = Call(SCI_GETFIRSTVISIBLELINE, 0, 0);
    sptr_t docLine = Call(SCI_DOCLINEFROMVISIBLE, visibleLine, 0);
    return static_cast<int>(docLine) + 1; // 1-indexed
}

void EditorView::ScrollToDocLine(int docLine) {
    int targetDoc = (std::max)(1, docLine) - 1; // to 0-indexed
    sptr_t targetVisible = Call(SCI_VISIBLEFROMDOCLINE, targetDoc, 0);
    sptr_t currentVisible = Call(SCI_GETFIRSTVISIBLELINE, 0, 0);
    sptr_t delta = targetVisible - currentVisible;
    if (delta != 0) {
        Call(SCI_LINESCROLL, 0, delta);
    }
}

bool EditorView::FindNext(std::string_view text, bool matchCase, bool wholeWord, bool regex, bool forward) {
    if (text.empty()) {
        return false;
    }

    int flags = 0;
    if (matchCase) flags |= SCFIND_MATCHCASE;
    if (wholeWord) flags |= SCFIND_WHOLEWORD;
    if (regex) flags |= (SCFIND_REGEXP | SCFIND_POSIX);

    Call(SCI_SETSEARCHFLAGS, flags);

    auto docLength = Call(SCI_GETLENGTH);

    if (forward) {
        auto selEnd = Call(SCI_GETSELECTIONEND);
        Call(SCI_SETTARGETSTART, selEnd);
        Call(SCI_SETTARGETEND, docLength);

        auto pos = Call(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<sptr_t>(text.data()));
        if (pos == -1 && selEnd > 0) {
            Call(SCI_SETTARGETSTART, 0);
            Call(SCI_SETTARGETEND, selEnd);
            pos = Call(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<sptr_t>(text.data()));
        }

        if (pos != -1) {
            auto matchStart = Call(SCI_GETTARGETSTART);
            auto matchEnd = Call(SCI_GETTARGETEND);
            Call(SCI_SETSEL, matchStart, matchEnd);
            Call(SCI_SCROLLCARET);
            return true;
        }
    } else {
        auto selStart = Call(SCI_GETSELECTIONSTART);
        Call(SCI_SETTARGETSTART, selStart);
        Call(SCI_SETTARGETEND, 0);

        auto pos = Call(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<sptr_t>(text.data()));
        if (pos == -1 && selStart < docLength) {
            Call(SCI_SETTARGETSTART, docLength);
            Call(SCI_SETTARGETEND, selStart);
            pos = Call(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<sptr_t>(text.data()));
        }

        if (pos != -1) {
            auto matchStart = Call(SCI_GETTARGETSTART);
            auto matchEnd = Call(SCI_GETTARGETEND);
            Call(SCI_SETSEL, matchStart, matchEnd);
            Call(SCI_SCROLLCARET);
            return true;
        }
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

bool EditorView::SelectionMatches(std::string_view text, bool matchCase) const {
    const sptr_t start = Call(SCI_GETSELECTIONSTART);
    const sptr_t end = Call(SCI_GETSELECTIONEND);
    if (text.empty() || end - start != static_cast<sptr_t>(text.size())) {
        return false;
    }
    const std::string selected = GetRange(start, end);
    if (matchCase) {
        return selected == text;
    }
    auto toWide = [](std::string_view utf8) {
        std::wstring w(utf8.size(), L'\0');
        const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                          w.data(), static_cast<int>(w.size()));
        w.resize(n > 0 ? static_cast<size_t>(n) : 0);
        return w;
    };
    const std::wstring a = toWide(selected);
    const std::wstring b = toWide(text);
    return CompareStringOrdinal(a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

void EditorView::ApplyTheme(bool darkMode) {
    m_isDarkMode = darkMode;
    if (m_hwndScintilla) {
        SetWindowTheme(m_hwndScintilla, darkMode ? L"DarkMode_Explorer" : L"", nullptr);
    }
    SetupStyles(darkMode);
}

void EditorView::SetupStyles(bool darkMode) {
    const char* fontName = EditorFontName();
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
    Call(SCI_SETCARETWIDTH, 2);
    Call(SCI_SETSELBACK, true, selBgColor);

    // Active line background highlight
    Call(SCI_SETCARETLINEVISIBLE, true);
    Call(SCI_SETCARETLINEVISIBLEALWAYS, true);
    Call(SCI_SETCARETLINEBACK, darkMode ? MakeSciColor(38, 38, 38) : MakeSciColor(245, 245, 245));

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

    // Fonts may have changed: recompute the line number margin.
    m_marginDigits = 0;
    UpdateLineNumberMargin();
}

void EditorView::WrapSelection(std::string_view prefix, std::string_view suffix) {
    const sptr_t start = Call(SCI_GETSELECTIONSTART);
    const sptr_t end = Call(SCI_GETSELECTIONEND);
    const auto prefixLen = static_cast<sptr_t>(prefix.size());
    const auto suffixLen = static_cast<sptr_t>(suffix.size());
    const sptr_t docLength = Call(SCI_GETLENGTH);

    Call(SCI_BEGINUNDOACTION);

    // Toggle off: the selection itself is already wrapped ("**text**" selected)...
    const std::string selected = GetRange(start, end);
    if (start != end && IsWrappedBy(selected, prefix, suffix)) {
        const std::string inner = selected.substr(prefix.size(), selected.size() - prefix.size() - suffix.size());
        Call(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(inner.c_str()));
        Call(SCI_SETSEL, start, start + static_cast<sptr_t>(inner.size()));
        Call(SCI_ENDUNDOACTION);
        return;
    }
    // ...or the markers sit right around it ("**" + text + "**").
    if (start >= prefixLen && end + suffixLen <= docLength) {
        const std::string around = GetRange(start - prefixLen, end + suffixLen);
        if (IsWrappedBy(around, prefix, suffix)) {
            Call(SCI_DELETERANGE, end, suffixLen);
            Call(SCI_DELETERANGE, start - prefixLen, prefixLen);
            Call(SCI_SETSEL, start - prefixLen, end - prefixLen);
            Call(SCI_ENDUNDOACTION);
            return;
        }
    }

    const std::string replacement = std::string(prefix) + selected + std::string(suffix);
    Call(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(replacement.c_str()));
    if (start != end) {
        Call(SCI_SETSEL, start + prefixLen, end + prefixLen);
    } else {
        Call(SCI_SETSEL, start + prefixLen, start + prefixLen);
    }
    Call(SCI_ENDUNDOACTION);
}

std::string EditorView::GetRange(sptr_t start, sptr_t end) const {
    if (end <= start) return {};
    std::string text(static_cast<size_t>(end - start), '\0');
    Sci_TextRangeFull tr{};
    tr.chrg.cpMin = start;
    tr.chrg.cpMax = end;
    tr.lpstrText = text.data();
    Call(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&tr));
    return text;
}

void EditorView::InsertBold() {
    WrapSelection("**", "**");
}

void EditorView::InsertItalic() {
    WrapSelection("*", "*");
}

void EditorView::InsertCode() {
    const sptr_t start = Call(SCI_GETSELECTIONSTART);
    const sptr_t end = Call(SCI_GETSELECTIONEND);
    if (start != end) {
        const std::string selected = GetRange(start, end);
        if (selected.find_first_of("\r\n") != std::string::npos) {
            // Reuse the line ending already present in the selection.
            const char* eol = selected.find("\r\n") != std::string::npos ? "\r\n"
                            : selected.find('\n') != std::string::npos   ? "\n"
                                                                          : "\r";
            WrapSelection(std::string("```") + eol, std::string(eol) + "```");
            return;
        }
    }
    WrapSelection("`", "`");
}

void EditorView::InsertStrikethrough() {
    WrapSelection("~~", "~~");
}

void EditorView::InsertLink() {
    const sptr_t start = Call(SCI_GETSELECTIONSTART);
    const sptr_t end = Call(SCI_GETSELECTIONEND);
    const std::string selected = GetRange(start, end);
    Call(SCI_BEGINUNDOACTION);
    if (!selected.empty()) {
        // A selected URL becomes the target; any other text becomes the label.
        const bool isUrl = selected.starts_with("http://") || selected.starts_with("https://") ||
                           selected.starts_with("www.");
        const std::string replacement = isUrl ? "[](" + selected + ")" : "[" + selected + "](url)";
        Call(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(replacement.c_str()));
        if (isUrl) {
            Call(SCI_SETSEL, start + 1, start + 1);
        } else {
            const sptr_t urlStart = start + static_cast<sptr_t>(selected.size()) + 3;
            Call(SCI_SETSEL, urlStart, urlStart + 3);
        }
    } else {
        Call(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("[](url)"));
        Call(SCI_SETSEL, start + 1, start + 1);
    }
    Call(SCI_ENDUNDOACTION);
}

EditorView::CursorPos EditorView::GetCursorPosition() const {
    sptr_t pos = Call(SCI_GETCURRENTPOS);
    sptr_t line = Call(SCI_LINEFROMPOSITION, pos);
    sptr_t col = Call(SCI_GETCOLUMN, pos);
    return { static_cast<int>(line) + 1, static_cast<int>(col) + 1 };
}

EditorView::DocumentStats EditorView::GetDocumentStats() const {
    DocumentStats stats{};
    const auto length = static_cast<size_t>(Call(SCI_GETLENGTH));
    if (length == 0) {
        return stats;
    }
    // Zero-copy view of the buffer (valid until the next modification).
    const auto* text = reinterpret_cast<const unsigned char*>(Call(SCI_GETCHARACTERPOINTER));
    if (!text) {
        return stats;
    }

    bool inWord = false;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = text[i];
        if ((c & 0xC0) != 0x80) {
            stats.characters++; // Count code points, not UTF-8 bytes ("ñ" is one character)
        }
        const bool isSpace = c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
        if (!isSpace) {
            if (!inWord) {
                stats.words++;
                inWord = true;
            }
        } else {
            inWord = false;
        }
    }
    return stats;
}

void EditorView::UpdateLineNumberMargin() {
    auto lines = Call(SCI_GETLINECOUNT);
    int digits = 1;
    while (lines >= 10) {
        lines /= 10;
        digits++;
    }
    digits = (std::max)(3, digits);
    if (digits == m_marginDigits) {
        return;
    }
    m_marginDigits = digits;
    // Measured with the actual font, so it follows DPI and font changes.
    const std::string sample(static_cast<size_t>(digits), '9');
    const auto textWidth = Call(SCI_TEXTWIDTH, STYLE_LINENUMBER, reinterpret_cast<sptr_t>(sample.c_str()));
    Call(SCI_SETMARGINWIDTHN, 0, textWidth + textWidth / digits + 8);
}

} // namespace Pluma::Editor
