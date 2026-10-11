## 1. Lexical classification

- [x] 1.1 Add keyword/invalid token kinds, restricted valid rules and whole-candidate fallback classification; verify keyword prefixes, digit suffixes, numeric forms, retained reserved vocabulary, and non-EOF output in lexer tests.
- [x] 1.2 Diagnose invalid spellings once in the lexer adapter while retaining tokens and continuing; verify exact messages, counts, and locations across multiline trivia in lexer tests.

## 2. Parser adaptation

- [x] 2.1 Replace keyword lexeme dispatch and redundant spelling checks with token kinds, documenting lexer diagnostic ownership; verify valid complete programs, reserved words, names, literals, precedence, controls, and unchanged public API/AST.
- [x] 2.2 Propagate invalid tokens into existing panic recovery without duplicate diagnostics or local syntax status for lexical errors; verify mixed multiline errors, safe continuation, mandatory AST children, nested/for ownership, repeated parsing, and termination.

## 3. Integration verification

- [x] 3.1 Build production targets with warnings as errors and run all executable lexer/parser tests against freshly built libraries, independently if the unrelated missing interface-check source still blocks normal CMake tests; record results and remaining build limitation.
- [x] 3.2 Run strict OpenSpec validation and review the focused diff for unrelated changes, grammar/API preservation, and documented synchronization order without synchronizing or archiving.

## Verification record

- Debug and Release production CMake/Ninja builds passed with `BUILD_TESTING=OFF`
  and `INT3402E_WARNINGS_AS_ERRORS=ON`.
- Runtime tests compiled with C++20 and project warning flags plus `-Werror`,
  linked against freshly built Debug libraries and cached GoogleTest: 13 lexer
  and 41 parser tests passed, each executable run under a 10-second timeout.
- Normal testing configuration was attempted with cached GoogleTest and remains
  blocked by missing `tests/parser/parser_interface_check.cpp`; no unrelated
  test configuration or interface-check source was changed. Disabled CLI smoke
  testing was not run.
- Formatting checks, `git diff --check`, and strict OpenSpec validation passed.
  Validation reports an informational archive prerequisite: syntax MODIFIED
  headers are supplied by `implement-parser`, which must be synchronized first.
- Grammar, AST definitions, and public parser signatures remain unchanged.
  Neither change was synchronized or archived.
