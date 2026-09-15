#include "lexer/lexer.h"

#include <string>
#include <vector>

#include "lexer.yy.h"
#include "lexer/lexer_extra.h"
#include "lexer/token.h"
#include "support/diagnostic.h"

namespace lexer {

std::vector<Token> Lexer::Tokenize(const std::string& src_text) noexcept {
    yyscan_t scanner;
    if (yylex_init_extra(LexerExtra{}, &scanner) != 0) {
        diag_.Report(
            {0, 0, support::Severity::kError, "lexer initialization failed"}
        );
        return {};
    }

    // Copy data into flex's internal buffer.
    YY_BUFFER_STATE buf = yy_scan_bytes(
        src_text.data(), static_cast<int>(src_text.size()), scanner
    );

    std::vector<Token> tokens;

    int kind;
    while ((kind = yylex(scanner)) != static_cast<int>(TokenKind::eof)) {
        Token tok;
        tok.kind   = static_cast<TokenKind>(kind);
        tok.lexeme = std::string(
            yyget_text(scanner), static_cast<size_t>(yyget_leng(scanner))
        );
        tok.line = yyget_lineno(scanner);
        tok.col  = yyget_extra(scanner).start_col;

        if (tok.kind == TokenKind::unknown) {
            support::Diagnostic d{
                tok.line, tok.col, support::Severity::kError,
                "unexpected token: '" + tok.lexeme + "'"
            };
            diag_.Report(d);
            return {};
        }

        tokens.push_back(std::move(tok));
    }

    yy_delete_buffer(buf, scanner);
    yylex_destroy(scanner);
    return tokens;
}

};  // namespace lexer
