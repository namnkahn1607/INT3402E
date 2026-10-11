## 1. Function-free source parsing

- [x] 1.1 Replace the main wrapper and root declaration vector with ordered statement parsing; verify bare declarations, mixed top-level controls, empty/trivia input, ordinary main identifiers, and rejection of all function forms through the real lexer/parser.
- [x] 1.2 Preserve file/block/for recovery ownership and progress; verify multiple errors across lines, unmatched top-level braces, malformed functions followed by valid statements, and truncation termination with valid partial AST children.

## 2. Contracts and verification

- [x] 2.1 Migrate existing executable parser fixtures and AST assertions away from main, retaining precedence/control/lexical/lifecycle coverage; run the complete lexer/parser suite and warnings-as-errors Debug and Release builds.
- [x] 2.2 Document function-free syntax in README and synchronize the prerequisite deltas followed by this change, reconciling stale Delivery Scope/examples without archiving; run strict OpenSpec validation and review the focused diff.

## Verification record

- Added public lexer/parser integration regressions first and observed the old
  parser reject bare declarations and empty/trivia-only inputs.
- Debug and Release CMake/Ninja builds passed with project warnings as errors,
  using locally cached GoogleTest sources. CTest passed all 62 enabled tests in
  each configuration; the pre-existing CLI smoke test remains disabled.
- A separate Release production build also passed with BUILD_TESTING=OFF and
  warnings as errors, without a GoogleTest dependency.
- Strict main-spec and remove-main-wrapper validation, formatting checks, and
  git diff --check passed. The prerequisite implementation and lexer deltas were
  synchronized first, then this delta. Delivery Scope and obsolete examples were
  reconciled; historical change artifacts were left intact and nothing archived.
