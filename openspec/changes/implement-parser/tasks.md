## 1. Shared parser infrastructure, cursor, diagnostics, and virtual EOF

- [x] 1.1 Prepare the production implementation file and static parser module; verify all parsing definitions are consolidated in parser.cpp with one shared Parser and no temporary stubs or alternate expression definitions.
- [x] 1.2 Update parser-header status/grammar/recovery comments and align the compile-only check with the current TranslationUnit root rather than the obsolete Program alias; verify self-contained header compilation and unchanged public signatures/AST hierarchy.
- [x] 1.3 Implement constructor, shared cursor, overflow-safe lookahead, non-consumable virtual EOF, and complete-source EOF location; verify focused tests for empty input, maximum lookahead from a nonzero cursor, repeated EOF consumption/matching, and trailing multiline trivia without adding public cursor APIs.
- [x] 1.4 Implement the common syntax-reporting path and per-operation cursor/local-status reset without clearing shared errors; verify local syntax status is independent of earlier errors and document repeated-call tests for the public entry point once structural parsing is available.

## 2. Declaration and assignment parsing

- [x] 2.1 Implement exact parser-side do/print recognition and reusable ASCII identifier validation across declaration names, assignment targets, and expression references; verify valid digit suffixes/keyword prefixes and rejection of underscores, embedded digits, and reserved spellings using unchanged Flex output.
- [x] 2.2 Implement ParseType and ParseDeclaration for int/bool, optional initialization, owned names, and type-token locations; verify uninitialized declarations versus diagnosed malformed initializers and component separator ownership once production expressions are available.
- [x] 2.3 Implement ParseAssignment with a name-reference target and production expression value; verify target/assignment locations, required children, unconsumed caller separators, and no name-resolution/type validation or assignment infix support.

## 3. Production Pratt expressions and precedence

- [x] 3.1 Implement ParsePrimary for restricted identifiers, decimal-integer spellings, booleans, and grouped expressions; verify owning node data/locations, grouped delimiter ownership, and rejection of broader numeric tokens and unsupported primary forms.
- [x] 3.2 Implement ParseExpression using the existing minimum-binding-power interface and only the agreed operators; verify multiplication over addition, comparison over equality, parentheses, and left-associated repeated addition/multiplication through AST assertions.
- [x] 3.3 Enforce one unparenthesized comparison and equality per corresponding grammar level; verify a > b > c, a >= b > c, and a == b == c are rejected while a > b == c > d and (a > b) > c remain syntactically accepted.
- [x] 3.4 Implement diagnosed expression failure for missing operands, invalid spellings, restricted chains, and malformed grouping; verify failures before/after partial consumption preserve caller semicolons, outer closing delimiters, braces, and EOF without attaching incomplete binary nodes or accepting unsupported expression syntax.

## 4. Statements, blocks, main function, and control flow

- [x] 4.1 Implement block/statement dispatch and declaration/assignment wrappers with caller-owned statement semicolons; verify ordered and nested bodies, leading locations, and rejection of unsupported statement/type forms using production parsing only.
- [x] 4.2 Implement main and translation-unit parsing for exactly int main() with a braced body; verify non-null owning roots, malformed headers, extra functions/trailing tokens, earlier-error isolation, repeated independent trees, and AST validity after parser/input destruction.
- [x] 4.3 Implement print, braced if/else, and do/while with required operands/bodies and correct separator ownership; verify nested/grouped production expressions, optional else distinction, source locations, and rejection of unbraced bodies or missing do tails.
- [x] 4.4 Implement for with declaration-or-assignment initialization, mandatory expression condition, assignment update, and braced body; verify both initializer shapes, all separator ownership, and rejection of omitted fields or unsupported initializer/update forms without restoring redundant private dispatch wrappers.
- [x] 4.5 Run executable expression and structural tests through the now-complete production parser; verify parser.cpp provides all required definitions and every valid retained node has non-null mandatory children.

## 5. Basic panic-mode recovery across parser components

- [x] 5.1 Implement statement/block panic recovery at safe semicolons, enclosing braces, and EOF; verify consumed recovery semicolons, owner-held braces, retained completed nodes/safely reached siblings, and missing-semicolon recovery without insertion requirements.
- [x] 5.2 Propagate diagnosed expression failures to structural recovery without re-reporting the same operand/chain error; verify malformed arithmetic/grouping and declaration/assignment/print operands retain safely reached later statements without null mandatory children.
- [x] 5.3 Add minimal nesting context and progress-or-owner-yield guarantees; verify nested braces are not mistaken for enclosing closers and no caller retries an unchanged failure, allowing malformed controls/ambiguous regions to be omitted rather than requiring complex partial-tree preservation.
- [x] 5.4 Add minimal for-header recovery context; verify header separators/fragments do not become surrounding statements, malformed loops are omitted without null required children, and parsing terminates without requiring precise balanced header/body reconstruction or retention of the following sibling.
- [x] 5.5 Add bounded-diagnostic truncation/unexpected-token tests with timeouts across expressions, declarations, control headers, and nested blocks; verify safe EOF termination and non-null partial roots with no fabricated optional children hiding failed required syntax.

## 6. Full parser integration, AST validation, and multiline errors

- [x] 6.1 Add complete-program tests combining every agreed statement form, arithmetic/comparison/equality, both for initializers, and comments; verify all use the real Flex lexer and production parser, assert AST structure/locations/ownership, and cover the authoritative grammar rather than restricted operands.
- [x] 6.2 Add the spec's independent multiline-error program, basic missing-semicolon panic case, and multiline expression/comment/EOF cases; verify primary diagnostic counts/locations, safely reached valid prints/subtrees, true local status, and no duplicate diagnostics or nontermination without testing deferred advanced recovery as iteration-1 acceptance.
- [x] 6.3 Add syntax-only boundary and unsupported-feature tests; verify undeclared/redeclared names and operand-type issues receive no semantic diagnostics, while broader operators, types, calls, returns, extra functions, unbraced bodies, and missing for fields are rejected.
- [x] 6.4 Run cmake --preset debug, cmake --build --preset debug, and ctest --preset debug, plus warning-as-error and BUILD_TESTING=OFF builds; verify all enabled lexer/parser tests and compile checks pass, and a parser-using client links solely to the production library with all parser definitions while the lexer-only CLI remains unchanged.
- [x] 6.5 Review the complete-implementation boundary and run openspec validate implement-parser --strict; verify no semantic/codegen/CLI or extra language behavior slipped into scope and no executable tests substitute expression definitions.
- [x] 6.6 Verify the synchronization/archive handoff records the design's complete Delivery Scope replacement and retirement of obsolete interface-only/future-runtime descriptions while preserving explicit advanced-recovery deferrals; require strict validation of the reconciled main spec at later authorized synchronization/archive, without prematurely editing it or starting synchronization/archive now.
