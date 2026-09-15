#include <iostream>

#include "frontend/lexer/scanner.h"
#include "frontend/lexer/token.h"

int main() {
    LexerExtra extra{};
    yyscan_t   scanner = nullptr;

    if (yylex_init_extra(extra, &scanner) != 0) {
        std::cerr << "Failed to initialize lexer\n";
        return 1;
    }

    while (true) {
        const int token = yylex(scanner);

        // The scanner returns our Token enum, including its explicit EOF value.
        if (token == static_cast<int>(Token::eof)) {
            break;
        }

        std::cout << "token = " << token << ", lexeme = \""
                  << yyget_text(scanner) << "\"\n";
    }

    yylex_destroy(scanner);

    return 0;
}
