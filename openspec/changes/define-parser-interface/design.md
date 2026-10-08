## Context

See [proposal.md](proposal.md) for motivation. The
[syntax-analysis spec](specs/syntax-analysis/spec.md) is the authoritative target
grammar and contract; its runtime scenarios are future acceptance criteria.
There is no parser implementation or parser test suite. The AST remains a
prototype rooted at `Program`. The lexer omits EOF, recognizes `do`/`print`
as identifiers, and accepts broader identifier and numeric spellings.
Both `DiagnosisEngine::HasErrors()` and `Report()` return sticky aggregate status.

## Goals / Non-Goals

**Goals:** Share one cursor, a small public interface, and explicit ownership and
error boundaries between the two developers.

**Non-Goals:** Runtime method bodies, lexer changes, CLI wiring, semantic
validation, visitors, recovery nodes, and promotion of prototype nodes to language
features. Compile-only checks are not parser acceptance tests.

## Decisions

### 1. Return an owning result with stage-local status

Use `ParseResult` to distinguish parser diagnostics from earlier lexical errors.
The local boolean records syntax errors only; it is not a second diagnostic store
or a substitute for aggregate compilation status. Future `ReportSyntaxError()`
sets the local flag before forwarding to the shared engine. Neither its sticky
return value nor a before/after comparison of `HasErrors()` can identify new
parser errors. Reset the local flag at entry to each parse operation, together
with the cursor; do not reset the shared engine.

Alternative rejected: returning only a pointer and querying global error state,
which cannot express the agreed per-operation syntax status.

### 2. Proposed header

All non-deleted methods below are declarations only in this increment.

```cpp
#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include "ast/ast.h"
#include "common/diagnostic.h"
#include "common/source_location.h"
#include "lexer/token.h"

namespace parser {

struct ParseResult {
    std::unique_ptr<ast::TranslationUnit> translation_unit;
    bool has_syntax_errors = false;
};

class Parser final {
public:
    // Inputs outlive the parser. Lexer output needs no appended EOF token.
    Parser(std::string_view source, std::span<const lexer::Token> tokens,
           common::DiagnosisEngine& diagnostics);

    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    // Each call reparses from the start with fresh local status, not a reset engine.
    // Future implementation retains a root after ordinary syntax errors.
    [[nodiscard]] ParseResult ParseTranslationUnit();

private:
    [[nodiscard]] bool AtEnd() const;
    // nullptr when lookahead reaches or exceeds the token span.
    [[nodiscard]] const lexer::Token* PeekToken(
        std::size_t lookahead = 0) const;
    // TokenKind::eof when lookahead reaches or exceeds the token span.
    [[nodiscard]] lexer::TokenKind PeekKind(
        std::size_t lookahead = 0) const;
    // nullptr at EOF; never advances past the end.
    const lexer::Token* Consume();
    // EOF is observable only: Match(TokenKind::eof) always returns false.
    bool Match(lexer::TokenKind kind);
    bool Expect(lexer::TokenKind kind, std::string_view message);
    [[nodiscard]] common::SourceLocation CurrentLocation() const;
    void ReportSyntaxError(common::SourceLocation loc, std::string_view message);

    // Program structure (user).
    std::unique_ptr<ast::FuncDecl> ParseMainFunction();
    std::unique_ptr<ast::CompoundStmt> ParseBlock();

    // Statement dispatch (user).
    ast::StmtPtr ParseStatement();

    // Reusable grammatical constructs (user).
    std::optional<ast::Type> ParseType();
    std::unique_ptr<ast::VarDecl> ParseDeclaration();  // No trailing ';'.
    std::unique_ptr<ast::AssignExpr> ParseAssignment();  // No trailing ';'.

    // Specialized statements (user).
    ast::StmtPtr ParsePrintStatement();
    ast::StmtPtr ParseIfStatement();
    ast::StmtPtr ParseDoWhileStatement();
    ast::StmtPtr ParseForStatement();

    // Expressions (coworker): Pratt placeholders, no implementations.
    ast::ExprPtr ParseExpression(int min_binding_power = 0);
    ast::ExprPtr ParsePrimary();

    // Contextual recovery helpers will be designed with the method bodies.
    std::string_view source_;
    std::span<const lexer::Token> tokens_;
    common::DiagnosisEngine& diagnostics_;
    std::size_t current_index_ = 0;
    common::SourceLocation eof_location_{};
    bool has_syntax_errors_ = false;
};

}  // namespace parser
```

Keep grammar-component parsing separate from statement completion without adding
wrapper methods. `ParseStatement()` dispatches declarations and assignments to
the reusable parsers, handles their trailing semicolon, and wraps successful
results in `DeclStmt` or `ExprStmt`. Other statement forms dispatch to their
specialized methods, or to `ParseBlock()` for a compound statement.

`ParseForStatement()` chooses declaration or assignment directly for its mandatory
initializer and creates the same AST wrapper. It owns both header semicolons,
calls `ParseExpression()` for the mandatory condition, calls `ParseAssignment()`
for the mandatory update, and owns the closing parenthesis and braced body.
It must not use unrestricted `ParseStatement()` for the initializer, which would
admit forms outside the grammar. `ParseDeclaration()` and `ParseAssignment()`
consume neither trailing semicolons nor the header's closing parenthesis.

`ParseDeclaration()` handles variable declarations only in this subset;
`ParseMainFunction()` handles the fixed function definition. The specialized print
and do/while methods own their terminating semicolons. Recovery and failure
propagation remain with the enclosing owner of the statement or for header.

Alternative rejected: separate declaration-statement, assignment-statement, and
for-initializer helpers that only duplicate this small dispatch/wrapping boundary.
No one-to-one mapping between grammar productions and parser methods is required.

### 3. Share an indexed cursor with virtual EOF

Both developers use the same private cursor helpers. Choose the existing
`TokenKind::eof` sentinel over `optional<TokenKind>`: the enum already represents
EOF, while a null token pointer makes absence explicit without constructing a
fake token. Future lookahead checks the remaining span length before adding an
offset, preventing overflow. Consuming at EOF is a no-op, and matching the EOF
kind returns false. Parser and recovery loops must explicitly check EOF and exit
or yield; neither matching nor repeated no-op consumption can provide progress.
The grammar's final EOF is an observation through `AtEnd()`, not a token consumed
through `Match()` or `Expect()`.

Keep source text alongside tokens so the future constructor can compute the true
1-based EOF location through trailing trivia. The returned tree owns its spellings.
A failed `Expect()` reports a syntax error but leaves contextual synchronization
to its caller, rather than choosing a generic skip strategy.

Alternative rejected: requiring callers to mutate lexer output or exposing
independently advancing statement and expression parsers.

### 4. Name the root for its actual role

Rename the owning container to `ast::TranslationUnit`, retaining its
`std::vector<DeclPtr> decls` and adding `using Program = TranslationUnit;`.
It does not inherit from `Decl`; the name describes that fact and agrees with
the project architecture notes. No `TranslationUnitDecl` compatibility alias is
needed because that proposed type has not been implemented.

Keep `Expr` separate from `Stmt`. Wrap assignments in `ExprStmt` and variables
in `DeclStmt`; use existing control-flow nodes. The for initializer is a
declaration or assignment wrapper, and the step is an assignment. Factor/term
are grammar levels, not new AST classes. Extra prototype nodes are not promises.

Alternative rejected: copying Clang's root name without its declaration hierarchy,
or introducing expression-as-statement inheritance or a visitor just for parsing.

### 5. Fix the handoff contract, not recovery signatures

| Boundary | User owns | Coworker owns |
| --- | --- | --- |
| Declarations/assignments | Type, identifier, `=`, AST wrappers | Initializer/right-hand expression |
| Statements | Dispatch, keywords, braces, outer parentheses and separators | Conditions and print operands |
| Expression grouping | Caller-owned delimiters | Parentheses opened inside an expression |
| Errors | Contextual recovery and tree retention | Local expression errors via `ReportSyntaxError()` |

The coworker implements Pratt expressions using the same cursor, possibly in
`src/parser/parser_expr.cpp`. The spec governs operator restrictions; ordinary
Pratt associativity defaults must not broaden them. Private expression helpers
can evolve behind the agreed entry point.

On expression success, return a complete non-null node and leave caller-owned
`;`, `)`, `}`, or EOF untouched. On failure, report locally and signal failure
to the immediate caller without attaching null mandatory children. Local success
must not depend on an earlier error elsewhere. Assignment stays in the user's
grammar components, not the Pratt infix table.

Enclosing methods propagate an already-diagnosed failure to the appropriate
recovery owner without adding generic diagnostics for the same error. That owner
can synchronize and resume construction of the partial tree; propagation does
not require returning a null root. Genuinely distinct syntax errors are still
reported independently.

Use context-aware panic-mode recovery during future implementation, governed by
the spec's progress, delimiter, and partial-tree invariants. Do not prescribe
`SynchronizeForHeader()` or similar signatures before actual recovery contexts
are known. Until explicit error nodes exist, omit unrecoverable constructs rather
than misrepresent malformed required children as valid optional absence.
Semantic analysis may inspect intact subtrees; no safe handling of arbitrary
malformed trees or semantic implementation is promised here.

Alternative rejected: fixed zero-argument recovery APIs or returning a null
entire tree whenever a statement fails.

## Risks / Trade-offs

- [Lexer compatibility is incomplete] - Before executable integration, resolve
  terminal recognition and identifier/integer validation against the spec. Broad
  lexer capability can remain; do not silently reinterpret it as language support.
- [Declarations cannot link as a parser] - Use compile-only interface checks;
  executable parsing and runtime acceptance tests are follow-up work.
- [Recovery representation is incomplete] - Preserve intact syntax and design
  explicit invalid representations before promising complete malformed-tree retention.

## Migration Plan

Implement the header, root rename/alias, and compile-only checks described in
[tasks.md](tasks.md), preserving the lexer baseline and unrelated worktree edits.
No parser CLI consumer or data migration is involved. Rollback removes the unused
header/checks and restores the root name if downstream adoption has not occurred.
