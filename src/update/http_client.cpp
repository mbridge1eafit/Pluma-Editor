#include "http_client.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <fstream>
#include <vector>

namespace Pluma::Update {

namespace {

struct InternetHandle {
    HINTERNET handle = nullptr;
    explicit InternetHandle(HINTERNET h = nullptr) : handle(h) {}
    ~InternetHandle() {
        if (handle) WinHttpCloseHandle(handle);
    }
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    explicit operator bool() const noexcept { return handle != nullptr; }
};

std::wstring ErrorMessage(DWORD code) {
    switch (code) {
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
    case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_CONNECTION_ERROR:
        return L"No se pudo conectar con el servidor. Compruebe la conexión a Internet.";
    case ERROR_WINHTTP_TIMEOUT:
        return L"El servidor tardó demasiado en responder.";
    case ERROR_WINHTTP_SECURE_FAILURE:
        return L"No se pudo establecer una conexión segura (certificado no válido).";
    default:
        return L"Error de red (código " + std::to_wstring(code) + L").";
    }
}

// Streams the response body of a GET request to `sink`. Returns false and sets `error` on failure.
template <typename Sink>
bool Transfer(const std::wstring& url, const HttpOptions& options, const ProgressCallback& progress, Sink&& sink,
              std::wstring& error) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
        error = L"Dirección de descarga no válida.";
        return false;
    }
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    InternetHandle session(WinHttpOpen(options.userAgent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                       WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
        error = ErrorMessage(GetLastError());
        return false;
    }
    // Resolve, connect, send, receive (per read).
    WinHttpSetTimeouts(session.handle, 10000, 10000, 15000, 30000);
    // Redirects (GitHub serves assets from another host) are followed, but never from HTTPS to HTTP.
    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(session.handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    InternetHandle connection(WinHttpConnect(session.handle, host.c_str(), parts.nPort, 0));
    if (!connection) {
        error = ErrorMessage(GetLastError());
        return false;
    }
    InternetHandle request(WinHttpOpenRequest(connection.handle, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) {
        error = ErrorMessage(GetLastError());
        return false;
    }

    std::wstring headers;
    if (!options.accept.empty()) headers = L"Accept: " + options.accept + L"\r\n";
    if (!WinHttpSendRequest(request.handle, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                            headers.empty() ? 0 : static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.handle, nullptr)) {
        error = ErrorMessage(GetLastError());
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        if (status == 403 || status == 429) {
            error = L"El servidor rechazó la solicitud temporalmente (límite de consultas). Inténtelo más tarde.";
        } else if (status == 404) {
            error = L"No se encontró ninguna versión publicada.";
        } else {
            error = L"El servidor respondió con el código HTTP " + std::to_wstring(status) + L".";
        }
        return false;
    }

    uint64_t total = 0;
    wchar_t lengthText[32]{};
    DWORD lengthSize = sizeof(lengthText);
    if (WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, lengthText,
                            &lengthSize, WINHTTP_NO_HEADER_INDEX)) {
        total = _wcstoui64(lengthText, nullptr, 10);
    }
    if (total > options.maxBytes) {
        error = L"La respuesta del servidor es demasiado grande.";
        return false;
    }

    std::vector<char> buffer(1 << 16);
    uint64_t received = 0;
    if (progress && !progress(0, total)) {
        error.clear(); // Cancelled
        return false;
    }
    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(request.handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
            error = ErrorMessage(GetLastError());
            return false;
        }
        if (read == 0) break;
        received += read;
        if (received > options.maxBytes) {
            error = L"La respuesta del servidor es demasiado grande.";
            return false;
        }
        if (!sink(buffer.data(), static_cast<size_t>(read))) {
            error = L"No se pudo escribir el archivo descargado.";
            return false;
        }
        if (progress && !progress(received, total)) {
            error.clear(); // Cancelled
            return false;
        }
    }
    if (total != 0 && received != total) {
        error = L"La descarga se interrumpió antes de completarse.";
        return false;
    }
    return true;
}

} // namespace

bool HttpGet(const std::wstring& url, const HttpOptions& options, std::string& body, std::wstring& error) {
    body.clear();
    return Transfer(
        url, options, nullptr,
        [&body](const char* data, size_t size) {
            body.append(data, size);
            return true;
        },
        error);
}

bool HttpDownloadFile(const std::wstring& url, const HttpOptions& options, const std::filesystem::path& destination,
                      const ProgressCallback& progress, std::wstring& error) {
    bool ok = false;
    {
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = L"No se pudo crear el archivo de descarga:\n" + destination.wstring();
            return false;
        }
        ok = Transfer(
            url, options, progress,
            [&out](const char* data, size_t size) {
                out.write(data, static_cast<std::streamsize>(size));
                return static_cast<bool>(out);
            },
            error);
        out.close();
        if (ok && !out) {
            error = L"No se pudo escribir el archivo descargado.";
            ok = false;
        }
    }
    if (!ok) {
        std::error_code ec;
        std::filesystem::remove(destination, ec);
    }
    return ok;
}

} // namespace Pluma::Update
