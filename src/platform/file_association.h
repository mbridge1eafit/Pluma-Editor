#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>

namespace Pluma::Platform {

// Markdown extensions handled by Pluma.
inline constexpr const wchar_t* kMarkdownExtensions[] = {L".md", L".markdown", L".mdown"};

// Full path of the running executable.
std::wstring GetExecutablePath();

// Registers Pluma for the current user (HKCU, no elevation): ProgID Pluma.Markdown, "Open with"
// entries, and the Default Apps capabilities. Does not change a default the user already chose.
// Not for the Store package: it declares its file types in its manifest, and the registry writes of
// a packaged app are private to it.
bool RegisterMarkdownHandler(const std::wstring& exePath);

// True when Windows opens .md files with Pluma: with `exePath`, or with this packaged app (compared
// by Application User Model ID) when running from the Store package.
bool IsDefaultMarkdownHandler(const std::wstring& exePath);

// "ms-settings:defaultapps" URI that opens Pluma's own page in Settings > Default apps: by its
// per-user RegisteredApplications name, or by `appUserModelId` when it is not empty (packaged app).
std::wstring DefaultAppsSettingsUri(const std::wstring& appUserModelId);

// Windows protects the user's default-app choice: it can only be changed through the system UI.
// Opens Settings > Default apps (Pluma's own page on recent Windows 11). Non-blocking.
// Returns false when Settings could not be launched.
bool OpenDefaultAppsSettings(HWND owner);

} // namespace Pluma::Platform
