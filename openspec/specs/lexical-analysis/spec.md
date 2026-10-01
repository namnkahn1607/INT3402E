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
`const`, `continue`, `default`, `double`, `else`, `false`, `float`, `for`,
`if`, `int`, `long`, `return`, `short`, `switch`, `true`, `void`, and `while`.

The system SHALL recognize identifiers beginning with an ASCII letter or
underscore and followed by zero or more ASCII letters, digits, or underscores.

The lexer SHALL classify a keyword only when its full matched lexeme is one of
the listed keyword spellings. A longer identifier containing a keyword
spelling SHALL remain an identifier.

#### Scenario: Keeping a keyword prefix in an identifier

- **WHEN** the source text is `integer`
- **THEN** it is returned as one `identifier` token with lexeme `integer`

### Requirement: Recognize numeric constants

The system SHALL recognize decimal numeric constants according to the
implemented scanner rules, including integer constants, fractional forms such
as `.5` and `3.`, and exponent forms such as `1e10` and `1E-5`.

#### Scenario: Recognizing representative numeric constants

- **WHEN** the source text is `42 3.14 .5 3. 1e10 1E-5 3.14f`
- **THEN** each lexeme is returned as one `numeric_constant` token

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

## Current Boundaries and Discrepancies

- This specification describes lexing, not the supported MiniC++ language
  subset. Recognition of a token does not imply that a complete program using
  that token can be parsed, analyzed, or compiled.
- `TokenKind::eof` exists as the scanner's internal terminator and is not
  emitted in the returned token sequence.
- The public token-header comment describes numeric constants as having an
  optional `f` or `F` suffix, while some implemented fractional and exponent
  rules also accept `l` or `L`. The implemented scanner behavior is preserved
  by this baseline; its detailed suffix grammar is not independently covered
  by tests.
- Block comments are recognized only when closed. Unterminated and nested block
  comments have no dedicated lexical diagnostic contract.
- String literals, character literals, and unlisted punctuation or operators
  are not recognized lexical categories and may yield `unknown` tokens.
- The AST types are a prototype with no parser producer, runtime consumer, or
  tests. They are intentionally outside this baseline.
- The command-line token-printing format is explicitly marked unstable by its
  disabled smoke test and is intentionally outside this baseline.
