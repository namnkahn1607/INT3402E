%{
#include "frontend/lexer/lexer_extra.h"
#include "frontend/lexer/token.h" /* Token enum comes from here */

#define TOK(x) return static_cast<int>(Token::x)
%}

%option noyywrap
%option nounput
%option noinput
%option reentrant
%option yylineno

/* next_col = column of the NEXT character to be scanned; while
 * start_col = column where the token matched by the most recent
 * rule began. YY_USER_ACTION updates both on every match. */
%option extra-type="LexerExtra"

%{
#define YY_USER_ACTION \
    yyextra.start_col = yyextra.next_col; \
    yyextra.next_col += yyleng;
%}

DIGIT   [0-9]
ALPHA   [a-zA-Z_]
ALNUM   [a-zA-Z0-9_]

%%

"//".*                      { /* line comment: consume without token */ }
"/*"([^*]|\*+[^*/])*\*+"/"  { /* block comment: consume without token */ }

[ \t\r]+  { /* whitespace: consume without token */ }
\n        { yyextra.next_col = 1; }

"bool"      { TOK(kw_bool); }
"break"     { TOK(kw_break); }
"case"      { TOK(kw_case); }
"const"     { TOK(kw_const); }
"continue"  { TOK(kw_continue); }
"default"   { TOK(kw_default); }
"double"    { TOK(kw_double); }
"else"      { TOK(kw_else); }
"false"     { TOK(kw_false); }
"float"     { TOK(kw_float); }
"for"       { TOK(kw_for); }
"if"        { TOK(kw_if); }
"int"       { TOK(kw_int); }
"long"      { TOK(kw_long); }
"return"    { TOK(kw_return); }
"short"     { TOK(kw_short); }
"switch"    { TOK(kw_switch); }
"true"      { TOK(kw_true); }
"void"      { TOK(kw_void); }
"while"     { TOK(kw_while); }

{ALPHA}{ALNUM}*             { TOK(identifier); }
{DIGIT}+(\.{DIGIT}+)?[fF]?  { TOK(numeric_constant); }

"("  { TOK(l_paren); }
")"  { TOK(r_paren); }
"{"  { TOK(l_brace); }
"}"  { TOK(r_brace); }
";"  { TOK(semi); }
":"  { TOK(colon); }

"=="  { TOK(equalequal); }
"="   { TOK(equal); }
"!"   { TOK(exclaim); }
"++"  { TOK(plusplus); }
"+"   { TOK(plus); }
"--"  { TOK(minusminus); }
"-"   { TOK(minus); }
"*"   { TOK(star); }
"/"   { TOK(slash); }
"<="  { TOK(lessequal); }
"<"   { TOK(less); }
">="  { TOK(greaterequal); }
">"   { TOK(greater); }
"&&"  { TOK(ampamp); }
"||"  { TOK(pipepipe); }

<<EOF>>  { TOK(eof); }
.        { TOK(unknown); } /* adapter treats this as a lexical error */

%%
