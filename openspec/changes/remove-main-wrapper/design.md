## Context

See proposal.md. The implemented parser currently calls ParseMainFunction and
stores one FuncDecl in TranslationUnit::decls. All existing statement forms are
already reusable; lexer classification and Pratt expressions need no change.

## Goals / Non-Goals

Goals: remove the mandatory main wrapper, reject functions, and retain ordered
top-level items with safe multiline recovery. Empty input is valid.

Non-goals: expression assignment, general expression/empty statements, optional
for clauses, Expr inheritance, semantic execution, and CLI integration.

## Decisions

- Keep TranslationUnit and ParseTranslationUnit names/signatures; replace the
  root's declaration vector with vector<StmtPtr> statements. DeclStmt already
  represents declarations alongside other statements. A fabricated main or block
  would obscure source structure, so neither is introduced.
- Remove ParseMainFunction. Parse statements repeatedly until EOF, reusing
  declaration/statement dispatch and existing panic recovery. The file owns
  unmatched closing braces: diagnose and consume them; block owners still consume
  their own braces. Failed for parsers retain their existing recovery ownership.
- Preserve unsupported FuncDecl/CallExpr/ReturnStmt types as prototype scaffolding
  rather than deleting unrelated design work. Reject function syntax uniformly,
  including the old main wrapper and nested functions. Main remains an identifier.
- Existing block-focused tests may use explicit standalone blocks, not hidden
  function wrappers. Add direct top-level tests for bare programs, root ordering,
  locations, recovery, ownership, and function rejection.

## Risks / Trade-offs

- Root contents are an AST API break: update all consumers/tests to statements.
- A stray file-scope brace can stall owner-yield recovery: the top-level loop
  explicitly consumes it and is covered by truncation and repeated-brace tests.
- Older deltas can restore the main grammar if replayed out of order: synchronize
  implement-parser, align-lexer-classification, then this change. Leave historical
  artifacts intact and document that dependency; do not archive automatically.

## Migration Plan

Remove the source main wrapper and use the root's statement vector. Preserve the
existing expression grammar and built-in print. Verify runtime tests and strict
spec validation before syncing this delta. Update the main Delivery Scope to
describe function-free syntax analysis; do not claim execution or CLI parsing.
