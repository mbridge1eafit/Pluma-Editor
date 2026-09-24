#include "version.h"

#include <charconv>
#include <vector>

namespace Pluma::Update {

namespace {

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

bool ParseNumber(std::string_view text, int& out) {
    if (text.empty() || text.size() > 9) return false;
    for (char c : text) {
        if (!IsDigit(c)) return false;
    }
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
    return ec == std::errc{} && ptr == text.data() + text.size();
}

std::vector<std::string_view> Split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    for (;;) {
        const size_t pos = text.find(separator);
        parts.push_back(text.substr(0, pos));
        if (pos == std::string_view::npos) break;
        text.remove_prefix(pos + 1);
    }
    return parts;
}

bool IsNumeric(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!IsDigit(c)) return false;
    }
    return true;
}

// Compares dot-separated pre-release identifiers: numeric ones numerically and below alphanumeric ones.
int ComparePrerelease(std::string_view a, std::string_view b) {
    const auto pa = Split(a, '.');
    const auto pb = Split(b, '.');
    for (size_t i = 0; i < pa.size() && i < pb.size(); ++i) {
        const bool na = IsNumeric(pa[i]);
        const bool nb = IsNumeric(pb[i]);
        if (na && nb) {
            if (pa[i].size() != pb[i].size()) return pa[i].size() < pb[i].size() ? -1 : 1;
            const int cmp = pa[i].compare(pb[i]);
            if (cmp != 0) return cmp < 0 ? -1 : 1;
        } else if (na != nb) {
            return na ? -1 : 1;
        } else {
            const int cmp = pa[i].compare(pb[i]);
            if (cmp != 0) return cmp < 0 ? -1 : 1;
        }
    }
    if (pa.size() == pb.size()) return 0;
    return pa.size() < pb.size() ? -1 : 1;
}

} // namespace

std::optional<Version> ParseVersion(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) text.remove_prefix(1);

    if (const size_t plus = text.find('+'); plus != std::string_view::npos) text = text.substr(0, plus);

    Version v;
    if (const size_t dash = text.find('-'); dash != std::string_view::npos) {
        const std::string_view pre = text.substr(dash + 1);
        if (pre.empty()) return std::nullopt;
        for (std::string_view id : Split(pre, '.')) {
            if (id.empty()) return std::nullopt;
            for (char c : id) {
                const bool ok = IsDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
                if (!ok) return std::nullopt;
            }
        }
        v.prerelease = std::string(pre);
        text = text.substr(0, dash);
    }

    const auto parts = Split(text, '.');
    if (parts.size() < 2 || parts.size() > 3) return std::nullopt;
    if (!ParseNumber(parts[0], v.major) || !ParseNumber(parts[1], v.minor)) return std::nullopt;
    if (parts.size() == 3 && !ParseNumber(parts[2], v.patch)) return std::nullopt;
    return v;
}

int CompareVersions(const Version& a, const Version& b) {
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    if (a.prerelease.empty() || b.prerelease.empty()) {
        if (a.prerelease.empty() == b.prerelease.empty()) return 0;
        return a.prerelease.empty() ? 1 : -1; // A final release outranks its pre-releases
    }
    return ComparePrerelease(a.prerelease, b.prerelease);
}

std::string FormatVersion(const Version& version) {
    std::string text = std::to_string(version.major) + "." + std::to_string(version.minor) + "." +
                       std::to_string(version.patch);
    if (!version.prerelease.empty()) text += "-" + version.prerelease;
    return text;
}

} // namespace Pluma::Update
