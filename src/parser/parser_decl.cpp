// clang-format off
#include "parser/parser.h"
// clang-format on

#include <string>
#include <utility>

namespace parser {

std::unique_ptr<ast::VarDecl> Parser::ParseDeclaration() {
    const auto location = cursor_.CurrentLocation();
    const auto type     = ParseType();
    if (!type) {
        return nullptr;
    }

    const auto* name_token = cursor_.PeekToken();
    if (name_token == nullptr || name_token->kind != TokenKind::identifier) {
        cursor_.ReportSyntaxError(cursor_.CurrentLocation(),
                                  "expected a valid variable name");
        return nullptr;
    }
    std::string name = name_token->lexeme;
    cursor_.Consume();

    ast::ExprPtr initializer;
    if (cursor_.Match(TokenKind::equal)) {
        initializer = expr_parser_.ParseExpression();
        if (initializer == nullptr) {
            // A failed initializer is not an uninitialized declaration.
            return nullptr;
        }
    }
    return std::make_unique<ast::VarDecl>(location, *type, std::move(name),
                                          std::move(initializer));
}

std::optional<ast::Type> Parser::ParseType() {
    if (cursor_.Match(TokenKind::kw_int)) {
        return ast::Type::Int;
    }
    if (cursor_.Match(TokenKind::kw_bool)) {
        return ast::Type::Bool;
    }
    cursor_.ReportSyntaxError(cursor_.CurrentLocation(),
                              "expected 'int' or 'bool'");
    return std::nullopt;
}

}  // namespace parser
