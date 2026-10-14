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
    return {.result      = std::move(result),
            .diagnostics = std::move(reports),
            .token_count = tokens.size(),
            .has_errors  = diagnostics.HasErrors()};
}

std::vector<common::Location> ErrorLocations(std::string_view diagnostics) {
    std::vector<common::Location> locations;
    std::istringstream            lines{std::string{diagnostics}};
    std::string                   line;
    while (std::getline(lines, line)) {
        EXPECT_NE(line.find(": error: "), std::string::npos) << line;
        std::istringstream fields{line};
        common::Location   location;
        char               separator = '\0';
        fields >> location.line >> separator;
        EXPECT_EQ(separator, ':');
        fields >> location.col >> separator;
        EXPECT_EQ(separator, ':');
        EXPECT_FALSE(fields.fail()) << line;
        locations.push_back(location);
    }
    return locations;
}

void ExpectLocation(common::Location actual, int line, int column) {
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
                dynamic_cast<const ast::BinaryExpr&>(*expression);
            ExpectValidExpression(binary.lhs.get());
            ExpectValidExpression(binary.rhs.get());
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
                dynamic_cast<const ast::CompoundStmt&>(*statement);
            for (const auto& child : block.body) {
                ExpectValidStatement(child.get());
            }
            break;
        }
        case ast::StmtKind::Decl: {
            const auto& wrapper =
                dynamic_cast<const ast::DeclStmt&>(*statement);
            ASSERT_NE(wrapper.decl, nullptr);
            ASSERT_EQ(wrapper.decl->kind, ast::DeclKind::Var);
            const auto& declaration =
                dynamic_cast<const ast::VarDecl&>(*wrapper.decl);
            if (declaration.init) {
                ExpectValidExpression(declaration.init.get());
            }
            break;
        }
        case ast::StmtKind::Assign: {
            const auto& assignment =
                dynamic_cast<const ast::AssignStmt&>(*statement);
            ASSERT_NE(assignment.target, nullptr);
            EXPECT_EQ(assignment.target->kind, ast::ExprKind::DeclRef);
            ExpectValidExpression(assignment.target.get());
            ExpectValidExpression(assignment.value.get());
            break;
        }
        case ast::StmtKind::If: {
            const auto& selection =
                dynamic_cast<const ast::IfStmt&>(*statement);
            ExpectValidExpression(selection.condition.get());
            ExpectValidStatement(selection.then_branch.get());
            if (selection.else_branch) {
                ExpectValidStatement(selection.else_branch.get());
            }
            break;
        }
        case ast::StmtKind::DoWhile: {
            const auto& loop =
                dynamic_cast<const ast::DoWhileStmt&>(*statement);
            ExpectValidStatement(loop.body.get());
            ExpectValidExpression(loop.condition.get());
            break;
        }
        case ast::StmtKind::For: {
            const auto& loop = dynamic_cast<const ast::ForStmt&>(*statement);
            ASSERT_NE(loop.init, nullptr);
            EXPECT_TRUE(loop.init->kind == ast::StmtKind::Decl ||
                        loop.init->kind == ast::StmtKind::Assign);
            ExpectValidStatement(loop.init.get());
            ExpectValidExpression(loop.condition.get());
            ASSERT_NE(loop.step, nullptr);
            EXPECT_EQ(loop.step->kind, ast::StmtKind::Assign);
            ExpectValidStatement(loop.step.get());
            ExpectValidStatement(loop.body.get());
            break;
        }
        case ast::StmtKind::Print: {
            const auto& print = dynamic_cast<const ast::PrintStmt&>(*statement);
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
    for (const auto& statement : result.translation_unit->statements) {
        ExpectValidStatement(statement.get());
    }
}

const ast::CompoundStmt* OnlyBlock(const parser::ParseResult& result) {
    if (!result.translation_unit ||
        result.translation_unit->statements.size() != 1U) {
        return nullptr;
    }
    return dynamic_cast<const ast::CompoundStmt*>(
        result.translation_unit->statements.front().get());
}

const ast::PrintStmt* PrintAt(const ast::CompoundStmt& body,
                              std::size_t              index) {
    if (index >= body.body.size() || !body.body[index] ||
        body.body[index]->kind != ast::StmtKind::Print) {
        return nullptr;
    }
    return dynamic_cast<const ast::PrintStmt*>(body.body[index].get());
}

void ExpectInteger(const ast::Expr* expression, std::string_view spelling) {
    ASSERT_NE(expression, nullptr);
    ASSERT_EQ(expression->kind, ast::ExprKind::IntLiteral);
    EXPECT_EQ(dynamic_cast<const ast::IntLiteralExpr&>(*expression).spelling,
              spelling);
}

TEST(ParserRecovery, IndependentMultilineErrorsRetainAllSafelyReachedPrints) {
    const auto parsed = ParseSource(
        "{\n"
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

    const auto* body = OnlyBlock(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 3U);
    const auto* first = PrintAt(*body, 0);
    ASSERT_NE(first, nullptr);
    ExpectInteger(first->value.get(), "1");
    const auto* second = PrintAt(*body, 1);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(second->value, nullptr);
    ASSERT_EQ(second->value->kind, ast::ExprKind::DeclRef);
    EXPECT_EQ(dynamic_cast<const ast::DeclRefExpr&>(*second->value).name,
              "flag");
    const auto* last = PrintAt(*body, 2);
    ASSERT_NE(last, nullptr);
    ASSERT_NE(last->value, nullptr);
    ASSERT_EQ(last->value->kind, ast::ExprKind::Binary);
    const auto& addition = dynamic_cast<const ast::BinaryExpr&>(*last->value);
    EXPECT_EQ(addition.op, ast::BinaryOp::Add);
    ExpectInteger(addition.lhs.get(), "2");
    ASSERT_NE(addition.rhs, nullptr);
    ASSERT_EQ(addition.rhs->kind, ast::ExprKind::Binary);
    const auto& multiplication =
        dynamic_cast<const ast::BinaryExpr&>(*addition.rhs);
    EXPECT_EQ(multiplication.op, ast::BinaryOp::Mul);
    ExpectInteger(multiplication.lhs.get(), "3");
    ExpectInteger(multiplication.rhs.get(), "4");
}

// Next to PrintAt(). Returns "" on any mismatch so a failing EXPECT_EQ stays
// readable instead of dereferencing a wrong node.
std::string VarNameAt(const ast::CompoundStmt& block, std::size_t index) {
    if (index >= block.body.size() || block.body[index] == nullptr ||
        block.body[index]->kind != ast::StmtKind::Decl) {
        return {};
    }
    const auto& stmt = dynamic_cast<const ast::DeclStmt&>(*block.body[index]);
    if (stmt.decl == nullptr || stmt.decl->kind != ast::DeclKind::Var) {
        return {};
    }
    return dynamic_cast<const ast::VarDecl&>(*stmt.decl).name;
}

TEST(ParserRecovery, MissingSemicolonAtLineEndKeepsCompleteStatement) {
    const auto parsed = ParseSource(
        "{\n"
        "int before; int x\n"
        "print(1); print(2);\n"
        "}");

    EXPECT_TRUE(parsed.result.has_syntax_errors);
    const auto errors = ErrorLocations(parsed.diagnostics);
    ASSERT_EQ(errors.size(), 1U) << parsed.diagnostics;
    ExpectLocation(errors[0], 2, 18);
    ExpectValidPartialTree(parsed.result);
    const auto* body = OnlyBlock(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 4U);
    EXPECT_EQ(VarNameAt(*body, 0), "before");
    EXPECT_EQ(VarNameAt(*body, 1), "x");
    const auto* first = PrintAt(*body, 2);
    ASSERT_NE(first, nullptr);
    ExpectInteger(first->value.get(), "1");
    const auto* last = PrintAt(*body, 3);
    ASSERT_NE(last, nullptr);
    ExpectInteger(last->value.get(), "2");
}

TEST(ParserRecovery,
     MissingSemicolonMidLineSkipsToSafeBoundaryWithoutInsertingTokens) {
    const auto parsed = ParseSource(
        "{\n"
        "int before; int x print(1); print(2);\n"
        "}");

    EXPECT_TRUE(parsed.result.has_syntax_errors);
    const auto errors = ErrorLocations(parsed.diagnostics);
    ASSERT_EQ(errors.size(), 1U) << parsed.diagnostics;
    ExpectLocation(errors[0], 2, 19);
    ExpectValidPartialTree(parsed.result);
    const auto* body = OnlyBlock(parsed.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 2U);
    EXPECT_EQ(VarNameAt(*body, 0), "before");
    const auto* last = PrintAt(*body, 1);
    ASSERT_NE(last, nullptr);
    ExpectInteger(last->value.get(), "2");
}

TEST(ParserRecovery,
     FailedInitializersAreOmittedAndEachOperandIsDiagnosedOnce) {
    const auto parsed =
        ParseSource("{ int x = ; print(1); int y = ; print(2); }");
    EXPECT_TRUE(parsed.result.has_syntax_errors);
    EXPECT_EQ(ErrorLocations(parsed.diagnostics).size(), 2U)
        << parsed.diagnostics;
    ExpectValidPartialTree(parsed.result);
    const auto* body = OnlyBlock(parsed.result);
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
        const auto parsed = ParseSource("{ " + broken + " print(9); }");
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        EXPECT_EQ(ErrorLocations(parsed.diagnostics).size(), 1U)
            << parsed.diagnostics;
        ExpectValidPartialTree(parsed.result);
        const auto* body = OnlyBlock(parsed.result);
        ASSERT_NE(body, nullptr);
        ASSERT_EQ(body->body.size(), 1U);
        const auto* print = PrintAt(*body, 0);
        ASSERT_NE(print, nullptr);
        ExpectInteger(print->value.get(), "9");
    }
}

TEST(ParserRecovery, AnEnclosingBraceIsLeftToItsBlockOwner) {
    const auto parsed = ParseSource("{ int x = }\n// tail\n");
    EXPECT_TRUE(parsed.result.has_syntax_errors);
    const auto errors = ErrorLocations(parsed.diagnostics);
    ASSERT_EQ(errors.size(), 1U) << parsed.diagnostics;
    ExpectLocation(errors[0], 1, 11);
    ExpectValidPartialTree(parsed.result);
    const auto* body = OnlyBlock(parsed.result);
    ASSERT_NE(body, nullptr);
    EXPECT_TRUE(body->body.empty());
}

TEST(ParserRecovery,
     MultilineOperandAndTrailingCommentEofLocationsAreAccurate) {
    const auto malformed_expression = ParseSource(
        "{\n"
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
    const auto* body = OnlyBlock(malformed_expression.result);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->body.size(), 1U);
    ASSERT_NE(PrintAt(*body, 0), nullptr);

    for (const auto& source :
         {std::string{"{\n// tail\n"}, std::string{"{ int x =\n/* tail\n*/\n"},
          std::string{"{ print((1 +\n// tail\n"}}) {
        SCOPED_TRACE(source);
        const auto parsed = ParseSource(source);
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        const auto errors = ErrorLocations(parsed.diagnostics);
        ASSERT_FALSE(errors.empty()) << parsed.diagnostics;
        EXPECT_LE(errors.size(), parsed.token_count + 1U);
        if (source == "{\n// tail\n") {
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
        const auto parsed =
            ParseSource("{ for (" + header + ") { print(1); } print(2); }");
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        const auto errors = ErrorLocations(parsed.diagnostics);
        EXPECT_FALSE(errors.empty());
        EXPECT_LE(errors.size(), parsed.token_count + 1U);
        ExpectValidPartialTree(parsed.result);
        if (const auto* body = OnlyBlock(parsed.result)) {
            for (const auto& statement : body->body) {
                ASSERT_NE(statement, nullptr);
                EXPECT_NE(statement->kind, ast::StmtKind::For);
                EXPECT_NE(statement->kind, ast::StmtKind::Assign)
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
         {"{ if () { print(1); } print(2); }",
          "{ if (true { int hidden; } print(2); }",
          "{ if (true) { int x = ; } else { print(2); } }",
          "{ if (true) { } else print(2); print(3); }",
          "{ do { print(1); } while (); print(2); }",
          "{ do { if (true) { print(1); } } print(2); }",
          "{ for (int i = 0; true; i = 1) { if (true) { int x = } } }",
          "{ { if (true { { int x; } } } print(3); }",
          "{ if (true) { do { print(1); } while (1 + } }",
          "{ for (int i = 0; (true; i = 1) { print(1); } }",
          "{ int x = (1 + 2; print(9); }"}) {
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
        "{\n"
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
    const auto* body = OnlyBlock(parsed.result);
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
        "{\n"
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
    const auto* body = OnlyBlock(parsed.result);
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
        const auto parsed = ParseSource("{ " + broken + " print(9); }", true);
        EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
        EXPECT_EQ(ErrorLocations(parsed.diagnostics).size(), 1U);
        EXPECT_TRUE(parsed.has_errors);
        ExpectValidPartialTree(parsed.result);
        const auto* body = OnlyBlock(parsed.result);
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
        const auto parsed = ParseSource("{ " + broken + " print(9); }", true);
        EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
        EXPECT_EQ(ErrorLocations(parsed.diagnostics).size(), 1U);
        ExpectValidPartialTree(parsed.result);
    }
}

TEST(ParserRecovery, LexicalFailureDoesNotHideAnIndependentEofSyntaxError) {
    const auto parsed = ParseSource("{ int _x;\n// tail\n", true);
    EXPECT_TRUE(parsed.result.has_syntax_errors);
    EXPECT_EQ(parsed.diagnostics,
              "1:7: error: invalid identifier: '_x'\n"
              "3:1: error: expected '}' to close block\n");
    ExpectValidPartialTree(parsed.result);
}

TEST(ParserRecovery, InvalidTokenPrefixesTerminateWithBoundedDiagnostics) {
    const std::string source =
        "{ for (int _x = 1e3; a1b; x = 3.14) { print(_x); } "
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

TEST(ParserRecovery, FileScopeRecoversAcrossLinesAndConsumesStrayBraces) {
    const auto parsed =
        ParseSource("int x = ;\nprint(1);\n}\nint y = 2;\nprint(y);\n}");
    EXPECT_TRUE(parsed.result.has_syntax_errors);
    const auto errors = ErrorLocations(parsed.diagnostics);
    ASSERT_EQ(errors.size(), 3U) << parsed.diagnostics;
    ExpectLocation(errors[0], 1, 9);
    ExpectLocation(errors[1], 3, 1);
    ExpectLocation(errors[2], 6, 1);
    ExpectValidPartialTree(parsed.result);
    const auto& statements = parsed.result.translation_unit->statements;
    ASSERT_EQ(statements.size(), 3U);
    EXPECT_EQ(statements[0]->kind, ast::StmtKind::Print);
    EXPECT_EQ(statements[1]->kind, ast::StmtKind::Decl);
    EXPECT_EQ(statements[2]->kind, ast::StmtKind::Print);
}

TEST(ParserRecovery, MalformedAssignmentsRetainLaterAssignmentStatements) {
    const auto parsed = ParseSource("x = ;\ny = 2;\nprint(y = 3);\nz = 4;");
    EXPECT_TRUE(parsed.result.has_syntax_errors);
    const auto errors = ErrorLocations(parsed.diagnostics);
    ASSERT_EQ(errors.size(), 2U) << parsed.diagnostics;
    ExpectLocation(errors[0], 1, 5);
    ExpectLocation(errors[1], 3, 9);
    ExpectValidPartialTree(parsed.result);
    const auto& statements = parsed.result.translation_unit->statements;
    ASSERT_EQ(statements.size(), 2U);
    for (const auto& statement : statements) {
        ASSERT_EQ(statement->kind, ast::StmtKind::Assign);
    }
    const auto& first = dynamic_cast<const ast::AssignStmt&>(*statements[0]);
    const auto& last  = dynamic_cast<const ast::AssignStmt&>(*statements[1]);
    EXPECT_EQ(dynamic_cast<const ast::DeclRefExpr&>(*first.target).name, "y");
    EXPECT_EQ(dynamic_cast<const ast::DeclRefExpr&>(*last.target).name, "z");
    ExpectLocation(first.loc, 2, 1);
    ExpectLocation(last.loc, 4, 1);
}

TEST(ParserRecovery, UnsupportedFunctionsDoNotLeakBodiesOrEraseLaterItems) {
    for (const std::string function :
         {"int main() { print(2); }", "int other();",
          "bool other() { print(2); }", "int other(int x) { print(x); }"}) {
        SCOPED_TRACE(function);
        const auto parsed = ParseSource("print(1); " + function + " print(3);");
        EXPECT_TRUE(parsed.result.has_syntax_errors);
        EXPECT_EQ(ErrorLocations(parsed.diagnostics).size(), 1U);
        ExpectValidPartialTree(parsed.result);
        const auto& statements = parsed.result.translation_unit->statements;
        ASSERT_EQ(statements.size(), 2U);
        ASSERT_EQ(statements[0]->kind, ast::StmtKind::Print);
        ASSERT_EQ(statements[1]->kind, ast::StmtKind::Print);
        ExpectInteger(
            dynamic_cast<const ast::PrintStmt&>(*statements[0]).value.get(),
            "1");
        ExpectInteger(
            dynamic_cast<const ast::PrintStmt&>(*statements[1]).value.get(),
            "3");
    }
}

TEST(ParserRecovery, FileScopeLexicalFailuresKeepIndependentSyntaxStatus) {
    const auto lexical =
        ParseSource("int a1b; print(1); int x = 3.14; print(2);", true);
    EXPECT_FALSE(lexical.result.has_syntax_errors);
    EXPECT_EQ(ErrorLocations(lexical.diagnostics).size(), 2U);
    ExpectValidPartialTree(lexical.result);
    ASSERT_EQ(lexical.result.translation_unit->statements.size(), 2U);

    const auto mixed = ParseSource("int x = 1e3;\nint y = ;\nprint(2);", true);
    EXPECT_TRUE(mixed.result.has_syntax_errors);
    const auto errors = ErrorLocations(mixed.diagnostics);
    ASSERT_EQ(errors.size(), 2U);
    ExpectLocation(errors[0], 1, 9);
    ExpectLocation(errors[1], 2, 9);
    ExpectValidPartialTree(mixed.result);
    ASSERT_EQ(mixed.result.translation_unit->statements.size(), 1U);
    EXPECT_EQ(mixed.result.translation_unit->statements[0]->kind,
              ast::StmtKind::Print);
}

TEST(ParserRecovery, FileScopeTokenPrefixesTerminateWithValidPartialTrees) {
    const std::string complete =
        "int x = 1; print(x); if (true) { int y = 2; print(y); } "
        "for (x = 1; true; x = x + 1) { print(x); } print(3);";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            scanner{diagnostics};
    const auto              tokens = scanner.Tokenize(complete);
    ASSERT_FALSE(diagnostics.HasErrors());
    std::string prefix;
    for (std::size_t count = 0; count <= tokens.size(); ++count) {
        SCOPED_TRACE(prefix);
        const auto parsed = ParseSource(prefix);
        EXPECT_LE(ErrorLocations(parsed.diagnostics).size(),
                  parsed.token_count + 1U);
        ExpectValidPartialTree(parsed.result);
        if (count == tokens.size()) {
            EXPECT_FALSE(parsed.result.has_syntax_errors) << parsed.diagnostics;
        } else {
            prefix += tokens[count].lexeme + ' ';
        }
    }
}

TEST(ParserRecovery, EveryTokenPrefixTerminatesWithBoundedDiagnostics) {
    const std::string complete =
        "{ int x = 1 + 2 * 3; bool flag = true; "
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
        EXPECT_EQ(parsed.result.has_syntax_errors, count != 0);
        const auto errors = ErrorLocations(parsed.diagnostics);
        EXPECT_EQ(errors.empty(), count == 0);
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
