#include "json.h"

#include <charconv>
#include <cstdint>

namespace Pluma::Update {

namespace {

class Parser {
public:
    Parser(std::string_view text, int maxDepth) : m_text(text), m_maxDepth(maxDepth) {}

    std::optional<JsonValue> ParseDocument() {
        JsonValue value;
        SkipWhitespace();
        if (!ParseValue(value, 0)) return std::nullopt;
        SkipWhitespace();
        if (m_pos != m_text.size()) return std::nullopt;
        return value;
    }

private:
    bool AtEnd() const { return m_pos >= m_text.size(); }
    char Peek() const { return AtEnd() ? '\0' : m_text[m_pos]; }

    void SkipWhitespace() {
        while (!AtEnd()) {
            const char c = m_text[m_pos];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
            ++m_pos;
        }
    }

    bool Consume(std::string_view literal) {
        if (m_text.substr(m_pos, literal.size()) != literal) return false;
        m_pos += literal.size();
        return true;
    }

    bool ParseValue(JsonValue& out, int depth) {
        if (depth > m_maxDepth) return false;
        switch (Peek()) {
        case '{': return ParseObject(out, depth);
        case '[': return ParseArray(out, depth);
        case '"':
            out.type = JsonValue::Type::String;
            return ParseString(out.string);
        case 't':
            out.type = JsonValue::Type::Bool;
            out.boolean = true;
            return Consume("true");
        case 'f':
            out.type = JsonValue::Type::Bool;
            out.boolean = false;
            return Consume("false");
        case 'n':
            out.type = JsonValue::Type::Null;
            return Consume("null");
        default:
            return ParseNumber(out);
        }
    }

    bool ParseObject(JsonValue& out, int depth) {
        out.type = JsonValue::Type::Object;
        ++m_pos; // '{'
        SkipWhitespace();
        if (Peek() == '}') {
            ++m_pos;
            return true;
        }
        for (;;) {
            SkipWhitespace();
            std::string key;
            if (Peek() != '"' || !ParseString(key)) return false;
            SkipWhitespace();
            if (Peek() != ':') return false;
            ++m_pos;
            SkipWhitespace();
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.keys.push_back(std::move(key));
            out.items.push_back(std::move(value));
            SkipWhitespace();
            if (Peek() == ',') {
                ++m_pos;
                continue;
            }
            if (Peek() == '}') {
                ++m_pos;
                return true;
            }
            return false;
        }
    }

    bool ParseArray(JsonValue& out, int depth) {
        out.type = JsonValue::Type::Array;
        ++m_pos; // '['
        SkipWhitespace();
        if (Peek() == ']') {
            ++m_pos;
            return true;
        }
        for (;;) {
            SkipWhitespace();
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.items.push_back(std::move(value));
            SkipWhitespace();
            if (Peek() == ',') {
                ++m_pos;
                continue;
            }
            if (Peek() == ']') {
                ++m_pos;
                return true;
            }
            return false;
        }
    }

    bool ParseHex4(uint32_t& out) {
        if (m_pos + 4 > m_text.size()) return false;
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = m_text[m_pos++];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<uint32_t>(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    static void AppendUtf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool ParseString(std::string& out) {
        ++m_pos; // '"'
        while (!AtEnd()) {
            const char c = m_text[m_pos++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return false; // Control characters must be escaped
            if (c != '\\') {
                out += c;
                continue;
            }
            if (AtEnd()) return false;
            const char e = m_text[m_pos++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                uint32_t cp = 0;
                if (!ParseHex4(cp)) return false;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    // High surrogate: must be followed by an escaped low surrogate.
                    uint32_t low = 0;
                    if (!Consume("\\u") || !ParseHex4(low) || low < 0xDC00 || low > 0xDFFF) return false;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return false;
                }
                AppendUtf8(out, cp);
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    bool ParseNumber(JsonValue& out) {
        const size_t start = m_pos;
        if (Peek() == '-') ++m_pos;
        if (Peek() == '0') {
            ++m_pos;
        } else if (Peek() >= '1' && Peek() <= '9') {
            while (Peek() >= '0' && Peek() <= '9') ++m_pos;
        } else {
            return false;
        }
        if (Peek() == '.') {
            ++m_pos;
            if (!(Peek() >= '0' && Peek() <= '9')) return false;
            while (Peek() >= '0' && Peek() <= '9') ++m_pos;
        }
        if (Peek() == 'e' || Peek() == 'E') {
            ++m_pos;
            if (Peek() == '+' || Peek() == '-') ++m_pos;
            if (!(Peek() >= '0' && Peek() <= '9')) return false;
            while (Peek() >= '0' && Peek() <= '9') ++m_pos;
        }
        out.type = JsonValue::Type::Number;
        const char* first = m_text.data() + start;
        const char* last = m_text.data() + m_pos;
        const auto [ptr, ec] = std::from_chars(first, last, out.number);
        // Out-of-range magnitudes are valid JSON; they are not needed here, so they read as 0.
        if (ec == std::errc::result_out_of_range) out.number = 0.0;
        return ptr == last;
    }

    std::string_view m_text;
    size_t m_pos = 0;
    int m_maxDepth;
};

} // namespace

const JsonValue* JsonValue::Find(std::string_view key) const {
    if (type != Type::Object) return nullptr;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == key) return &items[i];
    }
    return nullptr;
}

std::string JsonValue::GetString(std::string_view key, std::string_view fallback) const {
    const JsonValue* v = Find(key);
    return (v && v->type == Type::String) ? v->string : std::string(fallback);
}

bool JsonValue::GetBool(std::string_view key, bool fallback) const {
    const JsonValue* v = Find(key);
    return (v && v->type == Type::Bool) ? v->boolean : fallback;
}

double JsonValue::GetNumber(std::string_view key, double fallback) const {
    const JsonValue* v = Find(key);
    return (v && v->type == Type::Number) ? v->number : fallback;
}

std::optional<JsonValue> ParseJson(std::string_view text, int maxDepth) {
    // Tolerate a UTF-8 byte order mark.
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }
    return Parser(text, maxDepth).ParseDocument();
}

} // namespace Pluma::Update
