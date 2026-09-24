#include "update_ui.h"

#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <iterator>
#include <memory>
#include <thread>

#include "../update/updater.h"
#include "../../res/resource.h"

namespace Pluma::App {

namespace {

constexpr int kButtonInstall = 1001;
constexpr int kButtonLater = 1002;
constexpr int kButtonSkip = 1003;
constexpr UINT kProgressRange = 1000;

std::wstring Utf8ToUtf16(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), count);
    return result;
}

bool IsHttpsUrl(std::wstring_view url) { return url.substr(0, 8) == L"https://"; }

// "3,4 MB" (Spanish decimal separator).
std::wstring FormatMegabytes(uint64_t bytes) {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    std::wstring text = buffer;
    for (wchar_t& c : text) {
        if (c == L'.') c = L',';
    }
    return text;
}

// Task dialogs interpret "<a ...>" in every text field when hyperlinks are enabled.
std::wstring NeutralizeMarkup(std::wstring text) {
    for (wchar_t& c : text) {
        if (c == L'<') c = L'‹'; // ‹
        else if (c == L'>') c = L'›'; // ›
    }
    return text;
}

HRESULT CALLBACK UpdateDialogCallback(HWND hwnd, UINT notification, WPARAM, LPARAM lParam, LONG_PTR) {
    if (notification == TDN_HYPERLINK_CLICKED) {
        OpenUrl(hwnd, reinterpret_cast<const wchar_t*>(lParam));
    }
    return S_OK;
}

struct DownloadState {
    const Update::ReleaseInfo* release = nullptr;
    std::wstring userAgent;
    std::atomic<uint64_t> received{0};
    std::atomic<uint64_t> total{0};
    std::atomic<bool> cancel{false};
    std::atomic<bool> done{false};
    // Written by the worker before `done` is set.
    bool ok = false;
    std::wstring error;
    std::filesystem::path installer;
    // Dialog state
    bool marquee = true;
};

HRESULT CALLBACK DownloadDialogCallback(HWND hwnd, UINT notification, WPARAM wParam, LPARAM, LONG_PTR refData) {
    auto* state = reinterpret_cast<DownloadState*>(refData);
    switch (notification) {
    case TDN_CREATED:
        SendMessageW(hwnd, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
        SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 30);
        break;

    case TDN_TIMER: {
        if (state->done.load(std::memory_order_acquire)) {
            // Closes the dialog; the caller tells completion from cancellation through `done`.
            SendMessageW(hwnd, TDM_CLICK_BUTTON, IDCANCEL, 0);
            break;
        }
        const uint64_t total = state->total.load();
        const uint64_t received = state->received.load();
        if (total > 0) {
            if (state->marquee) {
                state->marquee = false;
                SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_MARQUEE, FALSE, 0);
                SendMessageW(hwnd, TDM_SET_MARQUEE_PROGRESS_BAR, FALSE, 0);
                SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(0, kProgressRange));
            }
            const auto pos = static_cast<WPARAM>((std::min)(received, total) * kProgressRange / total);
            SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_POS, pos, 0);
            const std::wstring text = L"Descargado " + FormatMegabytes(received) + L" de " + FormatMegabytes(total) + L".";
            SendMessageW(hwnd, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, reinterpret_cast<LPARAM>(text.c_str()));
        }
        break;
    }

    case TDN_BUTTON_CLICKED:
        if (wParam == IDCANCEL && !state->done.load()) state->cancel = true;
        break;

    default:
        break;
    }
    return S_OK;
}

} // namespace

void OpenUrl(HWND owner, const std::wstring& url) {
    if (!IsHttpsUrl(url)) return;
    ShellExecuteW(owner, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void StartUpdateCheck(HWND target, UINT message, bool manual, const std::wstring& userAgent) {
    // Detached: the thread only owns its own copies and posts one message; if the window is gone
    // by then, the result is freed here.
    std::thread([target, message, manual, userAgent] {
        auto result = std::make_unique<UpdateCheckResult>();
        result->manual = manual;
        result->ok = Update::FetchLatestRelease(userAgent, result->release, result->error);
        if (PostMessageW(target, message, 0, reinterpret_cast<LPARAM>(result.get()))) {
            result.release();
        }
    }).detach();
}

UpdateChoice ShowUpdateAvailableDialog(HWND owner, HINSTANCE hInstance, const Update::ReleaseInfo& release,
                                       const std::wstring& currentVersion, bool installedCopy) {
    const std::wstring newVersion = Utf8ToUtf16(Update::FormatVersion(release.version));
    const std::wstring content = L"Pluma " + newVersion + L" ya está disponible. Tiene instalada la versión " +
                                 currentVersion + L".";
    const std::wstring notes = NeutralizeMarkup(Utf8ToUtf16(Update::SummarizeReleaseNotes(release.notes)));
    const std::wstring pageUrl = IsHttpsUrl(Utf8ToUtf16(release.pageUrl)) ? Utf8ToUtf16(release.pageUrl)
                                                                           : std::wstring(Update::kReleasesPageUrl);
    const std::wstring footer = L"<a href=\"" + pageUrl + L"\">Ver la versión " + newVersion + L" en GitHub</a>";

    const std::wstring installText =
        installedCopy
            ? L"Actualizar ahora\nDescarga el instalador, comprueba su integridad e instala la nueva versión. "
              L"Pluma se cerrará y volverá a abrirse con su documento."
            : L"Descargar desde GitHub\nEsta copia de Pluma es portable (no se instaló con el instalador): "
              L"descargue la nueva versión desde la página de la versión.";
    const std::wstring skipText = L"Omitir esta versión\nNo volver a avisar de la versión " + newVersion +
                                  L". Puede buscarla más tarde en Ayuda > Buscar actualizaciones.";
    const TASKDIALOG_BUTTON buttons[] = {
        {kButtonInstall, installText.c_str()},
        {kButtonLater, L"Recordármelo más tarde"},
        {kButtonSkip, skipText.c_str()},
    };

    TASKDIALOGCONFIG config{sizeof(config)};
    config.hwndParent = owner;
    config.hInstance = hInstance;
    config.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_ENABLE_HYPERLINKS |
                     TDF_POSITION_RELATIVE_TO_WINDOW;
    config.pszWindowTitle = L"Actualización de Pluma";
    config.pszMainIcon = MAKEINTRESOURCEW(IDI_APP_ICON);
    config.pszMainInstruction = L"Hay una nueva versión de Pluma";
    config.pszContent = content.c_str();
    if (!notes.empty()) {
        config.pszExpandedInformation = notes.c_str();
        config.pszExpandedControlText = L"Ocultar novedades";
        config.pszCollapsedControlText = L"Ver novedades";
    }
    config.pszFooter = footer.c_str();
    config.pszFooterIcon = TD_INFORMATION_ICON;
    config.cButtons = static_cast<UINT>(std::size(buttons));
    config.pButtons = buttons;
    config.nDefaultButton = kButtonInstall;
    config.pfCallback = UpdateDialogCallback;

    int pressed = 0;
    if (FAILED(TaskDialogIndirect(&config, &pressed, nullptr, nullptr))) return UpdateChoice::Later;
    switch (pressed) {
    case kButtonInstall: return installedCopy ? UpdateChoice::Install : UpdateChoice::OpenPage;
    case kButtonSkip:    return UpdateChoice::Skip;
    default:             return UpdateChoice::Later;
    }
}

bool DownloadInstallerWithProgress(HWND owner, HINSTANCE hInstance, const Update::ReleaseInfo& release,
                                   const std::wstring& userAgent, std::filesystem::path& installer,
                                   std::wstring& error) {
    DownloadState state;
    state.release = &release;
    state.userAgent = userAgent;

    std::thread worker([&state] {
        std::wstring workerError;
        std::filesystem::path path;
        const bool ok = Update::DownloadInstaller(
            *state.release, state.userAgent,
            [&state](uint64_t received, uint64_t total) {
                state.received = received;
                state.total = total;
                return !state.cancel.load();
            },
            path, workerError);
        state.ok = ok;
        state.error = std::move(workerError);
        state.installer = std::move(path);
        state.done.store(true, std::memory_order_release);
    });

    const std::wstring instruction =
        L"Descargando Pluma " + Utf8ToUtf16(Update::FormatVersion(release.version)) + L"…";
    TASKDIALOGCONFIG config{sizeof(config)};
    config.hwndParent = owner;
    config.hInstance = hInstance;
    config.dwFlags = TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER | TDF_ALLOW_DIALOG_CANCELLATION |
                     TDF_POSITION_RELATIVE_TO_WINDOW;
    config.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    config.pszWindowTitle = L"Actualización de Pluma";
    config.pszMainIcon = MAKEINTRESOURCEW(IDI_APP_ICON);
    config.pszMainInstruction = instruction.c_str();
    config.pszContent = L"Conectando con GitHub…";
    config.pfCallback = DownloadDialogCallback;
    config.lpCallbackData = reinterpret_cast<LONG_PTR>(&state);

    if (FAILED(TaskDialogIndirect(&config, nullptr, nullptr, nullptr))) state.cancel = true;
    if (!state.done.load()) state.cancel = true;
    worker.join(); // Returns quickly: the transfer checks `cancel` after every chunk

    if (state.cancel.load() && !state.ok) {
        error.clear();
        return false;
    }
    if (!state.ok) {
        error = state.error;
        return false;
    }
    installer = state.installer;
    return true;
}

} // namespace Pluma::App
