## Why

The current parser binds a source file to exactly one `int main()` despite the
current iteration not requiring function support. Accept declarations and
statements directly, keeping entry-point policy separate from source-file syntax.

## What Changes

- **BREAKING**: Replace the mandatory main definition with an ordered, possibly
  empty sequence of the existing declarations and statements.
- Reject all function declarations and definitions, including `int main()`;
  function support is deferred and must not reinstate a mandatory main prefix.
- Store top-level statement nodes in TranslationUnit, using DeclStmt for variables;
  do not fabricate a function or enclosing source block.
- Preserve print as a keyword and built-in statement, expression precedence,
  assignment restrictions, mandatory for fields, and braced control bodies.
- Apply basic panic recovery at file scope as well as within blocks.
- Synchronize the completed parser/lexer prerequisite contracts and this delta
  into the main specs; do not archive changes or integrate the lexer-only CLI.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `syntax-analysis`: Function-free source files, ordered top-level AST items,
  empty input acceptance, and safe file-scope recovery without a main requirement.

## Impact

Parser implementation/header, TranslationUnit contents, executable parser tests,
README syntax documentation, and OpenSpec contracts. The parser public entry-point
signature, lexer behavior, statement/expression hierarchy, and shared diagnostics
remain unchanged. Existing FuncDecl/CallExpr/ReturnStmt scaffolding is retained but
does not constitute supported function behavior. Semantic execution is not added.
