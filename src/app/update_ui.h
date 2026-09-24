#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <filesystem>
#include <string>

#include "../update/release_info.h"

namespace Pluma::App {

// Result of a background update check, posted to the window as the LPARAM of the message given to
// StartUpdateCheck. The receiver owns it (delete / unique_ptr).
struct UpdateCheckResult {
    bool manual = false; // Requested from the menu: report "up to date" and errors too
    bool ok = false;
    std::wstring error;
    Update::ReleaseInfo release;
};

// Queries the latest release on a worker thread and posts `message` to `target` when done.
void StartUpdateCheck(HWND target, UINT message, bool manual, const std::wstring& userAgent);

enum class UpdateChoice {
    Install,  // Download and run the installer
    OpenPage, // Portable copy: open the release page
    Later,
    Skip      // Do not offer this version again automatically
};

// "Nueva versión disponible" dialog with the release notes and the available choices.
UpdateChoice ShowUpdateAvailableDialog(HWND owner, HINSTANCE hInstance, const Update::ReleaseInfo& release,
                                       const std::wstring& currentVersion, bool installedCopy);

// Downloads and verifies the installer behind a progress dialog that can be cancelled.
// Returns false with an empty `error` when the user cancels.
bool DownloadInstallerWithProgress(HWND owner, HINSTANCE hInstance, const Update::ReleaseInfo& release,
                                   const std::wstring& userAgent, std::filesystem::path& installer,
                                   std::wstring& error);

// Opens an https:// URL in the default browser.
void OpenUrl(HWND owner, const std::wstring& url);

} // namespace Pluma::App
