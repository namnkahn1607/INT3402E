## Purpose

Define the parser interface and the authoritative target grammar for the agreed
MiniC++ subset, independently of the broader lexer vocabulary and AST prototype.

## Delivery Scope

This increment delivers declarations and documented contracts only. Compile-time
and documentation scenarios apply now. Scenarios labelled **Future executable
acceptance** are criteria for a later parser implementation, not behavior delivered
by this change or evidence of current language support. The existing
lexical-analysis baseline remains unchanged.

## ADDED Requirements

### Requirement: Borrowed parser inputs

The interface SHALL accept immutable source text, an immutable lexer-token
sequence, and a shared diagnostic context. The caller SHALL retain these inputs
for the parser's lifetime. The returned AST SHALL own its node data rather than
borrowing source or token spellings.

#### Scenario: Existing lexer output fits the interface

- **WHEN** a C++20 client compiles construction from source text, the lexer token
  sequence, and its diagnostic context
- **THEN** the declarations accept those inputs without requiring a copied or
  mutated token stream or an appended EOF token
- **AND** this increment does not promise linkable parser methods

### Requirement: Owning parse result with parser-local syntax status

The public entry point SHALL declare a result containing an exclusively owned
translation unit and a boolean syntax-error status. Its documented runtime
contract SHALL retain a non-null root after ordinary syntax errors and SHALL
set the boolean only for syntax errors diagnosed during that parsing operation.
Earlier lexical errors and warnings alone SHALL NOT set it. All parser errors
SHALL still be reported through the shared diagnostic context; a false local
status SHALL NOT imply overall compilation success.

Each invocation of `ParseTranslationUnit()` SHALL start from the beginning of the
supplied token sequence with a fresh parser-local syntax-error status, without
resetting the shared diagnostic engine. Each invocation SHALL produce a separately
owned result; an earlier returned tree SHALL remain valid.

#### Scenario: Result shape is available to clients

- **WHEN** a client compiles unevaluated checks of the public entry-point result
- **THEN** it can identify the owning translation-unit member and boolean
  syntax-error member without linking parser definitions

#### Scenario: Earlier lexer error does not become a syntax error

**Future executable acceptance**

- **WHEN** the shared diagnostic context already has a lexical error and the
  parser receives an otherwise syntactically valid token sequence
- **THEN** the result's syntax-error status is false
- **AND** the shared diagnostic context still indicates compilation errors

#### Scenario: Syntax error preserves recoverable syntax

**Future executable acceptance**

- **WHEN** the shared diagnostic context already has a lexical error and a
  statement is missing a semicolon before another valid statement
- **THEN** a syntax diagnostic is reported and the result's syntax-error status is true
- **AND** a non-null root retains recoverable statements instead of discarding the file

#### Scenario: Repeated invocation reparses the input without clearing shared errors

**Future executable acceptance**

- **WHEN** `ParseTranslationUnit()` is invoked twice on the same parser with
  source `int main() { print(1); }` and an error diagnostic is added to the shared engine
  between invocations
- **THEN** both results contain a separately owned main function and print statement
- **AND** both results have false parser-local syntax-error status
- **AND** the earlier tree remains valid and the shared engine retains its error

### Requirement: Explicit end-of-input contract

The interface SHALL document end-of-span as EOF without requiring a physical EOF
token. Lookahead at or beyond the end SHALL expose the existing EOF kind and no
token object. Consuming at EOF SHALL leave the cursor unchanged. Lookahead SHALL
be bounds-safe even for offsets too large to add safely to the cursor. EOF
diagnostics SHALL use the 1-based position after the complete source, including
trailing whitespace and comments.

EOF SHALL be observable but not consumable. `Match(TokenKind::eof)` SHALL return
false, and repeated calls to `Consume()` at EOF SHALL return without advancing
the cursor. Parser and recovery loops SHALL terminate or yield at EOF rather than
retrying consumption indefinitely. A no-op `Consume()` alone does not guarantee
termination of a caller's loop; EOF observation, not successful matching or cursor
advancement, SHALL govern termination at end-of-input.

#### Scenario: Empty token sequence is safe

**Future executable acceptance**

- **WHEN** the source and token sequence are empty
- **THEN** lookahead identifies EOF without reading a token and repeated consumption stays at EOF
- **AND** the missing main function is diagnosed at line 1, column 1

#### Scenario: EOF cannot be successfully matched or consumed

**Future executable acceptance**

- **WHEN** the cursor reaches the end of the supplied token sequence
- **THEN** `PeekKind()` reports `TokenKind::eof` and `Match(TokenKind::eof)` returns false
- **AND** repeated `Consume()` calls each return null immediately without advancing
- **AND** parser and recovery loops terminate or return control instead of retrying at EOF

#### Scenario: Large lookahead cannot overflow into the token sequence

**Future executable acceptance**

- **WHEN** lookahead requests the maximum representable index offset from a
  nonzero cursor
- **THEN** it identifies EOF with no token object or out-of-bounds access
- **AND** it leaves the cursor unchanged

#### Scenario: End location includes ignored trailing text

**Future executable acceptance**

- **WHEN** the source is `int main() { }\n// tail\n`
- **THEN** end-of-input is located at line 3, column 1, not at the last token

### Requirement: Authoritative target grammar

The interface documentation SHALL reference the following grammar as its sole
target syntax definition. Other planning artifacts SHALL reference it rather
than maintain another grammar. Quoted strings are terminals, `epsilon` is empty,
and `EOF` is end-of-span. `ID` matches `[A-Za-z]+[0-9]*` excluding reserved
words; `INTEGER` matches `[0-9]+`.

```bnf
<translation-unit> ::= "int" "main" "(" ")" <block> EOF
<block> ::= "{" <statement-list> "}"
<statement-list> ::= <statement> <statement-list> | epsilon
<statement> ::= <declaration> ";"
              | <assignment> ";"
              | <print-statement> ";"
              | <if-statement>
              | <do-statement>
              | <for-statement>
              | <block>
<declaration> ::= <type> ID <initializer>
<type> ::= "int" | "bool"
<initializer> ::= "=" <expression> | epsilon
<assignment> ::= ID "=" <expression>
<print-statement> ::= "print" "(" <expression> ")"
<if-statement> ::= "if" "(" <expression> ")" <block> <else-part>
<else-part> ::= "else" <block> | epsilon
<do-statement> ::= "do" <block> "while" "(" <expression> ")" ";"
<for-statement> ::= "for" "(" <for-initializer> ";" <expression> ";"
                    <assignment> ")" <block>
<for-initializer> ::= <declaration> | <assignment>
<expression> ::= <equality>
<equality> ::= <comparison> <equality-part>
<equality-part> ::= "==" <comparison> | epsilon
<comparison> ::= <addition> <comparison-part>
<comparison-part> ::= ">" <addition> | ">=" <addition> | epsilon
<addition> ::= <term> <addition-tail>
<addition-tail> ::= "+" <term> <addition-tail> | epsilon
<term> ::= <factor> <term-tail>
<term-tail> ::= "*" <factor> <term-tail> | epsilon
<factor> ::= ID | INTEGER | "true" | "false" | "(" <expression> ")"
```

Only one zero-parameter `int main()` is permitted. Control bodies require braces;
all three `for` fields are mandatory. Extra functions, parameters, calls, returns,
unary operators, standalone `while`, empty statements, and arbitrary expression
statements are not part of this target. Variable resolution and type checking
remain semantic-analysis concerns, not syntax checks. Existing well-formed
`//` and `/* ... */` comments remain lexer-handled trivia, not AST statements.

#### Scenario: Required control forms are grammatical

**Future executable acceptance**

- **WHEN** a single main contains declarations, assignments, print, braced if/else,
  do/while, and a for with declaration-or-assignment initialization, a condition,
  and an assignment update
- **THEN** those forms are recognized according to this grammar

#### Scenario: Unsupported structural extensions are rejected

**Future executable acceptance**

- **WHEN** otherwise valid input adds another function, omits any for field, or
  uses an unbraced control body
- **THEN** each unsupported form produces a syntax diagnostic

### Requirement: Expression precedence and operator restrictions

The documented expression contract SHALL follow the grammar above, including
left-associated repeated addition/multiplication, multiplication over addition,
comparison over equality, and parentheses overriding precedence. Each comparison
production SHALL allow only one `>` or `>=`; each equality production SHALL
allow only one `==`. Assignment SHALL remain outside the expression grammar.

#### Scenario: Multiplication binds more tightly than addition

**Future executable acceptance**

- **WHEN** the expression is `a + b * c`
- **THEN** the AST groups it as `a + (b * c)`
- **AND** `(a + b) * c` instead groups the addition first

#### Scenario: Repeated arithmetic is left-associated

**Future executable acceptance**

- **WHEN** the expressions are `a + b + c` and `a * b * c`
- **THEN** their ASTs group as `(a + b) + c` and `(a * b) * c`

#### Scenario: Unsupported chains are rejected

**Future executable acceptance**

- **WHEN** an expression contains `a > b > c`, `a >= b > c`, or `a == b == c`
- **THEN** the extra operator produces a syntax diagnostic rather than a valid chained AST

#### Scenario: Separate comparison operands and explicit grouping remain grammatical

**Future executable acceptance**

- **WHEN** an expression is `a > b == c > d` or `(a > b) > c`
- **THEN** it is syntactically permitted by the grammar
- **AND** any operand-type issue is left to semantic analysis

### Requirement: Recovery invariants without fixed private helpers

The interface SHALL document contextual recovery that can report multiple syntax
errors, preserve intact sibling statements, and either advance input or yield to
an enclosing owner on every recovery iteration. It SHALL distinguish absent
optional children from malformed syntax and SHALL NOT require named private
recovery helpers or fixed helper signatures. Recovery SHALL respect statement
boundaries, enclosing delimiters, and for-header separators.

When a nested parser reports a syntax error and returns failure, enclosing parsers
SHALL propagate that failure without redundantly diagnosing the same underlying
error. Propagation SHALL continue to an appropriate recovery owner, which SHALL
resume at a suitable grammar boundary when possible, preserving the partial-tree
contract rather than necessarily failing the entire translation unit. Distinct
syntax errors SHALL still receive their own diagnostics.

#### Scenario: Nested expression failure is diagnosed once before recovery

**Future executable acceptance**

- **WHEN** the source is `int main() { int x = ; print(1); }` and expression parsing
  reports the missing initializer expression
- **THEN** declaration, statement, block, and translation-unit parsing do not
  redundantly diagnose that same missing expression
- **AND** contextual recovery reaches the print statement and retains it in a
  non-null root with true parser-local syntax-error status

#### Scenario: Multiple statement errors do not erase valid siblings

**Future executable acceptance**

- **WHEN** main contains `int x = ; print(1); int y = ; print(2);`
- **THEN** both malformed declarations receive syntax diagnostics
- **AND** recovery reaches both valid print statements and retains them in the partial tree
- **AND** malformed initializers are not represented as absent initialization

#### Scenario: Missing for initializer does not swallow the body or following statement

**Future executable acceptance**

- **WHEN** main contains `for (; true; x = 1) { print(1); } print(2);`
- **THEN** the missing mandatory initializer receives a syntax diagnostic
- **AND** recovery respects the header's closing parenthesis and reaches `print(2);`

### Requirement: Declaration-only handoff and lexer readiness

The interface SHALL document declaration/statement ownership and expression
ownership, including caller-owned separators and expression-owned parentheses.
Expression methods SHALL remain declarations without dummy bodies. No runtime
parsing, semantic checking, or CLI integration SHALL be delivered here.

Before executable integration, the lexical interface SHALL provide unambiguous
recognition of `do`, `print`, and all other grammar terminals, and a defined way
to enforce the identifier and integer spelling restrictions. Broader existing
lexing SHALL NOT be treated as broader language support, and invalid spellings
SHALL NOT be split into apparently valid constructs to bypass the restrictions.

#### Scenario: Coworker can implement expressions against the shared contract

- **WHEN** both developers inspect the interface documentation
- **THEN** expression parsing owns its internal parentheses, leaves caller-owned
  semicolons and closing delimiters unconsumed, and reports through the common
  syntax-diagnostic path
- **AND** no expression placeholder silently returns a dummy tree or null result

#### Scenario: Lexer compatibility is a prerequisite, not an implemented claim

- **WHEN** the implementation handoff is reviewed
- **THEN** current identifier treatment of `do`/`print` and broader
  identifier/numeric spellings are identified as unresolved integration work
- **AND** header compilation is not presented as proof of grammar acceptance
