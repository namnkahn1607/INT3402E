#include <gtest/gtest.h>

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "lexer/lexer.h"
#include "parser/parser.h"

namespace {

struct ParsedSource {
    parser::ParseResult result;
    std::string         diagnostics;
    std::size_t         token_count;
    bool                has_errors;
};

ParsedSource ParseSource(const std::string& source,
                         bool               lexical_errors = false) {
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            scanner{diagnostics};
    testing::internal::CaptureStderr();
    const auto tokens = scanner.Tokenize(source);
    EXPECT_EQ(diagnostics.HasErrors(), lexical_errors);

    parser::Parser syntax{source, tokens, diagnostics};
    auto           result  = syntax.ParseTranslationUnit();
    auto           reports = testing::internal::GetCapturedStderr();
    EXPECT_EQ(diagnostics.HasErrors(),
              lexical_errors || result.has_syntax_errors);
    return {std::move(result), std::move(reports), tokens.size(),
            diagnostics.HasErrors()};
}

std::vector<common::SourceLocation> ErrorLocations(
    std::string_view diagnostics) {
    std::vector<common::SourceLocation> locations;
    std::istringstream                  lines{std::string{diagnostics}};
    std::string                         line;
    while (std::getline(lines, line)) {
        EXPECT_NE(line.find(": error: "), std::string::npos) << line;
        std::istringstream     fields{line};
        common::SourceLocation location;
        char                   separator = '\0';
        fields >> location.line >> separator;
        EXPECT_EQ(separator, ':');
        fields >> location.col >> separator;
        EXPECT_EQ(separator, ':');
        EXPECT_FALSE(fields.fail()) << line;
        locations.push_back(location);
    }
    return locations;
}

void ExpectLocation(common::SourceLocation actual, int line, int column) {
    EXPECT_EQ(actual.line, line);
    EXPECT_EQ(actual.col, column);
}

void ExpectValidExpression(const ast::Expr* expression) {
    ASSERT_NE(expression, nullptr);
    switch (expression->kind) {
        case ast::ExprKind::IntLiteral:
        case ast::ExprKind::BoolLiteral:
        case ast::ExprKind::DeclRef: break;
        case ast::ExprKind::Binary: {
            const auto& binary =
                static_cast<const ast::BinaryExpr&>(*expression);
            ExpectValidExpression(binary.lhs.get());
            ExpectValidExpression(binary.rhs.get());
            break;
        }
        case ast::ExprKind::Assign: {
            const auto& assignment =
                static_cast<const ast::AssignExpr&>(*expression);
            ASSERT_NE(assignment.target, nullptr);
            EXPECT_EQ(assignment.target->kind, ast::ExprKind::DeclRef);
            ExpectValidExpression(assignment.target.get());
            ExpectValidExpression(assignment.value.get());
            break;
        }
        default:
            ADD_FAILURE()
                << "The parser retained an unsupported expression kind";
    }
}

void ExpectValidStatement(const ast::Stmt* statement) {
    ASSERT_NE(statement, nullptr);
    switch (statement->kind) {
        case ast::StmtKind::Compound: {
            const auto& block =
                static_cast<const ast::CompoundStmt&>(*statement);
            for (const auto& child : block.body) {
                ExpectValidStatement(child.get());
            }
            break;
        }
        case ast::StmtKind::Decl: {
            const auto& wrapper = static_cast<const ast::DeclStmt&>(*statement);
            ASSERT_NE(wrapper.decl, nullptr);
            ASSERT_EQ(wrapper.decl->kind, ast::DeclKind::Var);
            const auto& declaration =
                static_cast<const ast::VarDecl&>(*wrapper.decl);
            if (declaration.init) {
                ExpectValidExpression(declaration.init.get());
            }
            break;
        }
        case ast::StmtKind::Expr: {
            const auto& wrapper = static_cast<const ast::ExprStmt&>(*statement);
            ASSERT_NE(wrapper.expr, nullptr);
            EXPECT_EQ(wrapper.expr->kind, ast::ExprKind::Assign);
            ExpectValidExpression(wrapper.expr.get());
            break;
        }
        case ast::StmtKind::If: {
            const auto& selection = static_cast<const ast::IfStmt&>(*statement);
            ExpectValidExpression(selection.condition.get());
            ExpectValidStatement(selection.then_branch.get());
            if (selection.else_branch) {
                ExpectValidStatement(selection.else_branch.get());
            }
            break;
        }
        case ast::StmtKind::DoWhile: {
            const auto& loop = static_cast<const ast::DoWhileStmt&>(*statement);
            ExpectValidStatement(loop.body.get());
            ExpectValidExpression(loop.condition.get());
            break;
        }
        case ast::StmtKind::For: {
            const auto& loop = static_cast<const ast::ForStmt&>(*statement);
            ASSERT_NE(loop.init, nullptr);
            EXPECT_TRUE(loop.init->kind == ast::StmtKind::Decl ||
                        loop.init->kind == ast::StmtKind::Expr);
            ExpectValidStatement(loop.init.get());
            ExpectValidExpression(loop.condition.get());
            ASSERT_NE(loop.step, nullptr);
            EXPECT_EQ(loop.step->kind, ast::ExprKind::Assign);
            ExpectValidExpression(loop.step.get());
            ExpectValidStatement(loop.body.get());
            break;
        }
        case ast::StmtKind::Print: {
            const auto& print = static_cast<const ast::PrintStmt&>(*statement);
            ExpectValidExpression(print.value.get());
            break;
        }
        default:
            ADD_FAILURE()
                << "The parser retained an unsupported statement kind";
    }
}

void ExpectValidPartialTree(const parser::ParseResult& result) {
    ASSERT_NE(result.translation_unit, nullptr);
    EXPECT_LE(result.translation_unit->decls.size(), 1U);
    for (const auto& declaration : result.translation_unit->decls) {
        ASSERT_NE(declaration, nullptr);
        ASSERT_EQ(declaration->kind, ast::DeclKind::Func);
        const auto& function = static_cast<const ast::FuncDecl&>(*declaration);
        EXPECT_EQ(function.name, "main");
        EXPECT_EQ(function.return_type, ast::Type::Int);
        EXPECT_TRUE(function.params.empty());
        ExpectValidStatement(function.body.get());
    }
}

const ast::CompoundStmt* MainBody(const parser::ParseResult& result) {
    if (!result.translation_unit ||
        result.translation_unit->decls.size() != 1U ||
        !result.translation_unit->decls.front() ||
        result.translation_unit->decls.front()->kind != ast::DeclKind::Func) {
        return nullptr;
    }
    return static_cast<const ast::FuncDecl&>(
               *result.translation_unit->decls.front())
        .body.get();
}

const ast::PrintStmt* PrintAt(const ast::CompoundStmt& body,
                              std::size_t              index) {
    if (index >= body.body.size() || !body.body[index] ||
        body.body[index]->kind != ast::StmtKind::Print) {
        return nullptr;
    }
    return static_cast<const ast::PrintStmt*>(body.body[index].get());
}

void ExpectInteger(const ast::Expr* expression, std::string_view spelling) {
    ASSERT_NE(expression, nullptr);
    ASSERT_EQ(expression->kind, ast::ExprKind::IntLiteral);
    EXPECT_EQ(static_cast<const ast::IntLiteralExpr&>(*expression).spelling,
              spelling);
}

TEST(ParserRecovery, IndependentMultilineErrorsRetainAllSafelyReachedPrints) {
    const auto parsed = ParseSource(
        "int main() {\n"
        "int x = ;\n"
        "print(1);\n"
        "bool flag = 2 * ;\n"
        "print(flag);\n"
        "x = 1 > 2 > 3;\n"
        "print(2 + 3 * 4);\n"
        "}\n");

    EXPECT_TRUE(parsed.result.has_syntax_errors);
    const auto errors = ErrorLocations(parsed.diagnostics);
    ASSERT_EQ(errors.size(), 3U) << parsed.diagnostics;
    ExpectLocation(errors[0], 2, 9);
    ExpectLocation(errors[1], 4, 17);
    ExpectLocation(errors[2], 6, 11);
    ExpectValidPartialTree(parsed.result);

    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 3U);
    const auto* first = PrintAt(*body, 0);
    ASSERT_NE(first, nullptr);
    ExpectInteger(first->value.get(), "1");
    const auto* second = PrintAt(*body, 1);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(second->value, nullptr);
    ASSERT_EQ(second->value->kind, ast::ExprKind::DeclRef);
    EXPECT_EQ(static_cast<const ast::DeclRefExpr&>(*second->value).name,
              "flag");
    const auto* last = PrintAt(*body, 2);
    ASSERT_NE(last, nullptr);
    ASSERT_NE(last->value, nullptr);
    ASSERT_EQ(last->value->kind, ast::ExprKind::Binary);
    const auto& addition = static_cast<const ast::BinaryExpr&>(*last->value);
    EXPECT_EQ(addition.op, ast::BinaryOp::Add);
    ExpectInteger(addition.lhs.get(), "2");
    ASSERT_NE(addition.rhs, nullptr);
    ASSERT_EQ(addition.rhs->kind, ast::ExprKind::Binary);
    const auto& multiplication =
        static_cast<const ast::BinaryExpr&>(*addition.rhs);
    EXPECT_EQ(multiplication.op, ast::BinaryOp::Mul);
    ExpectInteger(multiplication.lhs.get(), "3");
    ExpectInteger(multiplication.rhs.get(), "4");
}

TEST(ParserRecovery,
     MissingSemicolonSkipsToSafeBoundaryWithoutInsertingTokens) {
    const auto parsed = ParseSource(
        "int main() {\n"
        "int before; int x\n"
        "print(1); print(2);\n"
        "}");

    EXPECT_TRUE(parsed.result.has_syntax_errors);
    const auto errors = ErrorLocations(parsed.diagnostics);
    ASSERT_EQ(errors.size(), 1U) << parsed.diagnostics;
    ExpectLocation(errors[0], 3, 1);
    ExpectValidPartialTree(parsed.result);
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_GE(body->body.size(), 2U);
    ASSERT_NE(body->body[0], nullptr);
    ASSERT_EQ(body->body[0]->kind, ast::StmtKind::Decl);
    const auto& declaration = static_cast<const ast::DeclStmt&>(*body->body[0]);
    ASSERT_NE(declaration.decl, nullptr);
    ASSERT_EQ(declaration.decl->kind, ast::DeclKind::Var);
    EXPECT_EQ(static_cast<const ast::VarDecl&>(*declaration.decl).name,
              "before");
    const auto* last = PrintAt(*body, body->body.size() - 1U);
    ASSERT_NE(last, nullptr);
    ExpectInteger(last->value.get(), "2");
    for (std::size_t index = 1; index < body->body.size(); ++index) {
        ASSERT_NE(body->body[index], nullptr);
        EXPECT_NE(body->body[index]->kind, ast::StmtKind::Decl);
    }
}

TEST(ParserRecovery,
     FailedInitializersAreOmittedAndEachOperandIsDiagnosedOnce) {
    const auto parsed =
        ParseSource("int main() { int x = ; print(1); int y = ; print(2); }");
    EXPECT_TRUE(parsed.result.has_syntax_errors);
    EXPECT_EQ(ErrorLocations(parsed.diagnostics).size(), 2U)
        << parsed.diagnostics;
    ExpectValidPartialTree(parsed.result);
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 2U);
    const auto* first  = PrintAt(*body, 0);
    const auto* second = PrintAt(*body, 1);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ExpectInteger(first->value.get(), "1");
    ExpectInteger(second->value.get(), "2");
}

TEST(ParserRecovery,
     ExpressionFailuresAfterConsumptionReachFollowingStatements) {
    for (const std::string broken :
         {"int x = 1 + ;", "x = 2 * ;", "print(1 + );", "int x = (1 + );",
          "print((1 + 2);", "int x = a > b > c;", "int x = a == b == c;"}) {
        SCOPED_TRACE(broken);
        const auto parsed =
            ParseSource("int main() { " + broken + " print(9); }");
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        EXPECT_EQ(ErrorLocations(parsed.diagnostics).size(), 1U)
            << parsed.diagnostics;
        ExpectValidPartialTree(parsed.result);
        const auto* body = MainBody(parsed.result);
        ASSERT_NE(body, nullptr);
        ASSERT_EQ(body->body.size(), 1U);
        const auto* print = PrintAt(*body, 0);
        ASSERT_NE(print, nullptr);
        ExpectInteger(print->value.get(), "9");
    }
}

TEST(ParserRecovery, AnEnclosingBraceIsLeftToItsBlockOwner) {
    const auto parsed = ParseSource("int main() { int x = }\n// tail\n");
    EXPECT_TRUE(parsed.result.has_syntax_errors);
    const auto errors = ErrorLocations(parsed.diagnostics);
    ASSERT_EQ(errors.size(), 1U) << parsed.diagnostics;
    ExpectLocation(errors[0], 1, 22);
    ExpectValidPartialTree(parsed.result);
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    EXPECT_TRUE(body->body.empty());
}

TEST(ParserRecovery,
     MultilineOperandAndTrailingCommentEofLocationsAreAccurate) {
    const auto malformed_expression = ParseSource(
        "int main() {\n"
        "/* first\n"
        "second */\n"
        "int x = 1 +\n"
        "  ;\n"
        "print(2);\n"
        "}");
    const auto operand_errors =
        ErrorLocations(malformed_expression.diagnostics);
    ASSERT_EQ(operand_errors.size(), 1U) << malformed_expression.diagnostics;
    ExpectLocation(operand_errors[0], 5, 3);
    ExpectValidPartialTree(malformed_expression.result);
    const auto* body = MainBody(malformed_expression.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 1U);
    ASSERT_NE(PrintAt(*body, 0), nullptr);

    for (const auto& source :
         {std::string{"int main() {\n// tail\n"},
          std::string{"int main() { int x =\n/* tail\n*/\n"},
          std::string{"int main() { print((1 +\n// tail\n"}}) {
        SCOPED_TRACE(source);
        const auto parsed = ParseSource(source);
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        const auto errors = ErrorLocations(parsed.diagnostics);
        ASSERT_FALSE(errors.empty()) << parsed.diagnostics;
        EXPECT_LE(errors.size(), parsed.token_count + 1U);
        if (source == "int main() {\n// tail\n") {
            EXPECT_EQ(errors.size(), 1U) << parsed.diagnostics;
        }
        const int expected_line =
            source.find("/*") == std::string::npos ? 3 : 4;
        for (const auto location : errors) {
            ExpectLocation(location, expected_line, 1);
        }
        ExpectValidPartialTree(parsed.result);
    }
}

TEST(ParserRecovery,
     MalformedForHeadersNeverLeakHeaderFragmentsIntoStatements) {
    for (const std::string header :
         {"; true; leaked = 1", "int i = ; true; leaked = 1",
          "int i = 0; ; leaked = 1", "int i = 0; 1 + ; leaked = 1",
          "int i = 0; true; leaked =", "int i = 0; true; leaked = 1 +",
          "int i = 0; true; leaked = (1 + 2"}) {
        SCOPED_TRACE(header);
        const auto parsed = ParseSource("int main() { for (" + header +
                                        ") { print(1); } print(2); }");
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        const auto errors = ErrorLocations(parsed.diagnostics);
        EXPECT_FALSE(errors.empty());
        EXPECT_LE(errors.size(), parsed.token_count + 1U);
        ExpectValidPartialTree(parsed.result);
        if (const auto* body = MainBody(parsed.result)) {
            for (const auto& statement : body->body) {
                ASSERT_NE(statement, nullptr);
                EXPECT_NE(statement->kind, ast::StmtKind::For);
                EXPECT_NE(statement->kind, ast::StmtKind::Expr)
                    << "A failed for header must not leak its assignment "
                       "update";
                EXPECT_NE(statement->kind, ast::StmtKind::Decl)
                    << "A failed for header must not leak its declaration "
                       "initializer";
            }
        }
    }
}

TEST(ParserRecovery,
     MalformedNestedControlsTerminateWithStructurallyValidTrees) {
    for (const std::string source :
         {"int main() { if () { print(1); } print(2); }",
          "int main() { if (true { int hidden; } print(2); }",
          "int main() { if (true) { int x = ; } else { print(2); } }",
          "int main() { if (true) { } else print(2); print(3); }",
          "int main() { do { print(1); } while (); print(2); }",
          "int main() { do { if (true) { print(1); } } print(2); }",
          "int main() { for (int i = 0; true; i = 1) { if (true) { int x = } } "
          "}",
          "int main() { { if (true { { int x; } } } print(3); }",
          "int main() { if (true) { do { print(1); } while (1 + } }",
          "int main() { for (int i = 0; (true; i = 1) { print(1); } }",
          "int main() { int x = (1 + 2; print(9); }"}) {
        SCOPED_TRACE(source);
        const auto parsed = ParseSource(source);
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        const auto errors = ErrorLocations(parsed.diagnostics);
        EXPECT_FALSE(errors.empty());
        EXPECT_LE(errors.size(), parsed.token_count + 1U);
        ExpectValidPartialTree(parsed.result);
    }
}

TEST(ParserRecovery, LexicalFailuresRetainValidStatementsWithoutSyntaxErrors) {
    const auto parsed = ParseSource(
        "int main() {\n"
        "int a1b;\n"
        "print(1);\n"
        "int x = 3.14;\n"
        "print(2);\n"
        "_x = 1;\n"
        "print(3);\n"
        "print(a1b);\n"
        "print(4);\n"
        "}",
        true);
    EXPECT_FALSE(parsed.result.has_syntax_errors);
    EXPECT_TRUE(parsed.has_errors);
    EXPECT_EQ(parsed.diagnostics,
              "2:5: error: invalid identifier: 'a1b'\n"
              "4:9: error: invalid numeric constant: '3.14'\n"
              "6:1: error: invalid identifier: '_x'\n"
              "8:7: error: invalid identifier: 'a1b'\n");
    ExpectValidPartialTree(parsed.result);
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 4U);
    for (std::size_t index = 0; index < body->body.size(); ++index) {
        const auto* print = PrintAt(*body, index);
        ASSERT_NE(print, nullptr);
        ExpectInteger(print->value.get(), std::to_string(index + 1U));
    }
}

TEST(ParserRecovery,
     MixedLexicalAndSyntaxErrorsKeepDistinctStatusAndLocations) {
    const auto parsed = ParseSource(
        "int main() {\n"
        "int x = 1e3;\n"
        "int y = ;\n"
        "print(2);\n"
        "/* first\n"
        "second */\n"
        "print(1 +\n"
        "  _x);\n"
        "print(3);\n"
        "x = 1 > 2 > 3;\n"
        "print(4);\n"
        "}",
        true);
    EXPECT_TRUE(parsed.result.has_syntax_errors);
    EXPECT_TRUE(parsed.has_errors);
    // The lexer scans the whole source first, so its diagnostics precede parser
    // reports. Each stage still reports the exact offending token location.
    EXPECT_EQ(
        parsed.diagnostics,
        "2:9: error: invalid numeric constant: '1e3'\n"
        "8:3: error: invalid identifier: '_x'\n"
        "3:9: error: expected an expression\n"
        "10:11: error: chained comparison or equality is not supported\n");
    ExpectValidPartialTree(parsed.result);
    const auto* body = MainBody(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 3U);
    for (std::size_t index = 0; index < body->body.size(); ++index) {
        const auto* print = PrintAt(*body, index);
        ASSERT_NE(print, nullptr);
        ExpectInteger(print->value.get(), std::to_string(index + 2U));
    }
}

TEST(ParserRecovery, InvalidSpellingsInControlsDoNotLeakMalformedChildren) {
    for (const std::string broken :
         {"if (_x) { print(1); }", "do { print(1); } while (1e3);",
          "for (int a1b = 0; true; x = 1) { print(1); }",
          "for (_x = 0; true; x = 1) { print(1); }",
          "for (int x = 0; 3.14; x = 1) { print(1); }",
          "for (int x = 0; true; _x = 1) { print(1); }",
          "for (int x = 0; true; x = 1e3) { print(1); }",
          "if (true) { int _x; print(1); }",
          "do { print(3.14); print(1); } while (true);"}) {
        SCOPED_TRACE(broken);
        const auto parsed =
            ParseSource("int main() { " + broken + " print(9); }", true);
        EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
        EXPECT_EQ(ErrorLocations(parsed.diagnostics).size(), 1U);
        EXPECT_TRUE(parsed.has_errors);
        ExpectValidPartialTree(parsed.result);
        const auto* body = MainBody(parsed.result);
        ASSERT_NE(body, nullptr);
        ASSERT_FALSE(body->body.empty());
        const auto* last = PrintAt(*body, body->body.size() - 1U);
        ASSERT_NE(last, nullptr);
        ExpectInteger(last->value.get(), "9");
    }
}

TEST(ParserRecovery, InvalidSpellingsAtExpectedDelimitersRemainLexerOwned) {
    for (const std::string broken :
         {"int x _x;", "x _x;", "print _x;", "print(1 _x);", "if _x { }",
          "if (true) _x;", "do { } _x;", "for _x;",
          "for (int x = 0; true; x = 1 _x) { }"}) {
        SCOPED_TRACE(broken);
        const auto parsed =
            ParseSource("int main() { " + broken + " print(9); }", true);
        EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
        EXPECT_EQ(ErrorLocations(parsed.diagnostics).size(), 1U);
        ExpectValidPartialTree(parsed.result);
    }
}

TEST(ParserRecovery, LexicalFailureDoesNotHideAnIndependentEofSyntaxError) {
    const auto parsed = ParseSource("int main() { int _x;\n// tail\n", true);
    EXPECT_TRUE(parsed.result.has_syntax_errors);
    EXPECT_EQ(parsed.diagnostics,
              "1:18: error: invalid identifier: '_x'\n"
              "3:1: error: expected '}' to close block\n");
    ExpectValidPartialTree(parsed.result);
}

TEST(ParserRecovery, InvalidTokenPrefixesTerminateWithBoundedDiagnostics) {
    const std::string source =
        "int main() { for (int _x = 1e3; a1b; x = 3.14) { print(_x); } "
        "if (_x) { print(1); } print(9); }";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            scanner{diagnostics};
    testing::internal::CaptureStderr();
    const auto tokens          = scanner.Tokenize(source);
    const auto lexical_reports = testing::internal::GetCapturedStderr();
    ASSERT_TRUE(diagnostics.HasErrors());
    ASSERT_EQ(ErrorLocations(lexical_reports).size(), 6U);
    std::string prefix;
    for (const auto& token : tokens) {
        prefix += token.lexeme + ' ';
        SCOPED_TRACE(prefix);
        const bool has_invalid = prefix.find("_x") != std::string::npos;
        const auto parsed      = ParseSource(prefix, has_invalid);
        EXPECT_LE(ErrorLocations(parsed.diagnostics).size(),
                  parsed.token_count + 1U);
        ExpectValidPartialTree(parsed.result);
    }
}

TEST(ParserRecovery, EveryTokenPrefixTerminatesWithBoundedDiagnostics) {
    const std::string complete =
        "int main() { int x = 1 + 2 * 3; bool flag = true; "
        "if (x > 1 == flag) { print((x + 1) * 2); } else { x = x + 1; } "
        "do { { print(x); } } while (x >= 1); "
        "for (int i = 0; i > 3; i = i + 1) { print(i); } }";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            scanner{diagnostics};
    const auto              complete_tokens = scanner.Tokenize(complete);
    ASSERT_FALSE(diagnostics.HasErrors());

    std::string prefix;
    for (std::size_t count = 0; count < complete_tokens.size(); ++count) {
        SCOPED_TRACE(prefix);
        const auto parsed = ParseSource(prefix);
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        const auto errors = ErrorLocations(parsed.diagnostics);
        EXPECT_FALSE(errors.empty());
        EXPECT_LE(errors.size(), parsed.token_count + 1U);
        for (const auto location : errors) {
            EXPECT_GE(location.line, 1);
            EXPECT_GE(location.col, 1);
            EXPECT_LE(location.col, static_cast<int>(prefix.size()) + 1);
        }
        ExpectValidPartialTree(parsed.result);
        prefix += complete_tokens[count].lexeme;
        prefix += ' ';
    }
    const auto parsed_complete = ParseSource(prefix);
    EXPECT_FALSE(parsed_complete.result.has_syntax_errors)
        << parsed_complete.diagnostics;
    ExpectValidPartialTree(parsed_complete.result);
}

}  // namespace
