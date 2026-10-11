// clang-format off
#include "parser/parser.h"
// clang-format on

#include <string>
#include <utility>

namespace parser {

Parser::Parser(std::string_view source, std::span<const lexer::Token> tokens,
               common::DiagnosisEngine& diagnostics)
    : source_{source}
    , tokens_{tokens}
    , diagnostics_{diagnostics}
    , eof_location_{.line = 1, .col = 1} {
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

ParseResult Parser::ParseTranslationUnit() {
    current_index_     = 0;
    has_syntax_errors_ = false;
    auto unit          = std::make_unique<ast::TranslationUnit>();
    while (!AtEnd()) {
        if (PeekKind() == TokenKind::r_brace) {
            // There is no enclosing block owner at file scope. Consume this
            // boundary rather than repeatedly yielding it during recovery.
            ReportSyntaxError(CurrentLocation(),
                              "unexpected '}' at file scope");
            Consume();
            continue;
        }
        const auto start     = current_index_;
        auto       statement = ParseStatement();
        if (statement) {
            unit->statements.push_back(std::move(statement));
        } else if (tokens_[start].kind != TokenKind::kw_for) {
            // Failed for parsers already own their synchronization.
            RecoverStatement(start);
        }
    }
    return {.translation_unit  = std::move(unit),
            .has_syntax_errors = has_syntax_errors_};
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
    diagnostics_.Report({.line     = loc.line,
                         .col      = loc.col,
                         .severity = common::Severity::kError,
                         .message  = std::string{message}});
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

}  // namespace parser
