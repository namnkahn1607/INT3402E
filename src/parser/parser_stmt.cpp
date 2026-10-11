// clang-format off
#include "parser/parser.h"
// clang-format on

#include <utility>

namespace parser {

ast::StmtPtr Parser::ParseStatement() {
    const auto loc = CurrentLocation();
    switch (PeekKind()) {
        case TokenKind::kw_int:
        case TokenKind::kw_bool: {
            auto declaration = ParseDeclaration();
            if (!declaration ||
                !Expect(TokenKind::semi, "expected ';' after declaration")) {
                return nullptr;
            }
            return std::make_unique<ast::DeclStmt>(loc, std::move(declaration));
        }
        case TokenKind::kw_if: return ParseIfStatement();
        case TokenKind::kw_for: return ParseForStatement();
        case TokenKind::kw_do: return ParseDoWhileStatement();
        case TokenKind::kw_print: return ParsePrintStatement();
        case TokenKind::l_brace: return ParseBlock();
        case TokenKind::identifier: {
            auto assignment = ParseAssignment();
            if (!assignment ||
                !Expect(TokenKind::semi, "expected ';' after assignment")) {
                return nullptr;
            }
            return assignment;
        }
        default:
            ReportSyntaxError(loc, "expected a supported statement");
            return nullptr;
    }
}

std::unique_ptr<ast::AssignStmt> Parser::ParseAssignment() {
    const auto* name_token = PeekToken();
    if (name_token == nullptr || name_token->kind != TokenKind::identifier) {
        ReportSyntaxError(CurrentLocation(),
                          "expected a valid assignment target");
        return nullptr;
    }
    const auto location = name_token->loc;
    auto       target =
        std::make_unique<ast::DeclRefExpr>(location, name_token->lexeme);
    Consume();
    if (!Expect(lexer::TokenKind::equal,
                "expected '=' after assignment target")) {
        return nullptr;
    }

    auto value = ParseExpression();
    if (value == nullptr) {
        return nullptr;
    }
    return std::make_unique<ast::AssignStmt>(location, std::move(target),
                                             std::move(value));
}

std::unique_ptr<ast::CompoundStmt> Parser::ParseBlock() {
    const auto loc = CurrentLocation();
    if (!Expect(TokenKind::l_brace, "expected '{' to start block")) {
        return nullptr;
    }
    auto block = std::make_unique<ast::CompoundStmt>(loc);
    while (!AtEnd() && PeekKind() != TokenKind::r_brace) {
        const auto start     = current_index_;
        auto       statement = ParseStatement();
        if (statement) {
            block->body.push_back(std::move(statement));
        } else {
            // A for parser has already synchronized its own failed header/body.
            if (tokens_[start].kind != TokenKind::kw_for) {
                RecoverStatement(start);
            }
            if (AtEnd()) {
                // A syntax failure already has a diagnostic. A lexical-only
                // failure must not hide the missing block closer at EOF.
                if (!has_syntax_errors_) {
                    ReportSyntaxError(CurrentLocation(),
                                      "expected '}' to close block");
                }
                return nullptr;
            }
        }
    }
    if (!Expect(TokenKind::r_brace, "expected '}' to close block")) {
        return nullptr;
    }
    return block;
}

ast::StmtPtr Parser::ParsePrintStatement() {
    const auto loc = Consume()->loc;  // Dispatch matched kw_print.
    if (!Expect(TokenKind::l_paren, "expected '(' after print")) {
        return nullptr;
    }
    auto value = ParseExpression();
    if (!value ||
        !Expect(TokenKind::r_paren, "expected ')' after print expression") ||
        !Expect(TokenKind::semi, "expected ';' after print")) {
        return nullptr;
    }
    return std::make_unique<ast::PrintStmt>(loc, std::move(value));
}

ast::StmtPtr Parser::ParseIfStatement() {
    const auto loc = Consume()->loc;
    if (!Expect(TokenKind::l_paren, "expected '(' after if")) {
        return nullptr;
    }
    auto condition = ParseExpression();
    if (!condition ||
        !Expect(TokenKind::r_paren, "expected ')' after if condition")) {
        return nullptr;
    }
    auto then_branch = ParseBlock();
    if (!then_branch) {
        return nullptr;
    }
    std::unique_ptr<ast::CompoundStmt> else_branch;
    if (Match(TokenKind::kw_else)) {
        else_branch = ParseBlock();
        if (!else_branch) {
            return nullptr;
        }
    }
    return std::make_unique<ast::IfStmt>(loc, std::move(condition),
                                         std::move(then_branch),
                                         std::move(else_branch));
}

ast::StmtPtr Parser::ParseDoWhileStatement() {
    const auto loc  = Consume()->loc;  // Dispatch matched kw_do.
    auto       body = ParseBlock();
    if (!body ||
        !Expect(TokenKind::kw_while, "expected 'while' after do body") ||
        !Expect(TokenKind::l_paren, "expected '(' after while")) {
        return nullptr;
    }
    auto condition = ParseExpression();
    if (!condition ||
        !Expect(TokenKind::r_paren, "expected ')' after do condition") ||
        !Expect(TokenKind::semi, "expected ';' after do/while")) {
        return nullptr;
    }
    return std::make_unique<ast::DoWhileStmt>(loc, std::move(body),
                                              std::move(condition));
}

ast::StmtPtr Parser::ParseForStatement() {
    const auto start = current_index_;
    const auto fail  = [&]() -> ast::StmtPtr {
        RecoverStatement(start);
        return nullptr;
    };
    const auto loc = Consume()->loc;
    if (!Expect(TokenKind::l_paren, "expected '(' after for")) {
        return fail();
    }
    ast::StmtPtr init;
    const auto   init_loc = CurrentLocation();
    if (PeekKind() == TokenKind::kw_int || PeekKind() == TokenKind::kw_bool) {
        auto declaration = ParseDeclaration();
        if (!declaration) {
            return fail();
        }
        init =
            std::make_unique<ast::DeclStmt>(init_loc, std::move(declaration));
    } else {
        auto assignment = ParseAssignment();
        if (!assignment) {
            return fail();
        }
        init = std::move(assignment);
    }
    if (!Expect(TokenKind::semi, "expected ';' after for initializer")) {
        return fail();
    }
    auto condition = ParseExpression();
    if (!condition ||
        !Expect(TokenKind::semi, "expected ';' after for condition")) {
        return fail();
    }
    auto step = ParseAssignment();
    if (!step || !Expect(TokenKind::r_paren, "expected ')' after for update")) {
        return fail();
    }
    auto body = ParseBlock();
    if (!body) {
        return fail();
    }
    return std::make_unique<ast::ForStmt>(loc, std::move(init),
                                          std::move(condition), std::move(step),
                                          std::move(body));
}

}  // namespace parser
