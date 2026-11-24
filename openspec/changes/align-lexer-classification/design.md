## Context

See proposal.md for scope. Flex currently recognizes broad identifiers and numeric
forms; parser.cpp validates their spellings and dispatches `do`/`print` by lexeme.
The complete parser is delivered by the unsynchronized `implement-parser` change.
Its grammar matches the authoritative main syntax spec and remains unchanged.

## Goals / Non-Goals

Goals: lexer-owned spelling classification and diagnostics, kind-based parser
dispatch, and unchanged safe recovery/AST contracts.

Non-goals: narrowing reserved keyword/operator vocabulary, changing grammar,
semantic checks, new numeric candidate forms, or advanced recovery.

## Decisions

- Keep exact keyword rules first, restricted valid identifier/integer rules next,
  and existing broader candidate rules as invalid fallbacks. Flex longest-match
  preserves complete invalid candidates; rule order resolves equal-length matches.
  Deleting broad rules would split invalid spellings and lose useful diagnostics.
- Diagnose invalid kinds in the lexer adapter and retain their tokens. Parser
  consumers trust classification; `main` still requires its exact identifier name.
- Suppress a syntax diagnostic only when its current offending token is an explicit
  invalid spelling token at that diagnostic location. Return failure normally and
  let existing statement/for recovery own synchronization. Do not infer failure
  from the diagnostic engine's sticky state or suppress independent syntax errors.
  Synthetic token clients must honor the same classification/diagnostic ownership.
- Keep all parser methods in parser.cpp, with no public API or AST changes.

## Risks / Trade-offs

- Lexer contract break: floating/exponent/suffixed forms and underscore/interleaved
  digit names now produce lexical errors. Update tests and separate error-stage status.
- Candidate boundaries remain the existing scanner boundaries, not a complete C++
  preprocessing-number grammar; malformed exponents/hex forms are not newly defined.
- Existing panic recovery may omit ambiguous regions; require safe continuation
  only at established boundaries and validate retained mandatory children.
- Missing tests/parser/parser_interface_check.cpp blocks normal testing configuration.
  Preserve this unrelated deletion and run runtime tests independently against fresh
  warnings-as-errors production libraries and cached GoogleTest if it remains missing.

## Migration Plan

Implement classifications and parser adaptation together. Strictly validate this
change and run lexer/parser suites. No synchronization/archive here. Later synchronize
`implement-parser` first, including its Delivery Scope/Purpose reconciliation, then
this change: its syntax MODIFIED requirements target that prerequisite delta's added
requirements. Replace obsolete broader-spelling/parser-lexeme contracts, not just
append contradictory rules. Reconcile lexical boundary notes about numeric suffix
acceptance and prototype-only AST status with the delivered parser, while retaining
the distinction between lexing and language support. Rollback must restore both
lexer and parser contracts together, not just the token kinds.
