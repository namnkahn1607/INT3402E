#pragma once

#include <vector>

#include "ast/stmt.h"

namespace ast {

// Function-free source file: declarations and statements in source order.
struct TranslationUnit {
    std::vector<StmtPtr> statements;
};

// Print a human-readable tree representation of the AST.
void DumpAst(const TranslationUnit& unit, std::ostream& out);

}  // namespace ast
