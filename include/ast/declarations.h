#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ast/expressions.h"
#include "common/source_location.h"

namespace ast {

enum class Type : std::uint8_t {
    Void,
    Bool,
    Int,
    Long,
    Float,
    Double,
};

struct Decl;
using DeclPtr = std::unique_ptr<Decl>;

enum class DeclKind : std::uint8_t { 
    Func, 
    Var 
};

struct Decl {
    DeclKind               kind;
    common::SourceLocation loc;
    virtual ~Decl() = default;

protected:
    Decl(DeclKind k, common::SourceLocation l) 
        : kind{k}
        , loc{l} 
    {}
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
        , init(std::move(i)) 
    {}
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
             std::vector<Param> p, std::unique_ptr<CompoundStmt> b);
             
    ~FuncDecl() override;
};

}  // namespace ast
