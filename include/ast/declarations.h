#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "ast/expressions.h"
#include "common/location.h"

namespace ast {

enum class Type : std::uint8_t {
    Bool,
    Int,
};

struct Decl;
using DeclPtr = std::unique_ptr<Decl>;

enum class DeclKind : std::uint8_t { Var };

struct Decl {
    DeclKind         kind;
    common::Location loc;
    virtual ~Decl() = default;

protected:
    Decl(DeclKind k, common::Location l) : kind{k}, loc{l} {}
};

struct VarDecl : Decl {
    Type        type;
    std::string name;
    ExprPtr     init;  // `nullptr` if uninitialized

    VarDecl(common::Location l, Type t, std::string n, ExprPtr i = nullptr)
        : Decl{DeclKind::Var, l}
        , type{t}
        , name{std::move(n)}
        , init(std::move(i)) {}
};

}  // namespace ast
