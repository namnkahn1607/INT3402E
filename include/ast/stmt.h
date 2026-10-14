#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "ast/decl.h"
#include "ast/expr.h"
#include "common/location.h"

namespace ast {

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

enum class StmtKind : std::uint8_t {
    Compound,
    Decl,
    Assign,
    If,
    DoWhile,
    For,
    Print,
};

struct Stmt {
    StmtKind         kind;
    common::Location loc;
    virtual ~Stmt() = default;

protected:
    Stmt(StmtKind k, common::Location l) : kind{k}, loc{l} {}
};

struct CompoundStmt : Stmt {
    std::vector<StmtPtr> body;
    explicit CompoundStmt(common::Location l) : Stmt{StmtKind::Compound, l} {}
};

struct DeclStmt : Stmt {
    DeclPtr decl;
    DeclStmt(common::Location l, DeclPtr d)
        : Stmt{StmtKind::Decl, l}, decl{std::move(d)} {}
};

// Assignments are first-class statements instead of expressions, this helps
// eliminate a whole class of bugs related to `a = b = c` or `if (x = 1)`.
struct AssignStmt : Stmt {
    ExprPtr target;
    ExprPtr value;
    AssignStmt(common::Location l, ExprPtr t, ExprPtr v)
        : Stmt{StmtKind::Assign, l}
        , target{std::move(t)}
        , value{std::move(v)} {}
};

struct IfStmt : Stmt {
    ExprPtr                       condition;
    std::unique_ptr<CompoundStmt> then_branch;
    std::unique_ptr<CompoundStmt> else_branch;  // `nullptr` if absent

    IfStmt(common::Location l, ExprPtr c,
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

    DoWhileStmt(common::Location l, std::unique_ptr<CompoundStmt> loop_body,
                ExprPtr c)
        : Stmt{StmtKind::DoWhile, l}
        , body{std::move(loop_body)}
        , condition{std::move(c)} {}
};

// Grammar: ForInitializer -> Declaration | Assignment
using ForInit =
    std::variant<std::unique_ptr<DeclStmt>, std::unique_ptr<AssignStmt>>;

struct ForStmt : Stmt {
    ForInit                       init;
    ExprPtr                       condition;
    std::unique_ptr<AssignStmt>   step;
    std::unique_ptr<CompoundStmt> body;

    ForStmt(common::Location l, ForInit i, ExprPtr c,
            std::unique_ptr<AssignStmt>   s,
            std::unique_ptr<CompoundStmt> loop_body)
        : Stmt{StmtKind::For, l}
        , init{std::move(i)}
        , condition{std::move(c)}
        , step{std::move(s)}
        , body{std::move(loop_body)} {}
};

struct PrintStmt : Stmt {
    ExprPtr value;
    PrintStmt(common::Location l, ExprPtr v)
        : Stmt{StmtKind::Print, l}, value{std::move(v)} {}
};

}  // namespace ast
