#pragma once

#include <string_view>
#include <memory>
#include "block_tree.h"

namespace Pluma::Markdown {

class Md4cAdapter {
public:
    // Parses a Markdown UTF-8 string into an AST BlockTree (CommonMark 0.31 + GFM)
    static std::unique_ptr<BlockTree> Parse(std::string_view markdown, uint64_t version = 0);
};

} // namespace Pluma::Markdown
