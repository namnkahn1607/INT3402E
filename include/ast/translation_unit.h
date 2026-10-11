#pragma once

#include <vector>

#include "ast/statements.h"

namespace ast {

// Function-free source file: declarations and statements in source order.
// Variable declarations use DeclStmt, just as they do within blocks.
struct TranslationUnit {
    std::vector<StmtPtr> statements;
};

}  // namespace ast
