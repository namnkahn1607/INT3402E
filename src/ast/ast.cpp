#include "ast/ast.h"

#include <utility>

namespace ast {

FuncDecl::FuncDecl(common::SourceLocation l, Type rt, std::string n,
                   std::vector<Param> p, std::unique_ptr<CompoundStmt> b)
    : Decl{DeclKind::Func, l}
    , return_type{rt}
    , name{std::move(n)}
    , params{std::move(p)}
    , body{std::move(b)} {}

FuncDecl::~FuncDecl() = default;

}  // namespace ast
