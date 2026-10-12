#pragma once

#include <span>
#include <string_view>

#include "common/diagnostic.h"
#include "lexer/token.h"

namespace parser {

using Token     = lexer::Token;
using TokenKind = lexer::TokenKind;

class Cursor final {
public:
    Cursor(std::string_view source, std::span<const Token> tokens,
           common::DiagnosisEngine& diag)
        : source_{source}, tokens_{tokens}, diagnostics_{diag} {
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

    // End-of-span is an observable EOF, not a physical token.
    // Lookahead must check bounds before addition, including oversized offsets.
    [[nodiscard]] bool AtEnd() const {
        return current_index_ >= tokens_.size();
    }

    // `nullptr` when lookahead reaches or exceeds the token span.
    [[nodiscard]] const Token* PeekToken(std::size_t lookahead = 0) const {
        if (AtEnd() || lookahead >= tokens_.size() - current_index_) {
            return nullptr;
        }
        return &tokens_[current_index_ + lookahead];
    }

    // `TokenKind::eof` when lookahead reaches or exceeds the token span.
    [[nodiscard]] TokenKind PeekKind(std::size_t lookahead = 0) const {
        const auto* token = PeekToken(lookahead);
        return token != nullptr ? token->kind : TokenKind::eof;
    }

    // `nullptr` immediately at EOF without advancing. Parser/recovery loops
    // must explicitly stop/yield at EOF.
    const Token* Consume() {
        const auto* token = PeekToken();
        if (token != nullptr) {
            ++current_index_;
        }
        return token;
    }

    // Returns `false` on `TokenKind::eof`: EOF is not consumable.
    bool Match(TokenKind kind) {
        if (kind == TokenKind::eof || PeekKind() != kind) {
            return false;
        }
        Consume();
        return true;
    }

    // On failure, diagnose without choosing a contextual synchronization
    // strategy. Observe EOF with `AtEnd()`, not `Expect(TokenKind::eof)`.
    bool Expect(TokenKind kind, std::string_view msg) {
        if (Match(kind)) {
            return true;
        }
        ReportSyntaxError(CurrentLocation(), msg);
        return false;
    }

    // At EOF, use the 1-based position after all source text, including trivia.
    [[nodiscard]] common::SourceLocation CurrentLocation() const {
        const auto* token = PeekToken();
        return token != nullptr ? token->loc : eof_location_;
    }

    // Set local syntax-error status, then report through the shared engine,
    // except at already-diagnosed invalid spelling tokens: propagate failure
    // without re-reporting the lexical error or changing local syntax status.
    // Its sticky return value/HasErrors() cannot determine local parse success.
    void ReportSyntaxError(common::SourceLocation loc, std::string_view msg) {
        const auto* token = PeekToken();
        if (token &&
            (token->kind == TokenKind::invalid_identifier ||
             token->kind == TokenKind::invalid_numeric_constant) &&
            token->loc.line == loc.line && token->loc.col == loc.col) {
            // Classification guarantees the lexer has already diagnosed this.
            // The caller still returns failure for contextual recovery.
            return;
        }
        has_syntax_errors_ = true;
        diagnostics_.Report({.line     = loc.line,
                             .col      = loc.col,
                             .severity = common::Severity::kError,
                             .message  = std::string{msg}});
    }

    // Resets the cursor back to its primary state.
    void Reset() noexcept {
        current_index_     = 0;
        has_syntax_errors_ = false;
    }

    [[nodiscard]] std::size_t CurrentIndex() const noexcept {
        return current_index_;
    }

    [[nodiscard]] const Token& GetToken(std::size_t index) const {
        return tokens_[index];
    }

    [[nodiscard]] bool HasSyntaxErrors() const noexcept {
        return has_syntax_errors_;
    }

private:
    std::string_view         source_;
    std::span<const Token>   tokens_;
    common::DiagnosisEngine& diagnostics_;
    std::size_t              current_index_ = 0;
    common::SourceLocation   eof_location_{};
    bool                     has_syntax_errors_ = false;
};

}  // namespace parser
