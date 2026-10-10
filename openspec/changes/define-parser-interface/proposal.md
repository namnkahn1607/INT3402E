## Why

The compiler has a Flex lexer and an AST prototype, but no parser interface for
the two developers to implement against. A shared interface will let declaration
and statement parsing proceed alongside expression parsing with agreed ownership,
token consumption, and recovery contracts.

## What Changes

- Introduce `include/parser/parser.h` with `parser::Parser`, one public
  `ParseTranslationUnit()` entry point, private declaration and statement methods,
  and private expression method declarations reserved for the coworker.
- Return `ParseResult` containing an owning translation-unit pointer and
  `has_syntax_errors`, independent of earlier lexer errors. Keep diagnostic
  reporting and aggregate compilation status in the shared `DiagnosisEngine`.
- Declare a borrowed token span, an index cursor, source text for EOF locations,
  and explicit end-of-span behavior using the existing EOF kind without requiring
  the lexer to append a token.
- Name the owning AST root `ast::TranslationUnit`, preserving `ast::Program`
  as a compatibility alias. Keep expressions and statements as separate AST
  hierarchies.
- Make the [syntax-analysis spec](specs/syntax-analysis/spec.md) the authoritative
  target grammar, including explicit operator-chain rejection scenarios.
- Document partial-tree and contextual-recovery invariants without fixing private
  recovery helper signatures. This change delivers declarations and documentation
  only; no parser method bodies, dummy expression results, or runtime parsing.

## Capabilities

### New Capabilities

- `syntax-analysis`: The compile-visible parser interface and ownership contract
  for the agreed subset, its authoritative target grammar, and future executable
  acceptance criteria. Executable parsing is explicitly deferred.

### Modified Capabilities

None. The `lexical-analysis` baseline remains unchanged.

## Impact

- Future implementation touches `include/parser/parser.h` and the root declaration
  in `include/ast/ast.h`, with compile-only interface checks under `tests/` and
  their CMake registration; it does not wire the CLI to a parser.
- Existing lexer token and diagnostic types are reused. No external dependency is
  added, and C++20 remains sufficient.
- Declaration/statement implementation belongs to the user; expression
  implementation belongs to the coworker. Both will share the same parser cursor.
- Lexer gaps (`do`, `print`, identifier restrictions, integer-literal
  classification) must be resolved before executable integration. Recovery AST
  nodes and runtime acceptance tests remain follow-up work. Header declarations
  do not establish language acceptance or promote the AST prototype to a tested
  compiler capability.
