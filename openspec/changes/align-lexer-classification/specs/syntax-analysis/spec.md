## MODIFIED Requirements

### Requirement: Expression leaves match the language spellings

Expression identifiers SHALL obey the same restricted spelling and reserved-word
rules as declarations and assignment targets. Integer literals SHALL match
`[0-9]+` and retain their owning spelling; boolean literals SHALL preserve their
true/false values. Identifier and integer spelling classification SHALL be supplied
by the lexer;
invalid spelling tokens SHALL fail parsing without a second diagnostic. Unary
operators, unsupported infix operators, calls, and assignment within expressions
SHALL produce syntax diagnostics rather than supported expression ASTs. Recognition
by the lexer or an existing prototype node SHALL NOT broaden this expression grammar.

#### Scenario: Literal and reference data is preserved

- **WHEN** main contains `int count12 = 00042; bool flag = false; print(count12);`
- **THEN** the AST retains the integer spelling `00042`, the boolean value false,
  and the owned name `count12` with their token start locations

#### Scenario: Broader token spellings are not supported expressions

- **WHEN** expression operands use `3.14`, `.5`, `1e3`, `1f`, `a1b`, or `_x`
- **THEN** those operands receive exactly one lexical diagnostic each and fail parsing
  without a duplicate syntax diagnostic or a retained malformed expression

#### Scenario: Unsupported expression extensions are rejected

- **WHEN** an operand uses `-1`, `!true`, `a - b`, `a < b`, `a && b`,
  `f(a)`, or expression assignment such as `print(a = 1);`
- **THEN** parsing diagnoses the unsupported syntax and does not retain a
  supported expression node that hides the extra syntax

### Requirement: Parser interpretation of broader lexer tokens

The parser SHALL recognize `kw_do` and `kw_print` as reserved terminals,
not ordinary variable names, and SHALL trust lexer identifier/integer classification
rather than repeat spelling checks. Declaration names and assignment targets SHALL
require `identifier` tokens. Longer valid spellings SHALL NOT be dispatched by
keyword prefix. `main` SHALL remain an identifier with its exact name checked only
for the main-function production. Unsupported type keywords SHALL produce syntax
diagnostics rather than acquire support from the lexer or AST vocabulary.

#### Scenario: Existing lexer tokens support do and print

- **WHEN** the existing lexer supplies tokens for
  `int main() { do { print(1); } while (true); }`
- **AND** production expressions recognize the integer and boolean operands
- **THEN** the statement parser constructs a do/while node containing a print node
  using `kw_do` and `kw_print` token kinds

#### Scenario: Restricted names are enforced without splitting tokens

- **WHEN** declarations, assignment targets, or expression references use
  `answer12`, `_answer`, `a1b`, `a_b`, `do`, or `print`
- **THEN** `answer12` is accepted as a name; `_answer`, `a1b`, and `a_b` receive
  lexical diagnostics; `do` and `print` in name positions produce syntax diagnostics
- **AND** `print1` and `done` remain ordinary valid identifiers
- **AND** no invalid spelling is split into smaller valid names

#### Scenario: Broader types remain unsupported

- **WHEN** a main body declares `long x;`, `float y;`, or `void z;`
- **THEN** those declarations produce syntax diagnostics rather than supported
  variable declarations

## ADDED Requirements

### Requirement: Lexer-owned failures preserve parser-local status

At an `invalid_identifier` or `invalid_numeric_constant` token the parser SHALL
propagate failure into existing contextual panic recovery without repeating the
lexer's diagnostic or setting parser-local syntax-error status for that spelling.
The shared engine SHALL retain lexical errors. Independent syntax errors SHALL
still be diagnosed and set local status. Affected malformed constructs SHALL be
omitted, not retained with null mandatory children or fake optional initialization.
Recovery SHALL preserve delimiter ownership, progress, termination, and virtual EOF.

#### Scenario: Lexical failures retain later valid statements

- **WHEN** main contains `int a1b; print(1); int x = 3.14; print(2);`
- **THEN** exactly two lexical errors are reported and local syntax-error status is false
- **AND** both malformed declarations are omitted and both print nodes are retained
- **AND** the shared diagnostics indicate compilation failure

#### Scenario: Mixed multiline failures have independent ownership

- **WHEN** main contains `int x = 1e3;` on line 2, `int y = ;` on line 3,
  and `print(2);` on line 4, each starting at column 1
- **THEN** one lexical error occurs at line 2 column 9 and one syntax error at line 3 column 9
- **AND** local syntax-error status is true and the valid print is retained
- **AND** no retained AST node has null mandatory children

#### Scenario: Invalid tokens in nested controls recover safely

- **WHEN** an if condition, do condition, or required for field contains an invalid spelling
- **THEN** the containing failed construct reaches contextual recovery without a duplicate
  spelling diagnostic, fabricated successful node, or leaked for-header fragment
- **AND** parsing terminates at safe boundaries or EOF
