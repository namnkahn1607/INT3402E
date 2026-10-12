#pragma once

#include "ast/expressions.h"
#include "cursor.h"

namespace parser {

class Parser;

// Pratt-based expression parser, holds mutual reference against `Parser`.
class ExprParser final {
public:
    explicit ExprParser(Cursor& stream, Parser& parent)
        : cursor_{stream}, parent_{parent} {}

    // Consumes parentheses opened within expressions, but leave caller-owned
    // ';', ')', '}' and EOF untouched. Success returns a complete non-null
    // expression. Syntax failure is diagnosed locally before returning null to
    // the immediate caller; never attach null mandatory children or use global
    // error status as success. Invalid spelling failures are lexer-diagnosed.
    // Enclosing methods propagate diagnosed failures without duplicate
    // diagnostics to the appropriate contextual recovery owner, which can
    // resume parsing. Assignment is not infix here.
    ast::ExprPtr ParseExpression(int min_binding_power = 0);

    ast::ExprPtr ParsePrimary();

private:
    Cursor& cursor_;
    Parser& parent_;
};

}  // namespace parser
