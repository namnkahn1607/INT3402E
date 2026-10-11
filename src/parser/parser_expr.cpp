// clang-format off
#include "parser/parser.h"
// clang-format on

#include <utility>

namespace {

struct InfixOperator {
    ast::BinaryOp operation;
    int           left_binding_power;
    int           right_binding_power;
};

std::optional<InfixOperator> GetInfixOperator(lexer::TokenKind kind) {
    switch (kind) {
        case lexer::TokenKind::equalequal:
            return InfixOperator{.operation           = ast::BinaryOp::Eq,
                                 .left_binding_power  = 1,
                                 .right_binding_power = 2};
        case lexer::TokenKind::greater:
            return InfixOperator{.operation           = ast::BinaryOp::Gt,
                                 .left_binding_power  = 3,
                                 .right_binding_power = 4};
        case lexer::TokenKind::greaterequal:
            return InfixOperator{.operation           = ast::BinaryOp::Ge,
                                 .left_binding_power  = 3,
                                 .right_binding_power = 4};
        case lexer::TokenKind::plus:
            return InfixOperator{.operation           = ast::BinaryOp::Add,
                                 .left_binding_power  = 5,
                                 .right_binding_power = 6};
        case lexer::TokenKind::star:
            return InfixOperator{.operation           = ast::BinaryOp::Mul,
                                 .left_binding_power  = 7,
                                 .right_binding_power = 8};
        default: return std::nullopt;
    }
}

bool IsCallerBoundary(lexer::TokenKind kind) {
    return kind == lexer::TokenKind::semi ||
           kind == lexer::TokenKind::r_paren ||
           kind == lexer::TokenKind::r_brace || kind == lexer::TokenKind::eof;
}

}  // namespace

namespace parser {

ast::ExprPtr Parser::ParsePrimary() {
    const auto* token = PeekToken();
    if (token == nullptr) {
        ReportSyntaxError(CurrentLocation(), "expected an expression");
        return nullptr;
    }

    switch (token->kind) {
        case TokenKind::identifier: {
            auto expression =
                std::make_unique<ast::DeclRefExpr>(token->loc, token->lexeme);
            Consume();
            return expression;
        }
        case TokenKind::numeric_constant: {
            auto expression = std::make_unique<ast::IntLiteralExpr>(
                token->loc, token->lexeme);
            Consume();
            return expression;
        }
        case TokenKind::kw_true:
        case TokenKind::kw_false: {
            auto expression = std::make_unique<ast::BoolLiteralExpr>(
                token->loc, token->kind == TokenKind::kw_true);
            Consume();
            return expression;
        }
        case TokenKind::l_paren: {
            Consume();
            auto expression = ParseExpression();
            if (expression == nullptr) {
                return nullptr;
            }
            if (!Expect(TokenKind::r_paren,
                        "expected ')' after parenthesized expression")) {
                return nullptr;
            }
            return expression;
        }
        default:
            ReportSyntaxError(token->loc, "expected an expression");
            return nullptr;
    }
}

ast::ExprPtr Parser::ParseExpression(int min_binding_power) {
    auto left = ParsePrimary();
    if (left == nullptr) {
        return nullptr;
    }

    // Each grouping or recursive operand gets fresh restriction state. In
    // particular, the two comparisons in a > b == c > d are distinct
    // productions.
    bool has_comparison = false;
    bool has_equality   = false;
    while (!AtEnd()) {
        const auto kind  = PeekKind();
        const auto infix = GetInfixOperator(kind);
        if (!infix) {
            if (IsCallerBoundary(kind)) {
                break;
            }
            ReportSyntaxError(CurrentLocation(),
                              "unexpected token in expression");
            return nullptr;
        }
        if (infix->left_binding_power < min_binding_power) {
            break;
        }

        const bool is_comparison =
            kind == TokenKind::greater || kind == TokenKind::greaterequal;
        const bool is_equality = kind == TokenKind::equalequal;
        if ((is_comparison && has_comparison) ||
            (is_equality && has_equality)) {
            ReportSyntaxError(
                CurrentLocation(),
                "chained comparison or equality is not supported");
            return nullptr;
        }
        has_comparison = has_comparison || is_comparison;
        has_equality   = has_equality || is_equality;

        const auto location = CurrentLocation();
        Consume();
        auto right = ParseExpression(infix->right_binding_power);
        if (right == nullptr) {
            return nullptr;
        }
        left = std::make_unique<ast::BinaryExpr>(
            location, infix->operation, std::move(left), std::move(right));
    }
    return left;
}

}  // namespace parser
