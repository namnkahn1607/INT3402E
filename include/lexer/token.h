#pragma once

#include <cstdint>
#include <string>

#include "common/source_location.h"

namespace lexer {

enum class TokenKind : std::uint8_t {
    unknown,
    eof,

    // Keywords
    kw_bool,      // bool
    kw_break,     // break
    kw_case,      // case
    kw_const,     // const
    kw_continue,  // continue
    kw_default,   // default
    kw_do,        // do
    kw_double,    // double
    kw_else,      // else
    kw_false,     // false
    kw_float,     // float
    kw_for,       // for
    kw_if,        // if
    kw_int,       // int
    kw_long,      // long
    kw_print,     // print
    kw_return,    // return
    kw_short,     // short
    kw_switch,    // switch
    kw_true,      // true
    kw_void,      // void
    kw_while,     // while

    // ASCII letters followed by optional trailing digits: [A-Za-z]+[0-9]*.
    identifier,
    // Decimal integer spelling: [0-9]+. No range checking during lexing.
    numeric_constant,
    // Whole broader candidates rejected and diagnosed by the lexer.
    invalid_identifier,
    invalid_numeric_constant,

    l_paren,  // (
    r_paren,  // )
    l_brace,  // {
    r_brace,  // }
    semi,     // ;
    colon,    // :

    equalequal,    // ==
    equal,         // =
    exclaim,       // !
    plusplus,      // ++
    plus,          // +
    minusminus,    // --
    minus,         // -
    star,          // *
    slash,         // /
    lessequal,     // <=
    less,          // <
    greaterequal,  // >=
    greater,       // >
    ampamp,        // &&
    pipepipe       // ||
};

struct Token {
    TokenKind   kind;
    std::string lexeme;

    common::SourceLocation loc;
};

}  // namespace lexer
