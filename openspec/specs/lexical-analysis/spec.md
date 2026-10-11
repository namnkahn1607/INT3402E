# Lexical Analysis Specification

## Purpose

The compiler provides lexical analysis of in-memory source text. Lexical
analysis converts source text into an ordered stream of tokens, preserving
each token's spelling and starting source location. This capability describes
token recognition only; it does not establish any parser, AST construction,
semantic analysis, or end-to-end language acceptance behavior.

## Requirements

### Requirement: Tokenize source text

The system SHALL tokenize an in-memory source string into an ordered sequence
of non-EOF tokens. Each returned token SHALL contain its token kind, its exact
matched lexeme, and the one-based line and column at which the token starts.

Empty input SHALL produce no tokens and no diagnostic error. End of input
SHALL terminate tokenization and SHALL NOT be returned as a token.

#### Scenario: Tokenizing a simple declaration

- **WHEN** the source text is `int answer = 42;`
- **THEN** the token sequence is `kw_int`, `identifier`, `equal`,
  `numeric_constant`, and `semi`
- **AND THEN** the identifier lexeme is `answer`
- **AND THEN** the numeric-constant lexeme is `42`

#### Scenario: Tokenizing empty input

- **WHEN** the source text is empty
- **THEN** tokenization returns an empty token sequence
- **AND THEN** no diagnostic error is reported

### Requirement: Recognize keywords and identifiers

The system SHALL recognize the exact keywords `bool`, `break`, `case`,
`const`, `continue`, `default`, `do`, `double`, `else`, `false`, `float`, `for`,
`if`, `int`, `long`, `print`, `return`, `short`, `switch`, `true`, `void`, and
`while`. `do` and `print` SHALL have kinds `kw_do` and `kw_print`.
`main` SHALL remain an identifier.

Valid identifiers SHALL match `[A-Za-z]+[0-9]*` excluding reserved keywords.
The lexer SHALL classify a keyword only when its full matched lexeme is one of
the listed spellings. Longer valid names containing keyword prefixes SHALL remain
identifiers. Unsupported reserved keywords SHALL remain recognizable keywords;
recognition SHALL NOT imply language-level support.

#### Scenario: Keeping a keyword prefix in an identifier

- **WHEN** the source text is `integer do1 done print1 printed true1 main`
- **THEN** each spelling is returned as one `identifier` token

#### Scenario: Classifying statement keywords and digit suffixes

- **WHEN** the source is `do print answer12 AbZ009`
- **THEN** the kinds are `kw_do`, `kw_print`, `identifier`, and `identifier`
- **AND** no error is reported

### Requirement: Recognize numeric constants

Valid numeric constants SHALL match `[0-9]+` and retain the kind
`numeric_constant` and exact spelling, including leading zeros. Broader numeric
candidates recognized by the scanner SHALL be retained as
`invalid_numeric_constant`, not split into apparently valid integer tokens.
Candidate recognition SHALL retain the existing fractional, exponent, and suffix
forms: `[0-9]+(\.[0-9]+)?[fF]?`, `([0-9]*\.[0-9]+|[0-9]+\.)`
with optional `[eE][+-]?[0-9]+` and optional `[fFlL]`, and
`[0-9]+[eE][+-]?[0-9]+[fFlL]?`. This SHALL NOT add new numeric syntax.

#### Scenario: Recognizing representative numeric constants

- **WHEN** the source text is `42 3.14 .5 3. 1e10 1E-5 3.14f`
- **THEN** `42` is one `numeric_constant` token
- **AND** each other spelling is one diagnosed `invalid_numeric_constant` token

#### Scenario: Preserving integer spelling

- **WHEN** the source is `0 00042 99999999999999999999999999999999999999`
- **THEN** each spelling is retained as one valid `numeric_constant` token
- **AND** lexing does not perform integer range checking

### Requirement: Recognize delimiters and operators

The system SHALL recognize the delimiters `(`, `)`, `{`, `}`, `;`, and `:`.

The system SHALL recognize the operators `==`, `=`, `!`, `++`, `+`, `--`,
`-`, `*`, `/`, `<=`, `<`, `>=`, `>`, `&&`, and `||`.

#### Scenario: Distinguishing overlapping operators

- **WHEN** the source text is `== = ++ +`
- **THEN** the token sequence is `equalequal`, `equal`, `plusplus`, and `plus`

### Requirement: Ignore whitespace and comments

The system SHALL omit spaces, tabs, carriage returns, newline characters,
line comments beginning with `//`, and closed block comments delimited by
`/*` and `*/` from the returned token sequence.

#### Scenario: Skipping comments around a declaration

- **WHEN** the source text is `/* comment */ int answer = 42; // end` followed
  by a newline
- **THEN** the token sequence is `kw_int`, `identifier`, `equal`,
  `numeric_constant`, and `semi`

### Requirement: Track token start locations

The system SHALL report token locations as the one-based start line and column
in the supplied source text. Consumed whitespace and comments, including
newlines inside a closed block comment, SHALL advance the location of a later
token.

#### Scenario: Tracking a token after a multiline block comment

- **WHEN** the source text is `int`, a newline, `/* a`, a newline, `b */ ans`
- **THEN** `int` starts at line 1, column 1
- **AND THEN** `ans` starts at line 3, column 6

#### Scenario: Resetting location state for a new tokenization

- **WHEN** a lexer tokenizes a source beginning with a newline and then
  tokenizes `ans`
- **THEN** `ans` starts at line 1, column 1 in the second tokenization

### Requirement: Report and retain unknown characters

When the system encounters a character that matches no lexical category, it
SHALL return that character as an `unknown` token, report an error diagnostic
at that token's start location with the message prefix `unexpected token:`,
and continue scanning later input.

#### Scenario: Reporting an unknown character without discarding tokens

- **WHEN** the source text is `int @`
- **THEN** the token sequence contains `kw_int` followed by `unknown`
- **AND THEN** the unknown token lexeme is `@`
- **AND THEN** the diagnostic engine records an error

#### Scenario: Continuing after multiple unknown characters

- **WHEN** source text contains more than one unknown character
- **THEN** the system reports an error for each unknown character
- **AND THEN** scanning continues through the remaining source text

### Requirement: Diagnose and retain invalid spellings

The lexer SHALL match complete identifier candidates `[A-Za-z_][A-Za-z0-9_]*`
before classifying their spelling. Candidates outside the valid identifier rule
SHALL have kind `invalid_identifier`. Invalid numeric candidates SHALL have kind
`invalid_numeric_constant`. The lexer SHALL report exactly one error per invalid
token at its one-based starting line/column, with prefix `invalid identifier:` or
`invalid numeric constant:` respectively, retaining the whole lexeme and location
and continuing to scan. Whitespace, comments, broader operators, unknown-character
handling, location tracking, and omission of physical EOF SHALL remain unchanged.

#### Scenario: Whole invalid identifiers and numbers continue scanning

- **WHEN** the source is `a1b _x 3.14 1e3 answer12`
- **THEN** the first two tokens are `invalid_identifier` and the next two are
  `invalid_numeric_constant`, each retaining its whole spelling
- **AND** exactly four lexical errors are reported and `answer12` remains valid

#### Scenario: Diagnostic locations include preceding multiline trivia

- **WHEN** the source is `/* first\nsecond */\n  _x 3.14\nanswer12`
- **THEN** lexical errors occur only at line 3 columns 3 and 6
- **AND** retained invalid-token locations match those diagnostics
- **AND** `answer12` starts at line 4 column 1

## Current Boundaries and Discrepancies

- This specification describes lexing, not the supported MiniC++ language
  subset. Recognition of a token does not imply that a complete program using
  that token can be parsed, analyzed, or compiled.
- `TokenKind::eof` exists as the scanner's internal terminator and is not
  emitted in the returned token sequence.
- Numeric candidates preserve the scanner's fractional, exponent, and suffix
  matching boundaries, but non-integer forms are diagnosed as invalid spellings.
  This is not a complete C++ preprocessing-number grammar.
- Block comments are recognized only when closed. Unterminated and nested block
  comments have no dedicated lexical diagnostic contract.
- String literals, character literals, and unlisted punctuation or operators
  are not recognized lexical categories and may yield `unknown` tokens.
- Established syntax AST construction is described by syntax-analysis. Unused
  prototype AST nodes do not establish additional supported syntax.
- The command-line token-printing format is explicitly marked unstable by its
  disabled smoke test and is intentionally outside this baseline.
