#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include "ast/declarations.h"
#include "ast/statements.h"
#include "ast/translation_unit.h"
#include "common/diagnostic.h"
#include "cursor.h"
#include "expr_parser.h"

namespace parser {

// Recursive descent parsing for declarations/statements.
class Parser final {
public:
    // Source/token storage must remain alive and unchanged for this parser's
    // lifetime. The diagnostic engine must also outlive it. No EOF token is
    // needed. Tokens must honor lexer classification: invalid spelling kinds
    // have already been diagnosed by the lexer.
    Parser(std::string_view source, std::span<const Token> tokens,
           common::DiagnosisEngine& diagnostics);

    Parser(const Parser&)            = delete;
    Parser& operator=(const Parser&) = delete;

    // Parsing retains a non-null root after ordinary syntax errors.
    // Nodes own their spellings; the returned tree does not borrow parser
    // inputs.
    // Each call starts at token zero with fresh local syntax-error status,
    // without resetting the shared engine. Each result owns a separate tree;
    // earlier results remain valid. Ordinary syntax errors retain safely
    // recoverable statements. Basic panic recovery does not insert tokens.
    // Absent optional children never stand in for malformed required syntax;
    // until error nodes exist, unrecoverable constructs are omitted instead.
    // Source files contain existing declarations/statements directly, possibly
    // none. Functions are unsupported; no main or outer block is synthesized.
    [[nodiscard]] std::unique_ptr<ast::TranslationUnit> ParseTranslationUnit();

private:
    friend struct ParserTestAccess;

    // Explicit source blocks; file scope is parsed by `ParseTranslationUnit()`.
    std::unique_ptr<ast::CompoundStmt> ParseBlock();

    // Dispatch declarations/assignments and handle their ';'. Declarations use
    // `DeclStmt` wrappers; assignments are direct `AssignStmt` nodes. Other
    // forms delegate to specialized statement parsers or `ParseBlock()`. This
    // stage does not resolve names or validate types.
    ast::StmtPtr ParseStatement();

    // Statement parsing plus panic-mode recovery on failure. The single owner
    // of `RecoveryStatement()`; used by `ParseTranslationUnit()` and
    // `ParseBlock()`. Requires `!AtEnd()`.
    ast::StmtPtr ParseStatementOrRecover();

    // Reusable grammatical constructs.
    std::optional<ast::Type> ParseType();

    // Variable declarations only. Neither component consumes trailing ';' or
    // the for header's ')'; both delegate expression operands to
    // `ParseExpression()`.
    std::unique_ptr<ast::VarDecl> ParseDeclaration();

    std::unique_ptr<ast::AssignStmt> ParseAssignment();

    // Specialized statements.
    // Print and do/while own their terminating semicolons.
    ast::StmtPtr ParsePrintStatement();

    ast::StmtPtr ParseIfStatement();

    ast::StmtPtr ParseDoWhileStatement();

    // Choose a `DeclStmt` or `AssignStmt` for the mandatory initializer; do not
    // use unrestricted `ParseStatement()`. Own both header ';', ')' and the
    // braced body. Condition and assignment update are also mandatory.
    ast::StmtPtr ParseForStatement();

    // Statement-terminating ';' for declarations, assignments, print and
    // do-while. Returns true when statement is structurally complete: the ';'
    // was present, or it is missing at an evident statement boundary (next
    // token on a later line, '}' or EOF). The latter is reported just after the
    // previous token, and the caller keeps the statement. Returns `false` when
    // the ';' is missing mid-line; the caller then fails and recovers.
    bool ExpectTerminator(std::string_view message);

    // Replay the failed statement's consumed delimiters, then skip to a safe
    // semicolon/brace/EOF. Header fragments never become surrounding
    // statements. Leaves the enclosing block's brace for its owner; no token
    // insertion.
    void RecoverStatement(std::size_t statement_start);

    Cursor     cursor_;
    ExprParser expr_parser_;
};

}  // namespace parser
