#include "parser/parser.h"

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ast/expr.h"
#include "ast/stmt.h"
#include "ast/translation_unit.h"
#include "common/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/expr_parser.h"
#include "parser_test_helper.h"

using namespace parser::test;

TEST(ParserProgramTest, EmptyBlockIsAnExplicitStatement) {
    const auto parsed = Parse("{ }");
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
)");
}

TEST(ParserProgramTest, BareSourceDoesNotRequireAnEntryFunction) {
    const auto parsed = Parse("int a = 1; print(a); a = a + 1;");
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
|-DeclStmt <1:1>
| `-VarDecl <1:1> a 'int' init
|   `-IntLiteralExpr <1:9> '1'
|-PrintStmt <1:12>
| `-DeclRefExpr <1:18> 'a'
`-AssignStmt <1:22>
  |-DeclRefExpr <1:22> 'a'
  `-BinaryExpr <1:28> '+'
    |-DeclRefExpr <1:26> 'a'
    `-IntLiteralExpr <1:30> '1'
)");
}

TEST(ParserProgramTest, EmptyAndTriviaOnlySourcesAreValid) {
    for (const std::string source : {"", " \n// tail\n/* closed */"}) {
        const auto parsed = Parse(source);
        EXPECT_FALSE(parsed.has_syntax_errors());
        EXPECT_FALSE(parsed.has_errors);
        ASSERT_NE(parsed.translation_unit, nullptr);
        ExpectAst(*parsed.translation_unit, "TranslationUnit");
    }
}

TEST(ParserProgramTest, MainIsAnOrdinaryTopLevelVariable) {
    const auto parsed = Parse("int main = 1; main = main + 1; print(main);");
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
|-DeclStmt <1:1>
| `-VarDecl <1:1> main 'int' init
|   `-IntLiteralExpr <1:12> '1'
|-AssignStmt <1:15>
| |-DeclRefExpr <1:15> 'main'
| `-BinaryExpr <1:27> '+'
|   |-DeclRefExpr <1:22> 'main'
|   `-IntLiteralExpr <1:29> '1'
`-PrintStmt <1:32>
  `-DeclRefExpr <1:38> 'main'
)");
}

TEST(ParserProgramTest, TopLevelControlsAndDeclarationsRetainSourceOrder) {
    const auto parsed = Parse(R"(int x = 1 + 2 * 3;
bool flag = x >= 3 == true;
if (flag) { print((x + 1) * 2); } else { x = x + 1; }
do { x = x + 1; } while (x > 10);
for (int i = 0; 3 > i; i = i + 1) { print(i); }
for (x = 1; flag; x = x * 2) { print(x); }
{ int local = 1; print(local); }
print(x);
)");
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
|-DeclStmt <1:1>
| `-VarDecl <1:1> x 'int' init
|   `-BinaryExpr <1:11> '+'
|     |-IntLiteralExpr <1:9> '1'
|     `-BinaryExpr <1:15> '*'
|       |-IntLiteralExpr <1:13> '2'
|       `-IntLiteralExpr <1:17> '3'
|-DeclStmt <2:1>
| `-VarDecl <2:1> flag 'bool' init
|   `-BinaryExpr <2:20> '=='
|     |-BinaryExpr <2:15> '>='
|     | |-DeclRefExpr <2:13> 'x'
|     | `-IntLiteralExpr <2:18> '3'
|     `-BoolLiteralExpr <2:23> true
|-IfStmt <3:1> has_else
| |-DeclRefExpr <3:5> 'flag'
| |-CompoundStmt <3:11>
| | `-PrintStmt <3:13>
| |   `-BinaryExpr <3:27> '*'
| |     |-BinaryExpr <3:22> '+'
| |     | |-DeclRefExpr <3:20> 'x'
| |     | `-IntLiteralExpr <3:24> '1'
| |     `-IntLiteralExpr <3:29> '2'
| `-CompoundStmt <3:40>
|   `-AssignStmt <3:42>
|     |-DeclRefExpr <3:42> 'x'
|     `-BinaryExpr <3:48> '+'
|       |-DeclRefExpr <3:46> 'x'
|       `-IntLiteralExpr <3:50> '1'
|-DoWhileStmt <4:1>
| |-CompoundStmt <4:4>
| | `-AssignStmt <4:6>
| |   |-DeclRefExpr <4:6> 'x'
| |   `-BinaryExpr <4:12> '+'
| |     |-DeclRefExpr <4:10> 'x'
| |     `-IntLiteralExpr <4:14> '1'
| `-BinaryExpr <4:28> '>'
|   |-DeclRefExpr <4:26> 'x'
|   `-IntLiteralExpr <4:30> '10'
|-ForStmt <5:1>
| |-DeclStmt <5:6>
| | `-VarDecl <5:6> i 'int' init
| |   `-IntLiteralExpr <5:14> '0'
| |-BinaryExpr <5:19> '>'
| | |-IntLiteralExpr <5:17> '3'
| | `-DeclRefExpr <5:21> 'i'
| |-AssignStmt <5:24>
| | |-DeclRefExpr <5:24> 'i'
| | `-BinaryExpr <5:30> '+'
| |   |-DeclRefExpr <5:28> 'i'
| |   `-IntLiteralExpr <5:32> '1'
| `-CompoundStmt <5:35>
|   `-PrintStmt <5:37>
|     `-DeclRefExpr <5:43> 'i'
|-ForStmt <6:1>
| |-AssignStmt <6:6>
| | |-DeclRefExpr <6:6> 'x'
| | `-IntLiteralExpr <6:10> '1'
| |-DeclRefExpr <6:13> 'flag'
| |-AssignStmt <6:19>
| | |-DeclRefExpr <6:19> 'x'
| | `-BinaryExpr <6:25> '*'
| |   |-DeclRefExpr <6:23> 'x'
| |   `-IntLiteralExpr <6:27> '2'
| `-CompoundStmt <6:30>
|   `-PrintStmt <6:32>
|     `-DeclRefExpr <6:38> 'x'
|-CompoundStmt <7:1>
| |-DeclStmt <7:3>
| | `-VarDecl <7:3> local 'int' init
| |   `-IntLiteralExpr <7:15> '1'
| `-PrintStmt <7:18>
|   `-DeclRefExpr <7:24> 'local'
`-PrintStmt <8:1>
  `-DeclRefExpr <8:7> 'x'
)");
}

TEST(ParserProgramTest, RepresentativeCompleteProgramUsesEveryControlForm) {
    const auto parsed = Parse(R"({
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
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-DeclStmt <3:5>
  | `-VarDecl <3:5> x 'int' init
  |   `-BinaryExpr <3:15> '+'
  |     |-IntLiteralExpr <3:13> '1'
  |     `-BinaryExpr <3:19> '*'
  |       |-IntLiteralExpr <3:17> '2'
  |       `-IntLiteralExpr <3:21> '3'
  |-DeclStmt <4:5>
  | `-VarDecl <4:5> flag 'bool' init
  |   `-BinaryExpr <4:24> '=='
  |     |-BinaryExpr <4:19> '>='
  |     | |-DeclRefExpr <4:17> 'x'
  |     | `-IntLiteralExpr <4:22> '3'
  |     `-BoolLiteralExpr <4:27> true
  |-IfStmt <7:5> has_else
  | |-DeclRefExpr <7:9> 'flag'
  | |-CompoundStmt <7:15>
  | | `-PrintStmt <7:17>
  | |   `-BinaryExpr <7:31> '*'
  | |     |-BinaryExpr <7:26> '+'
  | |     | |-DeclRefExpr <7:24> 'x'
  | |     | `-IntLiteralExpr <7:28> '1'
  | |     `-IntLiteralExpr <7:33> '2'
  | `-CompoundStmt <7:44>
  |   `-AssignStmt <7:46>
  |     |-DeclRefExpr <7:46> 'x'
  |     `-BinaryExpr <7:52> '+'
  |       |-DeclRefExpr <7:50> 'x'
  |       `-IntLiteralExpr <7:54> '1'
  |-DoWhileStmt <8:5>
  | |-CompoundStmt <8:8>
  | | `-AssignStmt <8:10>
  | |   |-DeclRefExpr <8:10> 'x'
  | |   `-BinaryExpr <8:16> '+'
  | |     |-DeclRefExpr <8:14> 'x'
  | |     `-IntLiteralExpr <8:18> '1'
  | `-BinaryExpr <8:32> '>'
  |   |-DeclRefExpr <8:30> 'x'
  |   `-IntLiteralExpr <8:34> '10'
  |-ForStmt <9:5>
  | |-DeclStmt <9:10>
  | | `-VarDecl <9:10> i 'int' init
  | |   `-IntLiteralExpr <9:18> '0'
  | |-BinaryExpr <9:23> '>'
  | | |-DeclRefExpr <9:21> 'i'
  | | `-IntLiteralExpr <9:25> '3'
  | |-AssignStmt <9:28>
  | | |-DeclRefExpr <9:28> 'i'
  | | `-BinaryExpr <9:34> '+'
  | |   |-DeclRefExpr <9:32> 'i'
  | |   `-IntLiteralExpr <9:36> '1'
  | `-CompoundStmt <9:39>
  |   `-PrintStmt <9:41>
  |     `-DeclRefExpr <9:47> 'i'
  |-ForStmt <10:5>
  | |-AssignStmt <10:10>
  | | |-DeclRefExpr <10:10> 'x'
  | | `-IntLiteralExpr <10:14> '1'
  | |-DeclRefExpr <10:17> 'flag'
  | |-AssignStmt <10:23>
  | | |-DeclRefExpr <10:23> 'x'
  | | `-BinaryExpr <10:29> '*'
  | |   |-DeclRefExpr <10:27> 'x'
  | |   `-IntLiteralExpr <10:31> '2'
  | `-CompoundStmt <10:34>
  |   `-PrintStmt <10:36>
  |     `-DeclRefExpr <10:42> 'x'
  `-CompoundStmt <11:5>
    |-DeclStmt <11:7>
    | `-VarDecl <11:7> unused 'int'
    `-IfStmt <11:19>
      |-BoolLiteralExpr <11:23> false
      `-CompoundStmt <11:30>
)");
}

TEST(ParserProgramTest, DeclarationsAssignmentsAndLiteralDataAreDistinct) {
    const auto parsed =
        ParseBody("int x; bool flag = false; x = 00042; print(flag);");
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-DeclStmt <1:3>
  | `-VarDecl <1:3> x 'int'
  |-DeclStmt <1:10>
  | `-VarDecl <1:10> flag 'bool' init
  |   `-BoolLiteralExpr <1:22> false
  |-AssignStmt <1:29>
  | |-DeclRefExpr <1:29> 'x'
  | `-IntLiteralExpr <1:33> '00042'
  `-PrintStmt <1:40>
    `-DeclRefExpr <1:46> 'flag'
)");

    // Retain direct node assertion to validate fine-grained spelling ownership
    const auto* block = dynamic_cast<const ast::CompoundStmt*>(
        parsed.translation_unit->statements[0].get());
    ASSERT_NE(block, nullptr);
    const auto* assign =
        dynamic_cast<const ast::AssignStmt*>(block->body[2].get());
    ASSERT_NE(assign, nullptr);
    const auto* value =
        dynamic_cast<const ast::IntLiteralExpr*>(assign->value.get());
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->spelling, "00042");
}

TEST(ParserProgramTest, SyntaxParsingDoesNotResolveNamesOrValidateTypes) {
    const auto parsed = ParseBody(
        "x = true; int x; bool x; if (1) { } print(missing + false);"
        "bool flag = 99999999999999999999999999999999999999;");
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
}

TEST(ParserProgramTest, LocationsUseLeadingTokensAndOperatorsAcrossTrivia) {
    const auto parsed = Parse(
        "{\n/* a\nb*/ int x=00042;\n  x = x + 1;\n  { print(false); "
        "}\n}");
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-DeclStmt <3:5>
  | `-VarDecl <3:5> x 'int' init
  |   `-IntLiteralExpr <3:11> '00042'
  |-AssignStmt <4:3>
  | |-DeclRefExpr <4:3> 'x'
  | `-BinaryExpr <4:9> '+'
  |   |-DeclRefExpr <4:7> 'x'
  |   `-IntLiteralExpr <4:11> '1'
  `-CompoundStmt <5:3>
    `-PrintStmt <5:5>
      `-BoolLiteralExpr <5:11> false
)");
}

TEST(ParserProgramTest,
     ControlStatementsUseKeywordLocationsAndAllowBoolForDeclaration) {
    const auto parsed = Parse(
        "{\n"
        "  if (true) { } else { }\n"
        "  do { } while (false);\n"
        "  for (bool flag; true; flag = false) { }\n"
        "}");
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-IfStmt <2:3> has_else
  | |-BoolLiteralExpr <2:7> true
  | |-CompoundStmt <2:13>
  | `-CompoundStmt <2:22>
  |-DoWhileStmt <3:3>
  | |-CompoundStmt <3:6>
  | `-BoolLiteralExpr <3:17> false
  `-ForStmt <4:3>
    |-DeclStmt <4:8>
    | `-VarDecl <4:8> flag 'bool'
    |-BoolLiteralExpr <4:19> true
    |-AssignStmt <4:25>
    | |-DeclRefExpr <4:25> 'flag'
    | `-BoolLiteralExpr <4:32> false
    `-CompoundStmt <4:39>
)");
}

TEST(ParserExpressionTest, PrecedenceGroupingAndBinaryLocations) {
    const auto parsed =
        Parse("{\nint x = 1 +\n  2 * 3;\nprint((1 + 2) * 3);\n}");
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-DeclStmt <2:1>
  | `-VarDecl <2:1> x 'int' init
  |   `-BinaryExpr <2:11> '+'
  |     |-IntLiteralExpr <2:9> '1'
  |     `-BinaryExpr <3:5> '*'
  |       |-IntLiteralExpr <3:3> '2'
  |       `-IntLiteralExpr <3:7> '3'
  `-PrintStmt <4:1>
    `-BinaryExpr <4:15> '*'
      |-BinaryExpr <4:10> '+'
      | |-IntLiteralExpr <4:8> '1'
      | `-IntLiteralExpr <4:12> '2'
      `-IntLiteralExpr <4:17> '3'
)");
}

TEST(ParserExpressionTest, RepeatedArithmeticIsLeftAssociated) {
    {
        const auto parsed = ParseBody("print(a + b + c);");
        EXPECT_FALSE(parsed.has_syntax_errors());
        ASSERT_NE(parsed.translation_unit, nullptr);
        ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  `-PrintStmt <1:3>
    `-BinaryExpr <1:15> '+'
      |-BinaryExpr <1:11> '+'
      | |-DeclRefExpr <1:9> 'a'
      | `-DeclRefExpr <1:13> 'b'
      `-DeclRefExpr <1:17> 'c'
)");
    }
    {
        const auto parsed = ParseBody("print(a * b * c);");
        EXPECT_FALSE(parsed.has_syntax_errors());
        ASSERT_NE(parsed.translation_unit, nullptr);
        ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  `-PrintStmt <1:3>
    `-BinaryExpr <1:15> '*'
      |-BinaryExpr <1:11> '*'
      | |-DeclRefExpr <1:9> 'a'
      | `-DeclRefExpr <1:13> 'b'
      `-DeclRefExpr <1:17> 'c'
)");
    }
}

TEST(ParserExpressionTest, ComparisonBindsMoreTightlyThanEquality) {
    const auto parsed = ParseBody("print(a + b * c >= d == e > f + g);");
    EXPECT_FALSE(parsed.has_syntax_errors());
    EXPECT_FALSE(parsed.has_errors);
    ASSERT_NE(parsed.translation_unit, nullptr);
    ExpectAst(*parsed.translation_unit, R"(
TranslationUnit
`-CompoundStmt <1:1>
  `-PrintStmt <1:3>
    `-BinaryExpr <1:24> '=='
      |-BinaryExpr <1:19> '>='
      | |-BinaryExpr <1:11> '+'
      | | |-DeclRefExpr <1:9> 'a'
      | | `-BinaryExpr <1:15> '*'
      | |   |-DeclRefExpr <1:13> 'b'
      | |   `-DeclRefExpr <1:17> 'c'
      | `-DeclRefExpr <1:22> 'd'
      `-BinaryExpr <1:29> '>'
        |-DeclRefExpr <1:27> 'e'
        `-BinaryExpr <1:33> '+'
          |-DeclRefExpr <1:31> 'f'
          `-DeclRefExpr <1:35> 'g'
)");
}

TEST(ParserExpressionTest, ExplicitGroupingHasIndependentChainRestrictions) {
    for (const std::string expression :
         {"a > b == c > d", "(a > b) > c", "a > (b >= c)", "(a == b) == c",
          "a == (b == c)", "((true))", "false"}) {
        SCOPED_TRACE(expression);
        const auto parsed = ParseBody("print(" + expression + ");");
        EXPECT_FALSE(parsed.has_syntax_errors());
        EXPECT_FALSE(parsed.has_errors);
        ASSERT_NE(parsed.translation_unit, nullptr);
    }
}

TEST(ParserExpressionTest,
     UnparenthesizedComparisonAndEqualityChainsAreRejected) {
    for (const std::string expression :
         {"a > b > c", "a >= b > c", "a > b >= c", "a == b == c",
          "a == b > c > d", "a + b > c + d > e"}) {
        SCOPED_TRACE(expression);
        const auto parsed = ParseBody("print(" + expression + ");");
        EXPECT_TRUE(parsed.has_syntax_errors());
        EXPECT_TRUE(parsed.has_errors);
        EXPECT_FALSE(parsed.syntax_errors.empty());
    }
}

TEST(ParserGrammarTest, AssignmentsRemainStatementsNotExpressions) {
    for (const std::string source :
         {"x = y = 1;", "int y = (x = 1);", "print(x = 1);", "if (x = 1) { }",
          "for (x = 0; x = 1; x = 2) { }"}) {
        SCOPED_TRACE(source);
        const auto parsed = Parse(source);
        EXPECT_TRUE(parsed.has_syntax_errors());
        EXPECT_TRUE(parsed.has_errors);
        ASSERT_NE(parsed.translation_unit, nullptr);
        EXPECT_TRUE(parsed.translation_unit->statements.empty());
    }
}

TEST(ParserGrammarTest, RestrictedNamesAllowDigitSuffixesAndKeywordPrefixes) {
    for (const std::string name : {"answer12", "A", "AbZ009", "print1", "done",
                                   "do1", "main", "integer", "true1"}) {
        SCOPED_TRACE(name);
        const auto parsed = ParseBody(
            std::format("int {0} = 1; {0} = {0} + 1; print({0});", name));
        EXPECT_FALSE(parsed.has_syntax_errors());
        EXPECT_FALSE(parsed.has_errors);
        ASSERT_NE(parsed.translation_unit, nullptr);
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
            EXPECT_EQ(parsed.has_syntax_errors(), !invalid_spelling);
            EXPECT_TRUE(parsed.has_errors);
            EXPECT_FALSE(parsed.all_diagnostics.empty());
            ASSERT_NE(parsed.translation_unit, nullptr);
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
        EXPECT_EQ(parsed.has_syntax_errors(), !invalid_spelling);
        EXPECT_TRUE(parsed.has_errors);
        EXPECT_FALSE(parsed.all_diagnostics.empty());
        ASSERT_NE(parsed.translation_unit, nullptr);
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
        EXPECT_TRUE(parsed.has_syntax_errors());
        EXPECT_FALSE(parsed.syntax_errors.empty());
        ASSERT_NE(parsed.translation_unit, nullptr);
    }
}

TEST(ParserGrammarTest, FunctionDeclarationsAndDefinitionsAreDeferred) {
    for (const std::string function :
         {"int main() { }", "int other() { }", "bool other() { }",
          "void main() { }", "int main(int x) { }", "int main()", "int main();",
          "int other();"}) {
        for (const auto& source : {function, "{ " + function + " }"}) {
            SCOPED_TRACE(source);
            const auto parsed = Parse(source);
            EXPECT_TRUE(parsed.has_syntax_errors());
            EXPECT_TRUE(parsed.has_errors);
            ASSERT_NE(parsed.translation_unit, nullptr);
        }
    }
}

TEST(ParserLifecycleTest, RepeatedCallsResetCursorAndKeepSharedErrorState) {
    const std::string       source = "{ print(1); }";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);
    parser::Parser          parser{source, tokens, diagnostics};
    const auto&             original_tokens = tokens;
    const auto              first           = parser.ParseTranslationUnit();

    diagnostics.Report({.loc      = {.line = 1, .col = 1},
                        .severity = common::Severity::kError,
                        .message  = "earlier lexical error"});
    const auto second = parser.ParseTranslationUnit();

    EXPECT_TRUE(diagnostics.HasErrors());
    EXPECT_NE(first.get(), second.get());
    ExpectAst(*first, R"(
TranslationUnit
`-CompoundStmt <1:1>
  `-PrintStmt <1:3>
    `-IntLiteralExpr <1:9> '1'
)");
    ExpectAst(*second, R"(
TranslationUnit
`-CompoundStmt <1:1>
  `-PrintStmt <1:3>
    `-IntLiteralExpr <1:9> '1'
)");
    ASSERT_EQ(diagnostics.Collected().size(), 1U);
    EXPECT_EQ(diagnostics.Collected()[0].message, "earlier lexical error");

    ASSERT_EQ(tokens.size(), original_tokens.size());
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        EXPECT_EQ(tokens[index].kind, original_tokens[index].kind);
        EXPECT_EQ(tokens[index].lexeme, original_tokens[index].lexeme);
        EXPECT_EQ(tokens[index].loc.line, original_tokens[index].loc.line);
        EXPECT_EQ(tokens[index].loc.col, original_tokens[index].loc.col);
    }
    EXPECT_EQ(source, "{ print(1); }");
}

TEST(ParserLifecycleTest, PriorWarningDoesNotBecomeLocalSyntaxError) {
    const std::string       source = "{ }";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);
    diagnostics.Report({.loc      = {.line = 1, .col = 1},
                        .severity = common::Severity::kWarning,
                        .message  = "earlier warning"});
    parser::Parser parser{source, tokens, diagnostics};
    const auto     result = parser.ParseTranslationUnit();
    EXPECT_FALSE(diagnostics.HasErrors());
    ExpectAst(*result, R"(
TranslationUnit
`-CompoundStmt <1:1>
)");
}

TEST(ParserLifecycleTest,
     RepeatedLexicalFailuresDoNotRediagnoseOrSetSyntaxStatus) {
    const std::string       source = "{ int a1b; print(9); }";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);
    ASSERT_TRUE(diagnostics.HasErrors());
    ASSERT_EQ(diagnostics.Collected().size(), 1U);
    EXPECT_EQ(diagnostics.Collected()[0].message, "invalid identifier: 'a1b'");

    parser::Parser parser{source, tokens, diagnostics};
    const auto     first  = parser.ParseTranslationUnit();
    const auto     second = parser.ParseTranslationUnit();

    // Syntax pass didn't add duplicate diagnostics
    EXPECT_EQ(diagnostics.Collected().size(), 1U);
    EXPECT_NE(first.get(), second.get());
    ExpectAst(*first, R"(
TranslationUnit
`-CompoundStmt <1:1>
  `-PrintStmt <1:12>
    `-IntLiteralExpr <1:18> '9'
)");
    ExpectAst(*second, R"(
TranslationUnit
`-CompoundStmt <1:1>
  `-PrintStmt <1:12>
    `-IntLiteralExpr <1:18> '9'
)");
}

TEST(ParserLifecycleTest, TreesOwnSpellingsAfterAllBorrowedInputsAreDestroyed) {
    std::unique_ptr<ast::TranslationUnit> result;
    {
        const std::string source = "{ int answer12 = 00042; print(answer12); }";
        common::DiagnosisEngine diagnostics;
        lexer::Lexer            lexer{diagnostics};
        const auto              tokens = lexer.Tokenize(source);
        parser::Parser          parser{source, tokens, diagnostics};
        result = parser.ParseTranslationUnit();
        EXPECT_FALSE(diagnostics.HasErrors());
    }
    ASSERT_NE(result, nullptr);
    ExpectAst(*result, R"(
TranslationUnit
`-CompoundStmt <1:1>
  |-DeclStmt <1:3>
  | `-VarDecl <1:3> answer12 'int' init
  |   `-IntLiteralExpr <1:18> '00042'
  `-PrintStmt <1:25>
    `-DeclRefExpr <1:31> 'answer12'
)");
}
