#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "ast/expressions.h"
#include "common/source_location.h"

namespace ast {

enum class Type : std::uint8_t {
    Bool,
    Int,
};

struct Decl;
using DeclPtr = std::unique_ptr<Decl>;

enum class DeclKind : std::uint8_t { Var };

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

}  // namespace ast
