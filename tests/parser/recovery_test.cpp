#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

#include "ast/expr.h"
#include "ast/stmt.h"
#include "ast/translation_unit.h"
#include "common/diagnostic.h"
#include "lexer/lexer.h"
#include "parser_test_helper.h"

using namespace parser::test;

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

    EXPECT_TRUE(parsed.has_syntax_errors());
    const auto errors = ErrorLocations(parsed);
    ASSERT_EQ(errors.size(), 3U);
    ExpectLocation(errors[0], 2, 9);
    ExpectLocation(errors[1], 4, 17);
    ExpectLocation(errors[2], 6, 11);
    ExpectValidPartialTree(parsed);

    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-PrintStmt <3:1>
  | `-IntLiteralExpr <3:7> '1'
  |-PrintStmt <5:1>
  | `-DeclRefExpr <5:7> 'flag'
  `-PrintStmt <7:1>
    `-BinaryExpr <7:9> '+'
      |-IntLiteralExpr <7:7> '2'
      `-BinaryExpr <7:13> '*'
        |-IntLiteralExpr <7:11> '3'
        `-IntLiteralExpr <7:15> '4'
)");
}

TEST(ParserRecovery, MissingSemicolonAtLineEndKeepsCompleteStatement) {
    const auto parsed = ParseSource(
        "{\n"
        "int before; int x\n"
        "print(1); print(2);\n"
        "}");

    EXPECT_TRUE(parsed.has_syntax_errors());
    const auto errors = ErrorLocations(parsed);
    ASSERT_EQ(errors.size(), 1U);
    ExpectLocation(errors[0], 2, 18);
    ExpectValidPartialTree(parsed);

    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-DeclStmt <2:1>
  | `-VarDecl <2:1> before 'int'
  |-DeclStmt <2:13>
  | `-VarDecl <2:13> x 'int'
  |-PrintStmt <3:1>
  | `-IntLiteralExpr <3:7> '1'
  `-PrintStmt <3:11>
    `-IntLiteralExpr <3:17> '2'
)");
}

TEST(ParserRecovery,
     MissingSemicolonMidLineSkipsToSafeBoundaryWithoutInsertingTokens) {
    const auto parsed = ParseSource(
        "{\n"
        "int before; int x print(1); print(2);\n"
        "}");

    EXPECT_TRUE(parsed.has_syntax_errors());
    const auto errors = ErrorLocations(parsed);
    ASSERT_EQ(errors.size(), 1U);
    ExpectLocation(errors[0], 2, 19);
    ExpectValidPartialTree(parsed);

    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-DeclStmt <2:1>
  | `-VarDecl <2:1> before 'int'
  `-PrintStmt <2:29>
    `-IntLiteralExpr <2:35> '2'
)");
}

TEST(ParserRecovery,
     FailedInitializersAreOmittedAndEachOperandIsDiagnosedOnce) {
    const auto parsed =
        ParseSource("{ int x = ; print(1); int y = ; print(2); }");
    EXPECT_TRUE(parsed.has_syntax_errors());
    EXPECT_EQ(ErrorLocations(parsed).size(), 2U);
    ExpectValidPartialTree(parsed);

    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-PrintStmt <1:13>
  | `-IntLiteralExpr <1:19> '1'
  `-PrintStmt <1:33>
    `-IntLiteralExpr <1:39> '2'
)");
}

TEST(ParserRecovery,
     ExpressionFailuresAfterConsumptionReachFollowingStatements) {
    for (const std::string broken :
         {"int x = 1 + ;", "x = 2 * ;", "print(1 + );", "int x = (1 + );",
          "print((1 + 2);", "int x = a > b > c;", "int x = a == b == c;"}) {
        SCOPED_TRACE(broken);
        const auto parsed = ParseSource("{ " + broken + " print(9); }");
        EXPECT_TRUE(parsed.has_syntax_errors());
        EXPECT_EQ(ErrorLocations(parsed).size(), 1U);
        ExpectValidPartialTree(parsed);

        ASSERT_NE(parsed.translation_unit, nullptr);
        ASSERT_EQ(parsed.translation_unit->statements.size(), 1U);
        const auto* block = dynamic_cast<const ast::CompoundStmt*>(
            parsed.translation_unit->statements[0].get());
        ASSERT_NE(block, nullptr);
        ASSERT_EQ(block->body.size(), 1U);
        const auto* print =
            dynamic_cast<const ast::PrintStmt*>(block->body[0].get());
        ASSERT_NE(print, nullptr);
        const auto* lit =
            dynamic_cast<const ast::IntLiteralExpr*>(print->value.get());
        ASSERT_NE(lit, nullptr);
        EXPECT_EQ(lit->spelling, "9");
    }
}

TEST(ParserRecovery, AnEnclosingBraceIsLeftToItsBlockOwner) {
    const auto parsed = ParseSource("{ int x = }\n// tail\n");
    EXPECT_TRUE(parsed.has_syntax_errors());
    const auto errors = ErrorLocations(parsed);
    ASSERT_EQ(errors.size(), 1U);
    ExpectLocation(errors[0], 1, 11);
    ExpectValidPartialTree(parsed);

    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
)");
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
    const auto operand_errors = ErrorLocations(malformed_expression);
    ASSERT_EQ(operand_errors.size(), 1U);
    ExpectLocation(operand_errors[0], 5, 3);
    ExpectValidPartialTree(malformed_expression);

    ExpectAst(*malformed_expression.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  `-PrintStmt <6:1>
    `-IntLiteralExpr <6:7> '2'
)");

    for (const auto& source :
         {std::string{"{\n// tail\n"}, std::string{"{ int x =\n/* tail\n*/\n"},
          std::string{"{ print((1 +\n// tail\n"}}) {
        SCOPED_TRACE(source);
        const auto parsed = ParseSource(source);
        EXPECT_TRUE(parsed.has_syntax_errors());
        const auto errors = ErrorLocations(parsed);
        ASSERT_FALSE(errors.empty());
        EXPECT_LE(errors.size(), parsed.token_count + 1U);
        if (source == "{\n// tail\n") {
            EXPECT_EQ(errors.size(), 1U);
        }
        const int expected_line =
            source.find("/*") == std::string::npos ? 3 : 4;
        for (const auto location : errors) {
            ExpectLocation(location, expected_line, 1);
        }
        ExpectValidPartialTree(parsed);
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
        EXPECT_TRUE(parsed.has_syntax_errors());
        const auto errors = ErrorLocations(parsed);
        EXPECT_FALSE(errors.empty());
        EXPECT_LE(errors.size(), parsed.token_count + 1U);
        ExpectValidPartialTree(parsed);

        ASSERT_NE(parsed.translation_unit, nullptr);
        if (parsed.translation_unit->statements.size() == 1U) {
            if (const auto* block = dynamic_cast<const ast::CompoundStmt*>(
                    parsed.translation_unit->statements[0].get())) {
                for (const auto& statement : block->body) {
                    ASSERT_NE(statement, nullptr);
                    EXPECT_NE(statement->kind, ast::StmtKind::For);
                    EXPECT_NE(statement->kind, ast::StmtKind::Assign);
                    EXPECT_NE(statement->kind, ast::StmtKind::Decl);
                }
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
        EXPECT_TRUE(parsed.has_syntax_errors());
        const auto errors = ErrorLocations(parsed);
        EXPECT_FALSE(errors.empty());
        EXPECT_LE(errors.size(), parsed.token_count + 1U);
        ExpectValidPartialTree(parsed);
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
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_TRUE(parsed.has_errors);
    ASSERT_EQ(parsed.lexical_errors.size(), 4U);
    ExpectLocation(parsed.lexical_errors[0].loc, 2, 5);
    EXPECT_EQ(parsed.lexical_errors[0].message, "invalid identifier: 'a1b'");
    ExpectLocation(parsed.lexical_errors[1].loc, 4, 9);
    EXPECT_EQ(parsed.lexical_errors[1].message,
              "invalid numeric constant: '3.14'");
    ExpectLocation(parsed.lexical_errors[2].loc, 6, 1);
    EXPECT_EQ(parsed.lexical_errors[2].message, "invalid identifier: '_x'");
    ExpectLocation(parsed.lexical_errors[3].loc, 8, 7);
    EXPECT_EQ(parsed.lexical_errors[3].message, "invalid identifier: 'a1b'");

    ExpectValidPartialTree(parsed);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-PrintStmt <3:1>
  | `-IntLiteralExpr <3:7> '1'
  |-PrintStmt <5:1>
  | `-IntLiteralExpr <5:7> '2'
  |-PrintStmt <7:1>
  | `-IntLiteralExpr <7:7> '3'
  `-PrintStmt <9:1>
    `-IntLiteralExpr <9:7> '4'
)");
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
    EXPECT_TRUE(parsed.has_syntax_errors());
    EXPECT_TRUE(parsed.has_errors);
    ASSERT_EQ(parsed.lexical_errors.size(), 2U);
    ExpectLocation(parsed.lexical_errors[0].loc, 2, 9);
    EXPECT_EQ(parsed.lexical_errors[0].message,
              "invalid numeric constant: '1e3'");
    ExpectLocation(parsed.lexical_errors[1].loc, 8, 3);
    EXPECT_EQ(parsed.lexical_errors[1].message, "invalid identifier: '_x'");

    ASSERT_EQ(parsed.syntax_errors.size(), 2U);
    ExpectLocation(parsed.syntax_errors[0].loc, 3, 9);
    EXPECT_EQ(parsed.syntax_errors[0].message, "expected an expression");
    ExpectLocation(parsed.syntax_errors[1].loc, 10, 11);
    EXPECT_EQ(parsed.syntax_errors[1].message,
              "chained comparison or equality is not supported");

    ExpectValidPartialTree(parsed);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-PrintStmt <4:1>
  | `-IntLiteralExpr <4:7> '2'
  |-PrintStmt <9:1>
  | `-IntLiteralExpr <9:7> '3'
  `-PrintStmt <11:1>
    `-IntLiteralExpr <11:7> '4'
)");
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
        EXPECT_FALSE(parsed.has_syntax_errors());
        EXPECT_EQ(ErrorLocations(parsed).size(), 1U);
        EXPECT_TRUE(parsed.has_errors);
        ExpectValidPartialTree(parsed);

        ASSERT_NE(parsed.translation_unit, nullptr);
        ASSERT_EQ(parsed.translation_unit->statements.size(), 1U);
        const auto* block = dynamic_cast<const ast::CompoundStmt*>(
            parsed.translation_unit->statements[0].get());
        ASSERT_NE(block, nullptr);
        ASSERT_FALSE(block->body.empty());
        const auto* last =
            dynamic_cast<const ast::PrintStmt*>(block->body.back().get());
        ASSERT_NE(last, nullptr);
        const auto* lit =
            dynamic_cast<const ast::IntLiteralExpr*>(last->value.get());
        ASSERT_NE(lit, nullptr);
        EXPECT_EQ(lit->spelling, "9");
    }
}

TEST(ParserRecovery, InvalidSpellingsAtExpectedDelimitersRemainLexerOwned) {
    for (const std::string broken :
         {"int x _x;", "x _x;", "print _x;", "print(1 _x);", "if _x { }",
          "if (true) _x;", "do { } _x;", "for _x;",
          "for (int x = 0; true; x = 1 _x) { }"}) {
        SCOPED_TRACE(broken);
        const auto parsed = ParseSource("{ " + broken + " print(9); }", true);
        EXPECT_FALSE(parsed.has_syntax_errors());
        EXPECT_EQ(ErrorLocations(parsed).size(), 1U);
        ExpectValidPartialTree(parsed);
    }
}

TEST(ParserRecovery, LexicalFailureDoesNotHideAnIndependentEofSyntaxError) {
    const auto parsed = ParseSource("{ int _x;\n// tail\n", true);
    EXPECT_TRUE(parsed.has_syntax_errors());
    ASSERT_EQ(parsed.lexical_errors.size(), 1U);
    ExpectLocation(parsed.lexical_errors[0].loc, 1, 7);
    EXPECT_EQ(parsed.lexical_errors[0].message, "invalid identifier: '_x'");
    ASSERT_EQ(parsed.syntax_errors.size(), 1U);
    ExpectLocation(parsed.syntax_errors[0].loc, 3, 1);
    EXPECT_EQ(parsed.syntax_errors[0].message, "expected '}' to close block");
    ExpectValidPartialTree(parsed);
}

TEST(ParserRecovery, InvalidTokenPrefixesTerminateWithBoundedDiagnostics) {
    const std::string source =
        "{ for (int _x = 1e3; a1b; x = 3.14) { print(_x); } "
        "if (_x) { print(1); } print(9); }";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            scanner{diagnostics};
    const auto              tokens = scanner.Tokenize(source);
    ASSERT_TRUE(diagnostics.HasErrors());
    ASSERT_EQ(ErrorLocations(diagnostics.Collected()).size(), 6U);

    std::string prefix;
    for (const auto& token : tokens) {
        prefix += token.lexeme + ' ';
        SCOPED_TRACE(prefix);
        const bool has_invalid = prefix.find("_x") != std::string::npos;
        const auto parsed      = ParseSource(prefix, has_invalid);
        EXPECT_LE(ErrorLocations(parsed).size(), parsed.token_count + 1U);
        ExpectValidPartialTree(parsed);
    }
}

TEST(ParserRecovery, FileScopeRecoversAcrossLinesAndConsumesStrayBraces) {
    const auto parsed =
        ParseSource("int x = ;\nprint(1);\n}\nint y = 2;\nprint(y);\n}");
    EXPECT_TRUE(parsed.has_syntax_errors());
    const auto errors = ErrorLocations(parsed);
    ASSERT_EQ(errors.size(), 3U);
    ExpectLocation(errors[0], 1, 9);
    ExpectLocation(errors[1], 3, 1);
    ExpectLocation(errors[2], 6, 1);
    ExpectValidPartialTree(parsed);

    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
|-PrintStmt <2:1>
| `-IntLiteralExpr <2:7> '1'
|-DeclStmt <4:1>
| `-VarDecl <4:1> y 'int' init
|   `-IntLiteralExpr <4:9> '2'
`-PrintStmt <5:1>
  `-DeclRefExpr <5:7> 'y'
)");
}

TEST(ParserRecovery, MalformedAssignmentsRetainLaterAssignmentStatements) {
    const auto parsed = ParseSource("x = ;\ny = 2;\nprint(y = 3);\nz = 4;");
    EXPECT_TRUE(parsed.has_syntax_errors());
    const auto errors = ErrorLocations(parsed);
    ASSERT_EQ(errors.size(), 2U);
    ExpectLocation(errors[0], 1, 5);
    ExpectLocation(errors[1], 3, 9);
    ExpectValidPartialTree(parsed);

    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
|-AssignStmt <2:1>
| |-DeclRefExpr <2:1> 'y'
| `-IntLiteralExpr <2:5> '2'
`-AssignStmt <4:1>
  |-DeclRefExpr <4:1> 'z'
  `-IntLiteralExpr <4:5> '4'
)");
}

TEST(ParserRecovery, UnsupportedFunctionsDoNotLeakBodiesOrEraseLaterItems) {
    for (const std::string function :
         {"int main() { print(2); }", "int other();",
          "bool other() { print(2); }", "int other(int x) { print(x); }"}) {
        SCOPED_TRACE(function);
        const auto parsed = ParseSource("print(1); " + function + " print(3);");
        EXPECT_TRUE(parsed.has_syntax_errors());
        EXPECT_EQ(ErrorLocations(parsed).size(), 1U);
        ExpectValidPartialTree(parsed);

        ASSERT_NE(parsed.translation_unit, nullptr);
        ASSERT_EQ(parsed.translation_unit->statements.size(), 2U);
        EXPECT_EQ(parsed.translation_unit->statements[0]->kind,
                  ast::StmtKind::Print);
        EXPECT_EQ(parsed.translation_unit->statements[1]->kind,
                  ast::StmtKind::Print);
        const auto* p1 = dynamic_cast<const ast::PrintStmt*>(
            parsed.translation_unit->statements[0].get());
        const auto* p2 = dynamic_cast<const ast::PrintStmt*>(
            parsed.translation_unit->statements[1].get());
        ASSERT_NE(p1, nullptr);
        ASSERT_NE(p2, nullptr);
        const auto* lit1 =
            dynamic_cast<const ast::IntLiteralExpr*>(p1->value.get());
        const auto* lit2 =
            dynamic_cast<const ast::IntLiteralExpr*>(p2->value.get());
        ASSERT_NE(lit1, nullptr);
        ASSERT_NE(lit2, nullptr);
        EXPECT_EQ(lit1->spelling, "1");
        EXPECT_EQ(lit2->spelling, "3");
    }
}

TEST(ParserRecovery, FileScopeLexicalFailuresKeepIndependentSyntaxStatus) {
    const auto lexical =
        ParseSource("int a1b; print(1); int x = 3.14; print(2);", true);
    EXPECT_FALSE(lexical.has_syntax_errors());
    EXPECT_EQ(ErrorLocations(lexical).size(), 2U);
    ExpectValidPartialTree(lexical);
    ExpectAst(*lexical.translation_unit, R"(
TranslationUnit
|-PrintStmt <1:10>
| `-IntLiteralExpr <1:16> '1'
`-PrintStmt <1:34>
  `-IntLiteralExpr <1:40> '2'
)");

    const auto mixed = ParseSource("int x = 1e3;\nint y = ;\nprint(2);", true);
    EXPECT_TRUE(mixed.has_syntax_errors());
    const auto errors = ErrorLocations(mixed);
    ASSERT_EQ(errors.size(), 2U);
    ExpectLocation(errors[0], 1, 9);
    ExpectLocation(errors[1], 2, 9);
    ExpectValidPartialTree(mixed);
    ExpectAst(*mixed.translation_unit, R"(
TranslationUnit
`-PrintStmt <3:1>
  `-IntLiteralExpr <3:7> '2'
)");
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
        EXPECT_LE(ErrorLocations(parsed).size(), parsed.token_count + 1U);
        ExpectValidPartialTree(parsed);
        if (count == tokens.size()) {
            EXPECT_FALSE(parsed.has_syntax_errors());
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
        EXPECT_EQ(parsed.has_syntax_errors(), count != 0);
        const auto errors = ErrorLocations(parsed);
        EXPECT_EQ(errors.empty(), count == 0);
        EXPECT_LE(errors.size(), parsed.token_count + 1U);
        for (const auto location : errors) {
            EXPECT_GE(location.line, 1);
            EXPECT_GE(location.col, 1);
            EXPECT_LE(location.col, static_cast<int>(prefix.size()) + 1);
        }
        ExpectValidPartialTree(parsed);
        prefix += complete_tokens[count].lexeme;
        prefix += ' ';
    }
    const auto parsed_complete = ParseSource(prefix);
    EXPECT_FALSE(parsed_complete.has_syntax_errors());
    ExpectValidPartialTree(parsed_complete);
}
