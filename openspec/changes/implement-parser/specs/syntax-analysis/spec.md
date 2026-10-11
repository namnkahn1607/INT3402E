## MODIFIED Requirements

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
- **AND** the parser library provides all production method definitions for
  executable clients

### Requirement: Owning parse result with parser-local syntax status

The public entry point SHALL return a result containing an exclusively owned
translation unit and a boolean syntax-error status. Parsing SHALL retain a
non-null root after ordinary syntax errors and SHALL
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

- **WHEN** the shared diagnostic context already has a lexical error and the
  parser receives an otherwise syntactically valid token sequence
- **THEN** the result's syntax-error status is false
- **AND** the shared diagnostic context still indicates compilation errors

#### Scenario: Syntax error preserves recoverable syntax

- **WHEN** the shared diagnostic context already has a lexical error and main
  contains `int before; int x\nprint(1); print(2);`
- **THEN** a syntax diagnostic is reported and the result's syntax-error status is true
- **AND** a non-null root retains `int before;` and the safely reached `print(2);`
- **AND** recovery may omit `int x` and the skipped `print(1);` without inserting a semicolon

#### Scenario: Repeated invocation reparses the input without clearing shared errors

- **WHEN** `ParseTranslationUnit()` is invoked twice on the same parser with
  source `int main() { print(1); }` and an error diagnostic is added to the shared engine
  between invocations
- **THEN** both results contain a separately owned main function and print statement
- **AND** both results have false parser-local syntax-error status
- **AND** the earlier tree remains valid and the shared engine retains its error

### Requirement: Explicit end-of-input contract

The parser SHALL treat end-of-span as EOF without requiring a physical EOF
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

- **WHEN** the source and token sequence are empty
- **THEN** lookahead identifies EOF without reading a token and repeated consumption stays at EOF
- **AND** the missing main function is diagnosed at line 1, column 1

#### Scenario: EOF cannot be successfully matched or consumed

- **WHEN** the cursor reaches the end of the supplied token sequence
- **THEN** `PeekKind()` reports `TokenKind::eof` and `Match(TokenKind::eof)` returns false
- **AND** repeated `Consume()` calls each return null immediately without advancing
- **AND** parser and recovery loops terminate or return control instead of retrying at EOF

#### Scenario: Large lookahead cannot overflow into the token sequence

- **WHEN** lookahead requests the maximum representable index offset from a
  nonzero cursor
- **THEN** it identifies EOF with no token object or out-of-bounds access
- **AND** it leaves the cursor unchanged

#### Scenario: End location includes ignored trailing text

- **WHEN** the source is `int main() { }\n// tail\n`
- **THEN** end-of-input is located at line 3, column 1, not at the last token

### Requirement: Authoritative target grammar

The parser SHALL implement the following grammar as its sole syntax definition.
Other planning artifacts SHALL reference it rather
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

- **WHEN** a single main contains declarations, assignments, print, braced if/else,
  do/while, and a for with declaration-or-assignment initialization, a condition,
  and an assignment update
- **THEN** those forms are recognized according to this grammar

#### Scenario: Unsupported structural extensions are rejected

- **WHEN** otherwise valid input adds another function, omits any for field, or
  uses an unbraced control body
- **THEN** each unsupported form produces a syntax diagnostic

### Requirement: Expression precedence and operator restrictions

Expression parsing SHALL follow the grammar above, including
left-associated repeated addition/multiplication, multiplication over addition,
comparison over equality, and parentheses overriding precedence. Each comparison
production SHALL allow only one `>` or `>=`; each equality production SHALL
allow only one `==`. Assignment SHALL remain outside the expression grammar.

#### Scenario: Multiplication binds more tightly than addition

- **WHEN** the expression is `a + b * c`
- **THEN** the AST groups it as `a + (b * c)`
- **AND** `(a + b) * c` instead groups the addition first

#### Scenario: Repeated arithmetic is left-associated

- **WHEN** the expressions are `a + b + c` and `a * b * c`
- **THEN** their ASTs group as `(a + b) + c` and `(a * b) * c`

#### Scenario: Unsupported chains are rejected

- **WHEN** an expression contains `a > b > c`, `a >= b > c`, or `a == b == c`
- **THEN** the extra operator produces a syntax diagnostic rather than a valid chained AST

#### Scenario: Separate comparison operands and explicit grouping remain grammatical

- **WHEN** an expression is `a > b == c > d` or `(a > b) > c`
- **THEN** it is syntactically permitted by the grammar
- **AND** any operand-type issue is left to semantic analysis

### Requirement: Recovery invariants without fixed private helpers

The parser SHALL perform basic panic-mode recovery that can report multiple
independent syntax errors and retain completed nodes and safely reached sibling
statements. Every recovery iteration SHALL advance input, terminate, or yield to
an enclosing owner. It SHALL distinguish absent optional children from malformed
syntax and SHALL NOT require named private recovery helpers or fixed signatures.
Synchronization SHALL use safe `;`, enclosing `}`, or EOF boundaries, with minimal
nesting and for-header context. Recovery MAY omit an ambiguous region or abandon
an affected block rather than reconstruct malformed control structures.

When a nested parser reports a syntax error and returns failure, enclosing parsers
SHALL propagate that failure without redundantly diagnosing the same underlying
error. Propagation SHALL continue to an appropriate recovery owner, which SHALL
resume at a suitable grammar boundary when possible, preserving the partial-tree
contract rather than necessarily failing the entire translation unit. Distinct
syntax errors SHALL still receive their own diagnostics.

#### Scenario: Nested expression failure is diagnosed once before recovery

- **WHEN** the source is `int main() { int x = ; print(1); }` and expression parsing
  reports the missing initializer expression
- **THEN** declaration, statement, block, and translation-unit parsing do not
  redundantly diagnose that same missing expression
- **AND** contextual recovery reaches the print statement and retains it in a
  non-null root with true parser-local syntax-error status

#### Scenario: Multiple statement errors do not erase valid siblings

- **WHEN** main contains `int x = ; print(1); int y = ; print(2);`
- **THEN** both malformed declarations receive syntax diagnostics
- **AND** recovery reaches both valid print statements and retains them in the partial tree
- **AND** malformed initializers are not represented as absent initialization

#### Scenario: Missing for initializer does not swallow the body or following statement

**Deferred advanced recovery - not iteration-1 acceptance.**

- **WHEN** main contains `for (; true; x = 1) { print(1); } print(2);`
- **THEN** the missing mandatory initializer receives a syntax diagnostic
- **AND** recovery respects the header's closing parenthesis and reaches `print(2);`

## ADDED Requirements

### Requirement: Complete syntax-analysis delivery

The system SHALL deliver production declaration, statement, and expression
parsing for the authoritative grammar in one parser library. Runtime scenarios
not explicitly marked deferred advanced recovery SHALL be executable acceptance
for this change. Production expression parsing SHALL NOT be deferred to later
integration. Executable tests SHALL use production parsing definitions.
Semantic analysis, code generation, and parser CLI integration SHALL remain
out of scope; syntax success SHALL NOT imply semantic validity.

Structural parsing SHALL own its separators. Expression parsing SHALL consume
the complete grammatical expression and parentheses it opens, preserve owning
operand data and source locations, and leave caller-owned semicolons, outer
closing parentheses, braces, and EOF untouched. Success SHALL return a complete
non-null expression. Failure SHALL diagnose the underlying syntax error before
returning failure, preserve caller boundaries, and need not restore its starting
cursor if part of the expression was consumed. Enclosing parsers SHALL use that
per-call result rather than global diagnostic state and SHALL NOT duplicate the
same operand error.

#### Scenario: Shared expression and structural contract

- **WHEN** declarations, assignments, and control/print operands are parsed
- **THEN** each component consumes its own syntax using the shared token stream
- **AND** complete expressions have non-null operands and failed expressions reach
  contextual recovery without duplicate diagnostics or fabricated successful nodes

#### Scenario: Grouping leaves caller delimiters intact

- **WHEN** the source is `int main() { int x = (1 + 2) * 3; print((x)); }`
- **THEN** production expressions construct the grouped arithmetic/name nodes and
  consume the parentheses they opened
- **AND** declaration/print parsing consumes its own semicolon or outer closing
  parenthesis without expression parsing stealing those delimiters

#### Scenario: Diagnosed failure can occur before or after consumption

- **WHEN** expression parsing fails at a missing operand, a missing grouped
  operand, or a missing right operand after a consumed binary operator
- **THEN** it diagnoses that error and leaves any encountered caller delimiter intact
- **AND** enclosing parsers recover without requiring cursor rollback

#### Scenario: Representative complete program uses production parsing

- **WHEN** the source is the following program
- **THEN** parsing succeeds with false local syntax-error status and an owning AST
  containing all declarations, expressions, statements, and control bodies in order

```cpp
int main() {
    int x = 1 + 2 * 3;
    bool flag = x >= 3 == true;
    if (flag) { print((x + 1) * 2); } else { x = x + 1; }
    do { x = x + 1; } while (x > 10);
    for (int i = 0; i > 3; i = i + 1) { print(i); }
    for (x = 1; flag; x = x * 2) { print(x); }
}
```

### Requirement: Expression leaves match the language spellings

Expression identifiers SHALL obey the same restricted spelling and reserved-word
rules as declarations and assignment targets. Integer literals SHALL match
`[0-9]+` and retain their owning spelling; boolean literals SHALL preserve their
true/false values. Broader numeric forms, unary operators, unsupported infix
operators, calls, and assignment within expressions SHALL produce syntax
diagnostics rather than supported expression ASTs. Recognition by the lexer or
an existing prototype node SHALL NOT broaden this expression grammar.

#### Scenario: Literal and reference data is preserved

- **WHEN** main contains `int count12 = 00042; bool flag = false; print(count12);`
- **THEN** the AST retains the integer spelling `00042`, the boolean value false,
  and the owned name `count12` with their token start locations

#### Scenario: Broader token spellings are not supported expressions

- **WHEN** expression operands use `3.14`, `.5`, `1e3`, `1f`, `a1b`, or `_x`
- **THEN** those operands receive syntax diagnostics instead of being accepted
  from the lexer's broader numeric/identifier categories

#### Scenario: Unsupported expression extensions are rejected

- **WHEN** an operand uses `-1`, `!true`, `a - b`, `a < b`, `a && b`,
  `f(a)`, or expression assignment such as `print(a = 1);`
- **THEN** parsing diagnoses the unsupported syntax and does not retain a
  supported expression node that hides the extra syntax

### Requirement: Parser interpretation of broader lexer tokens

The parser SHALL recognize identifier tokens spelled exactly `do` or `print` as
reserved terminals, not ordinary variable names. Declaration names and assignment
targets SHALL match `[A-Za-z]+[0-9]*` and SHALL exclude the grammar's keyword
spellings, including `do` and `print`. Longer spellings SHALL NOT be classified
by keyword prefix. Unsupported type keywords SHALL produce syntax diagnostics
rather than acquire support from the lexer or AST vocabulary.

#### Scenario: Existing lexer tokens support do and print

- **WHEN** the existing lexer supplies tokens for
  `int main() { do { print(1); } while (true); }`
- **AND** production expressions recognize the integer and boolean operands
- **THEN** the statement parser constructs a do/while node containing a print node
  despite `do` and `print` arriving as identifier tokens

#### Scenario: Restricted names are enforced without splitting tokens

- **WHEN** declarations, assignment targets, or expression references use
  `answer12`, `_answer`, `a1b`, `a_b`, `do`, or `print`
- **THEN** `answer12` is accepted as a name and each other spelling is diagnosed
- **AND** `print1` and `done` remain ordinary valid identifiers
- **AND** no invalid spelling is split into smaller valid names

#### Scenario: Broader types remain unsupported

- **WHEN** a main body declares `long x;`, `float y;`, or `void z;`
- **THEN** those declarations produce syntax diagnostics rather than supported
  variable declarations

### Requirement: Translation-unit execution

The public parser entry point SHALL implement the borrowed-input, owning-result,
parser-local syntax status,
repeated-invocation, and virtual EOF contracts already specified. It SHALL accept
exactly one zero-parameter `int main()` and consume its complete structural form.
Ordinary malformed input SHALL return a non-null partial root. It SHALL diagnose
missing required structure and tokens after main rather than silently accepting
an incomplete or extended translation unit.

#### Scenario: Empty main produces an owning root

- **WHEN** the lexer supplies tokens for `int main() { }`
- **THEN** the result owns one int-returning main function with no parameters and
  an empty compound body, and has false parser-local syntax-error status
- **AND** the inputs remain unchanged and the returned tree remains usable after
  the parser and its input storage are destroyed

#### Scenario: Earlier errors and repeated calls remain independent

- **WHEN** otherwise valid input is parsed twice and the shared engine has an
  earlier error
- **THEN** both calls return separate valid trees with false local syntax-error
  status, starting from the beginning of the tokens
- **AND** the earlier tree remains valid and the shared engine retains its error

#### Scenario: Malformed top-level structure retains a root

- **WHEN** input is empty, uses a different function name, supplies parameters,
  omits main's opening brace, or appends another function
- **THEN** parsing terminates, returns a non-null root with true local syntax-error
  status, and reports the offending structure
- **AND** it does not construct another supported function

#### Scenario: EOF diagnostics include trailing trivia

- **WHEN** the source is `int main() {\n// tail\n` and its token sequence has no
  physical EOF token
- **THEN** the missing closing brace is diagnosed at line 3, column 1
- **AND** parsing and recovery terminate at virtual EOF

### Requirement: Declaration and statement AST construction

The parser SHALL construct the existing AST shapes for all declaration and
statement productions of the authoritative
grammar. Blocks SHALL retain statements in source order. Uninitialized variable
declarations and absent else branches SHALL have absent optional children;
malformed required expressions or bodies SHALL NOT masquerade as these omissions.
Mandatory children of retained nodes SHALL be present.

Declarations SHALL be wrapped as declaration statements and assignments as
expression statements containing assignment nodes whose target is a name
reference. For initializers SHALL use the same declaration or assignment shapes;
their update SHALL be an assignment node. The three for fields and braced control
bodies SHALL remain mandatory. Name lookup, redeclaration, initialization status,
and operand typing SHALL NOT be checked by the parser.

Node start locations SHALL identify the leading source token: function and
variable declarations at their type, assignment nodes and targets at their name,
blocks at `{`, and specialized statements at their keyword. Declaration and
assignment statement wrappers SHALL preserve their child's start location.
Expression leaves SHALL use their token start location; binary nodes SHALL use
operator locations. Parenthesized expressions SHALL retain the inner node and its
location, without requiring a separate grouping node.

#### Scenario: Declarations and assignments reuse the same constructs

- **WHEN** the body is `{ int x; bool flag = true; x = 1; }`
- **THEN** its ordered children contain an uninitialized int declaration, an
  initialized bool declaration, and an assignment expression statement
- **AND** declaration names and assignment targets own their spellings

#### Scenario: Selection, loops, print, and nested blocks have required children

- **WHEN** the body contains `if (true) { print(1); } else { print(2); }`,
  `if (false) { }`, `do { } while (true);`, and a nested `{ int x; }`
- **THEN** their AST nodes preserve the supplied conditions, ordered bodies,
  print operands, and optional else distinction

#### Scenario: Both for initializer forms are accepted

- **WHEN** main contains `for (int x = 1; true; x = 2) { print(x); }` and
  `for (x = 1; false; x = 2) { }`
- **THEN** the first initializer is a declaration statement and the second is an
  assignment expression statement
- **AND** both loops have non-null condition, assignment update, and compound body
- **AND** the header separators are not consumed as statement terminators twice

#### Scenario: Semantic problems are not parser errors

- **WHEN** main contains `x = true; int x; bool x; if (1) { }`
- **THEN** those grammatical declarations and statements are retained without
  declaration-before-use, redeclaration, or condition-type diagnostics

#### Scenario: Unsupported statement extensions are diagnosed

- **WHEN** a main body contains `return 1;`, `while (true) { }`, `;`, `1;`,
  `if (true) print(1);`, or a for with an empty header field
- **THEN** each unsupported form produces a syntax diagnostic rather than being
  accepted merely because lexer tokens or AST node kinds exist

#### Scenario: Statement locations survive comments and newlines

- **WHEN** main contains a multiline comment followed by a declaration, assignment,
  nested block, and print on later lines
- **THEN** each retained node and wrapper has the start location of its leading
  token rather than the current cursor position after parsing its children

#### Scenario: Multiline expression locations follow their source tokens

- **WHEN** the source is `int main() {\nint x = 1 +\n  2 * 3;\n}`
- **THEN** the addition node is located at line 2 column 11 and multiplication
  at line 3 column 5
- **AND** the multiplication's integer operands are located at line 3 columns
  3 and 7, and grouping/precedence does not overwrite these token locations

### Requirement: Executable declaration and statement recovery

The parser SHALL implement basic panic-mode recovery across declarations,
statements, and expressions. A block SHALL own statement-list recovery: skip
malformed input to a safe statement semicolon, its enclosing closing brace, or
EOF. Recovery SHALL consume a synchronization semicolon, leave an enclosing brace
for its owner, and stop at EOF. It SHALL retain completed nodes and resume later
statements when a safe boundary is reached. A diagnosed nested failure SHALL be
propagated without repeated diagnostics for the same underlying error.

Minimal nesting context SHALL prevent a nested brace from being mistaken for the
surrounding block's closer. A for parser SHALL own header recovery; header
semicolons SHALL NOT be treated as ordinary statement terminators, and header
fragments SHALL NOT become surrounding statements. When a safe control/header
boundary cannot be established, recovery MAY discard a larger construct or
abandon the affected block. Exact reconstruction and retention of skipped syntax
SHALL NOT be required.

Each recovery loop SHALL advance, terminate, or yield to an owner that handles
the boundary, rather than retry an unchanged failed parse. Malformed constructs
SHALL be omitted instead of attaching null mandatory children or representing a
failed initializer as absent optional initialization. Missing semicolons SHALL
be diagnosed and handled by panic recovery without requiring token insertion.
Missing-semicolon repair, precise balanced malformed-for recovery, and complex
nested partial-control preservation are deferred improvements, not iteration-1
acceptance.

#### Scenario: Missing semicolon retains complete statements

**Deferred advanced recovery - not iteration-1 acceptance.**

- **WHEN** main contains `int x\nprint(1);`
- **THEN** the missing semicolon is diagnosed once and both complete statements
  are retained in source order

#### Scenario: Missing semicolon uses a safe later boundary

- **WHEN** main contains `int before; int x\nprint(1); print(2);`
- **THEN** the missing semicolon is diagnosed at `print` on the next line
- **AND** recovery retains `int before;` and resumes at `print(2);` after the safe semicolon
- **AND** the malformed declaration is omitted; retaining the skipped first print
  or inserting a semicolon is not required

#### Scenario: Multiple operand failures do not erase valid siblings

- **WHEN** main contains `int x = ; print(1); int y = ; print(2);`
- **AND** expression parsing reports each missing operand and returns failure
- **THEN** the result has true local syntax-error status and retains both prints
- **AND** exactly the two operand errors are reported, without duplicate missing
  expression diagnostics from enclosing parsers
- **AND** neither failed initializer becomes an uninitialized declaration

#### Scenario: Inner recovery preserves else and the do tail

**Deferred advanced recovery - not iteration-1 acceptance.**

- **WHEN** a braced if branch or do body contains a malformed declaration or
  expression operand followed by a valid print, and its enclosing delimiters
  and tail are intact
- **THEN** recovery retains the print and allows the corresponding else branch or
  `while (...) ;` tail to be parsed by its owner

#### Scenario: Malformed for header recovers conservatively

- **WHEN** main contains `for (; true; x = 1) { print(1); } print(2);`
- **THEN** the missing initializer is diagnosed and the malformed loop is not
  retained with a null initializer
- **AND** header fragments do not become surrounding statements and parsing terminates
- **AND** recovery may omit the body and following print if their safe boundary
  cannot be established, without null mandatory children or duplicate diagnostics

#### Scenario: Malformed nested controls terminate safely

- **WHEN** a nested control has a missing parenthesis or an incomplete braced body
- **THEN** parsing diagnoses the malformed syntax at the offending token or EOF
- **AND** recovery respects enclosing brace ownership and terminates with a non-null root
- **AND** malformed controls may be omitted without guaranteeing preservation of
  their branch/tail structure or skipped statements

#### Scenario: Malformed syntax ends safely at EOF

- **WHEN** input ends during a declaration, expression operand, control header,
  or nested block
- **THEN** parsing reports the missing required syntax, returns a non-null partial
  root with true local syntax-error status, and terminates without out-of-bounds
  access or repeated attempts to consume EOF

#### Scenario: Independent errors on different lines retain later expressions

- **WHEN** the source is the following malformed program
- **THEN** missing operands are diagnosed at line 2 column 9 and line 4 column 17,
  and the extra comparison is diagnosed at line 6 column 11
- **AND** these three independent errors are reported once each in one parsing run
- **AND** the non-null partial root retains all three valid print statements,
  including the correctly grouped final arithmetic expression
- **AND** local syntax-error status is true and no retained node has null mandatory children

```cpp
int main() {
int x = ;
print(1);
bool flag = 2 * ;
print(flag);
x = 1 > 2 > 3;
print(2 + 3 * 4);
}
```

## REMOVED Requirements

### Requirement: Declaration-only handoff and lexer readiness

**Reason**: The interface-only delivery boundary is superseded by complete
production syntax analysis in this change.

**Migration**: Use Complete syntax-analysis delivery, the implemented expression
requirements, and Parser interpretation of broader lexer tokens. Preserve the
existing delimiter/recovery contracts while providing all parser definitions.
