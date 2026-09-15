#pragma once

#include <string>

namespace lexer {

enum class TokenKind : int {
    unknown,
    eof,

    // Keywords
    kw_bool,      // bool
    kw_break,     // break
    kw_case,      // case
    kw_const,     // const
    kw_continue,  // continue
    kw_default,   // default
    kw_double,    // double
    kw_else,      // else
    kw_false,     // false
    kw_float,     // float
    kw_for,       // for
    kw_if,        // if
    kw_int,       // int
    kw_long,      // long
    kw_return,    // return
    kw_short,     // short
    kw_switch,    // switch
    kw_true,      // true
    kw_void,      // void
    kw_while,     // while

    // Starts with a letter or underscore, followed by letters, digits, or
    // underscores.
    identifier,
    // Decimal integers and floating-point constants with an optional f/F
    // suffix.
    numeric_constant,

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
    int         line, col;
};

}  // namespace lexer
