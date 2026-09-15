#include <gtest/gtest.h>

#include "frontend/lexer/scanner.h"
#include "frontend/lexer/token.h"

class LexerTest : public testing::Test {
protected:
    yyscan_t        scanner = nullptr;
    YY_BUFFER_STATE buffer  = nullptr;

    void SetUp() override {
        ASSERT_EQ(yylex_init_extra(LexerExtra{}, &scanner), 0);
    }
    void TearDown() override {
        if (buffer != nullptr) yy_delete_buffer(buffer, scanner);
        if (scanner != nullptr) yylex_destroy(scanner);
    }
    void  scan(const char* input) { buffer = yy_scan_string(input, scanner); }
    Token next() { return static_cast<Token>(yylex(scanner)); }
};

TEST_F(LexerTest, EmptyInputEndsImmediately) {
    scan("");
    EXPECT_EQ(next(), Token::eof);
}

TEST_F(LexerTest, ScansDeclarationAndSkipsComments) {
    scan("/* comment */ int answer = 42; // trailing comment\n");
    EXPECT_EQ(next(), Token::kw_int);
    EXPECT_EQ(next(), Token::identifier);
    EXPECT_STREQ(yyget_text(scanner), "answer");
    EXPECT_EQ(next(), Token::equal);
    EXPECT_EQ(next(), Token::numeric_constant);
    EXPECT_EQ(next(), Token::semi);
    EXPECT_EQ(next(), Token::eof);
}

TEST_F(LexerTest, DistinguishesOperatorsAndUnknownCharacters) {
    scan("== = ++ + @");
    EXPECT_EQ(next(), Token::equalequal);
    EXPECT_EQ(next(), Token::equal);
    EXPECT_EQ(next(), Token::plusplus);
    EXPECT_EQ(next(), Token::plus);
    EXPECT_EQ(next(), Token::unknown);
    EXPECT_EQ(next(), Token::eof);
}
