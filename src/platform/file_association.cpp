#include "file_association.h"

#include <shellapi.h>
#include <shlobj.h>  // SHChangeNotify
#include <shlwapi.h> // AssocQueryStringW

#include <filesystem>

#include "../../res/resource.h"

namespace Pluma::Platform {

namespace {

constexpr wchar_t kProgId[] = L"Pluma.Markdown";
constexpr wchar_t kAppName[] = L"Pluma";
constexpr wchar_t kCapabilitiesKey[] = L"Software\\Pluma\\Capabilities";

bool SetString(const std::wstring& subKey, const wchar_t* name, const std::wstring& value) {
    const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    return RegSetKeyValueW(HKEY_CURRENT_USER, subKey.c_str(), name, REG_SZ, value.c_str(), bytes) ==
           ERROR_SUCCESS;
}

bool SetEmpty(const std::wstring& subKey, const wchar_t* name) {
    return RegSetKeyValueW(HKEY_CURRENT_USER, subKey.c_str(), name, REG_NONE, nullptr, 0) == ERROR_SUCCESS;
}

// Long, absolute form so that short (8.3) and long spellings of the same path compare equal.
std::wstring NormalizePath(const std::wstring& path) {
    std::wstring result = path;
    const DWORD len = GetLongPathNameW(path.c_str(), nullptr, 0);
    if (len > 0) {
        std::wstring buffer(len, L'\0');
        const DWORD written = GetLongPathNameW(path.c_str(), buffer.data(), len);
        if (written > 0 && written < len) {
            buffer.resize(written);
            result = std::move(buffer);
        }
    }
    return result;
}

} // namespace

std::wstring GetExecutablePath() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD len = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (len == 0) return {};
        if (len < path.size()) {
            path.resize(len);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

bool RegisterMarkdownHandler(const std::wstring& exePath) {
    if (exePath.empty()) return false;

    const std::wstring classes = L"Software\\Classes\\";
    const std::wstring openCommand = L"\"" + exePath + L"\" \"%1\"";
    // Negative index = resource ID, independent of the order of the icons in the executable.
    const std::wstring docIcon = exePath + L",-" + std::to_wstring(IDI_DOC_MD);
    const std::wstring appIcon = exePath + L",-" + std::to_wstring(IDI_APP_ICON);

    bool ok = true;

    // ProgID describing a Markdown document opened by Pluma.
    const std::wstring progIdKey = classes + kProgId;
    ok &= SetString(progIdKey, nullptr, L"Documento Markdown");
    ok &= SetString(progIdKey, L"FriendlyTypeName", L"Documento Markdown");
    ok &= SetString(progIdKey + L"\\DefaultIcon", nullptr, docIcon);
    ok &= SetString(progIdKey + L"\\shell\\open", L"FriendlyAppName", kAppName);
    ok &= SetString(progIdKey + L"\\shell\\open\\command", nullptr, openCommand);

    // "Open with" entry for the executable itself.
    const std::wstring appKey = classes + L"Applications\\" + std::filesystem::path(exePath).filename().wstring();
    ok &= SetString(appKey, L"FriendlyAppName", kAppName);
    ok &= SetString(appKey + L"\\DefaultIcon", nullptr, appIcon);
    ok &= SetString(appKey + L"\\shell\\open\\command", nullptr, openCommand);

    // Capabilities: lists Pluma in Settings > Default apps.
    const std::wstring capabilities = kCapabilitiesKey;
    ok &= SetString(capabilities, L"ApplicationName", kAppName);
    ok &= SetString(capabilities, L"ApplicationDescription", L"Editor Markdown nativo para Windows");
    ok &= SetString(capabilities, L"ApplicationIcon", appIcon);
    ok &= SetString(L"Software\\RegisteredApplications", kAppName, capabilities);

    for (const wchar_t* ext : kMarkdownExtensions) {
        const std::wstring extKey = classes + ext;
        // Fallback default for this user; an explicit choice in Windows (UserChoice) still wins.
        ok &= SetString(extKey, nullptr, kProgId);
        ok &= SetString(extKey, L"Content Type", L"text/markdown");
        ok &= SetString(extKey, L"PerceivedType", L"text");
        ok &= SetEmpty(extKey + L"\\OpenWithProgids", kProgId);
        ok &= SetEmpty(appKey + L"\\SupportedTypes", ext);
        ok &= SetString(capabilities + L"\\FileAssociations", ext, kProgId);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSH, nullptr, nullptr);
    return ok;
}

bool IsDefaultMarkdownHandler(const std::wstring& exePath) {
    if (exePath.empty()) return false;
    wchar_t buffer[MAX_PATH * 2]{};
    DWORD size = static_cast<DWORD>(std::size(buffer));
    if (FAILED(AssocQueryStringW(ASSOCF_NOTRUNCATE, ASSOCSTR_EXECUTABLE, L".md", L"open", buffer, &size))) {
        return false;
    }
    const std::wstring current = NormalizePath(buffer);
    const std::wstring ours = NormalizePath(exePath);
    return CompareStringOrdinal(current.c_str(), static_cast<int>(current.size()), ours.c_str(),
                                static_cast<int>(ours.size()), TRUE) == CSTR_EQUAL;
}

bool OpenDefaultAppsSettings(HWND owner) {
    // SHOpenWithDialog cannot do this since Windows 10: it ignores the registration flags and, without
    // OAIF_EXEC, only shows a "go to Settings" notice. Settings is the supported path.
    // Windows 11 (2023-04 update or later) opens Pluma's own page (per-user RegisteredApplications
    // name); older builds, including Windows 10, ignore the query and open Default apps.
    const std::wstring uri = std::wstring(L"ms-settings:defaultapps?registeredAppUser=") + kAppName;
    const auto result =
        reinterpret_cast<INT_PTR>(ShellExecuteW(owner, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
}

} // namespace Pluma::Platform
