#include <gtest/gtest.h>
#include "platform/dpi.h"
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

