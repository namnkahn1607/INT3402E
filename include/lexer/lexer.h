#pragma once

#include <string>
#include <vector>

#include "support/diagnostic.h"
#include "token.h"

namespace lexer {

class Lexer final {
public:
    Lexer(support::DiagnosisEngine& diag) : diag_{diag} {};
    ~Lexer() = default;

    Lexer(const Lexer&)            = delete;
    Lexer& operator=(const Lexer&) = delete;
    Lexer(Lexer&&)                 = delete;
    Lexer& operator=(Lexer&&)      = delete;

    // Returns empty token array upon failure.
    std::vector<Token> Tokenize(const std::string& src_text) noexcept;

private:
    support::DiagnosisEngine& diag_;
};

};  // namespace lexer
