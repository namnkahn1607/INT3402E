## 1. AST root compatibility

- [x] 1.1 Introduce the non-`Decl` root `ast::TranslationUnit` with the existing owning declaration vector and preserve `ast::Program` as an alias, leaving other prototype nodes unchanged; verify both names designate the same type and existing AST code still compiles.

## 2. Parser interface

- [x] 2.1 Add the self-contained `include/parser/parser.h` declaration shown in `design.md`, including `ParseResult` with an owning `translation_unit` and `has_syntax_errors`, borrowed inputs, shared diagnostics, indexed cursor, and EOF-helper declarations; verify it compiles as the first project header included in a C++20 translation unit.
- [x] 2.2 Declare the simplified private program-structure, statement-dispatch, reusable-construct, specialized-statement, and coworker-owned expression methods shown in `design.md`, with no separate declaration-statement, assignment-statement, or for-initializer helpers and no fixed recovery signatures; verify every form in the unchanged grammar remains covered and no parser method definitions, dummy results, or CLI calls were added.
- [x] 2.3 Document input lifetimes, `TokenKind::eof`/null-pointer lookahead, false matching and no advancement at EOF, explicit EOF loop termination, true EOF locations, fresh cursor/local status per invocation without resetting the shared engine, partial-tree intent, and expression delimiter/error responsibilities; verify agreement with the spec and design, including common syntax-error reporting, failure propagation without duplicate diagnostics, semicolon-free declaration/assignment components, statement-owned AST wrapping, and for-owned restricted initializer dispatch and separators with all three fields mandatory.
- [x] 2.4 Reference the syntax-analysis spec as the sole authoritative grammar in interface documentation and document lexer compatibility and runtime acceptance tests as prerequisites for the later executable implementation; verify no duplicate BNF, stale README grammar-conflict claim, fixed recovery helper API, or claim of current runtime support remains in the change artifacts.

## 3. Compile-only contract checks

- [x] 3.1 Add a compile-only interface check under `tests/parser/`, registered as an object target when `BUILD_TESTING` is enabled, using the project's C++20/warning options; verify parser constructibility from the declared inputs, the `ParseResult` return type, its owning translation-unit and boolean member types, its move-only ownership, and the root compatibility alias with unevaluated type checks, without calling undefined methods or linking a runnable parser.
- [x] 3.2 Run `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug`; verify the interface check builds and existing enabled tests pass, reporting the already-disabled CLI smoke test separately and making no claim of runtime parser coverage.
- [x] 3.3 Review the final diff against the interface delta spec and validate with `openspec validate define-parser-interface --strict`; verify lexical-analysis specs, lexer production behavior, existing unrelated worktree changes, and unsupported AST nodes were left untouched.

## Future executable acceptance (not tasks for this increment)

The spec's labelled runtime scenarios must become tests when parser bodies and
lexer integration are implemented. They cover grammar forms, operator-chain
rejection, precedence/association, EOF safety, non-consumability and loop termination,
source locations, repeat invocations without clearing shared errors, parse-local
status with prior lexical errors, and multi-error recovery without redundant
diagnostics for propagated failures. Resolve `do`/`print`
recognition and identifier/integer validation before enabling executable parser
integration. This change neither implements those prerequisites nor claims those
tests have passed.
