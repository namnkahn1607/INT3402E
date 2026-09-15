#include "lexer/lexer.h"
#include "support/diagnostic.h"
#include "support/source_load.h"

int main([[maybe_unused]] int argc, char* argv[]) {
    support::DiagnosisEngine diag;

    auto opt_src = support::LoadSourceFile(argv[1], diag);
    if (!opt_src.has_value()) {
        diag.Summarize();
        return 1;
    }

    lexer::Lexer lex{diag};

    auto tokens = lex.Tokenize(opt_src.value().text);
    if (tokens.empty()) {
        diag.Summarize();
        return 1;
    }

    return 0;
}
