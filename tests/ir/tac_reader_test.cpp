#include "ir/tac_reader.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace ir {
namespace {
TEST(TacReader, ReadsTheDocumentedProgram) {
    constexpr auto source = R"(# int x = 1 + 2 * 3;
t0 = mul 2 3
t1 = add 1 t0
x.1 = t1

# bool flag = x >= 7 == true;
t2 = ge x.1 7
t3 = eq t2 1
flag.2 = t3

# if (flag) { int x = 10; print(x); } else { print(0); }
ifFalse flag.2 goto L0
x.3 = 10
print x.3
goto L1
L0:
print 0
L1:

# for (int i = 0; 2 > i; i = i + 1) { ... }
i.4 = 0
L2:
t4 = gt 2 i.4
ifFalse t4 goto L3
L4:
t5 = add x.1 1
x.1 = t5
t6 = eq x.1 8
ifTrue t6 goto L4
t7 = add i.4 1
i.4 = t7
goto L2
L3:

# print(x);
print x.1
)";
    const Var      x{"x", 1};
    const Var      flag{"flag", 2};
    const Var      inner_x{"x", 3};
    const Var      i{"i", 4};
    const Program  expected{{
        BinOpInst{Temp{0}, BinOp::Mul, Int{2}, Int{3}},
        BinOpInst{Temp{1}, BinOp::Add, Int{1}, Temp{0}},
        CopyInst{x, Temp{1}},
        BinOpInst{Temp{2}, BinOp::Ge, x, Int{7}},
        BinOpInst{Temp{3}, BinOp::Eq, Temp{2}, Int{1}},
        CopyInst{flag, Temp{3}},
        CondGotoInst{BranchCond::IfFalse, flag, Label{0}},
        CopyInst{inner_x, Int{10}},
        PrintInst{inner_x},
        GotoInst{Label{1}},
        LabelInst{Label{0}},
        PrintInst{Int{0}},
        LabelInst{Label{1}},
        CopyInst{i, Int{0}},
        LabelInst{Label{2}},
        BinOpInst{Temp{4}, BinOp::Gt, Int{2}, i},
        CondGotoInst{BranchCond::IfFalse, Temp{4}, Label{3}},
        LabelInst{Label{4}},
        BinOpInst{Temp{5}, BinOp::Add, x, Int{1}},
        CopyInst{x, Temp{5}},
        BinOpInst{Temp{6}, BinOp::Eq, x, Int{8}},
        CondGotoInst{BranchCond::IfTrue, Temp{6}, Label{4}},
        BinOpInst{Temp{7}, BinOp::Add, i, Int{1}},
        CopyInst{i, Temp{7}},
        GotoInst{Label{2}},
        LabelInst{Label{3}},
        PrintInst{x},
    }};

    common::DiagnosisEngine diag;
    const auto              program = ReadTac(source, diag);
    ASSERT_TRUE(program);
    EXPECT_EQ(*program, expected);
    EXPECT_TRUE(diag.Collected().empty());
}

TEST(TacReader, ReadsCrLfCommentsAndTrailingLabelWithoutNewline) {
    constexpr auto source =
        " \t# comment\r\n\r\n\tt0.1 \t=\t -2147483648# inline\r\n"
        "add.2 = 2147483647\r\n"
        "t0 = add t0.1 add.2\r\n"
        "goto L4294967295\r\n"
        "L4294967295:";
    common::DiagnosisEngine diag;
    const auto              program = ReadTac(source, diag);
    ASSERT_TRUE(program);

    constexpr auto max_id = std::numeric_limits<std::uint32_t>::max();
    const Program  expected{{
        CopyInst{Var{"t0", 1}, Int{std::numeric_limits<std::int32_t>::min()}},
        CopyInst{Var{"add", 2}, Int{std::numeric_limits<std::int32_t>::max()}},
        BinOpInst{Temp{0}, BinOp::Add, Var{"t0", 1}, Var{"add", 2}},
        GotoInst{Label{max_id}},
        LabelInst{Label{max_id}},
    }};
    EXPECT_EQ(*program, expected);
    EXPECT_TRUE(diag.Collected().empty());
}

TEST(TacReader, AcceptsEmptyAndTriviaOnlyPrograms) {
    common::DiagnosisEngine diag;
    for (const auto source : {"", "\t \r\n\n# no instructions"}) {
        const auto program = ReadTac(source, diag);
        ASSERT_TRUE(program);
        EXPECT_TRUE(program->instructions.empty());
    }
    EXPECT_TRUE(diag.Collected().empty());
}

TEST(TacReader, ReportsOutOfRangeIntegersAcrossLines) {
    common::DiagnosisEngine diag;
    EXPECT_FALSE(
        ReadTac("print 2147483648\r\n\tprint -2147483649\n"
                "t1 = add 1 999999999999999999999999999999",
                diag));
    const auto errors = diag.Collected();
    ASSERT_EQ(errors.size(), 3U);
    EXPECT_EQ(errors[0].loc.line, 1);
    EXPECT_EQ(errors[0].loc.col, 7);
    EXPECT_EQ(errors[1].loc.line, 2);
    EXPECT_EQ(errors[1].loc.col, 8);
    EXPECT_EQ(errors[2].loc.line, 3);
    EXPECT_EQ(errors[2].loc.col, 12);
    for (const auto& error : errors) {
        EXPECT_EQ(error.severity, common::Severity::kError);
        EXPECT_NE(error.message.find("int32"), std::string::npos);
    }
}

TEST(TacReader, RejectsMalformedTokensAndInstructionShapes) {
    struct TestCase {
        std::string_view source;
        int              column;
    };
    constexpr TestCase cases[]{
        {"t0= 1", 1},
        {"t0 =1", 4},
        {"t0 = add 1", 11},
        {"t0 = add 1 2 print 2", 14},
        {"t0 = ADD 1 2", 6},
        {"L0 :", 1},
        {"L0:print 1", 1},
        {"42 = 1", 1},
        {"print t0..1", 7},
        {"print a1b.2", 7},
        {"print +1", 7},
        {"print 1\r", 7},
        {"ifTrue 1 L0", 10},
        {"print t4294967296", 7},
        {"print x.4294967296", 7},
        {"goto L4294967296", 6},
    };
    for (const auto& [source, column] : cases) {
        SCOPED_TRACE(source);
        common::DiagnosisEngine diag;
        EXPECT_FALSE(ReadTac(source, diag));
        ASSERT_EQ(diag.Collected().size(), 1U);
        EXPECT_EQ(diag.Collected()[0].loc.line, 1);
        EXPECT_EQ(diag.Collected()[0].loc.col, column);
    }
}

TEST(TacReader, ReportsDuplicateLabelAndItsFirstDefinition) {
    common::DiagnosisEngine diag;
    EXPECT_FALSE(ReadTac(" L1:\n\n  L01:", diag));
    const auto errors = diag.Collected();
    ASSERT_EQ(errors.size(), 1U);
    EXPECT_EQ(errors[0].loc.line, 3);
    EXPECT_EQ(errors[0].loc.col, 3);
    EXPECT_NE(errors[0].message.find("duplicate TAC label 'L01'"), std::string::npos);
    ASSERT_EQ(errors[0].notes.size(), 1U);
    EXPECT_EQ(errors[0].notes[0].loc.line, 1);
    EXPECT_EQ(errors[0].notes[0].loc.col, 2);
}

TEST(TacReader, ReportsUndefinedTargetsAtEachBranchOperand) {
    common::DiagnosisEngine diag;
    EXPECT_FALSE(ReadTac("goto L12\n\tifFalse 0 goto L2\nifTrue 1 goto L3", diag));
    const auto errors = diag.Collected();
    ASSERT_EQ(errors.size(), 3U);
    EXPECT_EQ(errors[0].loc.line, 1);
    EXPECT_EQ(errors[0].loc.col, 6);
    EXPECT_EQ(errors[1].loc.line, 2);
    EXPECT_EQ(errors[1].loc.col, 17);
    EXPECT_EQ(errors[2].loc.line, 3);
    EXPECT_EQ(errors[2].loc.col, 15);
    for (const auto& error : errors) {
        EXPECT_NE(error.message.find("undefined TAC label"), std::string::npos);
    }
}

TEST(TacReader, ReusesDiagnosticEngineAcrossReads) {
    common::DiagnosisEngine diag;
    EXPECT_FALSE(ReadTac("goto L0", diag));
    ASSERT_EQ(diag.Collected().size(), 1U);

    const auto program = ReadTac("goto L0\nL0:", diag);
    ASSERT_TRUE(program);
    EXPECT_EQ(program->instructions.size(), 2U);
    EXPECT_EQ(diag.Collected().size(), 1U);
}
}  // namespace
}  // namespace ir
