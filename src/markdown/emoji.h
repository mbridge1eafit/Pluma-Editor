#pragma once

#include <string>
#include <string_view>

namespace Pluma::Markdown {

// Returns the emoji for a GitHub-style shortcode name (without colons), or an empty view.
std::string_view LookupEmojiShortcode(std::string_view name);

// Replaces every ":shortcode:" occurrence with its emoji. Unknown shortcodes are left untouched.
std::string ReplaceEmojiShortcodes(std::string_view text);

} // namespace Pluma::Markdown
