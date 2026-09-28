#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Pluma::Markdown {

// Returns the emoji for a GitHub-style shortcode name (without colons), or an empty view.
std::string_view LookupEmojiShortcode(std::string_view name);

struct EmojiShortcodeMatch {
    size_t pos = 0;    // Offset of the opening ':'
    size_t length = 0; // Including both colons
    std::string_view emoji;
};

// Finds every known ":shortcode:" occurrence, in order and without overlaps.
std::vector<EmojiShortcodeMatch> FindEmojiShortcodes(std::string_view text);

// Replaces every ":shortcode:" occurrence with its emoji. Unknown shortcodes are left untouched.
std::string ReplaceEmojiShortcodes(std::string_view text);

} // namespace Pluma::Markdown
