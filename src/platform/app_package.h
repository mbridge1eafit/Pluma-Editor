#pragma once

#include <string>
#include <string_view>

// Package identity. Pluma runs either as a classic Win32 app (Inno Setup installer or portable ZIP)
// or from the MSIX package published in the Microsoft Store. Packaged, Windows owns updates and file
// associations: the built-in updater is disabled and registry writes are private to the package.
namespace Pluma::Platform {

// True when the process has package identity (installed from an MSIX package). Cached after the
// first call.
bool IsPackaged();

// Application User Model ID of the packaged app ("<PackageFamilyName>!<AppId>"); empty when the
// process is not packaged.
std::wstring GetAppUserModelId();

// Percent-encodes everything except RFC 3986 unreserved characters (A-Z a-z 0-9 - _ . ~), like
// Uri.EscapeDataString. Non-ASCII characters are encoded as UTF-8.
std::wstring EscapeUriComponent(std::wstring_view text);

} // namespace Pluma::Platform
