#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "ast/declarations.h"
#include "ast/expressions.h"
#include "common/source_location.h"

namespace ast {

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

enum class StmtKind : std::uint8_t {
    Compound,
    Decl,
    Expr,
    Return,
    If,
    DoWhile,
    For,
    Print,
};

struct Stmt {
    StmtKind               kind;
    common::SourceLocation loc;
    virtual ~Stmt() = default;

protected:
    Stmt(StmtKind k, common::SourceLocation l) : kind{k}, loc{l} {}
};

struct CompoundStmt : Stmt {
    std::vector<StmtPtr> body;
    explicit CompoundStmt(common::SourceLocation l)
        : Stmt{StmtKind::Compound, l} {}
};

struct DeclStmt : Stmt {
    DeclPtr decl;
    DeclStmt(common::SourceLocation l, DeclPtr d)
        : Stmt{StmtKind::Decl, l}, decl{std::move(d)} {}
};

struct ExprStmt : Stmt {
    ExprPtr expr;
    ExprStmt(common::SourceLocation l, ExprPtr e)
        : Stmt{StmtKind::Expr, l}, expr{std::move(e)} {}
};

struct ReturnStmt : Stmt {
    ExprPtr value;  // `nullptr` in a void function return
    ReturnStmt(common::SourceLocation l, ExprPtr v)
        : Stmt{StmtKind::Return, l}, value{std::move(v)} {}
};

struct IfStmt : Stmt {
    ExprPtr                       condition;
    std::unique_ptr<CompoundStmt> then_branch;
    std::unique_ptr<CompoundStmt> else_branch;  // `nullptr` if absent

    IfStmt(common::SourceLocation l, ExprPtr c,
           std::unique_ptr<CompoundStmt> then_body,
           std::unique_ptr<CompoundStmt> else_body = nullptr)
        : Stmt{StmtKind::If, l}
        , condition{std::move(c)}
        , then_branch{std::move(then_body)}
        , else_branch{std::move(else_body)} {}
};

struct DoWhileStmt : Stmt {
    std::unique_ptr<CompoundStmt> body;
    ExprPtr                       condition;

    DoWhileStmt(common::SourceLocation        l,
                std::unique_ptr<CompoundStmt> loop_body, ExprPtr c)
        : Stmt{StmtKind::DoWhile, l}
        , body{std::move(loop_body)}
        , condition{std::move(c)} {}
};

struct ForStmt : Stmt {
    StmtPtr                       init;  // `DeclStmt` or assignment `ExprStmt`
    ExprPtr                       condition;
    ExprPtr                       step;  // `AssignExpr`
    std::unique_ptr<CompoundStmt> body;

    ForStmt(common::SourceLocation l, StmtPtr i, ExprPtr c, ExprPtr s,
            std::unique_ptr<CompoundStmt> loop_body)
        : Stmt{StmtKind::For, l}
        , init{std::move(i)}
        , condition{std::move(c)}
        , step{std::move(s)}
        , body{std::move(loop_body)} {}
};

struct PrintStmt : Stmt {
    ExprPtr value;
    PrintStmt(common::SourceLocation l, ExprPtr v)
        : Stmt{StmtKind::Print, l}, value{std::move(v)} {}
};

}  // namespace ast
