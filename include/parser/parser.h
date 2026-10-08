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

// Declaration-only interface: parser methods have no definitions yet.
// Authoritative target grammar and future executable acceptance criteria:
// openspec/changes/define-parser-interface/specs/syntax-analysis/spec.md
// Lexer terminal recognition (notably do/print) and identifier/integer spelling
// validation must be resolved before executable integration. Broader lexer and
// AST vocabulary does not imply additional language support.
struct ParseResult {
    // The future parser retains a non-null root after ordinary syntax errors.
    // Nodes own their spellings; the returned tree does not borrow parser inputs.
    std::unique_ptr<ast::TranslationUnit> translation_unit;

    // Only syntax errors from this parse operation, not earlier lexer errors.
    // False does not imply compilation success: shared diagnostics remain sticky.
    bool has_syntax_errors = false;
};

class Parser final {
public:
    // Source/token storage must remain alive and unchanged for this parser's
    // lifetime. The diagnostic engine must also outlive it. No EOF token is needed.
    Parser(std::string_view source, std::span<const lexer::Token> tokens,
           common::DiagnosisEngine& diagnostics);

    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    // Future runtime contract: each call starts at token zero with fresh local
    // syntax-error status, without resetting the shared engine. Each result owns
    // a separate tree; earlier results remain valid. Ordinary syntax errors retain
    // recoverable statements rather than discarding the entire translation unit.
    // Missing semicolons can be logically inserted after complete statements.
    // Absent optional children never stand in for malformed required syntax;
    // until error nodes exist, unrecoverable constructs are omitted instead.
    [[nodiscard]] ParseResult ParseTranslationUnit();

private:
    // Shared cursor helpers. End-of-span is observable EOF, not a physical token.
    // Lookahead must check bounds before addition, including oversized offsets.
    [[nodiscard]] bool AtEnd() const;

    // nullptr when lookahead reaches or exceeds the token span.
    [[nodiscard]] const lexer::Token* PeekToken(
        std::size_t lookahead = 0) const;

    // TokenKind::eof when lookahead reaches or exceeds the token span.
    [[nodiscard]] lexer::TokenKind PeekKind(
        std::size_t lookahead = 0) const;

    // nullptr immediately at EOF, without advancing. Parser/recovery loops must
    // explicitly stop or yield at EOF, not retry no-op consumption indefinitely.
    const lexer::Token* Consume();

    // Match(TokenKind::eof) always returns false: EOF is not consumable.
    bool Match(lexer::TokenKind kind);

    // On failure, diagnose without choosing a contextual synchronization strategy.
    // Observe final EOF with AtEnd(), not Expect(TokenKind::eof).
    bool Expect(lexer::TokenKind kind, std::string_view message);

    // At EOF, use the 1-based position after all source text, including trivia.
    [[nodiscard]] common::SourceLocation CurrentLocation() const;

    // Set local syntax-error status, then report through the shared engine.
    // Its sticky return value/HasErrors() cannot determine local parse success.
    void ReportSyntaxError(common::SourceLocation loc, std::string_view message);
    
    // Program structure (declaration/statement owner).
    std::unique_ptr<ast::FuncDecl> ParseMainFunction();

    std::unique_ptr<ast::CompoundStmt> ParseBlock();

    // Dispatch declarations/assignments, handle their ';', and wrap successful
    // nodes in DeclStmt/ExprStmt. Other forms delegate to specialized statements
    // or ParseBlock(). This stage does not resolve names or validate types.
    ast::StmtPtr ParseStatement();

    // Reusable grammatical constructs (declaration/statement owner).
    std::optional<ast::Type> ParseType();

    // Variable declarations only. Neither component consumes trailing ';' or the
    // for header's ')'; both delegate expression operands to ParseExpression().
    std::unique_ptr<ast::VarDecl> ParseDeclaration();

    std::unique_ptr<ast::AssignExpr> ParseAssignment();

    // Specialized statements (declaration/statement owner).
    // Print and do/while own their terminating semicolons.
    ast::StmtPtr ParsePrintStatement();

    ast::StmtPtr ParseIfStatement();

    ast::StmtPtr ParseDoWhileStatement();

    // Choose declaration or assignment for the mandatory initializer and wrap it
    // directly; do not use unrestricted ParseStatement(). Own both header ';',
    // ')' and the braced body. Condition and assignment update are also mandatory.
    ast::StmtPtr ParseForStatement();

    // Expressions (coworker): Pratt placeholders, declarations without bodies.
    // Consume parentheses opened within expressions, but leave caller-owned ';',
    // ')', '}' and EOF untouched. Success returns a complete non-null expression.
    // Failure is diagnosed locally before returning null to the immediate caller;
    // never attach null mandatory children or use global error status as success.
    // Enclosing methods propagate diagnosed failures without duplicate diagnostics
    // to the appropriate contextual recovery owner, which can resume parsing.
    // Respect the spec's operator/chain restrictions; assignment is not infix here.
    ast::ExprPtr ParseExpression(int min_binding_power = 0);

    ast::ExprPtr ParsePrimary();

    // Recovery helper signatures remain open until method bodies are implemented.
    // Recovery must respect enclosing delimiters and for-header separators, retain
    // intact siblings, and advance or yield on every iteration, terminating at EOF.
    std::string_view source_;
    std::span<const lexer::Token> tokens_;
    common::DiagnosisEngine& diagnostics_;
    std::size_t current_index_ = 0;
    common::SourceLocation eof_location_{};
    bool has_syntax_errors_ = false;
};

}  // namespace parser
