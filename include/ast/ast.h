#pragma once

#include <span>
#include <vector>

#include "ast/declarations.h"
#include "ast/expressions.h"
#include "ast/statements.h"
#include "common/diagnostic.h"
#include "lexer/token.h"

namespace ast {

// Function-free source file: declarations and statements in source order.
// Variable declarations use DeclStmt, just as they do within blocks.
struct TranslationUnit {
    std::vector<StmtPtr> statements;
};

}  // namespace ast
