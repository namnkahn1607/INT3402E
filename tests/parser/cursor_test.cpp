#include "parser/cursor.h"

#include <gtest/gtest.h>

#include "common/diagnostic.h"
#include "lexer/lexer.h"
#include "parser_test_helper.h"

using namespace parser::test;

TEST(CursorTest, VirtualEofIsObservableButNeverConsumable) {
    const std::string       source = "{ }\n// tail\n";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);
    ASSERT_FALSE(tokens.empty());

    parser::Cursor cursor{source, tokens, diagnostics};
    EXPECT_FALSE(cursor.AtEnd());
    ASSERT_EQ(cursor.Consume(), &tokens[0]);

    const auto before  = cursor.CurrentIndex();
    const auto maximum = std::numeric_limits<std::size_t>::max();
    EXPECT_EQ(cursor.PeekToken(maximum), nullptr);
    EXPECT_EQ(cursor.PeekKind(maximum), lexer::TokenKind::eof);
    EXPECT_EQ(cursor.CurrentIndex(), before);

    while (!cursor.AtEnd()) {
        ASSERT_NE(cursor.Consume(), nullptr);
    }
    const auto end = cursor.CurrentIndex();
    for (int repetition = 0; repetition < 4; ++repetition) {
        EXPECT_EQ(cursor.Consume(), nullptr);
        EXPECT_FALSE(cursor.Match(lexer::TokenKind::eof));
        EXPECT_EQ(cursor.PeekToken(), nullptr);
        EXPECT_EQ(cursor.PeekKind(), lexer::TokenKind::eof);
        EXPECT_EQ(cursor.CurrentIndex(), end);
    }
    ExpectLocation(cursor.CurrentLocation(), 3, 1);
}

TEST(CursorTest, EmptySequenceStartsAtOneBasedEof) {
    const std::string               source;
    const std::vector<lexer::Token> tokens;
    common::DiagnosisEngine         diagnostics;

    parser::Cursor cursor{source, tokens, diagnostics};
    EXPECT_TRUE(cursor.AtEnd());
    EXPECT_EQ(cursor.Consume(), nullptr);
    EXPECT_FALSE(cursor.Match(lexer::TokenKind::eof));
    ExpectLocation(cursor.CurrentLocation(), 1, 1);

    parser::Parser parser{source, tokens, diagnostics};
    const auto     unit = parser.ParseTranslationUnit();
    EXPECT_FALSE(diagnostics.HasErrors());
    ASSERT_NE(unit, nullptr);
    EXPECT_TRUE(unit->statements.empty());
}
