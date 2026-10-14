#include <cstring>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

#include "ast/translation_unit.h"
#include "common/diagnostic.h"
#include "common/source.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

int main(int argc, char* argv[]) {
    bool        dump_ast    = false;
    const char* source_file = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dump-ast") == 0) {
            dump_ast = true;
        } else if (source_file == nullptr) {
            source_file = argv[i];
        } else {
            std::cerr << "Usage: int3402e [--dump-ast] [source-file]\n";
            return 1;
        }
    }

    common::DiagnosisEngine diag;

    std::string source;
    if (source_file != nullptr) {
        auto loaded = common::LoadSource(source_file, diag);
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

    if (dump_ast) {
        parser::Parser parser{source, tokens, diag};
        auto           result = parser.ParseTranslationUnit();
        if (diag.HasErrors()) {
            diag.Summarize();
            return 1;
        }
        ast::DumpAst(*result, std::cout);
        return 0;
    }

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
