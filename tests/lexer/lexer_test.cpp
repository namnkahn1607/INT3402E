#include "lexer/lexer.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "common/diagnostic.h"

class LexerTest : public testing::Test {
protected:
    common::DiagnosisEngine diag;
    lexer::Lexer            scanner{diag};
};

TEST_F(LexerTest, EmptyInputSucceeds) {
    EXPECT_TRUE(scanner.Tokenize("").empty());
    EXPECT_FALSE(diag.HasErrors());
}

TEST_F(LexerTest, ScansDeclarationAndSkipsComments) {
    const auto tokens =
        scanner.Tokenize("/* comment */ int answer = 42; // end\n");
    ASSERT_EQ(tokens.size(), 5U);
    EXPECT_EQ(tokens[0].kind, lexer::TokenKind::kw_int);
    EXPECT_EQ(tokens[1].kind, lexer::TokenKind::identifier);
    EXPECT_EQ(tokens[1].lexeme, "answer");
    EXPECT_EQ(tokens[2].kind, lexer::TokenKind::equal);
    EXPECT_EQ(tokens[3].kind, lexer::TokenKind::numeric_constant);
    EXPECT_EQ(tokens[3].lexeme, "42");
    EXPECT_EQ(tokens[4].kind, lexer::TokenKind::semi);
    EXPECT_FALSE(diag.HasErrors());
}

TEST_F(LexerTest, DistinguishesOperators) {
    const auto tokens = scanner.Tokenize("== = ++ +");
    ASSERT_EQ(tokens.size(), 4U);
    EXPECT_EQ(tokens[0].kind, lexer::TokenKind::equalequal);
    EXPECT_EQ(tokens[1].kind, lexer::TokenKind::equal);
    EXPECT_EQ(tokens[2].kind, lexer::TokenKind::plusplus);
    EXPECT_EQ(tokens[3].kind, lexer::TokenKind::plus);
    EXPECT_FALSE(diag.HasErrors());
}

TEST_F(LexerTest, ReportsUnknownCharacters) {
    const auto tokens = scanner.Tokenize("int @");
    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0].kind, lexer::TokenKind::kw_int);
    EXPECT_EQ(tokens[1].kind, lexer::TokenKind::unknown);
    EXPECT_TRUE(diag.HasErrors());
}

TEST_F(LexerTest, TracksPositionsAcrossMultilineComments) {
    const auto tokens = scanner.Tokenize("int\n/* a\nb */ ans");
    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0].loc.line, 1);
    EXPECT_EQ(tokens[0].loc.col, 1);
    EXPECT_EQ(tokens[1].loc.line, 3);
    EXPECT_EQ(tokens[1].loc.col, 6);
    EXPECT_FALSE(diag.HasErrors());
}

TEST_F(LexerTest, ResetsStateForEachInput) {
    ASSERT_EQ(scanner.Tokenize("\nint").size(), 1U);
    const auto tokens = scanner.Tokenize("ans");
    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0].loc.line, 1);
    EXPECT_EQ(tokens[0].loc.col, 1);
    EXPECT_FALSE(diag.HasErrors());
}

TEST_F(LexerTest, LexesIntegerConstants) {
    const auto tokens = scanner.Tokenize("42");
    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0].kind, lexer::TokenKind::numeric_constant);
    EXPECT_EQ(tokens[0].lexeme, "42");
    EXPECT_FALSE(diag.HasErrors());
}

TEST_F(LexerTest, RetainsAndDiagnosesInvalidNumericCandidates) {
    for (const std::string src :
         {"3.14", ".5", "3.", "1e10", "1E-5", "3.14f", "1f", "42F", ".5L",
          "3.l", "3.14e+2F", "1e3L"}) {
        SCOPED_TRACE(src);
        testing::internal::CaptureStderr();
        const auto tokens  = scanner.Tokenize(src);
        const auto reports = testing::internal::GetCapturedStderr();
        ASSERT_EQ(tokens.size(), 1U);
        EXPECT_EQ(tokens[0].kind, lexer::TokenKind::invalid_numeric_constant);
        EXPECT_EQ(tokens[0].lexeme, src);
        EXPECT_EQ(tokens[0].loc.line, 1);
        EXPECT_EQ(tokens[0].loc.col, 1);
        EXPECT_TRUE(diag.HasErrors());
        EXPECT_EQ(reports,
                  "1:1: error: invalid numeric constant: '" + src + "'\n");
    }
}

TEST_F(LexerTest, KeywordsRequireExactWholeSpellings) {
    const std::vector<std::string> spellings{
        "do",      "print", "do1",  "done",     "print1", "printed",
        "integer", "true1", "main", "answer12", "AbZ009"};
    const auto tokens = scanner.Tokenize(
        "do print do1 done print1 printed integer true1 main answer12 AbZ009");
    ASSERT_EQ(tokens.size(), spellings.size());
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        EXPECT_EQ(tokens[index].lexeme, spellings[index]);
        const auto expected = index == 0   ? lexer::TokenKind::kw_do
                              : index == 1 ? lexer::TokenKind::kw_print
                                           : lexer::TokenKind::identifier;
        EXPECT_EQ(tokens[index].kind, expected);
    }
    EXPECT_FALSE(diag.HasErrors());
}

TEST_F(LexerTest, RetainsAndDiagnosesWholeInvalidIdentifiers) {
    for (const std::string spelling :
         {"a1b", "_x", "a_b", "_", "print_", "do1again", "A1B2", "x12y34"}) {
        SCOPED_TRACE(spelling);
        testing::internal::CaptureStderr();
        const auto tokens  = scanner.Tokenize(spelling + "; answer12");
        const auto reports = testing::internal::GetCapturedStderr();
        ASSERT_EQ(tokens.size(), 3U);
        EXPECT_EQ(tokens[0].kind, lexer::TokenKind::invalid_identifier);
        EXPECT_EQ(tokens[0].lexeme, spelling);
        EXPECT_EQ(tokens[0].loc.line, 1);
        EXPECT_EQ(tokens[0].loc.col, 1);
        EXPECT_EQ(tokens[1].kind, lexer::TokenKind::semi);
        EXPECT_EQ(tokens[2].kind, lexer::TokenKind::identifier);
        EXPECT_EQ(tokens[2].lexeme, "answer12");
        EXPECT_TRUE(diag.HasErrors());
        EXPECT_EQ(reports,
                  "1:1: error: invalid identifier: '" + spelling + "'\n");
    }
}

TEST_F(LexerTest, InvalidDiagnosticsAndTokensShareLocationsAcrossTrivia) {
    testing::internal::CaptureStderr();
    const auto tokens = scanner.Tokenize(
        "/* first\nsecond */\n  _x 3.14 // skip a1b 1e3\nanswer12\n\ta1b 1e3");
    const auto reports = testing::internal::GetCapturedStderr();
    ASSERT_EQ(tokens.size(), 5U);
    const std::vector<std::pair<int, int>> positions{
        {3, 3}, {3, 6}, {4, 1}, {5, 2}, {5, 6}};
    const std::vector<lexer::TokenKind> kinds{
        lexer::TokenKind::invalid_identifier,
        lexer::TokenKind::invalid_numeric_constant,
        lexer::TokenKind::identifier, lexer::TokenKind::invalid_identifier,
        lexer::TokenKind::invalid_numeric_constant};
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        EXPECT_EQ(tokens[index].kind, kinds[index]);
        EXPECT_EQ(tokens[index].loc.line, positions[index].first);
        EXPECT_EQ(tokens[index].loc.col, positions[index].second);
    }
    EXPECT_EQ(reports,
              "3:3: error: invalid identifier: '_x'\n"
              "3:6: error: invalid numeric constant: '3.14'\n"
              "5:2: error: invalid identifier: 'a1b'\n"
              "5:6: error: invalid numeric constant: '1e3'\n");
    EXPECT_TRUE(diag.HasErrors());
}

TEST_F(LexerTest, IntegerSpellingsArePreservedWithoutRangeChecking) {
    const std::vector<std::string> spellings{
        "0", "00042", "99999999999999999999999999999999999999"};
    const auto tokens =
        scanner.Tokenize("0 00042 99999999999999999999999999999999999999");
    ASSERT_EQ(tokens.size(), spellings.size());
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        EXPECT_EQ(tokens[index].kind, lexer::TokenKind::numeric_constant);
        EXPECT_EQ(tokens[index].lexeme, spellings[index]);
    }
    EXPECT_FALSE(diag.HasErrors());
}

TEST_F(LexerTest, BroaderReservedVocabularyAndOperatorsRemainRecognizable) {
    using lexer::TokenKind;
    const auto tokens = scanner.Tokenize(
        "bool break case const continue default double else false float for if "
        "int long return short switch true void while "
        "( ) { } ; : == = ! ++ + -- - * / <= < >= > && ||");
    const std::vector<TokenKind> kinds{
        TokenKind::kw_bool,   TokenKind::kw_break,     TokenKind::kw_case,
        TokenKind::kw_const,  TokenKind::kw_continue,  TokenKind::kw_default,
        TokenKind::kw_double, TokenKind::kw_else,      TokenKind::kw_false,
        TokenKind::kw_float,  TokenKind::kw_for,       TokenKind::kw_if,
        TokenKind::kw_int,    TokenKind::kw_long,      TokenKind::kw_return,
        TokenKind::kw_short,  TokenKind::kw_switch,    TokenKind::kw_true,
        TokenKind::kw_void,   TokenKind::kw_while,     TokenKind::l_paren,
        TokenKind::r_paren,   TokenKind::l_brace,      TokenKind::r_brace,
        TokenKind::semi,      TokenKind::colon,        TokenKind::equalequal,
        TokenKind::equal,     TokenKind::exclaim,      TokenKind::plusplus,
        TokenKind::plus,      TokenKind::minusminus,   TokenKind::minus,
        TokenKind::star,      TokenKind::slash,        TokenKind::lessequal,
        TokenKind::less,      TokenKind::greaterequal, TokenKind::greater,
        TokenKind::ampamp,    TokenKind::pipepipe};
    ASSERT_EQ(tokens.size(), kinds.size());
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        EXPECT_EQ(tokens[index].kind, kinds[index]);
    }
    EXPECT_FALSE(diag.HasErrors());
}
