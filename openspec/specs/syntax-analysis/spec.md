# syntax-analysis Specification

## Purpose

Define implemented syntax analysis and its authoritative function-free MiniC++
grammar, independently of the broader lexer vocabulary and unused AST prototypes.

## Delivery Scope

Syntax analysis implements the authoritative grammar using recursive descent for
structural parsing and Pratt expressions. Source files contain ordered declarations
and statements directly, with no required entry function or outer block. Functions
are deferred. It constructs owning ASTs and uses basic
panic recovery to report multiple syntax errors with source locations, retaining
valid syntax when safely reachable. Explicitly deferred advanced recovery is not
iteration-1 acceptance. Semantic analysis, code generation, and parser CLI
integration are not delivered. Lexical spelling classification and diagnostics
are supplied by the lexical-analysis capability.

## Requirements

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

- **WHEN** the shared diagnostic context already has a lexical error and the source
  contains `int before; int x\nprint(1); print(2);`
- **THEN** a syntax diagnostic is reported and the result's syntax-error status is true
- **AND** a non-null root retains `int before;` and the safely reached `print(2);`
- **AND** recovery may omit `int x` and the skipped `print(1);` without inserting a semicolon

#### Scenario: Repeated invocation reparses the input without clearing shared errors

- **WHEN** `ParseTranslationUnit()` is invoked twice on the same parser with
  source `{ print(1); }` and an error diagnostic is added to the shared engine
  between invocations
- **THEN** both results contain a separately owned block and print statement
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
- **AND** parsing succeeds with an empty owning translation unit and no diagnostic

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

- **WHEN** the source is `{ }\n// tail\n`
- **THEN** end-of-input is located at line 3, column 1, not at the last token

### Requirement: Authoritative target grammar

The parser SHALL implement the following grammar as its sole syntax definition.
Other planning artifacts SHALL reference it rather
than maintain another grammar. Quoted strings are terminals, `epsilon` is empty,
and `EOF` is end-of-span. `ID` matches `[A-Za-z]+[0-9]*` excluding reserved
words; `INTEGER` matches `[0-9]+`.

```bnf
<translation-unit> ::= <statement-list> EOF
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

A source file is an ordered, possibly empty sequence of the supported declarations
and statements. No entry function or enclosing block is required or synthesized.
Function declarations and definitions, including `int main()`, are unsupported.
Control bodies require braces; all three `for` fields are mandatory. Parameters,
calls, returns,
unary operators, standalone `while`, empty statements, and arbitrary expression
statements are not part of this target. Variable resolution and type checking
remain semantic-analysis concerns, not syntax checks. Existing well-formed
`//` and `/* ... */` comments remain lexer-handled trivia, not AST statements.

#### Scenario: Required control forms are grammatical

- **WHEN** a source contains declarations, assignments, print, braced if/else,
  do/while, and a for with declaration-or-assignment initialization, a condition,
  and an assignment update
- **THEN** those forms are recognized according to this grammar

#### Scenario: Unsupported structural extensions are rejected

- **WHEN** otherwise valid input adds a function declaration or definition,
  omits any for field, or
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

- **WHEN** the source is `{ int x = ; print(1); }` and expression parsing
  reports the missing initializer expression
- **THEN** declaration, statement, block, and translation-unit parsing do not
  redundantly diagnose that same missing expression
- **AND** contextual recovery reaches the print statement and retains it in a
  non-null root with true parser-local syntax-error status

#### Scenario: Multiple statement errors do not erase valid siblings

- **WHEN** the source contains `int x = ; print(1); int y = ; print(2);`
- **THEN** both malformed declarations receive syntax diagnostics
- **AND** recovery reaches both valid print statements and retains them in the partial tree
- **AND** malformed initializers are not represented as absent initialization

#### Scenario: Missing for initializer does not swallow the body or following statement

**Deferred advanced recovery - not iteration-1 acceptance.**

- **WHEN** the source contains `for (; true; x = 1) { print(1); } print(2);`
- **THEN** the missing mandatory initializer receives a syntax diagnostic
- **AND** recovery respects the header's closing parenthesis and reaches `print(2);`

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

- **WHEN** the source is `{ int x = (1 + 2) * 3; print((x)); }`
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
{
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
true/false values. Identifier and integer spelling classification SHALL be supplied
by the lexer;
invalid spelling tokens SHALL fail parsing without a second diagnostic. Unary
operators, unsupported infix operators, calls, and assignment within expressions
SHALL produce syntax diagnostics rather than supported expression ASTs. Recognition
by the lexer or an existing prototype node SHALL NOT broaden this expression grammar.

#### Scenario: Literal and reference data is preserved

- **WHEN** the source contains `int count12 = 00042; bool flag = false; print(count12);`
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
keyword prefix. `main` SHALL remain an ordinary identifier with no special parser
treatment. Unsupported type keywords SHALL produce syntax
diagnostics rather than acquire support from the lexer or AST vocabulary.

#### Scenario: Existing lexer tokens support do and print

- **WHEN** the existing lexer supplies tokens for
  `{ do { print(1); } while (true); }`
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

- **WHEN** a source file declares `long x;`, `float y;`, or `void z;`
- **THEN** those declarations produce syntax diagnostics rather than supported
  variable declarations

### Requirement: Declaration and statement AST construction

The parser SHALL construct the existing AST shapes for all declaration and
statement productions of the authoritative
grammar. Translation units and blocks SHALL retain statement nodes in source order.
Variable declarations SHALL be wrapped as declaration statements at either level.
The root SHALL NOT contain a synthetic function or synthetic compound statement.
Uninitialized variable
declarations and absent else branches SHALL have absent optional children;
malformed required expressions or bodies SHALL NOT masquerade as these omissions.
Mandatory children of retained nodes SHALL be present.

Declarations SHALL be wrapped as declaration statements. Assignments SHALL be
direct `AssignStmt` nodes with `StmtKind::Assign`, an identifier-reference target,
and a required value expression, not expressions or expression-statement wrappers.
For initializers SHALL use the same declaration or assignment shapes;
their update SHALL own an `AssignStmt`. Assignment SHALL remain outside the
expression grammar; this representation SHALL NOT broaden accepted syntax.
The three for fields and braced control
bodies SHALL remain mandatory. Name lookup, redeclaration, initialization status,
and operand typing SHALL NOT be checked by the parser.

Node start locations SHALL identify the leading source token: variable
declarations at their type, assignment nodes and targets at their name,
blocks at `{`, and specialized statements at their keyword. Declaration
statement wrappers SHALL preserve their child's start location.
Expression leaves SHALL use their token start location; binary nodes SHALL use
operator locations. Parenthesized expressions SHALL retain the inner node and its
location, without requiring a separate grouping node.

#### Scenario: Declarations and assignments reuse the same constructs

- **WHEN** the body is `{ int x; bool flag = true; x = 1; }`
- **THEN** its ordered children contain an uninitialized int declaration, an
  initialized bool declaration, and a direct assignment statement
- **AND** declaration names and assignment targets own their spellings

#### Scenario: Selection, loops, print, and nested blocks have required children

- **WHEN** the body contains `if (true) { print(1); } else { print(2); }`,
  `if (false) { }`, `do { } while (true);`, and a nested `{ int x; }`
- **THEN** their AST nodes preserve the supplied conditions, ordered bodies,
  print operands, and optional else distinction

#### Scenario: Both for initializer forms are accepted

- **WHEN** the source contains `for (int x = 1; true; x = 2) { print(x); }` and
  `for (x = 1; false; x = 2) { }`
- **THEN** the first initializer is a declaration statement and the second is an
  direct assignment statement
- **AND** both loops have non-null condition, assignment update, and compound body
- **AND** the header separators are not consumed as statement terminators twice

#### Scenario: Assignment expressions remain unsupported

- **WHEN** source contains `x = y = 1;`, `int y = (x = 1);`,
  `print(x = 1);`, or `if (x = 1) { }`
- **THEN** parsing reports unsupported syntax and retains no malformed assignment
  or containing construct
- **AND** assignment AST nodes belong to the statement hierarchy, not expressions

#### Scenario: Semantic problems are not parser errors

- **WHEN** the source contains `x = true; int x; bool x; if (1) { }`
- **THEN** those grammatical declarations and statements are retained without
  declaration-before-use, redeclaration, or condition-type diagnostics

#### Scenario: Unsupported statement extensions are diagnosed

- **WHEN** a source file contains `return 1;`, `while (true) { }`, `;`, `1;`,
  `if (true) print(1);`, or a for with an empty header field
- **THEN** each unsupported form produces a syntax diagnostic rather than being
  accepted merely because lexer tokens or AST node kinds exist

#### Scenario: Statement locations survive comments and newlines

- **WHEN** the source contains a multiline comment followed by a declaration, assignment,
  nested block, and print on later lines
- **THEN** each retained node and wrapper has the start location of its leading
  token rather than the current cursor position after parsing its children

#### Scenario: Multiline expression locations follow their source tokens

- **WHEN** the source is `{\nint x = 1 +\n  2 * 3;\n}`
- **THEN** the addition node is located at line 2 column 11 and multiplication
  at line 3 column 5
- **AND** the multiplication's integer operands are located at line 3 columns
  3 and 7, and grouping/precedence does not overwrite these token locations

### Requirement: Executable declaration and statement recovery

The parser SHALL implement basic panic-mode recovery across declarations,
statements, and expressions. Translation units and blocks SHALL own statement-list
recovery: skip
malformed input to a safe statement semicolon, its enclosing closing brace, or
EOF. Recovery SHALL consume a synchronization semicolon, leave an enclosing brace
for its owner, and stop at EOF. At file scope, an unmatched closing brace SHALL
be diagnosed and consumed so later items can be parsed without retrying the same
boundary. A failed for parser's recovery SHALL NOT be repeated by the list owner.
Recovery SHALL retain completed nodes and resume later
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

- **WHEN** the source contains `int x\nprint(1);`
- **THEN** the missing semicolon is diagnosed once and both complete statements
  are retained in source order

#### Scenario: Missing semicolon uses a safe later boundary

- **WHEN** the source contains `int before; int x\nprint(1); print(2);`
- **THEN** the missing semicolon is diagnosed at `print` on the next line
- **AND** recovery retains `int before;` and resumes at `print(2);` after the safe semicolon
- **AND** the malformed declaration is omitted; retaining the skipped first print
  or inserting a semicolon is not required

#### Scenario: Multiple operand failures do not erase valid siblings

- **WHEN** the source contains `int x = ; print(1); int y = ; print(2);`
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

- **WHEN** the source contains `for (; true; x = 1) { print(1); } print(2);`
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
{
int x = ;
print(1);
bool flag = 2 * ;
print(flag);
x = 1 > 2 > 3;
print(2 + 3 * 4);
}
```

#### Scenario: File-scope recovery retains independent later items

- **WHEN** the source is `int x = ;\nprint(1);\n}\nint y = 2;\nprint(y);`
- **THEN** errors occur at line 1 column 9 and line 3 column 1
- **AND** the malformed declaration and unmatched brace are omitted
- **AND** the two prints and valid declaration remain in source order
- **AND** parsing terminates with a non-null root and non-null mandatory children

### Requirement: Lexer-owned failures preserve parser-local status

At an `invalid_identifier` or `invalid_numeric_constant` token the parser SHALL
propagate failure into existing contextual panic recovery without repeating the
lexer's diagnostic or setting parser-local syntax-error status for that spelling.
The shared engine SHALL retain lexical errors. Independent syntax errors SHALL
still be diagnosed and set local status. Affected malformed constructs SHALL be
omitted, not retained with null mandatory children or fake optional initialization.
Recovery SHALL preserve delimiter ownership, progress, termination, and virtual EOF.

#### Scenario: Lexical failures retain later valid statements

- **WHEN** the source contains `int a1b; print(1); int x = 3.14; print(2);`
- **THEN** exactly two lexical errors are reported and local syntax-error status is false
- **AND** both malformed declarations are omitted and both print nodes are retained
- **AND** the shared diagnostics indicate compilation failure

#### Scenario: Mixed multiline failures have independent ownership

- **WHEN** the source contains `int x = 1e3;` on line 2, `int y = ;` on line 3,
  and `print(2);` on line 4, each starting at column 1
- **THEN** one lexical error occurs at line 2 column 9 and one syntax error at line 3 column 9
- **AND** local syntax-error status is true and the valid print is retained
- **AND** no retained AST node has null mandatory children

#### Scenario: Invalid tokens in nested controls recover safely

- **WHEN** an if condition, do condition, or required for field contains an invalid spelling
- **THEN** the containing failed construct reaches contextual recovery without a duplicate
  spelling diagnostic, fabricated successful node, or leaked for-header fragment
- **AND** parsing terminates at safe boundaries or EOF

### Requirement: Function-free source-file parsing

The public parser entry point SHALL implement the borrowed-input, owning-result,
parser-local syntax status, repeated-invocation, and virtual EOF contracts.
It SHALL accept an ordered, possibly empty sequence of existing declarations and
statements directly through EOF. No source-level entry function, main spelling,
or outer block SHALL be required or synthesized. Function declarations and
definitions SHALL be rejected at file scope and inside blocks, regardless of
their name; existing function-related AST scaffolding SHALL NOT imply support.

The translation-unit root SHALL own an ordered list of statement nodes, with
variable declarations represented by declaration statements. Future function
support SHALL NOT make a source file require a leading `int main()`; entry-point
validation is independent of the source-file parsing contract. This requirement
does not implement functions, execution, semantic analysis, or CLI integration.

#### Scenario: Bare declarations and statements produce an owning root

- **WHEN** the source is `int a = 1; print(a); a = a + 1;`
- **THEN** the root directly owns a declaration, print, and assignment statement
  in source order, with no function or artificial block
- **AND** parsing succeeds and nodes retain their exact source locations and owned data

#### Scenario: Empty and trivia-only sources are valid

- **WHEN** the input is empty or contains only whitespace and closed comments
- **THEN** parsing succeeds with a non-null root containing no statements

#### Scenario: Main is an ordinary variable name

- **WHEN** the source is `int main = 1; main = main + 1; print(main);`
- **THEN** parsing succeeds without treating the name as an entry point

#### Scenario: Functions are deferred regardless of spelling or position

- **WHEN** the source contains `int main() {}`, `int other();`,
  `bool other() {}`, a parameterized function, or a function inside a block
- **THEN** parsing reports unsupported syntax and constructs no function node
- **AND** safely reachable later declarations and statements can still be retained

#### Scenario: Earlier errors and repeated calls remain independent

- **WHEN** otherwise valid input is parsed twice and the shared engine has an earlier error
- **THEN** both calls return separate valid trees with false local syntax-error status,
  starting from the beginning of the tokens
- **AND** the earlier tree remains valid and the shared engine retains its error

#### Scenario: EOF diagnostics include trailing trivia

- **WHEN** the source is `{\n// tail\n` with no physical EOF token
- **THEN** the missing closing brace is diagnosed at line 3 column 1
- **AND** parsing and recovery terminate with a non-null root
