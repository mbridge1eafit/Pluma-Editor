#include <gtest/gtest.h>
#include <windows.h>
#include <commctrl.h>
#include "platform/dpi.h"
#include "platform/icon_font.h"
#include "platform/theme.h"

TEST(PlatformDpiTest, ScaleForDpiCalculations) {
    // 96 DPI corresponds to 100% scale
    EXPECT_EQ(Pluma::Platform::ScaleForDpi(100, 96), 100);
    EXPECT_EQ(Pluma::Platform::ScaleForDpi(1024, 96), 1024);

    // 144 DPI corresponds to 150% scale
    EXPECT_EQ(Pluma::Platform::ScaleForDpi(100, 144), 150);
    EXPECT_EQ(Pluma::Platform::ScaleForDpi(1024, 144), 1536);

    // 192 DPI corresponds to 200% scale
    EXPECT_EQ(Pluma::Platform::ScaleForDpi(100, 192), 200);
    EXPECT_EQ(Pluma::Platform::ScaleForDpi(720, 192), 1440);
}

TEST(PlatformDpiTest, NullWindowFallback) {
    // Passing null HWND should safely fallback to default 96 DPI
    EXPECT_EQ(Pluma::Platform::GetWindowDpi(nullptr), 96u);
}

TEST(PlatformThemeTest, SystemThemeQueryDoesNotCrash) {
    // Querying the system theme should execute and return a valid boolean without errors
    bool isDark = Pluma::Platform::IsSystemDarkMode();
    // Valid result is either true or false
    EXPECT_TRUE(isDark == true || isDark == false);
}

TEST(PlatformThemeTest, ThemeModeResolution) {
    EXPECT_TRUE(Pluma::Platform::IsDarkModeActive(Pluma::Platform::AppTheme::Dark));
    EXPECT_FALSE(Pluma::Platform::IsDarkModeActive(Pluma::Platform::AppTheme::Light));
    EXPECT_EQ(Pluma::Platform::IsDarkModeActive(Pluma::Platform::AppTheme::System),
              Pluma::Platform::IsSystemDarkMode());
}


TEST(PlatformIconFontTest, GlyphStripFillsAnImageList) {
    constexpr int kSize = 16;
    const Pluma::Platform::GlyphColor glyphs[] = {
        {Pluma::Platform::Glyph::kFolderClosed, RGB(200, 150, 30)},
        {Pluma::Platform::Glyph::kFile, RGB(60, 120, 180)},
        {L"", RGB(0, 0, 0)}, // Blank
    };
    HBITMAP mask = nullptr;
    HBITMAP strip = Pluma::Platform::CreateGlyphStrip(glyphs, kSize, &mask);
    ASSERT_NE(strip, nullptr);
    ASSERT_NE(mask, nullptr);

    BITMAP info{};
    ASSERT_NE(GetObjectW(strip, sizeof(info), &info), 0);
    EXPECT_EQ(info.bmWidth, kSize * 3);
    EXPECT_EQ(info.bmHeight, kSize);
    EXPECT_EQ(info.bmBitsPixel, 32);

    // Drawn glyphs have opaque pixels in their color (premultiplied); the blank image has none.
    const auto* pixels = static_cast<const DWORD*>(info.bmBits);
    int opaque[3] = {};
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize * 3; ++x) {
            if ((pixels[y * kSize * 3 + x] >> 24) == 0xFF) ++opaque[x / kSize];
        }
    }
    EXPECT_GT(opaque[0], 0);
    EXPECT_GT(opaque[1], 0);
    EXPECT_EQ(opaque[2], 0);

    HIMAGELIST images = ImageList_Create(kSize, kSize, ILC_COLOR32 | ILC_MASK, 3, 0);
    ASSERT_NE(images, nullptr);
    EXPECT_EQ(ImageList_Add(images, strip, mask), 0);
    EXPECT_EQ(ImageList_GetImageCount(images), 3);
    ImageList_Destroy(images);
    DeleteObject(strip);
    DeleteObject(mask);
}
