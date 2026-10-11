## Why

MiniC++ spelling restrictions currently live in the parser while the intentionally
broader lexer accepts those spellings. Move classification and spelling diagnostics
to the lexical stage without making the lexer enforce statement grammar.

## What Changes

- **BREAKING**: Classify only `[A-Za-z]+[0-9]*` as identifiers and `[0-9]+`
  as valid `numeric_constant` tokens. Retain broader matched candidates as
  `invalid_identifier` or `invalid_numeric_constant`, diagnosed once by the lexer.
- Introduce `kw_do` and `kw_print`; keep `main` an identifier.
- Dispatch parser keywords by token kind and propagate lexical failures into
  existing recovery without duplicate diagnostics or local syntax-error status.
- Preserve broader operators/reserved keywords, trivia, locations, virtual EOF,
  authoritative grammar, AST hierarchy, and public parser API.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `lexical-analysis`: Restricted valid spellings, new keywords, retained invalid
  candidates, and lexer-owned diagnostics.
- `syntax-analysis`: Consume lexer classification rather than revalidate spelling;
  distinguish lexical failures from independently diagnosed syntax errors.

## Impact

TokenKind, Flex rules, lexer adapter, parser implementation/comments, and runtime
lexer/parser tests. No semantic analysis, CLI integration, or language extensions.
This change follows `implement-parser`; synchronize that change first, then this
delta. Neither change is synchronized or archived during implementation here.
