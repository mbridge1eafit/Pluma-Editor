#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "../platform/theme.h"
#include "../io/document_io.h"
#include "../export/html_exporter.h"
#include "../export/pdf_exporter.h"

namespace Pluma::Config {

enum class ViewLayout {
    EditorOnly,
    Split,
    PreviewOnly
};

enum class StartupView {
    Remember, // Reopen with the layout used when Pluma was closed
    EditorOnly,
    Split,
    PreviewOnly
};

// Allowed ranges (values read from disk are clamped to them).
constexpr int kMinEditorFontSize = 8;
constexpr int kMaxEditorFontSize = 32;
constexpr int kMinPreviewZoom = 50;
constexpr int kMaxPreviewZoom = 200;
constexpr int kMinTabWidth = 1;
constexpr int kMaxTabWidth = 16;
constexpr int kMinOutlineWidth = 140;  // Logical pixels (96 DPI)
constexpr int kMaxOutlineWidth = 600;
constexpr float kMinSplitRatio = 0.15f;
constexpr float kMaxSplitRatio = 0.85f;
constexpr int kMinPdfMarginMm = 5;
constexpr int kMaxPdfMarginMm = 50;

// User preferences, persisted as UTF-8 INI (%APPDATA%\Pluma\pluma.ini, or pluma.ini
// next to the executable for portable installs).
struct Settings {
    // Appearance
    Platform::AppTheme theme = Platform::AppTheme::System;
    std::wstring editorFont;          // Empty = automatic (Cascadia Code / Consolas)
    int editorFontSize = 11;          // Points
    int previewZoom = 100;            // Percent

    // Editor
    bool wordWrap = true;
    bool showLineNumbers = true;
    bool highlightCurrentLine = true;
    int tabWidth = 4;
    bool useTabs = false;
    IO::LineEnding newFileLineEnding = IO::LineEnding::CRLF;

    // View
    StartupView startupView = StartupView::Remember;
    ViewLayout lastView = ViewLayout::Split;
    bool showOutline = false;
    bool showStatusBar = true;
    bool syncScroll = true;
    int outlineWidth = 240;           // Logical pixels
    float splitRatio = 0.5f;

    // Window
    bool rememberWindow = true;
    bool hasWindowRect = false;
    int windowX = 0;
    int windowY = 0;
    int windowWidth = 0;
    int windowHeight = 0;
    bool windowMaximized = false;

    // Files
    bool reopenLastFile = false;
    std::wstring lastFile;

    // Export
    Export::PageSize pdfPageSize = Export::PageSize::A4;
    int pdfMarginMm = 20;
    Export::HtmlTheme htmlTheme = Export::HtmlTheme::Auto;

    // The layout to use when a window opens.
    ViewLayout InitialView() const noexcept;

    // Clamps every numeric value to its allowed range.
    void Sanitize();
};

// Parses INI text; unknown sections/keys and malformed values keep their defaults.
Settings ParseSettings(std::string_view iniUtf8);

// Serializes all settings as INI text (UTF-8, CRLF line endings are added by the writer).
std::string SerializeSettings(const Settings& settings);

// pluma.ini next to the executable when it exists (portable mode), otherwise %APPDATA%\Pluma\pluma.ini.
std::filesystem::path DefaultSettingsPath();

// Returns defaults when the file does not exist or cannot be read.
Settings LoadSettings(const std::filesystem::path& path);

// Writes the file atomically, creating its folder when needed.
bool SaveSettings(const std::filesystem::path& path, const Settings& settings);

} // namespace Pluma::Config
