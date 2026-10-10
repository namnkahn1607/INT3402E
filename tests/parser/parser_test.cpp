#include "parser/parser.h"

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "lexer/lexer.h"

namespace parser {

// Narrow access to contracts otherwise hidden behind the public parse entry.
struct ParserTestAccess {
    static bool AtEnd(const Parser& parser) { return parser.AtEnd(); }
    static lexer::TokenKind PeekKind(const Parser& parser,
                                     std::size_t   offset = 0) {
        return parser.PeekKind(offset);
    }
    static const lexer::Token* PeekToken(const Parser& parser,
                                         std::size_t   offset = 0) {
        return parser.PeekToken(offset);
    }
    static const lexer::Token* Consume(Parser& parser) {
        return parser.Consume();
    }
    static bool Match(Parser& parser, lexer::TokenKind kind) {
        return parser.Match(kind);
    }
    static std::size_t Index(const Parser& parser) {
        return parser.current_index_;
    }
    static common::SourceLocation Location(const Parser& parser) {
        return parser.CurrentLocation();
    }
    static ast::ExprPtr Expression(Parser& parser, int min_binding_power = 0) {
        return parser.ParseExpression(min_binding_power);
    }
    static std::unique_ptr<ast::VarDecl> Declaration(Parser& parser) {
        return parser.ParseDeclaration();
    }
    static std::unique_ptr<ast::AssignExpr> Assignment(Parser& parser) {
        return parser.ParseAssignment();
    }
};

}  // namespace parser

namespace {

struct Parsed {
    parser::ParseResult result;
    std::string         diagnostics;
    bool                has_errors;
};

Parsed Parse(const std::string& source) {
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    testing::internal::CaptureStderr();
    const auto     tokens = lexer.Tokenize(source);
    parser::Parser parser{source, tokens, diagnostics};
    auto           result = parser.ParseTranslationUnit();
    return {std::move(result), testing::internal::GetCapturedStderr(),
            diagnostics.HasErrors()};
}

Parsed ParseBody(std::string_view body) {
    return Parse("int main() { " + std::string{body} + " }");
}

const ast::CompoundStmt* MainBody(const parser::ParseResult& result) {
    if (!result.translation_unit ||
        result.translation_unit->decls.size() != 1) {
        return nullptr;
    }
    const auto* main = dynamic_cast<const ast::FuncDecl*>(
        result.translation_unit->decls[0].get());
    return main ? main->body.get() : nullptr;
}

const ast::VarDecl* Variable(const ast::Stmt* statement) {
    const auto* declaration = dynamic_cast<const ast::DeclStmt*>(statement);
    return declaration
               ? dynamic_cast<const ast::VarDecl*>(declaration->decl.get())
               : nullptr;
}

void ExpectLocation(common::SourceLocation location, int line, int column) {
    EXPECT_EQ(location.line, line);
    EXPECT_EQ(location.col, column);
}

void ExpectExpression(const ast::Expr* expression) {
    ASSERT_NE(expression, nullptr);
    EXPECT_GT(expression->loc.line, 0);
    EXPECT_GT(expression->loc.col, 0);
    switch (expression->kind) {
        case ast::ExprKind::IntLiteral:
        case ast::ExprKind::BoolLiteral:
        case ast::ExprKind::DeclRef: break;
        case ast::ExprKind::Binary: {
            const auto* binary =
                dynamic_cast<const ast::BinaryExpr*>(expression);
            ASSERT_NE(binary, nullptr);
            ExpectExpression(binary->lhs.get());
            ExpectExpression(binary->rhs.get());
            break;
        }
        case ast::ExprKind::Assign: {
            const auto* assignment =
                dynamic_cast<const ast::AssignExpr*>(expression);
            ASSERT_NE(assignment, nullptr);
            ASSERT_NE(assignment->target, nullptr);
            EXPECT_EQ(assignment->target->kind, ast::ExprKind::DeclRef);
            ExpectExpression(assignment->target.get());
            ExpectExpression(assignment->value.get());
            break;
        }
        default:
            ADD_FAILURE() << "Parser produced an unsupported expression kind";
    }
}

void ExpectStatement(const ast::Stmt* statement) {
    ASSERT_NE(statement, nullptr);
    EXPECT_GT(statement->loc.line, 0);
    EXPECT_GT(statement->loc.col, 0);
    switch (statement->kind) {
        case ast::StmtKind::Compound: {
            const auto* block =
                dynamic_cast<const ast::CompoundStmt*>(statement);
            ASSERT_NE(block, nullptr);
            for (const auto& child : block->body) {
                ExpectStatement(child.get());
            }
            break;
        }
        case ast::StmtKind::Decl: {
            const auto* declaration = Variable(statement);
            ASSERT_NE(declaration, nullptr);
            EXPECT_TRUE(declaration->type == ast::Type::Int ||
                        declaration->type == ast::Type::Bool);
            if (declaration->init) {
                ExpectExpression(declaration->init.get());
            }
            break;
        }
        case ast::StmtKind::Expr: {
            const auto* wrapper = dynamic_cast<const ast::ExprStmt*>(statement);
            ASSERT_NE(wrapper, nullptr);
            ASSERT_NE(wrapper->expr, nullptr);
            EXPECT_EQ(wrapper->expr->kind, ast::ExprKind::Assign);
            ExpectExpression(wrapper->expr.get());
            break;
        }
        case ast::StmtKind::Print: {
            const auto* print = dynamic_cast<const ast::PrintStmt*>(statement);
            ASSERT_NE(print, nullptr);
            ExpectExpression(print->value.get());
            break;
        }
        case ast::StmtKind::If: {
            const auto* selection = dynamic_cast<const ast::IfStmt*>(statement);
            ASSERT_NE(selection, nullptr);
            ExpectExpression(selection->condition.get());
            ExpectStatement(selection->then_branch.get());
            if (selection->else_branch) {
                ExpectStatement(selection->else_branch.get());
            }
            break;
        }
        case ast::StmtKind::DoWhile: {
            const auto* loop = dynamic_cast<const ast::DoWhileStmt*>(statement);
            ASSERT_NE(loop, nullptr);
            ExpectStatement(loop->body.get());
            ExpectExpression(loop->condition.get());
            break;
        }
        case ast::StmtKind::For: {
            const auto* loop = dynamic_cast<const ast::ForStmt*>(statement);
            ASSERT_NE(loop, nullptr);
            ASSERT_NE(loop->init, nullptr);
            EXPECT_TRUE(loop->init->kind == ast::StmtKind::Decl ||
                        loop->init->kind == ast::StmtKind::Expr);
            ExpectStatement(loop->init.get());
            ExpectExpression(loop->condition.get());
            ASSERT_NE(loop->step, nullptr);
            EXPECT_EQ(loop->step->kind, ast::ExprKind::Assign);
            ExpectExpression(loop->step.get());
            ExpectStatement(loop->body.get());
            break;
        }
        default:
            ADD_FAILURE() << "Parser produced an unsupported statement kind";
    }
}

TEST(ParserProgramTest, EmptyMainHasExactlyOneOwningFunction) {
    const auto parsed = Parse("int main() { }");
    EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
    EXPECT_TRUE(parsed.diagnostics.empty());
    ASSERT_NE(parsed.result.translation_unit, nullptr);
    ASSERT_EQ(parsed.result.translation_unit->decls.size(), 1U);
    const auto* main = dynamic_cast<const ast::FuncDecl*>(
        parsed.result.translation_unit->decls[0].get());
    ASSERT_NE(main, nullptr);
    EXPECT_EQ(main->name, "main");
    EXPECT_EQ(main->return_type, ast::Type::Int);
    EXPECT_TRUE(main->params.empty());
    ASSERT_NE(main->body, nullptr);
    EXPECT_TRUE(main->body->body.empty());
    ExpectLocation(main->loc, 1, 1);
    ExpectLocation(main->body->loc, 1, 12);
}

TEST(ParserProgramTest, RepresentativeCompleteProgramUsesEveryControlForm) {
    const auto parsed = Parse(R"(int main() {
    // Single-line comment is trivia.
    int x = 1 + 2 * 3;
    bool flag = x >= 3 == true;
    /* A multiline
       comment is also trivia. */
    if (flag) { print((x + 1) * 2); } else { x = x + 1; }
    do { x = x + 1; } while (x > 10);
    for (int i = 0; i > 3; i = i + 1) { print(i); }
    for (x = 1; flag; x = x * 2) { print(x); }
    { int unused; if (false) { } }
})");
    EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
    EXPECT_TRUE(parsed.diagnostics.empty());
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 7U);
    const std::vector<ast::StmtKind> expected{
        ast::StmtKind::Decl,    ast::StmtKind::Decl, ast::StmtKind::If,
        ast::StmtKind::DoWhile, ast::StmtKind::For,  ast::StmtKind::For,
        ast::StmtKind::Compound};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(body->body[index]->kind, expected[index]);
    }
    const auto* selection =
        dynamic_cast<const ast::IfStmt*>(body->body[2].get());
    ASSERT_NE(selection, nullptr);
    ASSERT_NE(selection->else_branch, nullptr);
    ASSERT_EQ(selection->then_branch->body.size(), 1U);
    EXPECT_EQ(selection->then_branch->body[0]->kind, ast::StmtKind::Print);
    EXPECT_EQ(selection->else_branch->body[0]->kind, ast::StmtKind::Expr);
    const auto* declaration_for =
        dynamic_cast<const ast::ForStmt*>(body->body[4].get());
    const auto* assignment_for =
        dynamic_cast<const ast::ForStmt*>(body->body[5].get());
    ASSERT_NE(declaration_for, nullptr);
    ASSERT_NE(assignment_for, nullptr);
    EXPECT_EQ(declaration_for->init->kind, ast::StmtKind::Decl);
    EXPECT_EQ(assignment_for->init->kind, ast::StmtKind::Expr);
    const auto* nested =
        dynamic_cast<const ast::CompoundStmt*>(body->body[6].get());
    ASSERT_NE(nested, nullptr);
    const auto* no_else =
        dynamic_cast<const ast::IfStmt*>(nested->body[1].get());
    ASSERT_NE(no_else, nullptr);
    EXPECT_EQ(no_else->else_branch, nullptr);
    ExpectStatement(body);
}

TEST(ParserProgramTest, DeclarationsAssignmentsAndLiteralDataAreDistinct) {
    const auto parsed =
        ParseBody("int x; bool flag = false; x = 00042; print(flag);");
    EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 4U);
    const auto* uninitialized = Variable(body->body[0].get());
    const auto* initialized   = Variable(body->body[1].get());
    ASSERT_NE(uninitialized, nullptr);
    ASSERT_NE(initialized, nullptr);
    EXPECT_EQ(uninitialized->type, ast::Type::Int);
    EXPECT_EQ(uninitialized->name, "x");
    EXPECT_EQ(uninitialized->init, nullptr);
    EXPECT_EQ(initialized->type, ast::Type::Bool);
    const auto* boolean =
        dynamic_cast<const ast::BoolLiteralExpr*>(initialized->init.get());
    ASSERT_NE(boolean, nullptr);
    EXPECT_FALSE(boolean->value);
    const auto* wrapper =
        dynamic_cast<const ast::ExprStmt*>(body->body[2].get());
    ASSERT_NE(wrapper, nullptr);
    const auto* assignment =
        dynamic_cast<const ast::AssignExpr*>(wrapper->expr.get());
    ASSERT_NE(assignment, nullptr);
    const auto* target =
        dynamic_cast<const ast::DeclRefExpr*>(assignment->target.get());
    const auto* value =
        dynamic_cast<const ast::IntLiteralExpr*>(assignment->value.get());
    ASSERT_NE(target, nullptr);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(target->name, "x");
    EXPECT_EQ(value->spelling, "00042");
    ExpectStatement(body);
}

TEST(ParserProgramTest, SyntaxParsingDoesNotResolveNamesOrValidateTypes) {
    const auto parsed = ParseBody(
        "x = true; int x; bool x; if (1) { } print(missing + false);"
        "bool flag = 99999999999999999999999999999999999999;");
    EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
    EXPECT_TRUE(parsed.diagnostics.empty());
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->body.size(), 6U);
    ExpectStatement(body);
}

TEST(ParserProgramTest, LocationsUseLeadingTokensAndOperatorsAcrossTrivia) {
    const auto parsed = Parse(
        "int main() {\n/* a\nb*/ int x=00042;\n  x = x + 1;\n  { print(false); "
        "}\n}");
    EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 3U);
    const auto* variable = Variable(body->body[0].get());
    ASSERT_NE(variable, nullptr);
    ExpectLocation(variable->loc, 3, 5);
    ExpectLocation(body->body[0]->loc, 3, 5);
    ExpectLocation(variable->init->loc, 3, 11);
    const auto* wrapper =
        dynamic_cast<const ast::ExprStmt*>(body->body[1].get());
    ASSERT_NE(wrapper, nullptr);
    const auto* assignment =
        dynamic_cast<const ast::AssignExpr*>(wrapper->expr.get());
    ASSERT_NE(assignment, nullptr);
    ExpectLocation(wrapper->loc, 4, 3);
    ExpectLocation(assignment->loc, 4, 3);
    ExpectLocation(assignment->target->loc, 4, 3);
    ExpectLocation(assignment->value->loc, 4, 9);
    const auto* nested =
        dynamic_cast<const ast::CompoundStmt*>(body->body[2].get());
    ASSERT_NE(nested, nullptr);
    ExpectLocation(nested->loc, 5, 3);
    ASSERT_EQ(nested->body.size(), 1U);
    ExpectLocation(nested->body[0]->loc, 5, 5);
    const auto* print =
        dynamic_cast<const ast::PrintStmt*>(nested->body[0].get());
    ASSERT_NE(print, nullptr);
    ExpectLocation(print->value->loc, 5, 11);
}

TEST(ParserProgramTest,
     ControlStatementsUseKeywordLocationsAndAllowBoolForDeclaration) {
    const auto parsed = Parse(
        "int main() {\n"
        "  if (true) { } else { }\n"
        "  do { } while (false);\n"
        "  for (bool flag; true; flag = false) { }\n"
        "}");
    EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 3U);
    ExpectLocation(body->body[0]->loc, 2, 3);
    ExpectLocation(body->body[1]->loc, 3, 3);
    ExpectLocation(body->body[2]->loc, 4, 3);
    const auto* loop = dynamic_cast<const ast::ForStmt*>(body->body[2].get());
    ASSERT_NE(loop, nullptr);
    const auto* declaration = Variable(loop->init.get());
    ASSERT_NE(declaration, nullptr);
    EXPECT_EQ(declaration->type, ast::Type::Bool);
    EXPECT_EQ(declaration->init, nullptr);
    ExpectLocation(declaration->loc, 4, 8);
    ExpectStatement(body);
}

TEST(ParserExpressionTest, PrecedenceGroupingAndBinaryLocations) {
    const auto parsed =
        Parse("int main() {\nint x = 1 +\n  2 * 3;\nprint((1 + 2) * 3);\n}");
    EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    const auto* variable = Variable(body->body[0].get());
    ASSERT_NE(variable, nullptr);
    const auto* add =
        dynamic_cast<const ast::BinaryExpr*>(variable->init.get());
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->op, ast::BinaryOp::Add);
    ExpectLocation(add->loc, 2, 11);
    const auto* multiply = dynamic_cast<const ast::BinaryExpr*>(add->rhs.get());
    ASSERT_NE(multiply, nullptr);
    EXPECT_EQ(multiply->op, ast::BinaryOp::Mul);
    ExpectLocation(multiply->loc, 3, 5);
    ExpectLocation(multiply->lhs->loc, 3, 3);
    ExpectLocation(multiply->rhs->loc, 3, 7);
    const auto* print =
        dynamic_cast<const ast::PrintStmt*>(body->body[1].get());
    ASSERT_NE(print, nullptr);
    const auto* grouped_multiply =
        dynamic_cast<const ast::BinaryExpr*>(print->value.get());
    ASSERT_NE(grouped_multiply, nullptr);
    EXPECT_EQ(grouped_multiply->op, ast::BinaryOp::Mul);
    const auto* grouped_add =
        dynamic_cast<const ast::BinaryExpr*>(grouped_multiply->lhs.get());
    ASSERT_NE(grouped_add, nullptr);
    EXPECT_EQ(grouped_add->op, ast::BinaryOp::Add);
    ExpectLocation(grouped_add->loc, 4, 10);
}

TEST(ParserExpressionTest, RepeatedArithmeticIsLeftAssociated) {
    for (const auto& [expression, operation] :
         std::vector<std::pair<std::string, ast::BinaryOp>>{
             {"a + b + c", ast::BinaryOp::Add},
             {"a * b * c", ast::BinaryOp::Mul}}) {
        SCOPED_TRACE(expression);
        const auto parsed = ParseBody("print(" + expression + ");");
        EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
        const auto* body = MainBody(parsed.result);
        ASSERT_NE(body, nullptr);
        const auto* print =
            dynamic_cast<const ast::PrintStmt*>(body->body[0].get());
        ASSERT_NE(print, nullptr);
        const auto* outer =
            dynamic_cast<const ast::BinaryExpr*>(print->value.get());
        ASSERT_NE(outer, nullptr);
        EXPECT_EQ(outer->op, operation);
        const auto* left =
            dynamic_cast<const ast::BinaryExpr*>(outer->lhs.get());
        ASSERT_NE(left, nullptr);
        EXPECT_EQ(left->op, operation);
        const auto* right =
            dynamic_cast<const ast::DeclRefExpr*>(outer->rhs.get());
        ASSERT_NE(right, nullptr);
        EXPECT_EQ(right->name, "c");
    }
}

TEST(ParserExpressionTest, ComparisonBindsMoreTightlyThanEquality) {
    const auto parsed = ParseBody("print(a + b * c >= d == e > f + g);");
    EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    const auto* print =
        dynamic_cast<const ast::PrintStmt*>(body->body[0].get());
    ASSERT_NE(print, nullptr);
    const auto* equality =
        dynamic_cast<const ast::BinaryExpr*>(print->value.get());
    ASSERT_NE(equality, nullptr);
    EXPECT_EQ(equality->op, ast::BinaryOp::Eq);
    const auto* left =
        dynamic_cast<const ast::BinaryExpr*>(equality->lhs.get());
    const auto* right =
        dynamic_cast<const ast::BinaryExpr*>(equality->rhs.get());
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->op, ast::BinaryOp::Ge);
    EXPECT_EQ(right->op, ast::BinaryOp::Gt);
    const auto* addition =
        dynamic_cast<const ast::BinaryExpr*>(left->lhs.get());
    ASSERT_NE(addition, nullptr);
    EXPECT_EQ(addition->op, ast::BinaryOp::Add);
    const auto* multiplication =
        dynamic_cast<const ast::BinaryExpr*>(addition->rhs.get());
    ASSERT_NE(multiplication, nullptr);
    EXPECT_EQ(multiplication->op, ast::BinaryOp::Mul);
    ExpectStatement(body);
}

TEST(ParserExpressionTest, ExplicitGroupingHasIndependentChainRestrictions) {
    for (const std::string expression :
         {"a > b == c > d", "(a > b) > c", "a > (b >= c)", "(a == b) == c",
          "a == (b == c)", "((true))", "false"}) {
        SCOPED_TRACE(expression);
        const auto parsed = ParseBody("print(" + expression + ");");
        EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
        const auto* body = MainBody(parsed.result);
        ASSERT_NE(body, nullptr);
        ASSERT_EQ(body->body.size(), 1U);
        ExpectStatement(body);
    }
}

TEST(ParserExpressionTest,
     UnparenthesizedComparisonAndEqualityChainsAreRejected) {
    for (const std::string expression :
         {"a > b > c", "a >= b > c", "a > b >= c", "a == b == c",
          "a == b > c > d", "a + b > c + d > e"}) {
        SCOPED_TRACE(expression);
        const auto parsed = ParseBody("print(" + expression + ");");
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        EXPECT_FALSE(parsed.diagnostics.empty());
        const auto* body = MainBody(parsed.result);
        ASSERT_NE(body, nullptr);
        EXPECT_TRUE(body->body.empty());
    }
}

TEST(ParserGrammarTest, RestrictedNamesAllowDigitSuffixesAndKeywordPrefixes) {
    for (const std::string name : {"answer12", "A", "AbZ009", "print1", "done",
                                   "do1", "main", "integer", "true1"}) {
        SCOPED_TRACE(name);
        const auto parsed = ParseBody("int " + name + " = 1; " + name + " = " +
                                      name + " + 1; print(" + name + ");");
        EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
        const auto* body = MainBody(parsed.result);
        ASSERT_NE(body, nullptr);
        EXPECT_EQ(body->body.size(), 3U);
        ExpectStatement(body);
    }
}

TEST(ParserGrammarTest, InvalidNamesAreRejectedInEveryNameContext) {
    for (const std::string name :
         {"_answer", "a1b", "a_b", "do", "print", "int", "while"}) {
        for (const auto& body : std::vector<std::string>{
                 "int " + name + ";", name + " = 1;", "print(" + name + ");"}) {
            SCOPED_TRACE(body);
            const auto parsed = ParseBody(body);
            const bool invalid_spelling =
                name == "_answer" || name == "a1b" || name == "a_b";
            EXPECT_EQ(parsed.result.has_syntax_errors, !invalid_spelling);
            EXPECT_TRUE(parsed.has_errors);
            EXPECT_FALSE(parsed.diagnostics.empty());
            ASSERT_NE(parsed.result.translation_unit, nullptr);
        }
    }
}

TEST(ParserGrammarTest, LexerRecognitionDoesNotBroadenSupportedExpressions) {
    for (const std::string expression :
         {"3.14", ".5", "3.", "1e3", "1f", "-1", "!true", "+1", "a - b",
          "a / b", "a < b", "a <= b", "a && b", "a || b", "a++", "a--", "f(a)",
          "a = 1", "a + (b = 1)"}) {
        SCOPED_TRACE(expression);
        const auto parsed = ParseBody("print(" + expression + ");");
        const bool invalid_spelling =
            expression == "3.14" || expression == ".5" || expression == "3." ||
            expression == "1e3" || expression == "1f";
        EXPECT_EQ(parsed.result.has_syntax_errors, !invalid_spelling);
        EXPECT_TRUE(parsed.has_errors);
        EXPECT_FALSE(parsed.diagnostics.empty());
        const auto* body = MainBody(parsed.result);
        ASSERT_NE(body, nullptr);
        EXPECT_TRUE(body->body.empty());
    }
}

TEST(ParserGrammarTest, UnsupportedStatementsTypesAndForFieldsAreRejected) {
    for (const std::string body : {"return 1;",
                                   "while (true) { }",
                                   ";",
                                   "1;",
                                   "x + 1;",
                                   "long x;",
                                   "float x;",
                                   "double x;",
                                   "void x;",
                                   "short x;",
                                   "if (true) print(1);",
                                   "if (true) { } else print(1);",
                                   "do print(1); while (true);",
                                   "do { }",
                                   "for (; true; x = 1) { }",
                                   "for (int x; ; x = 1) { }",
                                   "for (int x; true; ) { }",
                                   "for (print(1); true; x = 1) { }",
                                   "for (x = 1; true; x + 1) { }",
                                   "for (x = 1; true; x = 2) print(1);",
                                   "int x, y;",
                                   "int *x;"}) {
        SCOPED_TRACE(body);
        const auto parsed = ParseBody(body);
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        EXPECT_FALSE(parsed.diagnostics.empty());
        ASSERT_NE(parsed.result.translation_unit, nullptr);
    }
}

TEST(ParserGrammarTest, ExactlyOneParameterlessIntMainIsRequired) {
    for (const std::string source :
         {"", "int other() { }", "bool main() { }", "void main() { }",
          "int main(int x) { }", "int main()", "int main();",
          "int main() { } int main() { }", "int main() { } print(1);",
          "int main() { } }"}) {
        SCOPED_TRACE(source);
        const auto parsed = Parse(source);
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        EXPECT_FALSE(parsed.diagnostics.empty());
        ASSERT_NE(parsed.result.translation_unit, nullptr);
        EXPECT_LE(parsed.result.translation_unit->decls.size(), 1U);
    }
    const auto extra = Parse("int main() { print(1); } int other() { }");
    EXPECT_TRUE(extra.result.has_syntax_errors);
    const auto* retained_main = MainBody(extra.result);
    ASSERT_NE(retained_main, nullptr);
    EXPECT_EQ(retained_main->body.size(), 1U);
}

TEST(ParserCursorTest, VirtualEofIsObservableButNeverConsumable) {
    const std::string       source = "int main() { }\n// tail\n";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);
    ASSERT_FALSE(tokens.empty());
    parser::Parser parser{source, tokens, diagnostics};
    EXPECT_FALSE(parser::ParserTestAccess::AtEnd(parser));
    ASSERT_EQ(parser::ParserTestAccess::Consume(parser), &tokens[0]);
    const auto before  = parser::ParserTestAccess::Index(parser);
    const auto maximum = std::numeric_limits<std::size_t>::max();
    EXPECT_EQ(parser::ParserTestAccess::PeekToken(parser, maximum), nullptr);
    EXPECT_EQ(parser::ParserTestAccess::PeekKind(parser, maximum),
              lexer::TokenKind::eof);
    EXPECT_EQ(parser::ParserTestAccess::Index(parser), before);
    while (!parser::ParserTestAccess::AtEnd(parser)) {
        ASSERT_NE(parser::ParserTestAccess::Consume(parser), nullptr);
    }
    const auto end = parser::ParserTestAccess::Index(parser);
    for (int repetition = 0; repetition < 4; ++repetition) {
        EXPECT_EQ(parser::ParserTestAccess::Consume(parser), nullptr);
        EXPECT_FALSE(
            parser::ParserTestAccess::Match(parser, lexer::TokenKind::eof));
        EXPECT_EQ(parser::ParserTestAccess::PeekToken(parser), nullptr);
        EXPECT_EQ(parser::ParserTestAccess::PeekKind(parser),
                  lexer::TokenKind::eof);
        EXPECT_EQ(parser::ParserTestAccess::Index(parser), end);
    }
    ExpectLocation(parser::ParserTestAccess::Location(parser), 3, 1);
}

TEST(ParserCursorTest, EmptySequenceStartsAtOneBasedEof) {
    const std::string               source;
    const std::vector<lexer::Token> tokens;
    common::DiagnosisEngine         diagnostics;
    parser::Parser                  parser{source, tokens, diagnostics};
    EXPECT_TRUE(parser::ParserTestAccess::AtEnd(parser));
    EXPECT_EQ(parser::ParserTestAccess::Consume(parser), nullptr);
    EXPECT_FALSE(
        parser::ParserTestAccess::Match(parser, lexer::TokenKind::eof));
    ExpectLocation(parser::ParserTestAccess::Location(parser), 1, 1);
    testing::internal::CaptureStderr();
    const auto result = parser.ParseTranslationUnit();
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(result.has_syntax_errors);
    EXPECT_NE(output.find("1:1: error:"), std::string::npos);
    ASSERT_NE(result.translation_unit, nullptr);
}

TEST(ParserLifecycleTest, RepeatedCallsResetCursorAndKeepSharedErrorState) {
    const std::string       source = "int main() { print(1); }";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);
    parser::Parser          parser{source, tokens, diagnostics};
    const auto              original_tokens = tokens;
    const auto              first           = parser.ParseTranslationUnit();
    testing::internal::CaptureStderr();
    diagnostics.Report(
        {1, 1, common::Severity::kError, "earlier lexical error"});
    const auto second = parser.ParseTranslationUnit();
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(first.has_syntax_errors);
    EXPECT_FALSE(second.has_syntax_errors);
    EXPECT_TRUE(diagnostics.HasErrors());
    EXPECT_NE(first.translation_unit.get(), second.translation_unit.get());
    const auto* first_body  = MainBody(first);
    const auto* second_body = MainBody(second);
    ASSERT_NE(first_body, nullptr);
    ASSERT_NE(second_body, nullptr);
    ASSERT_EQ(first_body->body.size(), 1U);
    ASSERT_EQ(second_body->body.size(), 1U);
    EXPECT_NE(first_body->body[0].get(), second_body->body[0].get());
    EXPECT_EQ(output, "1:1: error: earlier lexical error\n");
    ASSERT_EQ(tokens.size(), original_tokens.size());
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        EXPECT_EQ(tokens[index].kind, original_tokens[index].kind);
        EXPECT_EQ(tokens[index].lexeme, original_tokens[index].lexeme);
        EXPECT_EQ(tokens[index].loc.line, original_tokens[index].loc.line);
        EXPECT_EQ(tokens[index].loc.col, original_tokens[index].loc.col);
    }
    EXPECT_EQ(source, "int main() { print(1); }");
}

TEST(ParserLifecycleTest, PriorWarningDoesNotBecomeLocalSyntaxError) {
    const std::string       source = "int main() { }";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);
    testing::internal::CaptureStderr();
    diagnostics.Report({1, 1, common::Severity::kWarning, "earlier warning"});
    parser::Parser parser{source, tokens, diagnostics};
    const auto     result = parser.ParseTranslationUnit();
    const auto     output = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(result.has_syntax_errors);
    EXPECT_FALSE(diagnostics.HasErrors());
    EXPECT_EQ(output, "1:1: warning: earlier warning\n");
}

TEST(ParserLifecycleTest,
     RepeatedLexicalFailuresDoNotRediagnoseOrSetSyntaxStatus) {
    const std::string       source = "int main() { int a1b; print(9); }";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    testing::internal::CaptureStderr();
    const auto tokens          = lexer.Tokenize(source);
    const auto lexical_reports = testing::internal::GetCapturedStderr();
    EXPECT_EQ(lexical_reports, "1:18: error: invalid identifier: 'a1b'\n");
    ASSERT_TRUE(diagnostics.HasErrors());
    parser::Parser parser{source, tokens, diagnostics};
    testing::internal::CaptureStderr();
    const auto first          = parser.ParseTranslationUnit();
    const auto second         = parser.ParseTranslationUnit();
    const auto syntax_reports = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(syntax_reports.empty());
    EXPECT_FALSE(first.has_syntax_errors);
    EXPECT_FALSE(second.has_syntax_errors);
    EXPECT_TRUE(diagnostics.HasErrors());
    EXPECT_NE(first.translation_unit.get(), second.translation_unit.get());
    const auto* first_body  = MainBody(first);
    const auto* second_body = MainBody(second);
    ASSERT_NE(first_body, nullptr);
    ASSERT_NE(second_body, nullptr);
    ASSERT_EQ(first_body->body.size(), 1U);
    ASSERT_EQ(second_body->body.size(), 1U);
    EXPECT_EQ(first_body->body[0]->kind, ast::StmtKind::Print);
    EXPECT_EQ(second_body->body[0]->kind, ast::StmtKind::Print);
    ExpectStatement(first_body);
    ExpectStatement(second_body);
}

TEST(ParserLifecycleTest, TreesOwnSpellingsAfterAllBorrowedInputsAreDestroyed) {
    parser::ParseResult result;
    {
        const std::string source =
            "int main() { int answer12 = 00042; print(answer12); }";
        common::DiagnosisEngine diagnostics;
        lexer::Lexer            lexer{diagnostics};
        const auto              tokens = lexer.Tokenize(source);
        parser::Parser          parser{source, tokens, diagnostics};
        result = parser.ParseTranslationUnit();
        EXPECT_FALSE(result.has_syntax_errors);
    }
    const auto* body = MainBody(result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 2U);
    const auto* variable = Variable(body->body[0].get());
    ASSERT_NE(variable, nullptr);
    EXPECT_EQ(variable->name, "answer12");
    const auto* value =
        dynamic_cast<const ast::IntLiteralExpr*>(variable->init.get());
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->spelling, "00042");
    const auto* print =
        dynamic_cast<const ast::PrintStmt*>(body->body[1].get());
    ASSERT_NE(print, nullptr);
    const auto* reference =
        dynamic_cast<const ast::DeclRefExpr*>(print->value.get());
    ASSERT_NE(reference, nullptr);
    EXPECT_EQ(reference->name, "answer12");
}

TEST(ParserComponentTest, DeclarationsAndAssignmentsLeaveCallerSeparators) {
    for (const auto& [source, separator] :
         std::vector<std::pair<std::string, lexer::TokenKind>>{
             {"int x = (1 + 2) * 3;", lexer::TokenKind::semi},
             {"bool flag = true)", lexer::TokenKind::r_paren}}) {
        SCOPED_TRACE(source);
        common::DiagnosisEngine diagnostics;
        lexer::Lexer            lexer{diagnostics};
        const auto              tokens = lexer.Tokenize(source);
        parser::Parser          parser{source, tokens, diagnostics};
        const auto declaration = parser::ParserTestAccess::Declaration(parser);
        ASSERT_NE(declaration, nullptr);
        ASSERT_NE(declaration->init, nullptr);
        EXPECT_EQ(parser::ParserTestAccess::PeekKind(parser), separator);
        ExpectLocation(declaration->loc, 1, 1);
        EXPECT_FALSE(diagnostics.HasErrors());
    }
    const std::string       source = "answer12 = (1 + 2);";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);
    parser::Parser          parser{source, tokens, diagnostics};
    const auto assignment = parser::ParserTestAccess::Assignment(parser);
    ASSERT_NE(assignment, nullptr);
    ExpectLocation(assignment->loc, 1, 1);
    ExpectLocation(assignment->target->loc, 1, 1);
    EXPECT_EQ(parser::ParserTestAccess::PeekKind(parser),
              lexer::TokenKind::semi);
    EXPECT_FALSE(diagnostics.HasErrors());
}

TEST(ParserComponentTest, ExpressionsLeaveEveryCallerBoundaryIntact) {
    for (const auto& [source, separator] :
         std::vector<std::pair<std::string, lexer::TokenKind>>{
             {"(1 + 2) * 3;", lexer::TokenKind::semi},
             {"((answer12)))", lexer::TokenKind::r_paren},
             {"true}", lexer::TokenKind::r_brace},
             {"false", lexer::TokenKind::eof}}) {
        SCOPED_TRACE(source);
        common::DiagnosisEngine diagnostics;
        lexer::Lexer            lexer{diagnostics};
        const auto              tokens = lexer.Tokenize(source);
        parser::Parser          parser{source, tokens, diagnostics};
        const auto expression = parser::ParserTestAccess::Expression(parser);
        ASSERT_NE(expression, nullptr);
        ExpectExpression(expression.get());
        EXPECT_EQ(parser::ParserTestAccess::PeekKind(parser), separator);
        EXPECT_FALSE(diagnostics.HasErrors());
    }
}

TEST(ParserComponentTest,
     PrattMinimumBindingPowerLeavesWeakerOperatorForCaller) {
    const std::string       source = "a * b + c;";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);
    parser::Parser          parser{source, tokens, diagnostics};
    const auto expression = parser::ParserTestAccess::Expression(parser, 6);
    ASSERT_NE(expression, nullptr);
    const auto* multiplication =
        dynamic_cast<const ast::BinaryExpr*>(expression.get());
    ASSERT_NE(multiplication, nullptr);
    EXPECT_EQ(multiplication->op, ast::BinaryOp::Mul);
    EXPECT_EQ(parser::ParserTestAccess::PeekKind(parser),
              lexer::TokenKind::plus);
    EXPECT_FALSE(diagnostics.HasErrors());
}

TEST(ParserComponentTest,
     DiagnosedExpressionFailuresLeaveCallerBoundariesIntact) {
    for (const auto& [source, separator] :
         std::vector<std::pair<std::string, lexer::TokenKind>>{
             {";", lexer::TokenKind::semi},
             {"(1 + ;", lexer::TokenKind::semi},
             {"1 + )", lexer::TokenKind::r_paren},
             {"(1 + 2;", lexer::TokenKind::semi},
             {"1 * }", lexer::TokenKind::r_brace},
             {"1 +", lexer::TokenKind::eof}}) {
        SCOPED_TRACE(source);
        common::DiagnosisEngine diagnostics;
        lexer::Lexer            lexer{diagnostics};
        const auto              tokens = lexer.Tokenize(source);
        parser::Parser          parser{source, tokens, diagnostics};
        testing::internal::CaptureStderr();
        const auto expression = parser::ParserTestAccess::Expression(parser);
        const auto output     = testing::internal::GetCapturedStderr();
        EXPECT_EQ(expression, nullptr);
        EXPECT_TRUE(diagnostics.HasErrors());
        EXPECT_EQ(parser::ParserTestAccess::PeekKind(parser), separator);
        EXPECT_NE(output.find(": error:"), std::string::npos);
        const auto first_error = output.find(": error:");
        EXPECT_EQ(output.find(": error:", first_error + 1), std::string::npos)
            << output;
    }
}

}  // namespace
