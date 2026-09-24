#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "version.h"

namespace Pluma::Update {

// GitHub repository that publishes Pluma's releases.
inline constexpr wchar_t kLatestReleaseApiUrl[] =
    L"https://api.github.com/repos/mbridge1eafit/Pluma-Editor/releases/latest";
inline constexpr wchar_t kReleasesPageUrl[] = L"https://github.com/mbridge1eafit/Pluma-Editor/releases";

// Published asset names (see scripts/package_release.ps1).
inline constexpr char kChecksumsAssetName[] = "SHA256SUMS.txt";

// Minimum time between two automatic checks.
inline constexpr int64_t kUpdateCheckIntervalSeconds = 24 * 60 * 60;

struct ReleaseAsset {
    std::string name;
    std::string downloadUrl; // browser_download_url
    uint64_t size = 0;
};

struct ReleaseInfo {
    std::string tag;     // "v0.3.0"
    Version version;
    std::string name;    // "Pluma v0.3.0"
    std::string notes;   // Markdown body
    std::string pageUrl; // html_url
    bool draft = false;
    bool prerelease = false;
    std::vector<ReleaseAsset> assets;

    const ReleaseAsset* FindAsset(std::string_view assetName) const;
    // The Windows installer: "pluma-<version>-setup-x64.exe".
    const ReleaseAsset* FindInstaller() const;
};

// Parses a GitHub "release" object (GET /repos/{owner}/{repo}/releases/latest). Returns nullopt when
// the JSON is invalid or the tag is not a version.
std::optional<ReleaseInfo> ParseRelease(std::string_view json);

// True when `release` should be offered to a user running `current`: a newer, published, final release.
// Automatic checks also skip the version the user chose to ignore (`skippedTag`).
bool ShouldOfferUpdate(const Version& current, const ReleaseInfo& release, std::string_view skippedTag,
                       bool manualCheck);

// True when the last automatic check is older than the interval (or in the future: clock changes).
bool IsUpdateCheckDue(int64_t lastCheckUnix, int64_t nowUnix);

// Looks `fileName` up in a SHA256SUMS file ("<hex>  <name>" or "<hex> *<name>" lines).
// Returns the lower-case hex digest.
std::optional<std::string> FindChecksum(std::string_view sha256sums, std::string_view fileName);

// A plain file name that is safe to create inside a download folder (no paths, no reserved characters).
bool IsSafeFileName(std::string_view name);

// Readable plain-text excerpt of Markdown release notes for a dialog: headings and list markers are
// simplified, code blocks and the checksum/installation sections are dropped. At most `maxLines` lines.
std::string SummarizeReleaseNotes(std::string_view markdown, size_t maxLines = 18);

} // namespace Pluma::Update
