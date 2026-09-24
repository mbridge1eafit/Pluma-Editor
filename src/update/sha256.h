#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Pluma::Update {

// Lower-case hex SHA-256 digests computed with Windows CNG (bcrypt.dll).
std::optional<std::string> Sha256Hex(std::string_view data);
std::optional<std::string> Sha256HexOfFile(const std::filesystem::path& path);

} // namespace Pluma::Update
