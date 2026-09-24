#include <gtest/gtest.h>

#include <filesystem>

#include "config/settings.h"

using namespace Pluma::Config;

TEST(SettingsTest, EmptyInputGivesDefaults) {
    const Settings s = ParseSettings("");
    const Settings defaults;
    EXPECT_EQ(s.theme, defaults.theme);
    EXPECT_EQ(s.editorFontSize, 11);
    EXPECT_EQ(s.previewZoom, 100);
    EXPECT_TRUE(s.wordWrap);
    EXPECT_TRUE(s.showLineNumbers);
    EXPECT_FALSE(s.showOutline);
    EXPECT_FALSE(s.hasWindowRect);
    EXPECT_EQ(s.startupView, StartupView::Remember);
    EXPECT_EQ(s.InitialView(), ViewLayout::Split);
}

TEST(SettingsTest, RoundTripPreservesEveryValue) {
    Settings s;
    s.theme = Pluma::Platform::AppTheme::Dark;
    s.editorFont = L"Fira Code Ñandú";
    s.editorFontSize = 14;
    s.previewZoom = 125;
    s.wordWrap = false;
    s.showLineNumbers = false;
    s.highlightCurrentLine = false;
    s.tabWidth = 2;
    s.useTabs = true;
    s.newFileLineEnding = Pluma::IO::LineEnding::LF;
    s.startupView = StartupView::PreviewOnly;
    s.lastView = ViewLayout::EditorOnly;
    s.showOutline = true;
    s.showStatusBar = false;
    s.syncScroll = false;
    s.outlineWidth = 300;
    s.splitRatio = 0.625f;
    s.rememberWindow = true;
    s.hasWindowRect = true;
    s.windowX = -1200;
    s.windowY = 40;
    s.windowWidth = 1280;
    s.windowHeight = 800;
    s.windowMaximized = true;
    s.reopenLastFile = true;
    s.lastFile = L"C:\\Documentos\\Notas = ideas.md";
    s.pdfPageSize = Pluma::Export::PageSize::Letter;
    s.pdfMarginMm = 15;
    s.htmlTheme = Pluma::Export::HtmlTheme::Dark;
    s.checkForUpdates = false;
    s.lastUpdateCheck = 4102444800LL; // Beyond 2038: needs 64 bits
    s.skippedVersion = "v0.3.0";

    const Settings r = ParseSettings(SerializeSettings(s));
    EXPECT_EQ(r.theme, s.theme);
    EXPECT_EQ(r.editorFont, s.editorFont);
    EXPECT_EQ(r.editorFontSize, s.editorFontSize);
    EXPECT_EQ(r.previewZoom, s.previewZoom);
    EXPECT_EQ(r.wordWrap, s.wordWrap);
    EXPECT_EQ(r.showLineNumbers, s.showLineNumbers);
    EXPECT_EQ(r.highlightCurrentLine, s.highlightCurrentLine);
    EXPECT_EQ(r.tabWidth, s.tabWidth);
    EXPECT_EQ(r.useTabs, s.useTabs);
    EXPECT_EQ(r.newFileLineEnding, s.newFileLineEnding);
    EXPECT_EQ(r.startupView, s.startupView);
    EXPECT_EQ(r.lastView, s.lastView);
    EXPECT_EQ(r.showOutline, s.showOutline);
    EXPECT_EQ(r.showStatusBar, s.showStatusBar);
    EXPECT_EQ(r.syncScroll, s.syncScroll);
    EXPECT_EQ(r.outlineWidth, s.outlineWidth);
    EXPECT_FLOAT_EQ(r.splitRatio, s.splitRatio);
    EXPECT_TRUE(r.hasWindowRect);
    EXPECT_EQ(r.windowX, s.windowX);
    EXPECT_EQ(r.windowY, s.windowY);
    EXPECT_EQ(r.windowWidth, s.windowWidth);
    EXPECT_EQ(r.windowHeight, s.windowHeight);
    EXPECT_EQ(r.windowMaximized, s.windowMaximized);
    EXPECT_EQ(r.reopenLastFile, s.reopenLastFile);
    EXPECT_EQ(r.lastFile, s.lastFile);
    EXPECT_EQ(r.pdfPageSize, s.pdfPageSize);
    EXPECT_EQ(r.pdfMarginMm, s.pdfMarginMm);
    EXPECT_EQ(r.htmlTheme, s.htmlTheme);
    EXPECT_EQ(r.checkForUpdates, s.checkForUpdates);
    EXPECT_EQ(r.lastUpdateCheck, s.lastUpdateCheck);
    EXPECT_EQ(r.skippedVersion, s.skippedVersion);
    EXPECT_EQ(r.InitialView(), ViewLayout::PreviewOnly);
}

TEST(SettingsTest, UpdateDefaultsAndInvalidValues) {
    const Settings defaults = ParseSettings("");
    EXPECT_TRUE(defaults.checkForUpdates);
    EXPECT_EQ(defaults.lastUpdateCheck, 0);
    EXPECT_TRUE(defaults.skippedVersion.empty());

    const Settings s = ParseSettings("[Updates]\nCheckAutomatically=no\nLastCheck=-5\nSkippedVersion=v1 v2\n");
    EXPECT_FALSE(s.checkForUpdates);
    EXPECT_EQ(s.lastUpdateCheck, 0);
    EXPECT_TRUE(s.skippedVersion.empty());
}

TEST(SettingsTest, OutOfRangeValuesAreClamped) {
    const Settings s = ParseSettings(
        "[Appearance]\nEditorFontSize=200\nPreviewZoom=5\n"
        "[Editor]\nTabWidth=0\n"
        "[View]\nOutlineWidth=99999\nSplitRatio=3.5\n"
        "[Export]\nPdfMarginMm=-4\n");
    EXPECT_EQ(s.editorFontSize, kMaxEditorFontSize);
    EXPECT_EQ(s.previewZoom, kMinPreviewZoom);
    EXPECT_EQ(s.tabWidth, kMinTabWidth);
    EXPECT_EQ(s.outlineWidth, kMaxOutlineWidth);
    EXPECT_FLOAT_EQ(s.splitRatio, kMaxSplitRatio);
    EXPECT_EQ(s.pdfMarginMm, kMinPdfMarginMm);
}

TEST(SettingsTest, MalformedValuesKeepDefaults) {
    const Settings s = ParseSettings(
        "\xEF\xBB\xBF; comentario\r\n"
        "[appearance]\r\n"
        "theme = purple\r\n"
        "EditorFontSize=12px\r\n"
        "[EDITOR]\r\n"
        "WordWrap=maybe\r\n"
        "LineNumbers = OFF\r\n"
        "sin signo igual\r\n"
        "[Window]\r\nWidth=800\r\n"); // Height missing: no window rectangle
    EXPECT_EQ(s.theme, Pluma::Platform::AppTheme::System);
    EXPECT_EQ(s.editorFontSize, 11);
    EXPECT_TRUE(s.wordWrap);
    EXPECT_FALSE(s.showLineNumbers); // Keys and sections are case-insensitive
    EXPECT_FALSE(s.hasWindowRect);
}

TEST(SettingsTest, SaveAndLoadFile) {
    const auto dir = std::filesystem::temp_directory_path() / L"pluma_settings_test";
    std::filesystem::remove_all(dir);
    const auto path = dir / L"sub" / L"pluma.ini";

    Settings s;
    s.previewZoom = 150;
    s.showOutline = true;
    ASSERT_TRUE(SaveSettings(path, s)); // Creates the missing folders

    const Settings loaded = LoadSettings(path);
    EXPECT_EQ(loaded.previewZoom, 150);
    EXPECT_TRUE(loaded.showOutline);

    const Settings missing = LoadSettings(dir / L"no_existe.ini");
    EXPECT_EQ(missing.previewZoom, 100);
    std::filesystem::remove_all(dir);
}
