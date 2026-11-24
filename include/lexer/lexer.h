#pragma once

#include <string>
#include <vector>

#include "common/diagnostic.h"
#include "lexer/token.h"

namespace lexer {

class Lexer final {
public:
    explicit Lexer(common::DiagnosisEngine& diag) : diag_{diag} {}
    ~Lexer() = default;

    Lexer(const Lexer&)            = delete;
    Lexer& operator=(const Lexer&) = delete;
    Lexer(Lexer&&)                 = delete;
    Lexer& operator=(Lexer&&)      = delete;

    // Returns non-EOF tokens, retaining diagnosed unknown/invalid spellings and
    // continuing scanning. Empty input or scanner failure returns no tokens;
    // inspect shared diagnostics for compilation errors.
    std::vector<Token> Tokenize(const std::string& src_text);

private:
    common::DiagnosisEngine& diag_;
};

}  // namespace lexer
