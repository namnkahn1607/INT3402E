## Context

See `proposal.md` for motivation and `specs/syntax-analysis/spec.md` for this
increment's acceptance boundary. The sole grammar remains
`openspec/specs/syntax-analysis/spec.md`; this design does not introduce another.

`include/parser/parser.h` has the agreed minimal public entry point, borrowed
source/token span, indexed cursor, owning result, and simplified private methods.
None have definitions. The AST already has all required declaration/statement/expression
shapes. `tests/parser/parser_interface_check.cpp` checks declarations only.
`src/CMakeLists.txt` currently builds common, lexer, AST, and a lexer-only CLI.

The Flex scanner emits no EOF token, recognizes `do` and `print` as identifiers,
and accepts broader identifiers and numeric constants than the target grammar.
The shared diagnostic engine prints to stderr and has sticky error status; it
does not expose diagnostic storage or a reset operation.

## Goals / Non-Goals

**Goals:**

- Deliver all parser definitions and test the complete grammar through production
  code without changing the public API.
- Preserve recoverable trees, delimiter ownership, source locations, and explicit
  progress guarantees in malformed input.
- Keep declarations, statements, expressions, and shared infrastructure organized
  by responsibility in one implementation file under one owner.

**Non-Goals:**

- Changing expression inheritance, introducing visitors/error nodes, or broadening
  types/statements because prototype nodes exist.
- Linking parsing into the CLI, code generation, symbol tables, type validation,
  or initialization checks.
- Iteration-1 semicolon insertion, precise malformed-for reconstruction, or
  guaranteed preservation of complex nested partial control trees.

## Decisions

### 1. One parser and cursor, one implementation file

Implement common input/cursor, diagnostics, lifecycle, translation-unit,
declaration, statement, and production expression operations in
`src/parser/parser.cpp`, grouped by responsibility. One owner maintains this
source, private helpers, parser header, build wiring, and tests. All components use the same
cursor and diagnostic path, not independently maintained parser state.

Add `src/parser/CMakeLists.txt` using the existing static `add_module` convention
and public common/AST dependencies. Include all production parser definitions;
parser tests link this library and the real lexer, with no alternate expression
definitions. The existing CLI remains lexer-only. Verify tests-disabled builds
and production-only parser linkage as well as ordinary executable tests.

### 2. Keep the simplified grammar dispatchers

`ParseStatement()` dispatches declaration, assignment, print, if, do/while, for,
or nested block. It wraps successful declarations and assignments and owns their
statement semicolons. `ParseDeclaration()` and `ParseAssignment()` stop before
caller-owned separators. Print and do/while own their terminating semicolons.

`ParseForStatement()` chooses a declaration or assignment initializer directly,
wraps it in the existing statement shape, and owns both header semicolons, its
closing parenthesis, and braced body. Its update uses `ParseAssignment()`.
Do not restore `ParseDeclarationStatement`, `ParseAssignmentStatement`, or
`ParseForInitializer`, or use unrestricted statement dispatch in a for header.

Snapshot leading token locations before consuming children. Construct names as
owning strings and transfer node ownership only after required children exist.
Successful blocks may contain intact statements recovered around errors.

### 3. Parser-side adaptation preserves the lexical-analysis contract

Recognize `do` and `print` only as exact identifier-token spellings in dispatch.
Reject their use as declaration/assignment names. Validate other names against
the ASCII identifier restriction, without splitting a token or using locale-
dependent alphabetic classification. Existing keyword token kinds cannot be
ordinary identifiers. `main` is matched exactly in the function header; it is
not a newly reserved lexer keyword.

Reuse file-local spelling validation in `src/parser/parser.cpp` across declarations,
assignment targets, and expression leaves. Numeric tokens must match `[0-9]+`; retain integer spelling
in the AST without inventing range/type checks. Reject broader lexer numeric
forms and invalid names as whole tokens, not smaller valid pieces.

Adding lexer keyword kinds would change established lexer output and require a
separate lexical-analysis delta. Parser adaptation provides unambiguous grammar
terminals while keeping that out of this parser-focused change.

### 4. Implement lifecycle and virtual EOF before structural parsing

Reset the cursor and local error flag for every public parse call; never reset
the shared engine. Always allocate an owning translation-unit root for ordinary
syntax failure. Preserve a successfully parsed main even when trailing tokens
are rejected; do not produce additional function declarations.

Compute EOF from the complete borrowed source using the lexer's byte-column and
newline conventions, not from the final token. Guard lookahead by comparing the
offset against the remaining span before addition. EOF is only observed:
matching it always fails, consumption is a no-op returning null, and top-level
completion uses `AtEnd()` rather than `Expect(eof)`.

Every syntax report sets the local flag before calling the engine. A preexisting
global error must never cause a valid child to be treated as a parser failure.

### 5. Basic panic-mode recovery for iteration 1

Choose private recovery helpers while implementing; their signatures are not
part of the contract. Use the following ownership rules:

- Components diagnose their own structural errors. On an expression failure,
  propagate the already-diagnosed failure without another operand diagnostic.
- A block owns statement-list recovery. Skip malformed input to a safe `;`,
  enclosing `}`, or EOF. Consume a recovery semicolon, leave the enclosing brace
  to its block owner, and stop at EOF. Resume siblings only at a safe boundary.
- Keep minimal delimiter-depth/context information to avoid treating a nested
  brace as the surrounding block's closer or for-header semicolons as ordinary
  statement boundaries. Header fragments must not become surrounding statements.
- Omit malformed constructs instead of inserting tokens or attaching null
  required children. Retain already completed nodes and safely reached siblings;
  when a control/header boundary is unclear, discard a larger construct or stop
  the affected block rather than reconstruct its intended shape.
- Every loop must consume input, terminate at EOF, or yield an enclosing boundary.
  Boundary recognition must not cause the caller to retry the same failure
  indefinitely. Add assertions/progress guards as needed without swallowing an
  owner delimiter to force progress.

Missing-semicolon insertion, exact recovery past malformed for bodies, and
preservation of else/do tails across damaged nested structures are deferred
improvements, not iteration-1 acceptance. Basic panic recovery can discard valid
syntax in an ambiguous region; it does not promise to diagnose every error there.
The non-null root, accurate primary diagnostics, and non-null mandatory-child
contracts still apply. No new error-node hierarchy is required.

Implement basic statement synchronization first, then minimal nesting/header
guards and truncation tests. Acceptance centers on independent semicolon-delimited
errors on different lines, safely reached later nodes, and guaranteed termination.

### 6. Production Pratt expressions without broadening the grammar

Define the existing `ParseExpression(int min_binding_power)` and `ParsePrimary()`
in `parser.cpp`. Primary parsing creates owning name references, integer
spellings, and boolean values, or consumes expression-owned grouping parentheses
and returns the inner node. No new grouping node or expression/statement
inheritance change is needed. Leaves use their token's start location; binary
nodes use the operator's location and preserve both children.

Use a small operator-to-binding-power mapping: `==` (1, 2), `>`/`>=` (3, 4),
`+` (5, 6), and `*` (7, 8). With the usual stop when left power is below the
minimum, these pairs give the agreed precedence and left-associated arithmetic.
Assignment is parsed only by `ParseAssignment()`, not registered as an infix.
Do not register broader lexer/AST operators, unary forms, or calls.

Binding powers alone do not enforce the grammar's chain restrictions. Track
unparenthesized comparison/equality use within each Pratt invocation and diagnose
the second operator of either restricted level. Recursive operands and explicit
grouping have their own tracking. Do not infer a forbidden chain from the AST
kind of the left operand: `(a > b) > c` and `a > b == c > d` must remain grammatical.

On missing operands, invalid spellings, extra restricted operators, or malformed
grouping, report once at the offending token/EOF and return failure. Leave
caller-owned delimiters for structural recovery; null does not imply no input
was consumed. Do not attach an incomplete binary node or return an apparently
successful prefix to evade expression errors. Expression parsing owns local
group delimiters, while the surrounding statement/header owns synchronization.

### 7. Executable acceptance uses only production parsing

Test through actual Flex output and the public parse entry point. Capture stderr
to assert diagnostic counts and locations using existing GoogleTest facilities;
do not expand the diagnostic API just for these tests. Retain the compile-only
header contract check. For otherwise inaccessible cursor invariants such as
maximum lookahead and repeated EOF consumption, use narrowly scoped test access
if needed, without adding a public cursor API or a `#define private public` hack.

All executable tests use production expressions, including precedence/chain
checks and complete programs combining arithmetic, conditions, print, and loops.
Assert AST shape, owning data, mandatory children, and locations, not just success.
Malformed-program tests must include independent errors on different source lines,
failure after partial expression consumption, and valid later statements. Check
diagnostic counts/positions, retained AST portions, EOF, and CTest timeouts.
The current compile-only test still refers to `ast::Program`, which the current
AST header no longer exposes; update that obsolete assertion to the existing
TranslationUnit contract instead of reintroducing an alias.

## Risks / Trade-offs

- Pratt overaccepts chains -> Assert both rejected chains and permitted grouped
  or separate comparison operands against the authoritative grammar.
- `do`/`print` reserved only in the parser -> Reuse exact-spelling validation in
  every name context and cover keyword-prefix cases using real lexer tokens.
- Panic recovery can skip valid syntax -> Require continuation at demonstrably
  safe boundaries; test multiple independent errors, malformed nested/for input,
  and truncation for bounded diagnostics and termination, not exact reconstruction.
- Main spec still has interface-only Delivery Scope prose -> Require the explicit
  synchronization/archive reconciliation below; delta application alone cannot
  update this non-requirement section.

## Migration Plan

No public API or lexer migration is required. Build shared infrastructure,
declarations/assignments, expressions, structural parsing, and contextual recovery
in sequence; verify each stage as its dependencies become available. Do not add
temporary production stubs or test substitutions merely to link an early stage.
Use compile checks until dependent definitions exist, then run the production
parser tests. CLI integration remains a separate future change.
Any rollback of this increment must be focused on its own parser files and build
entries, preserving the already-established interface and unrelated user work.

### Required synchronization/archive reconciliation

After implementation is verified, an authorized synchronization/archive must
apply the requirement deltas and replace the main spec's Delivery Scope with:

> Syntax analysis implements the authoritative MiniC++ grammar using recursive
> descent for declarations/statements and Pratt expressions, constructs owning
> ASTs, and uses basic panic recovery to report multiple syntax errors with source
> locations, retaining valid syntax when safely reachable. Advanced recovery
> scenarios explicitly marked deferred are not iteration-1 acceptance.
> Non-deferred runtime scenarios are executable acceptance criteria using production parsing,
> not deferred expression integration. Semantic analysis, code generation, and
> parser CLI integration are not delivered. The lexical-analysis baseline remains
> unchanged.

Keep the authoritative grammar and expression requirements intact. Review the
synchronized spec for contradictory interface-only delivery claims, obsolete
future-runtime labels, replaced handoff requirements, and the borrowed-input
linkage disclaimer. Preserve explicit advanced-recovery deferrals and validate
strictly before declaring synchronization/archive complete. This reconciliation
is mandatory, not an automatic effect of applying requirement deltas. Do not
modify the main spec prematurely or initiate synchronization/archive unrequested.
