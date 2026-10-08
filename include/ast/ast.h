#pragma once

#include <vector>
#include <span>

#include "ast/declarations.h"
#include "ast/expressions.h"
#include "ast/statements.h"

#include "common/diagnostic.h"

#include "lexer/token.h"

namespace ast {

// Top-level translation unit: a list of declarations in file order.
struct TranslationUnit {
    std::vector<DeclPtr> decls;
};

}  // namespace ast
