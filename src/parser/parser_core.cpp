// clang-format off
#include "parser/parser.h"
// clang-format on

#include <memory>
#include <utility>

namespace parser {

Parser::Parser(std::string_view source, std::span<const Token> tokens,
               common::DiagnosisEngine& diagnostics)
    : cursor_{source, tokens, diagnostics}, expr_parser_{cursor_} {}

std::unique_ptr<ast::TranslationUnit> Parser::ParseTranslationUnit() {
    cursor_.Reset();

    auto unit = std::make_unique<ast::TranslationUnit>();
    while (!cursor_.AtEnd()) {
        if (cursor_.PeekKind() == TokenKind::r_brace) {
            // There is no enclosing block owner at file scope. Consume this
            // boundary rather than repeatedly yielding it during recovery.
            cursor_.ReportSyntaxError(cursor_.CurrentLocation(),
                                      "unexpected '}' at file scope");
            cursor_.Consume();
            continue;
        }
        if (auto statement = ParseStatementOrRecover()) {
            unit->statements.push_back(std::move(statement));
        }
    }
    return unit;
}

bool Parser::ExpectTerminator(std::string_view message) {
    if (cursor_.Match(TokenKind::semi)) {
        return true;
    }
    // The statement itself parsed completely; only its ';' is missing. If the
    // next token is on a later line (or closes the block / file), the missing
    // ';' belongs right after the previous token. Report it there and keep the
    // statement, so the next statement is not swallowed by recovery.
    const auto* next     = cursor_.PeekToken();
    const auto  curr_idx = cursor_.CurrentIndex();
    const bool  complete =
        curr_idx > 0 &&
        (next == nullptr || next->kind == TokenKind::r_brace ||
         next->loc.line > cursor_.GetToken(curr_idx - 1).loc.line);
    auto loc = cursor_.CurrentLocation();
    if (complete) {
        const auto& previous = cursor_.GetToken(curr_idx - 1);
        loc                  = previous.loc;
        loc.col += static_cast<decltype(loc.col)>(previous.lexeme.size());
    }
    cursor_.ReportSyntaxError(loc, message);
    return complete;
}

ast::StmtPtr Parser::ParseStatementOrRecover() {
    // Only caller of RecoverStatement(): every ParseXxx below reports its
    // error once and returns nullptr; this function alone skips tokens.
    const auto start      = cursor_.CurrentIndex();
    const auto start_kind = cursor_.PeekKind();
    auto       statement  = ParseStatement();
    if (statement) {
        return statement;
    }
    RecoverStatement(start);
    // Recovery from a failed `if` skips through its then-block. A trailing
    // `else` would then start a statement of its own and produce a bogus
    // second error, so skip that clause as well.
    if (start_kind == TokenKind::kw_if &&
        cursor_.PeekKind() == TokenKind::kw_else) {
        const auto else_start = cursor_.CurrentIndex();
        cursor_.Consume();
        RecoverStatement(else_start);
    }
    return nullptr;
}

void Parser::RecoverStatement(std::size_t statement_start) {
    std::size_t braces      = 0;
    std::size_t parentheses = 0;
    bool        for_header =
        cursor_.GetToken(statement_start).kind == TokenKind::kw_for;

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
    for (std::size_t index = statement_start; index < cursor_.CurrentIndex();
         ++index) {
        track(cursor_.GetToken(index).kind);
    }
    while (!cursor_.AtEnd()) {
        const auto kind = cursor_.PeekKind();
        if (kind == TokenKind::r_brace && braces == 0) {
            return;  // The enclosing block consumes this boundary.
        }
        if (kind == TokenKind::semi && braces == 0 && !for_header) {
            cursor_.Consume();
            return;
        }
        track(kind);
        cursor_.Consume();
        if (kind == TokenKind::r_brace && braces == 0) {
            return;  // A skipped nested body is also a safe boundary.
        }
    }
}

}  // namespace parser
