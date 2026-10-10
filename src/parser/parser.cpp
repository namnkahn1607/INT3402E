#include "parser/parser.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace parser {

using lexer::TokenKind;

Parser::Parser(std::string_view source, std::span<const lexer::Token> tokens,
               common::DiagnosisEngine& diagnostics)
    : source_{source}
    , tokens_{tokens}
    , diagnostics_{diagnostics}
    , eof_location_{1, 1} {
    // Match Flex's byte columns: only LF starts a new line, including in
    // trivia.
    for (const char ch : source_) {
        if (ch == '\n') {
            ++eof_location_.line;
            eof_location_.col = 1;
        } else {
            ++eof_location_.col;
        }
    }
}

bool Parser::AtEnd() const { return current_index_ >= tokens_.size(); }

const lexer::Token* Parser::PeekToken(std::size_t lookahead) const {
    if (AtEnd() || lookahead >= tokens_.size() - current_index_) {
        return nullptr;
    }
    return &tokens_[current_index_ + lookahead];
}

TokenKind Parser::PeekKind(std::size_t lookahead) const {
    const auto* token = PeekToken(lookahead);
    return token ? token->kind : TokenKind::eof;
}

const lexer::Token* Parser::Consume() {
    const auto* token = PeekToken();
    if (token) {
        ++current_index_;
    }
    return token;
}

bool Parser::Match(TokenKind kind) {
    if (kind == TokenKind::eof || PeekKind() != kind) {
        return false;
    }
    Consume();
    return true;
}

bool Parser::Expect(TokenKind kind, std::string_view message) {
    if (Match(kind)) {
        return true;
    }
    ReportSyntaxError(CurrentLocation(), message);
    return false;
}

common::SourceLocation Parser::CurrentLocation() const {
    const auto* token = PeekToken();
    return token ? token->loc : eof_location_;
}

void Parser::ReportSyntaxError(common::SourceLocation loc,
                               std::string_view       message) {
    const auto* token = PeekToken();
    if (token &&
        (token->kind == TokenKind::invalid_identifier ||
         token->kind == TokenKind::invalid_numeric_constant) &&
        token->loc.line == loc.line && token->loc.col == loc.col) {
        // Classification guarantees the lexer has already diagnosed this
        // spelling. The caller still returns failure for contextual recovery.
        return;
    }
    has_syntax_errors_ = true;
    diagnostics_.Report(
        {loc.line, loc.col, common::Severity::kError, std::string{message}});
}

ParseResult Parser::ParseTranslationUnit() {
    current_index_     = 0;
    has_syntax_errors_ = false;
    auto unit          = std::make_unique<ast::TranslationUnit>();
    auto main          = ParseMainFunction();
    if (main) {
        unit->decls.push_back(std::move(main));
        if (!AtEnd()) {
            ReportSyntaxError(CurrentLocation(),
                              "unexpected tokens after main function");
        }
    }
    return {std::move(unit), has_syntax_errors_};
}

std::unique_ptr<ast::FuncDecl> Parser::ParseMainFunction() {
    const auto loc = CurrentLocation();
    if (!Expect(TokenKind::kw_int, "expected 'int main()'")) {
        return nullptr;
    }
    const auto* name = PeekToken();
    if (!name || name->kind != TokenKind::identifier ||
        name->lexeme != "main") {
        ReportSyntaxError(CurrentLocation(), "expected 'main'");
        return nullptr;
    }
    Consume();
    if (!Expect(TokenKind::l_paren, "expected '(' after main") ||
        !Expect(TokenKind::r_paren,
                "expected ')' after main (parameters are unsupported)")) {
        return nullptr;
    }
    auto body = ParseBlock();
    if (!body) {
        return nullptr;
    }
    return std::make_unique<ast::FuncDecl>(loc, ast::Type::Int, "main",
                                           std::vector<ast::Param>{},
                                           std::move(body));
}

void Parser::RecoverStatement(std::size_t statement_start) {
    std::size_t braces      = 0;
    std::size_t parentheses = 0;
    bool        for_header = tokens_[statement_start].kind == TokenKind::kw_for;

    const auto track = [&](TokenKind kind) {
        if (kind == TokenKind::l_paren) {
            ++parentheses;
        } else if (kind == TokenKind::r_paren) {
            if (parentheses > 0) {
                --parentheses;
            }
            if (parentheses == 0) {
                for_header = false;
            }
        } else if (kind == TokenKind::l_brace) {
            ++braces;
            // An identifiable body ends an uncertain malformed for header.
            for_header = false;
        } else if (kind == TokenKind::r_brace && braces > 0) {
            --braces;
        }
    };

    // Account for delimiters already consumed by the failed nested parser.
    for (std::size_t index = statement_start; index < current_index_; ++index) {
        track(tokens_[index].kind);
    }
    while (!AtEnd()) {
        const auto kind = PeekKind();
        if (kind == TokenKind::r_brace && braces == 0) {
            return;  // The enclosing block consumes this boundary.
        }
        if (kind == TokenKind::semi && braces == 0 && !for_header) {
            Consume();
            return;
        }
        track(kind);
        Consume();
        if (kind == TokenKind::r_brace && braces == 0) {
            return;  // A skipped nested body is also a safe boundary.
        }
    }
}

// Declarations and assignments.

std::optional<ast::Type> Parser::ParseType() {
    if (Match(lexer::TokenKind::kw_int)) {
        return ast::Type::Int;
    }
    if (Match(lexer::TokenKind::kw_bool)) {
        return ast::Type::Bool;
    }
    ReportSyntaxError(CurrentLocation(), "expected 'int' or 'bool'");
    return std::nullopt;
}

std::unique_ptr<ast::VarDecl> Parser::ParseDeclaration() {
    const auto location = CurrentLocation();
    const auto type     = ParseType();
    if (!type) {
        return nullptr;
    }

    const auto* name_token = PeekToken();
    if (name_token == nullptr || name_token->kind != TokenKind::identifier) {
        ReportSyntaxError(CurrentLocation(), "expected a valid variable name");
        return nullptr;
    }
    std::string name = name_token->lexeme;
    Consume();

    ast::ExprPtr initializer;
    if (Match(lexer::TokenKind::equal)) {
        initializer = ParseExpression();
        if (initializer == nullptr) {
            // A failed initializer is not an uninitialized declaration.
            return nullptr;
        }
    }
    return std::make_unique<ast::VarDecl>(location, *type, std::move(name),
                                          std::move(initializer));
}

std::unique_ptr<ast::AssignExpr> Parser::ParseAssignment() {
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
    return std::make_unique<ast::AssignExpr>(location, std::move(target),
                                             std::move(value));
}

// Statements and control flow.

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
            return std::make_unique<ast::ExprStmt>(loc, std::move(assignment));
        }
        default:
            ReportSyntaxError(loc, "expected a supported statement");
            return nullptr;
    }
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
        init = std::make_unique<ast::ExprStmt>(init_loc, std::move(assignment));
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

// Pratt expressions.

namespace {

struct InfixOperator {
    ast::BinaryOp operation;
    int           left_binding_power;
    int           right_binding_power;
};

std::optional<InfixOperator> GetInfixOperator(lexer::TokenKind kind) {
    switch (kind) {
        case lexer::TokenKind::equalequal:
            return InfixOperator{ast::BinaryOp::Eq, 1, 2};
        case lexer::TokenKind::greater:
            return InfixOperator{ast::BinaryOp::Gt, 3, 4};
        case lexer::TokenKind::greaterequal:
            return InfixOperator{ast::BinaryOp::Ge, 3, 4};
        case lexer::TokenKind::plus:
            return InfixOperator{ast::BinaryOp::Add, 5, 6};
        case lexer::TokenKind::star:
            return InfixOperator{ast::BinaryOp::Mul, 7, 8};
        default: return std::nullopt;
    }
}

bool IsCallerBoundary(lexer::TokenKind kind) {
    return kind == lexer::TokenKind::semi ||
           kind == lexer::TokenKind::r_paren ||
           kind == lexer::TokenKind::r_brace || kind == lexer::TokenKind::eof;
}

}  // namespace

ast::ExprPtr Parser::ParsePrimary() {
    const auto* token = PeekToken();
    if (token == nullptr) {
        ReportSyntaxError(CurrentLocation(), "expected an expression");
        return nullptr;
    }

    switch (token->kind) {
        case lexer::TokenKind::identifier: {
            auto expression =
                std::make_unique<ast::DeclRefExpr>(token->loc, token->lexeme);
            Consume();
            return expression;
        }
        case lexer::TokenKind::numeric_constant: {
            auto expression = std::make_unique<ast::IntLiteralExpr>(
                token->loc, token->lexeme);
            Consume();
            return expression;
        }
        case lexer::TokenKind::kw_true:
        case lexer::TokenKind::kw_false: {
            auto expression = std::make_unique<ast::BoolLiteralExpr>(
                token->loc, token->kind == lexer::TokenKind::kw_true);
            Consume();
            return expression;
        }
        case lexer::TokenKind::l_paren: {
            Consume();
            auto expression = ParseExpression();
            if (expression == nullptr) {
                return nullptr;
            }
            if (!Expect(lexer::TokenKind::r_paren,
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

        const bool is_comparison = kind == lexer::TokenKind::greater ||
                                   kind == lexer::TokenKind::greaterequal;
        const bool is_equality   = kind == lexer::TokenKind::equalequal;
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
