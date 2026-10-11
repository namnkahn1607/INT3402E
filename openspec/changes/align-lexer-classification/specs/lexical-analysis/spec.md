## MODIFIED Requirements

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

## ADDED Requirements

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
