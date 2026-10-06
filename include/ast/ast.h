#pragma once

#include <vector>

#include "ast/declarations.h"
#include "ast/expressions.h"
#include "ast/statements.h"

namespace ast {

// Top-level translation unit: a list of declarations in file order.
struct Program {
    std::vector<DeclPtr> decls;
};

}  // namespace ast
