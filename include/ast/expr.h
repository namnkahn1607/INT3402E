#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "common/location.h"

namespace ast {

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

enum class ExprKind : std::uint8_t {
    IntLiteral,
    BoolLiteral,
    DeclRef,
    Binary,
};

struct Expr {
    ExprKind         kind;
    common::Location loc;
    virtual ~Expr() = default;

protected:
    Expr(ExprKind k, common::Location l) : kind{k}, loc{l} {}
};

struct IntLiteralExpr : Expr {
    std::string spelling;
    IntLiteralExpr(common::Location l, std::string s)
        : Expr{ExprKind::IntLiteral, l}, spelling{std::move(s)} {}
};

struct BoolLiteralExpr : Expr {
    bool value;
    BoolLiteralExpr(common::Location l, bool v)
        : Expr{ExprKind::BoolLiteral, l}, value{v} {}
};

struct DeclRefExpr : Expr {
    std::string name;
    DeclRefExpr(common::Location l, std::string n)
        : Expr{ExprKind::DeclRef, l}, name{std::move(n)} {}
};

enum class BinaryOp : std::uint8_t {
    Add,  // '+'
    Sub,  // '-'
    Mul,  // '*'
    Div,  // '/'
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
    BinaryExpr(common::Location l, BinaryOp o, ExprPtr a, ExprPtr b)
        : Expr{ExprKind::Binary, l}
        , op{o}
        , lhs{std::move(a)}
        , rhs{std::move(b)} {}
};

}  // namespace ast
