#include "parser/expr_parser.h"

#include <gtest/gtest.h>

#include "lexer/lexer.h"

TEST(ExprParserTest, ExpressionsLeaveEveryCallerBoundaryIntact) {
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

        parser::Cursor     cursor{source, tokens, diagnostics};
        parser::ExprParser expr_parser{cursor};

        const auto expression = expr_parser.ParseExpression();
        ASSERT_NE(expression, nullptr);
        EXPECT_EQ(cursor.PeekKind(), separator);
        EXPECT_FALSE(diagnostics.HasErrors());
    }
}

TEST(ExprParserTest, PrattMinimumBindingPowerLeavesWeakerOperatorForCaller) {
    const std::string       source = "a * b + c;";
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);

    parser::Cursor     cursor{source, tokens, diagnostics};
    parser::ExprParser expr_parser{cursor};

    const auto expression = expr_parser.ParseExpression(6);
    ASSERT_NE(expression, nullptr);
    const auto* multiplication =
        dynamic_cast<const ast::BinaryExpr*>(expression.get());
    ASSERT_NE(multiplication, nullptr);
    EXPECT_EQ(multiplication->op, ast::BinaryOp::Mul);
    EXPECT_EQ(cursor.PeekKind(), lexer::TokenKind::plus);
    EXPECT_FALSE(diagnostics.HasErrors());
}

TEST(ExprParserTest, DiagnosedExpressionFailuresLeaveCallerBoundariesIntact) {
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

        parser::Cursor     cursor{source, tokens, diagnostics};
        parser::ExprParser expr_parser{cursor};

        const auto expression = expr_parser.ParseExpression();
        EXPECT_EQ(expression, nullptr);
        EXPECT_TRUE(diagnostics.HasErrors());
        EXPECT_EQ(cursor.PeekKind(), separator);
        ASSERT_EQ(diagnostics.Collected().size(), 1U);
        EXPECT_EQ(diagnostics.Collected()[0].severity,
                  common::Severity::kError);
    }
}
