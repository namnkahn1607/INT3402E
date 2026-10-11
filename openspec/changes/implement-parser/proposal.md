## Why

The agreed parser API and grammar exist, but no parser methods are implemented.
One implementation owner will deliver the complete agreed syntax-analysis layer,
including AST construction and basic panic-mode recovery that reports multiple
syntax errors and retains valid nodes when safe continuation is possible.

## What Changes

- Implement translation-unit/main parsing, blocks, variable declarations,
  assignments, print, braced if/else, do/while, and the mandatory-field for form
  using recursive descent and the existing authoritative grammar.
- Implement production Pratt expressions with the exact precedence, associativity,
  chain restrictions, identifiers, integer/boolean literals, and grouping already agreed.
- Implement shared cursor, virtual EOF, source-location, parser-local error
  status, repeated invocation, and iteration-1 panic recovery at `;`, `}`, and EOF.
- Keep only minimal nesting/for-header awareness for safe synchronization;
  defer semicolon insertion and advanced nested/control-tree preservation.
- Preserve the simplified private dispatch API and current AST hierarchy.
- Recognize exact identifier-token spellings `do` and `print` as reserved parser
  terminals, and validate identifier and integer spellings throughout parsing without
  changing Flex tokenization or expanding the target language.
- Add a fully defined static parser library and executable tests using only
  production parser code, covering complete programs, expression AST structure,
  and independent syntax errors on multiple lines.
- Update the header's stale grammar reference and implementation-status comments;
  align compile-only checks with the current TranslationUnit root.
- Do not implement semantic checks, code generation, new AST node families,
  or parser CLI integration in this change.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `syntax-analysis`: Advance interface-only contracts to complete executable
  declaration, statement, and expression parsing, AST construction, and basic
  multiline error recovery. The authoritative grammar remains unchanged.

## Impact

- Implementation will touch `include/parser/parser.h`, new `src/parser/` files,
  `src/CMakeLists.txt`, and `tests/parser/` build/test files.
- Public parser signatures, lexer output, AST ownership, shared diagnostic API,
  and lexer-only CLI behavior remain unchanged.
- One owner maintains shared helpers, header, build wiring, and all parser methods
  and tests. All parsing definitions live in `src/parser/parser.cpp`, organized
  by responsibility, with no external expression definitions or test substitutions required.
- Complete syntax acceptance does not imply semantic validity or end-to-end compilation.
- No new external dependency is needed. Existing CMake, Ninja, and GoogleTest
  infrastructure will be reused; existing user changes will be preserved.
- Synchronization/archive must reconcile the main spec's interface-only Delivery
  Scope with complete syntax-analysis delivery. The authoritative spec is not
  changed prematurely during planning.
