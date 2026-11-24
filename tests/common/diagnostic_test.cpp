#include "common/diagnostic.h"

#include <gtest/gtest.h>

#include <sstream>

TEST(DiagnosisEngineTest, CollectsBothWarningsAndErrorsInOrder) {
    common::DiagnosisEngine diag;
    diag.Record({.loc      = {.line = 2, .col = 5},
                 .severity = common::Severity::kWarning,
                 .message  = "unused variable 'x'"});
    EXPECT_FALSE(diag.HasErrors());
    ASSERT_EQ(diag.Collected().size(), 1U);
    EXPECT_EQ(diag.Collected()[0].severity, common::Severity::kWarning);
    EXPECT_EQ(diag.Collected()[0].loc.line, 2);
    EXPECT_EQ(diag.Collected()[0].loc.col, 5);
    EXPECT_EQ(diag.Collected()[0].message, "unused variable 'x'");

    diag.Record({.loc      = {.line = 4, .col = 3},
                 .severity = common::Severity::kError,
                 .message  = "undeclared identifier 'y'"});
    EXPECT_TRUE(diag.HasErrors());
    ASSERT_EQ(diag.Collected().size(), 2U);
    EXPECT_EQ(diag.Collected()[0].severity, common::Severity::kWarning);
    EXPECT_EQ(diag.Collected()[1].severity, common::Severity::kError);
    EXPECT_EQ(diag.Collected()[1].loc.line, 4);
    EXPECT_EQ(diag.Collected()[1].loc.col, 3);
    EXPECT_EQ(diag.Collected()[1].message, "undeclared identifier 'y'");
}

TEST(DiagnosisEngineTest, ReportDoesNotWriteToStderrAndPrintRendersAll) {
    common::DiagnosisEngine diag;
    testing::internal::CaptureStderr();
    diag.Record({.loc      = {.line = 1, .col = 2},
                 .severity = common::Severity::kWarning,
                 .message  = "implicit conversion"});
    diag.Record({.loc      = {.line = 3, .col = 4},
                 .severity = common::Severity::kError,
                 .message  = "type mismatch"});
    EXPECT_TRUE(testing::internal::GetCapturedStderr().empty());

    std::ostringstream out;
    diag.Print(out);
    EXPECT_EQ(out.str(),
              "1:2: warning: implicit conversion\n"
              "3:4: error: type mismatch\n");
}

TEST(DiagnosisEngineTest, SummarizeFormatsAllCountCombinations) {
    {
        common::DiagnosisEngine diag;
        std::ostringstream      out;
        diag.Summarize(out);
        EXPECT_EQ(out.str(), "");
    }
    {
        common::DiagnosisEngine diag;
        diag.Record({.loc      = {.line = 1, .col = 1},
                     .severity = common::Severity::kWarning,
                     .message  = "w1"});
        diag.Record({.loc      = {.line = 2, .col = 1},
                     .severity = common::Severity::kWarning,
                     .message  = "w2"});
        std::ostringstream out;
        diag.Summarize(out);
        EXPECT_EQ(out.str(), "2 warning(s) generated\n");
    }
    {
        common::DiagnosisEngine diag;
        diag.Record({.loc      = {.line = 1, .col = 1},
                     .severity = common::Severity::kError,
                     .message  = "e1"});
        std::ostringstream out;
        diag.Summarize(out);
        EXPECT_EQ(out.str(), "1 error(s) found, compilation failed\n");
    }
    {
        common::DiagnosisEngine diag;
        diag.Record({.loc      = {.line = 1, .col = 1},
                     .severity = common::Severity::kWarning,
                     .message  = "w1"});
        diag.Record({.loc      = {.line = 2, .col = 1},
                     .severity = common::Severity::kError,
                     .message  = "e1"});
        diag.Record({.loc      = {.line = 3, .col = 1},
                     .severity = common::Severity::kError,
                     .message  = "e2"});
        std::ostringstream out;
        diag.Summarize(out);
        EXPECT_EQ(
            out.str(),
            "1 warning(s) generated, 2 error(s) found, compilation failed\n");
    }
}
