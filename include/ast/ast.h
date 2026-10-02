#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/source_location.h"

namespace ast {

struct Decl;
struct Stmt;
struct Expr;
using DeclPtr = std::unique_ptr<Decl>;
using StmtPtr = std::unique_ptr<Stmt>;
using ExprPtr = std::unique_ptr<Expr>;

// Top-level translation unit: a list of declarations in file order.
struct Program {
    std::vector<DeclPtr> decls;
};

// --- Type system ---
enum class Type : std::uint8_t { Void, Bool, Int, Long, Float, Double };

// --- Declaration ---
enum class DeclKind : std::uint8_t { Func, Var };

struct Decl {
    DeclKind               kind;
    common::SourceLocation loc;
    virtual ~Decl() = default;

protected:
    Decl(DeclKind k, common::SourceLocation l) : kind{k}, loc{l} {}
};

struct VarDecl : Decl {
    Type        type;
    std::string name;
    ExprPtr     init;  // `nullptr` if uninitialized
    VarDecl(common::SourceLocation l, Type t, std::string n,
            ExprPtr i = nullptr)
        : Decl{DeclKind::Var, l}
        , type{t}
        , name{std::move(n)}
        , init(std::move(i)) {}
};

// Not a `Decl` - never an independent AST node.
struct Param {
    Type        type;
    std::string name;
};

struct CompoundStmt;
struct FuncDecl : Decl {
    Type                          return_type;
    std::string                   name;
    std::vector<Param>            params;
    std::unique_ptr<CompoundStmt> body;
    FuncDecl(common::SourceLocation l, Type rt, std::string n,
             std::vector<Param> p, std::unique_ptr<CompoundStmt> b)
        : Decl{DeclKind::Func, l}
        , return_type{rt}
        , name{std::move(n)}
        , params{std::move(p)}
        , body{std::move(b)} {}
};

// --- Statement ---
enum class StmtKind : std::uint8_t { Compound, Decl, Expr, Return };

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

// --- Expression ---
enum class ExprKind : std::uint8_t {
    IntLiteral,
    FloatLiteral,
    BoolLiteral,
    DeclRef,
    Binary,
    Unary,
    Assign,
    Call
};

struct Expr {
    ExprKind               kind;
    common::SourceLocation loc;
    virtual ~Expr() = default;

protected:
    Expr(ExprKind k, common::SourceLocation l) : kind{k}, loc{l} {}
};

struct IntLiteralExpr : Expr {
    std::string spelling;
    IntLiteralExpr(common::SourceLocation l, std::string s)
        : Expr{ExprKind::IntLiteral, l}, spelling{std::move(s)} {}
};

struct FloatLiteralExpr : Expr {
    std::string spelling;
    FloatLiteralExpr(common::SourceLocation l, std::string s)
        : Expr{ExprKind::FloatLiteral, l}, spelling{std::move(s)} {}
};

struct BoolLiteralExpr : Expr {
    bool value;
    BoolLiteralExpr(common::SourceLocation l, bool v)
        : Expr{ExprKind::BoolLiteral, l}, value{v} {}
};

struct DeclRefExpr : Expr {
    std::string name;
    DeclRefExpr(common::SourceLocation l, std::string n)
        : Expr{ExprKind::DeclRef, l}, name{std::move(n)} {}
};

enum class BinaryOp : std::uint8_t {
    Add,  // '+'
    Sub,  // '-'
    Mul,  // '*'
    Div,  // '/'
    Mod,  // '%'
    Lt,   // '<'
    Le,   // '<='
    Gt,   // '>'
    Ge,   // '>='
    Eq,   // '=='
    Ne    // '!='
};

struct BinaryExpr : Expr {
    BinaryOp op;
    ExprPtr  lhs, rhs;
    BinaryExpr(common::SourceLocation l, BinaryOp o, ExprPtr a, ExprPtr b)
        : Expr{ExprKind::Binary, l}
        , op{o}
        , lhs{std::move(a)}
        , rhs{std::move(b)} {}
};

enum UnaryOp : std::uint8_t {
    Neg,  // '-'
    Not   // '!'
};

struct UnaryExpr : Expr {
    UnaryOp op;
    ExprPtr operand;
    UnaryExpr(common::SourceLocation l, UnaryOp o, ExprPtr e)
        : Expr{ExprKind::Unary, l}, op{o}, operand{std::move(e)} {}
};

struct AssignExpr : Expr {
    ExprPtr target;  // lvalue
    ExprPtr value;   // rvalue
    AssignExpr(common::SourceLocation l, ExprPtr t, ExprPtr v)
        : Expr{ExprKind::Assign, l}
        , target{std::move(t)}
        , value{std::move(v)} {}
};

struct CallExpr : Expr {
    std::string          callee;  // resolve to `FuncDecl`
    std::vector<ExprPtr> args;
    CallExpr(common::SourceLocation l, std::string c, std::vector<ExprPtr> a)
        : Expr{ExprKind::Call, l}, callee{std::move(c)}, args{std::move(a)} {}
};

}  // namespace ast
