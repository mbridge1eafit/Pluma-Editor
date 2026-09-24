#include "release_info.h"

#include "json.h"

namespace Pluma::Update {

namespace {

char ToLowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

std::string ToLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = ToLowerAscii(c);
    return out;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (ToLowerAscii(a[i]) != ToLowerAscii(b[i])) return false;
    }
    return true;
}

bool StartsWith(std::string_view s, std::string_view prefix) { return s.substr(0, prefix.size()) == prefix; }

bool EndsWithIgnoreCase(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && EqualsIgnoreCase(s.substr(s.size() - suffix.size()), suffix);
}

bool IsHex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

std::string_view Trim(std::string_view s) {
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && isSpace(s.front())) s.remove_prefix(1);
    while (!s.empty() && isSpace(s.back())) s.remove_suffix(1);
    return s;
}

// Removes Markdown emphasis and code markers ("**", "__", "`").
std::string StripInlineMarkup(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '`') continue;
        if ((s[i] == '*' || s[i] == '_') && i + 1 < s.size() && s[i + 1] == s[i]) {
            ++i;
            continue;
        }
        out += s[i];
    }
    return out;
}

// Drops a trailing commit reference such as " (1811ed2)".
std::string_view StripCommitHash(std::string_view s) {
    if (s.size() < 10 || s.back() != ')') return s;
    const size_t open = s.rfind('(');
    if (open == std::string_view::npos || open == 0 || s[open - 1] != ' ') return s;
    const std::string_view hash = s.substr(open + 1, s.size() - open - 2);
    if (hash.size() < 7 || hash.size() > 40) return s;
    for (char c : hash) {
        if (!IsHex(c)) return s;
    }
    return Trim(s.substr(0, open));
}

// Sections of the generated notes that are not useful in the update dialog.
bool IsSkippedSection(std::string_view heading) {
    const std::string lower = ToLower(heading);
    for (const char* keyword : {"sha-256", "checksum", "verificaci", "instrucciones", "instructions"}) {
        if (lower.find(keyword) != std::string::npos) return true;
    }
    return false;
}

} // namespace

const ReleaseAsset* ReleaseInfo::FindAsset(std::string_view assetName) const {
    for (const ReleaseAsset& asset : assets) {
        if (EqualsIgnoreCase(asset.name, assetName)) return &asset;
    }
    return nullptr;
}

const ReleaseAsset* ReleaseInfo::FindInstaller() const {
    for (const ReleaseAsset& asset : assets) {
        const std::string lower = ToLower(asset.name);
        if (StartsWith(lower, "pluma-") && EndsWithIgnoreCase(lower, "-setup-x64.exe")) return &asset;
    }
    return nullptr;
}

std::optional<ReleaseInfo> ParseRelease(std::string_view json) {
    const std::optional<JsonValue> root = ParseJson(json);
    if (!root || !root->IsObject()) return std::nullopt;

    ReleaseInfo info;
    info.tag = root->GetString("tag_name");
    const std::optional<Version> version = ParseVersion(info.tag);
    if (!version) return std::nullopt;
    info.version = *version;
    info.name = root->GetString("name", info.tag);
    info.notes = root->GetString("body");
    info.pageUrl = root->GetString("html_url");
    info.draft = root->GetBool("draft");
    info.prerelease = root->GetBool("prerelease");

    if (const JsonValue* assets = root->Find("assets"); assets && assets->IsArray()) {
        for (const JsonValue& item : assets->items) {
            if (!item.IsObject()) continue;
            ReleaseAsset asset;
            asset.name = item.GetString("name");
            asset.downloadUrl = item.GetString("browser_download_url");
            const double size = item.GetNumber("size");
            asset.size = size > 0 ? static_cast<uint64_t>(size) : 0;
            if (!asset.name.empty() && !asset.downloadUrl.empty()) info.assets.push_back(std::move(asset));
        }
    }
    return info;
}

bool ShouldOfferUpdate(const Version& current, const ReleaseInfo& release, std::string_view skippedTag,
                       bool manualCheck) {
    if (release.draft || release.prerelease || release.version.IsPrerelease()) return false;
    if (CompareVersions(release.version, current) <= 0) return false;
    if (!manualCheck && !skippedTag.empty()) {
        const std::optional<Version> skipped = ParseVersion(skippedTag);
        if (skipped && CompareVersions(*skipped, release.version) == 0) return false;
    }
    return true;
}

bool IsUpdateCheckDue(int64_t lastCheckUnix, int64_t nowUnix) {
    if (lastCheckUnix <= 0 || lastCheckUnix > nowUnix) return true;
    return nowUnix - lastCheckUnix >= kUpdateCheckIntervalSeconds;
}

std::optional<std::string> FindChecksum(std::string_view sha256sums, std::string_view fileName) {
    if (sha256sums.size() >= 3 && static_cast<unsigned char>(sha256sums[0]) == 0xEF &&
        static_cast<unsigned char>(sha256sums[1]) == 0xBB && static_cast<unsigned char>(sha256sums[2]) == 0xBF) {
        sha256sums.remove_prefix(3);
    }
    while (!sha256sums.empty()) {
        const size_t eol = sha256sums.find('\n');
        const std::string_view line = Trim(sha256sums.substr(0, eol));
        sha256sums = (eol == std::string_view::npos) ? std::string_view{} : sha256sums.substr(eol + 1);

        if (line.size() < 66) continue;
        const std::string_view hash = line.substr(0, 64);
        bool validHash = true;
        for (char c : hash) validHash = validHash && IsHex(c);
        if (!validHash || (line[64] != ' ' && line[64] != '\t')) continue;

        std::string_view name = Trim(line.substr(64));
        if (!name.empty() && name.front() == '*') name.remove_prefix(1); // Binary-mode marker
        if (EqualsIgnoreCase(name, fileName)) return ToLower(hash);
    }
    return std::nullopt;
}

bool IsSafeFileName(std::string_view name) {
    if (name.empty() || name.size() > 200 || name == "." || name == "..") return false;
    if (name.back() == '.' || name.back() == ' ') return false;
    for (char c : name) {
        if (static_cast<unsigned char>(c) < 0x20) return false;
        switch (c) {
        case '<': case '>': case ':': case '"': case '/': case '\\': case '|': case '?': case '*':
            return false;
        default:
            break;
        }
    }
    return true;
}

std::string SummarizeReleaseNotes(std::string_view markdown, size_t maxLines) {
    std::vector<std::string> lines;
    bool inFence = false;
    bool skipping = false;
    bool truncated = false;

    while (!markdown.empty()) {
        const size_t eol = markdown.find('\n');
        const std::string_view raw = markdown.substr(0, eol);
        markdown = (eol == std::string_view::npos) ? std::string_view{} : markdown.substr(eol + 1);
        const std::string_view line = Trim(raw);

        if (StartsWith(line, "```") || StartsWith(line, "~~~")) {
            inFence = !inFence;
            continue;
        }
        if (inFence) continue;

        std::string text;
        if (StartsWith(line, "#")) {
            size_t level = 0;
            while (level < line.size() && line[level] == '#') ++level;
            const std::string_view heading = Trim(line.substr(level));
            if (level == 1) continue; // The title repeats the version shown by the dialog
            skipping = IsSkippedSection(heading);
            if (skipping) continue;
            if (!lines.empty() && !lines.back().empty()) lines.emplace_back();
            text = StripInlineMarkup(heading);
        } else {
            if (skipping) continue;
            if (StartsWith(line, "- ") || StartsWith(line, "* ") || StartsWith(line, "+ ")) {
                text = "\xE2\x80\xA2 " + StripInlineMarkup(StripCommitHash(Trim(line.substr(2)))); // "• "
            } else {
                text = StripInlineMarkup(StripCommitHash(line));
            }
        }

        if (text.empty() && (lines.empty() || lines.back().empty())) continue;
        if (lines.size() >= maxLines) {
            truncated = !text.empty() || truncated;
            if (!text.empty()) break;
            continue;
        }
        lines.push_back(std::move(text));
    }

    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += '\n';
        result += lines[i];
    }
    if (truncated) result += "\n\xE2\x80\xA6"; // "…"
    return result;
}

} // namespace Pluma::Update
