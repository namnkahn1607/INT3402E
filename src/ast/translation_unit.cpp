#include "ast/translation_unit.h"

#include <cassert>
#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>

namespace ast {

namespace {

std::string_view TypeToString(Type type) {
    switch (type) {
        case Type::Bool: return "bool";
        case Type::Int: return "int";
    }
    assert(false && "unhandled Type");
    return "?";
}

std::string_view BinaryOpToString(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Lt: return "<";
        case BinaryOp::Le: return "<=";
        case BinaryOp::Gt: return ">";
        case BinaryOp::Ge: return ">=";
        case BinaryOp::Eq: return "==";
        case BinaryOp::Ne: return "!=";
    }
    assert(false && "unhandled BinaryOp");
    return "?";
}

void PrintLocation(std::ostream& out, common::Location loc) {
    out << '<' << loc.line << ':' << loc.col << '>';
}

void DumpExpr(const Expr& expr, std::ostream& out, const std::string& prefix,
              bool is_last);
void DumpStmt(const Stmt& stmt, std::ostream& out, const std::string& prefix,
              bool is_last);
void DumpDecl(const Decl& decl, std::ostream& out, const std::string& prefix,
              bool is_last);

// Print the connector and return the child prefix.
std::string PrintConnector(std::ostream& out, const std::string& prefix,
                           bool is_last) {
    out << prefix << (is_last ? "`-" : "|-");
    return prefix + (is_last ? "  " : "| ");
}

void DumpExpr(const Expr& expr, std::ostream& out, const std::string& prefix,
              bool is_last) {
    const auto child_prefix = PrintConnector(out, prefix, is_last);
    switch (expr.kind) {
        case ExprKind::IntLiteral: {
            const auto& node = dynamic_cast<const IntLiteralExpr&>(expr);
            out << "IntLiteralExpr ";
            PrintLocation(out, node.loc);
            out << " '" << node.spelling << "'\n";
            break;
        }
        case ExprKind::BoolLiteral: {
            const auto& node = dynamic_cast<const BoolLiteralExpr&>(expr);
            out << "BoolLiteralExpr ";
            PrintLocation(out, node.loc);
            out << ' ' << (node.value ? "true" : "false") << '\n';
            break;
        }
        case ExprKind::DeclRef: {
            const auto& node = dynamic_cast<const DeclRefExpr&>(expr);
            out << "DeclRefExpr ";
            PrintLocation(out, node.loc);
            out << " '" << node.name << "'\n";
            break;
        }
        case ExprKind::Binary: {
            const auto& node = dynamic_cast<const BinaryExpr&>(expr);
            out << "BinaryExpr ";
            PrintLocation(out, node.loc);
            out << " '" << BinaryOpToString(node.op) << "'\n";
            assert(node.lhs && "BinaryExpr missing lhs");
            assert(node.rhs && "BinaryExpr missing rhs");
            DumpExpr(*node.lhs, out, child_prefix, false);
            DumpExpr(*node.rhs, out, child_prefix, true);
            break;
        }
    }
}

void DumpDecl(const Decl& decl, std::ostream& out, const std::string& prefix,
              bool is_last) {
    const auto child_prefix = PrintConnector(out, prefix, is_last);
    switch (decl.kind) {
        case DeclKind::Var: {
            const auto& node = dynamic_cast<const VarDecl&>(decl);
            out << "VarDecl ";
            PrintLocation(out, node.loc);
            out << ' ' << node.name << " '" << TypeToString(node.type) << '\'';
            if (node.init) {
                out << " init\n";
                DumpExpr(*node.init, out, child_prefix, true);
            } else {
                out << '\n';
            }
            break;
        }
    }
}

void DumpStmt(const Stmt& stmt, std::ostream& out, const std::string& prefix,
              bool is_last) {
    const auto child_prefix = PrintConnector(out, prefix, is_last);
    switch (stmt.kind) {
        case StmtKind::Compound: {
            const auto& node = dynamic_cast<const CompoundStmt&>(stmt);
            out << "CompoundStmt ";
            PrintLocation(out, node.loc);
            out << '\n';
            for (std::size_t i = 0; i < node.body.size(); ++i) {
                assert(node.body[i] && "CompoundStmt has null child");
                DumpStmt(*node.body[i], out, child_prefix,
                         i + 1 == node.body.size());
            }
            break;
        }
        case StmtKind::Decl: {
            const auto& node = dynamic_cast<const DeclStmt&>(stmt);
            out << "DeclStmt ";
            PrintLocation(out, node.loc);
            out << '\n';
            assert(node.decl && "DeclStmt missing decl");
            DumpDecl(*node.decl, out, child_prefix, true);
            break;
        }
        case StmtKind::Assign: {
            const auto& node = dynamic_cast<const AssignStmt&>(stmt);
            out << "AssignStmt ";
            PrintLocation(out, node.loc);
            out << '\n';
            assert(node.target && "AssignStmt missing target");
            assert(node.value && "AssignStmt missing value");
            DumpExpr(*node.target, out, child_prefix, false);
            DumpExpr(*node.value, out, child_prefix, true);
            break;
        }
        case StmtKind::If: {
            const auto& node = dynamic_cast<const IfStmt&>(stmt);
            out << "IfStmt ";
            PrintLocation(out, node.loc);
            if (node.else_branch) {
                out << " has_else";
            }
            out << '\n';
            assert(node.condition && "IfStmt missing condition");
            assert(node.then_branch && "IfStmt missing then_branch");
            if (node.else_branch) {
                DumpExpr(*node.condition, out, child_prefix, false);
                DumpStmt(*node.then_branch, out, child_prefix, false);
                DumpStmt(*node.else_branch, out, child_prefix, true);
            } else {
                DumpExpr(*node.condition, out, child_prefix, false);
                DumpStmt(*node.then_branch, out, child_prefix, true);
            }
            break;
        }
        case StmtKind::DoWhile: {
            const auto& node = dynamic_cast<const DoWhileStmt&>(stmt);
            out << "DoWhileStmt ";
            PrintLocation(out, node.loc);
            out << '\n';
            assert(node.body && "DoWhileStmt missing body");
            assert(node.condition && "DoWhileStmt missing condition");
            DumpStmt(*node.body, out, child_prefix, false);
            DumpExpr(*node.condition, out, child_prefix, true);
            break;
        }
        case StmtKind::For: {
            const auto& node = dynamic_cast<const ForStmt&>(stmt);
            out << "ForStmt ";
            PrintLocation(out, node.loc);
            out << '\n';
            assert(node.condition && "ForStmt missing condition");
            assert(node.step && "ForStmt missing step");
            assert(node.body && "ForStmt missing body");
            std::visit(
                [&](const auto& init) {
                    assert(init && "ForStmt missing init");
                    DumpStmt(*init, out, child_prefix, false);
                },
                node.init);
            DumpExpr(*node.condition, out, child_prefix, false);
            DumpStmt(*node.step, out, child_prefix, false);
            DumpStmt(*node.body, out, child_prefix, true);
            break;
        }
        case StmtKind::Print: {
            const auto& node = dynamic_cast<const PrintStmt&>(stmt);
            out << "PrintStmt ";
            PrintLocation(out, node.loc);
            out << '\n';
            assert(node.value && "PrintStmt missing value");
            DumpExpr(*node.value, out, child_prefix, true);
            break;
        }
    }
}

}  // namespace

void DumpAst(const TranslationUnit& unit, std::ostream& out) {
    out << "TranslationUnit\n";
    for (std::size_t i = 0; i < unit.statements.size(); ++i) {
        assert(unit.statements[i] &&
               "TranslationUnit has null top-level statement");
        DumpStmt(*unit.statements[i], out, "", i + 1 == unit.statements.size());
    }
}

}  // namespace ast
