#pragma once

#include <string>
#include <string_view>

namespace Pluma::Markdown {

// Builds a GitHub-style anchor slug from heading text (F-10).
// ASCII letters are lowercased, Latin-1 accented capitals are folded to lowercase,
// non-ASCII letters are preserved as UTF-8, and punctuation is dropped.
std::string Slugify(std::string_view text);

// Decodes %XX escapes in a URL fragment (e.g. "#introducci%C3%B3n").
std::string PercentDecode(std::string_view text);

} // namespace Pluma::Markdown
