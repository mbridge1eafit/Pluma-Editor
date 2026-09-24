#include <gtest/gtest.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "update/json.h"
#include "update/release_info.h"
#include "update/sha256.h"
#include "update/updater.h"
#include "update/version.h"

using namespace Pluma::Update;

namespace {

Version V(std::string_view text) {
    const auto v = ParseVersion(text);
    EXPECT_TRUE(v.has_value()) << text;
    return v.value_or(Version{});
}

// Trimmed-down response of GET /repos/{owner}/{repo}/releases/latest.
constexpr char kReleaseJson[] = R"({
  "url": "https://api.github.com/repos/mbridge1eafit/Pluma-Editor/releases/1",
  "html_url": "https://github.com/mbridge1eafit/Pluma-Editor/releases/tag/v0.3.0",
  "id": 1,
  "tag_name": "v0.3.0",
  "name": "Pluma v0.3.0",
  "draft": false,
  "prerelease": false,
  "assets": [
    {"name": "pluma-v0.3.0-windows-x64.zip", "size": 1048576,
     "browser_download_url": "https://github.com/mbridge1eafit/Pluma-Editor/releases/download/v0.3.0/pluma-v0.3.0-windows-x64.zip"},
    {"name": "pluma-v0.3.0-setup-x64.exe", "size": 2097152, "uploader": {"login": "x", "id": 2},
     "browser_download_url": "https://github.com/mbridge1eafit/Pluma-Editor/releases/download/v0.3.0/pluma-v0.3.0-setup-x64.exe"},
    {"name": "SHA256SUMS.txt", "size": 200,
     "browser_download_url": "https://github.com/mbridge1eafit/Pluma-Editor/releases/download/v0.3.0/SHA256SUMS.txt"}
  ],
  "body": "# Pluma v0.3.0\r\n\r\nEditor Markdown.\r\n\r\n### Nuevas Caracteristicas (Features)\r\n- **update**: buscar actualizaciones (abc1234)\r\n\r\n### Verificacion de Integridad (SHA-256 Checksums)\r\n```text\r\ndeadbeef  pluma.exe\r\n```\r\n"
})";

} // namespace

// ---------------------------------------------------------------------------------------------
// Versions

TEST(UpdateVersionTest, ParsesTagsAndPlainVersions) {
    const Version v = V("v1.12.3");
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 12);
    EXPECT_EQ(v.patch, 3);
    EXPECT_FALSE(v.IsPrerelease());

    EXPECT_EQ(V("0.2").patch, 0);
    EXPECT_EQ(V(" 2.0.1+build.7 ").patch, 1);
    EXPECT_EQ(V("1.0.0-rc.1").prerelease, "rc.1");
    EXPECT_EQ(FormatVersion(V("V3.4.5-beta.2")), "3.4.5-beta.2");
}

TEST(UpdateVersionTest, RejectsMalformedVersions) {
    for (const char* text : {"", "v", "1", "1.2.3.4", "1..3", "a.b.c", "1.2.x", "1.2.3-", "1.2.3-a..b",
                             "-1.2.3", "1.2.3-ñ", "1234567890.0.0"}) {
        EXPECT_FALSE(ParseVersion(text).has_value()) << text;
    }
}

TEST(UpdateVersionTest, ComparesWithSemverPrecedence) {
    EXPECT_LT(CompareVersions(V("0.2.0"), V("0.10.0")), 0); // Numeric, not lexical
    EXPECT_GT(CompareVersions(V("1.0.0"), V("0.99.99")), 0);
    EXPECT_EQ(CompareVersions(V("v1.2"), V("1.2.0")), 0);
    EXPECT_LT(CompareVersions(V("1.0.0-rc.1"), V("1.0.0")), 0);
    EXPECT_LT(CompareVersions(V("1.0.0-alpha"), V("1.0.0-alpha.1")), 0);
    EXPECT_LT(CompareVersions(V("1.0.0-alpha.1"), V("1.0.0-alpha.beta")), 0);
    EXPECT_LT(CompareVersions(V("1.0.0-beta.2"), V("1.0.0-beta.11")), 0);
    EXPECT_LT(CompareVersions(V("1.0.0-beta.11"), V("1.0.0-rc.1")), 0);
}

// ---------------------------------------------------------------------------------------------
// JSON

TEST(UpdateJsonTest, ParsesNestedDocuments) {
    const auto doc = ParseJson(R"( {"a": [1, -2.5e2, true, false, null, {"b": "c"}], "d": {}} )");
    ASSERT_TRUE(doc.has_value());
    ASSERT_TRUE(doc->IsObject());
    const JsonValue* a = doc->Find("a");
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->items.size(), 6u);
    EXPECT_DOUBLE_EQ(a->items[0].number, 1.0);
    EXPECT_DOUBLE_EQ(a->items[1].number, -250.0);
    EXPECT_TRUE(a->items[2].boolean);
    EXPECT_EQ(a->items[4].type, JsonValue::Type::Null);
    EXPECT_EQ(a->items[5].GetString("b"), "c");
    EXPECT_TRUE(doc->Find("d")->IsObject());
    EXPECT_EQ(doc->Find("missing"), nullptr);
    EXPECT_EQ(doc->GetString("a", "fallback"), "fallback"); // Wrong type
}

TEST(UpdateJsonTest, DecodesEscapesToUtf8) {
    const auto doc = ParseJson(R"("tab\t \"q\" \\ \/ \u00f1 \u20AC \ud83d\ude00")");
    ASSERT_TRUE(doc.has_value());
    EXPECT_EQ(doc->string, "tab\t \"q\" \\ / \xC3\xB1 \xE2\x82\xAC \xF0\x9F\x98\x80");
}

TEST(UpdateJsonTest, RejectsInvalidDocuments) {
    for (const char* text : {"", "{", "[1,]", "{\"a\" 1}", "{\"a\":1,}", "tru", "01", "1.", "\"a", "\"\\x\"",
                             "\"\\ud800\"", "\"line\nbreak\"", "[1] 2", "{'a': 1}", "-", "1e"}) {
        EXPECT_FALSE(ParseJson(text).has_value()) << text;
    }
}

TEST(UpdateJsonTest, LimitsNestingDepth) {
    const std::string deep = std::string(100, '[') + std::string(100, ']');
    EXPECT_FALSE(ParseJson(deep).has_value());
    EXPECT_TRUE(ParseJson(deep, 200).has_value());
}

// ---------------------------------------------------------------------------------------------
// Releases

TEST(UpdateReleaseTest, ParsesGitHubRelease) {
    const auto release = ParseRelease(kReleaseJson);
    ASSERT_TRUE(release.has_value());
    EXPECT_EQ(release->tag, "v0.3.0");
    EXPECT_EQ(FormatVersion(release->version), "0.3.0");
    EXPECT_EQ(release->pageUrl, "https://github.com/mbridge1eafit/Pluma-Editor/releases/tag/v0.3.0");
    EXPECT_FALSE(release->draft);
    ASSERT_EQ(release->assets.size(), 3u);

    const ReleaseAsset* setup = release->FindInstaller();
    ASSERT_NE(setup, nullptr);
    EXPECT_EQ(setup->name, "pluma-v0.3.0-setup-x64.exe");
    EXPECT_EQ(setup->size, 2097152u);
    ASSERT_NE(release->FindAsset("sha256sums.txt"), nullptr); // Case-insensitive
    EXPECT_EQ(release->FindAsset("missing.txt"), nullptr);
}

TEST(UpdateReleaseTest, RejectsNonVersionTagsAndInvalidJson) {
    EXPECT_FALSE(ParseRelease(R"({"tag_name": "nightly"})").has_value());
    EXPECT_FALSE(ParseRelease(R"([])").has_value());
    EXPECT_FALSE(ParseRelease("<html>rate limited</html>").has_value());
}

TEST(UpdateReleaseTest, OffersOnlyNewerFinalReleases) {
    ReleaseInfo release = *ParseRelease(kReleaseJson);
    EXPECT_TRUE(ShouldOfferUpdate(V("0.2.0"), release, "", false));
    EXPECT_FALSE(ShouldOfferUpdate(V("0.3.0"), release, "", true));  // Same version
    EXPECT_FALSE(ShouldOfferUpdate(V("0.4.0"), release, "", true));  // Newer local build

    // A skipped version is not offered automatically, but a manual check shows it.
    EXPECT_FALSE(ShouldOfferUpdate(V("0.2.0"), release, "v0.3.0", false));
    EXPECT_TRUE(ShouldOfferUpdate(V("0.2.0"), release, "v0.3.0", true));
    EXPECT_TRUE(ShouldOfferUpdate(V("0.2.0"), release, "v0.2.5", false));

    release.prerelease = true;
    EXPECT_FALSE(ShouldOfferUpdate(V("0.2.0"), release, "", true));
    release.prerelease = false;
    release.draft = true;
    EXPECT_FALSE(ShouldOfferUpdate(V("0.2.0"), release, "", true));
}

TEST(UpdateReleaseTest, ChecksAtMostOncePerInterval) {
    const int64_t now = 1'800'000'000;
    EXPECT_TRUE(IsUpdateCheckDue(0, now));
    EXPECT_FALSE(IsUpdateCheckDue(now - 60, now));
    EXPECT_TRUE(IsUpdateCheckDue(now - kUpdateCheckIntervalSeconds, now));
    EXPECT_TRUE(IsUpdateCheckDue(now + 3600, now)); // Clock moved backwards
}

TEST(UpdateReleaseTest, FindsChecksumsBySha256SumsFormat) {
    const std::string hashA(64, 'A');
    const std::string hashB = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    const std::string sums = "\xEF\xBB\xBF" + hashA + "  pluma-v0.3.0-windows-x64.zip\r\n" + hashB +
                             " *pluma-v0.3.0-setup-x64.exe\r\nnot a checksum line\r\n";
    EXPECT_EQ(FindChecksum(sums, "pluma-v0.3.0-windows-x64.zip"), std::string(64, 'a'));
    EXPECT_EQ(FindChecksum(sums, "PLUMA-v0.3.0-setup-x64.exe"), hashB);
    EXPECT_FALSE(FindChecksum(sums, "pluma.exe").has_value());
    EXPECT_FALSE(FindChecksum("xyz  pluma.exe", "pluma.exe").has_value());
}

TEST(UpdateReleaseTest, ValidatesDownloadFileNames) {
    EXPECT_TRUE(IsSafeFileName("pluma-v0.3.0-setup-x64.exe"));
    for (const char* name : {"", ".", "..", "..\\evil.exe", "a/b.exe", "c:evil.exe", "name.", "a*b", "a\tb"}) {
        EXPECT_FALSE(IsSafeFileName(name)) << name;
    }
}

TEST(UpdateReleaseTest, SummarizesReleaseNotes) {
    const auto release = ParseRelease(kReleaseJson);
    ASSERT_TRUE(release.has_value());
    EXPECT_EQ(SummarizeReleaseNotes(release->notes),
              "Editor Markdown.\n\nNuevas Caracteristicas (Features)\n\xE2\x80\xA2 update: buscar actualizaciones");

    std::string longNotes;
    for (int i = 0; i < 30; ++i) longNotes += "- item " + std::to_string(i) + "\n";
    const std::string summary = SummarizeReleaseNotes(longNotes, 5);
    EXPECT_EQ(summary.substr(summary.size() - 3), "\xE2\x80\xA6"); // Ends with an ellipsis
    EXPECT_EQ(std::count(summary.begin(), summary.end(), '\n'), 5);
}

// ---------------------------------------------------------------------------------------------
// Hashing and installer command line

TEST(UpdateSha256Test, MatchesKnownVectors) {
    EXPECT_EQ(Sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(Sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    const auto path = std::filesystem::temp_directory_path() / L"pluma_sha256_test.bin";
    {
        std::ofstream out(path, std::ios::binary);
        const std::string block(100000, 'a');
        for (int i = 0; i < 10; ++i) out << block; // One million 'a' (NIST vector)
    }
    EXPECT_EQ(Sha256HexOfFile(path), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    std::filesystem::remove(path);
    EXPECT_FALSE(Sha256HexOfFile(path).has_value());
}

TEST(UpdateInstallerTest, BuildsSilentUpgradeCommandLine) {
    EXPECT_EQ(BuildInstallerArguments({}, {}), L"/SILENT /SUPPRESSMSGBOXES /NORESTART /SP- /PLUMAUPDATE=1");
    EXPECT_EQ(BuildInstallerArguments(L"C:\\Mis documentos\\notas.md", L"C:\\Temp\\setup.log"),
              L"/SILENT /SUPPRESSMSGBOXES /NORESTART /SP- /PLUMAUPDATE=1 /LOG=\"C:\\Temp\\setup.log\" "
              L"/PLUMAOPEN=\"C:\\Mis documentos\\notas.md\"");
}

TEST(UpdateInstallerTest, PortableCopiesAreNotInstalled) {
    // unit_tests.exe was never installed by the setup.
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    EXPECT_FALSE(IsInstalledCopy(exe));
}
