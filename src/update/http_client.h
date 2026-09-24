#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace Pluma::Update {

// Called after every received chunk with the bytes received so far and the total (0 = unknown).
// Returning false cancels the transfer.
using ProgressCallback = std::function<bool(uint64_t received, uint64_t total)>;

struct HttpOptions {
    std::wstring userAgent = L"Pluma";
    std::wstring accept;            // Accept header; empty = none
    uint64_t maxBytes = 64u << 20;  // Larger responses fail
};

// Blocking HTTPS GET (WinHTTP, system proxy settings, redirects followed). Only https:// URLs are
// allowed. On failure returns false and a user-readable message in `error`.
bool HttpGet(const std::wstring& url, const HttpOptions& options, std::string& body, std::wstring& error);

// Downloads to `destination` (overwritten). A partial file is deleted on failure or cancellation.
bool HttpDownloadFile(const std::wstring& url, const HttpOptions& options, const std::filesystem::path& destination,
                      const ProgressCallback& progress, std::wstring& error);

} // namespace Pluma::Update
