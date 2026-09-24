#include "settings.h"

#include <shlobj.h> // SHGetKnownFolderPath

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>
#include <system_error>

namespace Pluma::Config {

namespace {

std::wstring Utf8ToUtf16(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), count);
    return result;
}

std::string Utf16ToUtf8(std::wstring_view utf16) {
    if (utf16.empty()) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()),
                                          nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), result.data(), count,
                        nullptr, nullptr);
    return result;
}

std::string_view Trim(std::string_view s) {
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && isSpace(s.front())) s.remove_prefix(1);
    while (!s.empty() && isSpace(s.back())) s.remove_suffix(1);
    return s;
}

std::string ToLowerAscii(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

// "section.key" (lower case) -> raw value
using ValueMap = std::map<std::string, std::string, std::less<>>;

ValueMap ParseIni(std::string_view text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }

    ValueMap values;
    std::string section;
    while (!text.empty()) {
        const size_t eol = text.find('\n');
        std::string_view line = Trim(text.substr(0, eol));
        text = (eol == std::string_view::npos) ? std::string_view{} : text.substr(eol + 1);

        if (line.empty() || line.front() == ';' || line.front() == '#') continue;
        if (line.front() == '[') {
            const size_t close = line.find(']');
            if (close != std::string_view::npos) {
                section = ToLowerAscii(Trim(line.substr(1, close - 1)));
            }
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        std::string key = section + "." + ToLowerAscii(Trim(line.substr(0, eq)));
        values[std::move(key)] = std::string(Trim(line.substr(eq + 1)));
    }
    return values;
}

const std::string* Find(const ValueMap& values, std::string_view key) {
    const auto it = values.find(key);
    return it == values.end() ? nullptr : &it->second;
}

void ReadBool(const ValueMap& values, std::string_view key, bool& out) {
    const std::string* raw = Find(values, key);
    if (!raw) return;
    const std::string v = ToLowerAscii(*raw);
    if (v == "true" || v == "1" || v == "yes" || v == "on") out = true;
    else if (v == "false" || v == "0" || v == "no" || v == "off") out = false;
}

void ReadInt(const ValueMap& values, std::string_view key, int& out) {
    const std::string* raw = Find(values, key);
    if (!raw || raw->empty()) return;
    int value = 0;
    const char* first = raw->data();
    const char* last = first + raw->size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec == std::errc{} && ptr == last) out = value;
}

void ReadFloat(const ValueMap& values, std::string_view key, float& out) {
    const std::string* raw = Find(values, key);
    if (!raw || raw->empty()) return;
    float value = 0.0f;
    const char* first = raw->data();
    const char* last = first + raw->size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec == std::errc{} && ptr == last && std::isfinite(value)) out = value;
}

void ReadString(const ValueMap& values, std::string_view key, std::wstring& out) {
    if (const std::string* raw = Find(values, key)) out = Utf8ToUtf16(*raw);
}

// Maps a lower-case token to an enum value; unknown tokens keep the current value.
template <typename Enum, size_t N>
void ReadEnum(const ValueMap& values, std::string_view key, Enum& out,
              const std::pair<const char*, Enum> (&table)[N]) {
    const std::string* raw = Find(values, key);
    if (!raw) return;
    const std::string v = ToLowerAscii(*raw);
    for (const auto& [name, value] : table) {
        if (v == name) {
            out = value;
            return;
        }
    }
}

template <typename Enum, size_t N>
const char* EnumName(Enum value, const std::pair<const char*, Enum> (&table)[N]) {
    for (const auto& [name, v] : table) {
        if (v == value) return name;
    }
    return table[0].first;
}

constexpr std::pair<const char*, Platform::AppTheme> kThemes[] = {
    {"system", Platform::AppTheme::System},
    {"dark", Platform::AppTheme::Dark},
    {"light", Platform::AppTheme::Light},
};

constexpr std::pair<const char*, IO::LineEnding> kLineEndings[] = {
    {"crlf", IO::LineEnding::CRLF},
    {"lf", IO::LineEnding::LF},
};

constexpr std::pair<const char*, StartupView> kStartupViews[] = {
    {"remember", StartupView::Remember},
    {"editor", StartupView::EditorOnly},
    {"split", StartupView::Split},
    {"preview", StartupView::PreviewOnly},
};

constexpr std::pair<const char*, ViewLayout> kLayouts[] = {
    {"split", ViewLayout::Split},
    {"editor", ViewLayout::EditorOnly},
    {"preview", ViewLayout::PreviewOnly},
};

constexpr std::pair<const char*, Export::PageSize> kPageSizes[] = {
    {"a4", Export::PageSize::A4},
    {"letter", Export::PageSize::Letter},
};

constexpr std::pair<const char*, Export::HtmlTheme> kHtmlThemes[] = {
    {"auto", Export::HtmlTheme::Auto},
    {"light", Export::HtmlTheme::Light},
    {"dark", Export::HtmlTheme::Dark},
};

class IniWriter {
public:
    void Section(const char* name) {
        if (!m_out.empty()) m_out += "\n";
        m_out += "[";
        m_out += name;
        m_out += "]\n";
    }
    void Value(const char* key, std::string_view value) {
        m_out += key;
        m_out += "=";
        m_out += value;
        m_out += "\n";
    }
    void Bool(const char* key, bool value) { Value(key, value ? "true" : "false"); }
    void Int(const char* key, int value) { Value(key, std::to_string(value)); }
    void Float(const char* key, float value) {
        char buf[32]{};
        const auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::fixed, 3);
        Value(key, ec == std::errc{} ? std::string_view(buf, static_cast<size_t>(ptr - buf)) : "0.5");
    }
    std::string Take() { return std::move(m_out); }

private:
    std::string m_out;
};

} // namespace

ViewLayout Settings::InitialView() const noexcept {
    switch (startupView) {
    case StartupView::EditorOnly:  return ViewLayout::EditorOnly;
    case StartupView::Split:       return ViewLayout::Split;
    case StartupView::PreviewOnly: return ViewLayout::PreviewOnly;
    default:                       return lastView;
    }
}

void Settings::Sanitize() {
    editorFontSize = std::clamp(editorFontSize, kMinEditorFontSize, kMaxEditorFontSize);
    previewZoom = std::clamp(previewZoom, kMinPreviewZoom, kMaxPreviewZoom);
    tabWidth = std::clamp(tabWidth, kMinTabWidth, kMaxTabWidth);
    outlineWidth = std::clamp(outlineWidth, kMinOutlineWidth, kMaxOutlineWidth);
    if (!std::isfinite(splitRatio)) splitRatio = 0.5f;
    splitRatio = std::clamp(splitRatio, kMinSplitRatio, kMaxSplitRatio);
    pdfMarginMm = std::clamp(pdfMarginMm, kMinPdfMarginMm, kMaxPdfMarginMm);
    if (newFileLineEnding == IO::LineEnding::CR) newFileLineEnding = IO::LineEnding::CRLF;
    if (windowWidth <= 0 || windowHeight <= 0) hasWindowRect = false;
}

Settings ParseSettings(std::string_view iniUtf8) {
    const ValueMap v = ParseIni(iniUtf8);
    Settings s;

    ReadEnum(v, "appearance.theme", s.theme, kThemes);
    ReadString(v, "appearance.editorfont", s.editorFont);
    ReadInt(v, "appearance.editorfontsize", s.editorFontSize);
    ReadInt(v, "appearance.previewzoom", s.previewZoom);

    ReadBool(v, "editor.wordwrap", s.wordWrap);
    ReadBool(v, "editor.linenumbers", s.showLineNumbers);
    ReadBool(v, "editor.highlightcurrentline", s.highlightCurrentLine);
    ReadInt(v, "editor.tabwidth", s.tabWidth);
    ReadBool(v, "editor.usetabs", s.useTabs);
    ReadEnum(v, "editor.newfilelineending", s.newFileLineEnding, kLineEndings);

    ReadEnum(v, "view.startupview", s.startupView, kStartupViews);
    ReadEnum(v, "view.lastview", s.lastView, kLayouts);
    ReadBool(v, "view.showoutline", s.showOutline);
    ReadBool(v, "view.showstatusbar", s.showStatusBar);
    ReadBool(v, "view.syncscroll", s.syncScroll);
    ReadInt(v, "view.outlinewidth", s.outlineWidth);
    ReadFloat(v, "view.splitratio", s.splitRatio);

    ReadBool(v, "window.remember", s.rememberWindow);
    if (Find(v, "window.width") && Find(v, "window.height")) {
        s.hasWindowRect = true;
        ReadInt(v, "window.x", s.windowX);
        ReadInt(v, "window.y", s.windowY);
        ReadInt(v, "window.width", s.windowWidth);
        ReadInt(v, "window.height", s.windowHeight);
        ReadBool(v, "window.maximized", s.windowMaximized);
    }

    ReadBool(v, "files.reopenlastfile", s.reopenLastFile);
    ReadString(v, "files.lastfile", s.lastFile);

    ReadEnum(v, "export.pdfpagesize", s.pdfPageSize, kPageSizes);
    ReadInt(v, "export.pdfmarginmm", s.pdfMarginMm);
    ReadEnum(v, "export.htmltheme", s.htmlTheme, kHtmlThemes);

    s.Sanitize();
    return s;
}

std::string SerializeSettings(const Settings& s) {
    IniWriter w;
    w.Section("Appearance");
    w.Value("Theme", EnumName(s.theme, kThemes));
    w.Value("EditorFont", Utf16ToUtf8(s.editorFont));
    w.Int("EditorFontSize", s.editorFontSize);
    w.Int("PreviewZoom", s.previewZoom);

    w.Section("Editor");
    w.Bool("WordWrap", s.wordWrap);
    w.Bool("LineNumbers", s.showLineNumbers);
    w.Bool("HighlightCurrentLine", s.highlightCurrentLine);
    w.Int("TabWidth", s.tabWidth);
    w.Bool("UseTabs", s.useTabs);
    w.Value("NewFileLineEnding", EnumName(s.newFileLineEnding, kLineEndings));

    w.Section("View");
    w.Value("StartupView", EnumName(s.startupView, kStartupViews));
    w.Value("LastView", EnumName(s.lastView, kLayouts));
    w.Bool("ShowOutline", s.showOutline);
    w.Bool("ShowStatusBar", s.showStatusBar);
    w.Bool("SyncScroll", s.syncScroll);
    w.Int("OutlineWidth", s.outlineWidth);
    w.Float("SplitRatio", s.splitRatio);

    w.Section("Window");
    w.Bool("Remember", s.rememberWindow);
    if (s.hasWindowRect) {
        w.Int("X", s.windowX);
        w.Int("Y", s.windowY);
        w.Int("Width", s.windowWidth);
        w.Int("Height", s.windowHeight);
        w.Bool("Maximized", s.windowMaximized);
    }

    w.Section("Files");
    w.Bool("ReopenLastFile", s.reopenLastFile);
    w.Value("LastFile", Utf16ToUtf8(s.lastFile));

    w.Section("Export");
    w.Value("PdfPageSize", EnumName(s.pdfPageSize, kPageSizes));
    w.Int("PdfMarginMm", s.pdfMarginMm);
    w.Value("HtmlTheme", EnumName(s.htmlTheme, kHtmlThemes));
    return w.Take();
}

std::filesystem::path DefaultSettingsPath() {
    std::wstring exePath(MAX_PATH, L'\0');
    for (;;) {
        const DWORD len = GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
        if (len == 0) {
            exePath.clear();
            break;
        }
        if (len < exePath.size()) {
            exePath.resize(len);
            break;
        }
        exePath.resize(exePath.size() * 2);
    }

    std::error_code ec;
    if (!exePath.empty()) {
        std::filesystem::path portable = std::filesystem::path(exePath).parent_path() / L"pluma.ini";
        if (std::filesystem::is_regular_file(portable, ec)) return portable;
    }

    PWSTR appData = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &appData))) {
        result = std::filesystem::path(appData) / L"Pluma" / L"pluma.ini";
    }
    CoTaskMemFree(appData);
    return result;
}

Settings LoadSettings(const std::filesystem::path& path) {
    if (path.empty()) return {};
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return ParseSettings(text);
}

bool SaveSettings(const std::filesystem::path& path, const Settings& settings) {
    if (path.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    try {
        IO::WriteDocumentAtomic(path, SerializeSettings(settings), IO::Encoding::Utf8, IO::LineEnding::CRLF);
    } catch (...) {
        return false;
    }
    return true;
}

} // namespace Pluma::Config
