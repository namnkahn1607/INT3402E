#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/source_location.h"

namespace ast {

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

enum class ExprKind : std::uint8_t {
    IntLiteral,
    FloatLiteral,
    BoolLiteral,
    DeclRef,
    Binary,
    Unary,
    Assign,
    Call,
};

struct Expr {
    ExprKind               kind;
    common::SourceLocation loc;
    virtual ~Expr() = default;

protected:
    Expr(ExprKind k, common::SourceLocation l) 
        : kind{k}
        , loc{l} 
    {}
};

struct IntLiteralExpr : Expr {
    std::string spelling;
    IntLiteralExpr(common::SourceLocation l, std::string s)
        : Expr{ExprKind::IntLiteral, l}
        , spelling{std::move(s)} 
    {}
};

struct FloatLiteralExpr : Expr {
    std::string spelling;
    FloatLiteralExpr(common::SourceLocation l, std::string s)
        : Expr{ExprKind::FloatLiteral, l}
        , spelling{std::move(s)} 
    {}
};

struct BoolLiteralExpr : Expr {
    bool value;
    BoolLiteralExpr(common::SourceLocation l, bool v)
        : Expr{ExprKind::BoolLiteral, l}
        , value{v} {}
};

struct DeclRefExpr : Expr {
    std::string name;
    DeclRefExpr(common::SourceLocation l, std::string n)
        : Expr{ExprKind::DeclRef, l}
        , name{std::move(n)} 
    {}
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
    Ne,   // '!='
};

struct BinaryExpr : Expr {
    BinaryOp op;
    ExprPtr  lhs, rhs;
    BinaryExpr(common::SourceLocation l, BinaryOp o, ExprPtr a, ExprPtr b)
        : Expr{ExprKind::Binary, l}
        , op{o}
        , lhs{std::move(a)}
        , rhs{std::move(b)} 
    {}
};

enum UnaryOp : std::uint8_t {
    Neg,  // '-'
    Not,  // '!'
};

struct UnaryExpr : Expr {
    UnaryOp op;
    ExprPtr operand;
    UnaryExpr(common::SourceLocation l, UnaryOp o, ExprPtr e)
        : Expr{ExprKind::Unary, l}
        , op{o}
        , operand{std::move(e)} {}
};

struct AssignExpr : Expr {
    ExprPtr target;  // lvalue
    ExprPtr value;   // rvalue
    AssignExpr(common::SourceLocation l, ExprPtr t, ExprPtr v)
        : Expr{ExprKind::Assign, l}
        , target{std::move(t)}
        , value{std::move(v)} 
    {}
};

struct CallExpr : Expr {
    std::string          callee;  // resolve to `FuncDecl`
    std::vector<ExprPtr> args;
        CallExpr(common::SourceLocation l, std::string c, std::vector<ExprPtr> a)
            : Expr{ExprKind::Call, l}
            , callee{std::move(c)}
            , args{std::move(a)} 
        {}
};

}  // namespace ast
