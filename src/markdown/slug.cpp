#include "slug.h"

namespace Pluma::Markdown {

std::string Slugify(std::string_view text) {
    std::string slug;
    slug.reserve(text.size());

    auto pushHyphen = [&slug]() {
        if (!slug.empty() && slug.back() != '-') {
            slug.push_back('-');
        }
    };

    for (size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c >= 'A' && c <= 'Z') {
            slug.push_back(static_cast<char>(c - 'A' + 'a'));
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            slug.push_back(static_cast<char>(c));
        } else if (c == ' ' || c == '-' || c == '_' || c == '\t') {
            pushHyphen();
        } else if (c >= 0x80) {
            // Copy the whole UTF-8 sequence; fold Latin-1 capitals (U+00C0..U+00DE, except U+00D7).
            size_t len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
            if (i + len > text.size()) {
                break;
            }
            if (len == 2 && c == 0xC3) {
                const auto next = static_cast<unsigned char>(text[i + 1]);
                slug.push_back(static_cast<char>(c));
                const bool isUpper = next >= 0x80 && next <= 0x9E && next != 0x97;
                slug.push_back(static_cast<char>(isUpper ? next + 0x20 : next));
            } else if (len == 2 && c == 0xC2) {
                // U+0080..U+00BF are punctuation/symbols (¿, ¡, ©, «, »...): drop them.
            } else if (len == 4 && static_cast<unsigned char>(text[i + 1]) == 0x9F) {
                // U+1F000..U+1FFFF: emoji and pictographs, dropped like GitHub does.
            } else if (len == 3 && (c == 0xE2 || (c == 0xEF && static_cast<unsigned char>(text[i + 1]) == 0xB8))) {
                // U+2000..U+2FFF (punctuation, arrows, dingbats such as ✅ ⚠) and variation selectors.
            } else {
                slug.append(text.substr(i, len));
            }
            i += len - 1;
        }
    }

    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug;
}

std::string PercentDecode(std::string_view text) {
    auto hexValue = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };

    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const int hi = hexValue(text[i + 1]);
            const int lo = hexValue(text[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(text[i]);
    }
    return out;
}

} // namespace Pluma::Markdown
