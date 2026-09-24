#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Pluma::Update {

// Minimal JSON document model (RFC 8259), enough to read GitHub API responses.
class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;              // UTF-8
    std::vector<JsonValue> items;    // Array elements, or object values (parallel to `keys`)
    std::vector<std::string> keys;   // Object member names

    bool IsObject() const noexcept { return type == Type::Object; }
    bool IsArray() const noexcept { return type == Type::Array; }
    bool IsString() const noexcept { return type == Type::String; }

    // Object member, or nullptr when missing (or when this is not an object).
    const JsonValue* Find(std::string_view key) const;

    // Member helpers with defaults for missing or mistyped values.
    std::string GetString(std::string_view key, std::string_view fallback = {}) const;
    bool GetBool(std::string_view key, bool fallback = false) const;
    double GetNumber(std::string_view key, double fallback = 0.0) const;
};

// Parses a complete JSON text; nullopt on any syntax error or when nesting exceeds `maxDepth`.
std::optional<JsonValue> ParseJson(std::string_view text, int maxDepth = 64);

} // namespace Pluma::Update
