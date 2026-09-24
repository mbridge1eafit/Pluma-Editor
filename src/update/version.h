#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Pluma::Update {

// Semantic version (MAJOR.MINOR.PATCH[-prerelease]); build metadata (+...) is ignored.
struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease; // Empty for a final release

    bool IsPrerelease() const noexcept { return !prerelease.empty(); }
};

// Accepts "1.2.3", "v1.2.3", "1.2" (patch 0) and "1.2.3-beta.1". Returns nullopt for anything else.
std::optional<Version> ParseVersion(std::string_view text);

// SemVer precedence: < 0 when a < b, 0 when equal, > 0 when a > b (1.0.0-rc.1 < 1.0.0).
int CompareVersions(const Version& a, const Version& b);

// "1.2.3" or "1.2.3-beta.1".
std::string FormatVersion(const Version& version);

} // namespace Pluma::Update
