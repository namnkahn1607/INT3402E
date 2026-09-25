#include "lexer/lexer.h"

#include <gtest/gtest.h>

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
}

TEST_F(LexerTest, ReportsUnknownCharacters) {
    EXPECT_TRUE(scanner.Tokenize("int @").empty());
    EXPECT_TRUE(diag.HasErrors());
}

TEST_F(LexerTest, TracksPositionsAcrossMultilineComments) {
    const auto tokens = scanner.Tokenize("int\n/* a\nb */ ans");
    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].col, 1);
    EXPECT_EQ(tokens[1].line, 3);
    EXPECT_EQ(tokens[1].col, 6);
}

TEST_F(LexerTest, ResetsStateForEachInput) {
    ASSERT_EQ(scanner.Tokenize("\nint").size(), 1U);
    const auto tokens = scanner.Tokenize("ans");
    ASSERT_EQ(tokens.size(), 1U);
    EXPECT_EQ(tokens[0].line, 1);
    EXPECT_EQ(tokens[0].col, 1);
}
