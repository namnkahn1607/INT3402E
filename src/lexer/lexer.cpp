#include "lexer/lexer.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/source_location.h"
#include "lexer.yy.h"

namespace lexer {

std::vector<Token> Lexer::Tokenize(const std::string& src_text) {
    if (src_text.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        diag_.Report({.line     = 0,
                      .col      = 0,
                      .severity = common::Severity::kError,
                      .message  = "source exceeds scanner limit"});
        return {};
    }

    yyscan_t scanner = nullptr;
    if (yylex_init_extra(LexerExtra{}, &scanner) != 0) {
        diag_.Report({.line     = 0,
                      .col      = 0,
                      .severity = common::Severity::kError,
                      .message  = "lexer initialization failed"});
        return {};
    }

    // Flex destroys its current buffer with the scanner, including on early
    // returns.
    const std::unique_ptr<void, decltype(&yylex_destroy)> owner(scanner,
                                                                &yylex_destroy);
    yy_scan_bytes(src_text.data(), static_cast<int>(src_text.size()), scanner);
    yyset_lineno(1, scanner);

    std::vector<Token> tokens;
    int                kind;
    while ((kind = yylex(scanner)) != static_cast<int>(TokenKind::eof)) {
        Token tok{.kind   = static_cast<TokenKind>(kind),
                  .lexeme = std::string(
                      yyget_text(scanner),
                      static_cast<std::size_t>(yyget_leng(scanner))),
                  .loc = common::SourceLocation{
                      .line = yyget_lineno(scanner),
                      .col  = yyget_extra(scanner).start_col}};

        if (tok.kind == TokenKind::unknown ||
            tok.kind == TokenKind::invalid_identifier ||
            tok.kind == TokenKind::invalid_numeric_constant) {
            const char* prefix = "unexpected token: '";
            if (tok.kind == TokenKind::invalid_identifier) {
                prefix = "invalid identifier: '";
            } else if (tok.kind == TokenKind::invalid_numeric_constant) {
                prefix = "invalid numeric constant: '";
            }
            diag_.Report({.line     = tok.loc.line,
                          .col      = tok.loc.col,
                          .severity = common::Severity::kError,
                          .message  = prefix + tok.lexeme + "'"});
        }

        tokens.push_back(std::move(tok));
    }

    return tokens;
}

}  // namespace lexer
