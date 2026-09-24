#include "updater.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include "sha256.h"

namespace Pluma::Update {

namespace {

std::wstring Utf8ToUtf16(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), count);
    return result;
}

std::filesystem::path UpdateDirectory() {
    std::error_code ec;
    return std::filesystem::temp_directory_path(ec) / L"Pluma" / L"Update";
}

// Inno Setup reads "/NAME=value"; the value may be quoted and cannot contain quotes (paths never do).
std::wstring InnoParameter(const wchar_t* name, const std::wstring& value) {
    return std::wstring(L"/") + name + L"=\"" + value + L"\"";
}

} // namespace

bool FetchLatestRelease(const std::wstring& userAgent, ReleaseInfo& release, std::wstring& error) {
    HttpOptions options;
    options.userAgent = userAgent;
    options.accept = L"application/vnd.github+json";
    options.maxBytes = 4u << 20;
    std::string body;
    if (!HttpGet(kLatestReleaseApiUrl, options, body, error)) return false;

    std::optional<ReleaseInfo> parsed = ParseRelease(body);
    if (!parsed) {
        error = L"La respuesta del servidor de actualizaciones no es válida.";
        return false;
    }
    release = std::move(*parsed);
    return true;
}

bool DownloadInstaller(const ReleaseInfo& release, const std::wstring& userAgent, const ProgressCallback& progress,
                       std::filesystem::path& installer, std::wstring& error) {
    const ReleaseAsset* setup = release.FindInstaller();
    const ReleaseAsset* sums = release.FindAsset(kChecksumsAssetName);
    if (!setup || !IsSafeFileName(setup->name)) {
        error = L"La versión " + Utf8ToUtf16(release.tag) + L" no incluye un instalador para Windows.";
        return false;
    }
    if (!sums) {
        error = L"La versión " + Utf8ToUtf16(release.tag) +
                L" no publica sumas de verificación (SHA256SUMS.txt); no es posible comprobar el instalador.";
        return false;
    }

    HttpOptions options;
    options.userAgent = userAgent;
    options.maxBytes = 1u << 20;
    std::string sumsText;
    if (!HttpGet(Utf8ToUtf16(sums->downloadUrl), options, sumsText, error)) return false;
    const std::optional<std::string> expected = FindChecksum(sumsText, setup->name);
    if (!expected) {
        error = L"SHA256SUMS.txt no contiene la suma de verificación del instalador.";
        return false;
    }

    const std::filesystem::path dir = UpdateDirectory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::filesystem::path target = dir / Utf8ToUtf16(setup->name);

    // Reuse an installer downloaded earlier (e.g. the user postponed the installation).
    if (std::filesystem::is_regular_file(target, ec)) {
        if (Sha256HexOfFile(target) == expected) {
            if (progress) progress(1, 1);
            installer = target;
            return true;
        }
        std::filesystem::remove(target, ec);
    }

    std::filesystem::path partial = target;
    partial += L".download";
    options.maxBytes = 256u << 20;
    if (!HttpDownloadFile(Utf8ToUtf16(setup->downloadUrl), options, partial, progress, error)) return false;

    if (Sha256HexOfFile(partial) != expected) {
        std::filesystem::remove(partial, ec);
        error = L"El instalador descargado no coincide con su suma de verificación SHA-256. "
                L"Se ha descartado por seguridad.";
        return false;
    }
    std::filesystem::rename(partial, target, ec);
    if (ec) {
        std::filesystem::remove(partial, ec);
        error = L"No se pudo preparar el instalador descargado.";
        return false;
    }
    installer = target;
    return true;
}

bool IsInstalledCopy(const std::wstring& exePath) {
    const std::wstring key =
        std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\") + kInstallerAppId + L"_is1";
    wchar_t location[MAX_PATH * 2]{};
    DWORD size = sizeof(location);
    if (RegGetValueW(HKEY_CURRENT_USER, key.c_str(), L"InstallLocation", RRF_RT_REG_SZ, nullptr, location, &size) !=
        ERROR_SUCCESS) {
        return false;
    }
    std::error_code ec;
    const bool same =
        std::filesystem::equivalent(std::filesystem::path(exePath).parent_path(), std::filesystem::path(location), ec);
    return same && !ec;
}

std::wstring BuildInstallerArguments(const std::filesystem::path& reopenFile, const std::filesystem::path& logFile) {
    // /SILENT shows only the progress window; the installer keeps the previous folder and tasks
    // (file associations, shortcuts) and relaunches Pluma because of /PLUMAUPDATE=1.
    std::wstring args = L"/SILENT /SUPPRESSMSGBOXES /NORESTART /SP- /PLUMAUPDATE=1";
    if (!logFile.empty()) args += L" " + InnoParameter(L"LOG", logFile.wstring());
    if (!reopenFile.empty()) args += L" " + InnoParameter(L"PLUMAOPEN", reopenFile.wstring());
    return args;
}

bool LaunchInstaller(const std::filesystem::path& installer, const std::filesystem::path& reopenFile,
                     std::wstring& error) {
    const std::wstring args = BuildInstallerArguments(reopenFile, installer.parent_path() / L"setup.log");
    SHELLEXECUTEINFOW info{sizeof(info)};
    info.fMask = SEE_MASK_NOASYNC;
    info.lpVerb = L"open";
    info.lpFile = installer.c_str();
    info.lpParameters = args.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        error = L"No se pudo iniciar el instalador (código " + std::to_wstring(GetLastError()) + L").";
        return false;
    }
    return true;
}

} // namespace Pluma::Update
