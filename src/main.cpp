#include <iostream>
#include <iterator>
#include <string>
#include <utility>

#include "common/diagnostic.h"
#include "common/source_load.h"
#include "frontend/lexer/lexer.h"

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "Usage: int3402e [source-file]\n";
        return 1;
    }

    common::DiagnosisEngine diag;

    std::string source;
    if (argc == 2) {
        auto loaded = common::LoadSourceFile(argv[1], diag);
        if (!loaded) {
            diag.Summarize();
            return 1;
        }
        source = std::move(loaded->text);
    } else {
        source.assign(std::istreambuf_iterator<char>(std::cin), {});
        if (std::cin.bad()) {
            std::cerr << "Failed to read standard input\n";
            return 1;
        }
    }

    lexer::Lexer lex{diag};
    const auto   tokens = lex.Tokenize(source);
    if (diag.HasErrors()) {
        diag.Summarize();
        return 1;
    }

    for (const auto& token : tokens) {
        std::cout << "token = " << static_cast<int>(token.kind)
                  << ", lexeme = \"" << token.lexeme << "\"\n";
    }

    return 0;
}
