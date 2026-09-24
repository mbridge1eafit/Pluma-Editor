#pragma once

#include <filesystem>
#include <string>

#include "http_client.h"
#include "release_info.h"

namespace Pluma::Update {

// Named mutex held by every running Pluma process; the installer waits for it to disappear
// (installer/pluma.iss, PlumaMutexName).
inline constexpr wchar_t kAppMutexName[] = L"PlumaEditorAppMutex";

// Inno Setup AppId of the installer (installer/pluma.iss). Its uninstall entry is "<AppId>_is1".
inline constexpr wchar_t kInstallerAppId[] = L"{89A612FA-5872-409B-B129-2AA12F350F83}";

// Queries GitHub for the latest published release. Blocking: call it from a worker thread.
bool FetchLatestRelease(const std::wstring& userAgent, ReleaseInfo& release, std::wstring& error);

// Downloads the release's installer to %TEMP%\Pluma\Update and verifies it against the SHA256SUMS.txt
// published with the release. A previously downloaded installer with the right checksum is reused.
// Blocking; `progress` may cancel. On cancellation returns false with an empty `error`.
bool DownloadInstaller(const ReleaseInfo& release, const std::wstring& userAgent, const ProgressCallback& progress,
                       std::filesystem::path& installer, std::wstring& error);

// True when `exePath` belongs to a copy installed by Pluma's installer (per-user). Portable copies
// (ZIP) cannot update themselves: they are pointed to the release page instead.
bool IsInstalledCopy(const std::wstring& exePath);

// Starts the installer as a silent upgrade that relaunches Pluma, reopening `reopenFile` (optional),
// when it finishes. The caller must exit right after: the installer waits for every Pluma process.
bool LaunchInstaller(const std::filesystem::path& installer, const std::filesystem::path& reopenFile,
                     std::wstring& error);

// Command line passed to the installer (exposed for tests).
std::wstring BuildInstallerArguments(const std::filesystem::path& reopenFile, const std::filesystem::path& logFile);

} // namespace Pluma::Update
