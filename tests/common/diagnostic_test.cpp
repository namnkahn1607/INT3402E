#include "common/diagnostic.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>
#include <vector>

TEST(DiagnosisEngineTest, CollectsBothWarningsAndErrorsInOrder) {
    common::DiagnosisEngine diag;
    diag.Record({.loc      = {.line = 2, .col = 5},
                 .severity = common::Severity::kWarning,
                 .message  = "unused variable 'x'",
                 .notes{}});
    EXPECT_FALSE(diag.HasErrors());
    ASSERT_EQ(diag.Collected().size(), 1U);
    EXPECT_EQ(diag.Collected()[0].severity, common::Severity::kWarning);
    EXPECT_EQ(diag.Collected()[0].loc.line, 2);
    EXPECT_EQ(diag.Collected()[0].loc.col, 5);
    EXPECT_EQ(diag.Collected()[0].message, "unused variable 'x'");

    diag.Record({.loc      = {.line = 4, .col = 3},
                 .severity = common::Severity::kError,
                 .message  = "undeclared identifier 'y'",
                 .notes{}});
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
                 .message  = "implicit conversion",
                 .notes{}});
    diag.Record({.loc      = {.line = 3, .col = 4},
                 .severity = common::Severity::kError,
                 .message  = "type mismatch",
                 .notes{}});
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
                     .message  = "w1",
                     .notes{}});
        diag.Record({.loc      = {.line = 2, .col = 1},
                     .severity = common::Severity::kWarning,
                     .message  = "w2",
                     .notes{}});
        std::ostringstream out;
        diag.Summarize(out);
        EXPECT_EQ(out.str(), "2 warning(s) generated\n");
    }
    {
        common::DiagnosisEngine diag;
        diag.Record({.loc      = {.line = 1, .col = 1},
                     .severity = common::Severity::kError,
                     .message  = "e1",
                     .notes{}});
        std::ostringstream out;
        diag.Summarize(out);
        EXPECT_EQ(out.str(), "1 error(s) found, compilation failed\n");
    }
    {
        common::DiagnosisEngine diag;
        diag.Record({.loc      = {.line = 1, .col = 1},
                     .severity = common::Severity::kWarning,
                     .message  = "w1",
                     .notes{}});
        diag.Record({.loc      = {.line = 2, .col = 1},
                     .severity = common::Severity::kError,
                     .message  = "e1",
                     .notes{}});
        diag.Record({.loc      = {.line = 3, .col = 1},
                     .severity = common::Severity::kError,
                     .message  = "e2",
                     .notes{}});
        std::ostringstream out;
        diag.Summarize(out);
        EXPECT_EQ(out.str(), "1 warning(s) generated, 2 error(s) found, compilation failed\n");
    }
}

TEST(DiagnosisEngineTest, AttachesNotesToDiagnosticAndPrintsAfterParent) {
    common::DiagnosisEngine diag;
    diag.Record(
        {.loc      = {.line = 5, .col = 6},
         .severity = common::Severity::kError,
         .message  = "redeclaration of 'x'",
         .notes    = {{.loc = {.line = 1, .col = 5}, .message = "previous declaration is here"}}});
    diag.Record({.loc      = {.line = 2, .col = 3},
                 .severity = common::Severity::kWarning,
                 .message  = "unused variable 'y'",
                 .notes{}});

    ASSERT_EQ(diag.Collected().size(), 2U);
    ASSERT_EQ(diag.Collected()[0].notes.size(), 1U);
    EXPECT_EQ(diag.Collected()[0].notes[0].loc.line, 1);
    EXPECT_EQ(diag.Collected()[0].notes[0].loc.col, 5);
    EXPECT_EQ(diag.Collected()[0].notes[0].message, "previous declaration is here");
    EXPECT_TRUE(diag.Collected()[1].notes.empty());

    std::ostringstream summary;
    diag.Summarize(summary);
    EXPECT_EQ(summary.str(), "1 warning(s) generated, 1 error(s) found, compilation failed\n");

    std::ostringstream out;
    diag.Print(out);
    EXPECT_EQ(out.str(),
              "5:6: error: redeclaration of 'x'\n"
              "1:5: note: previous declaration is here\n"
              "2:3: warning: unused variable 'y'\n");

    std::vector<common::Diagnostic> sorted(diag.Collected().begin(), diag.Collected().end());
    std::sort(sorted.begin(), sorted.end(),
              [](const common::Diagnostic& a, const common::Diagnostic& b) {
                  if (a.loc.line != b.loc.line) {
                      return a.loc.line < b.loc.line;
                  }
                  return a.loc.col < b.loc.col;
              });
    ASSERT_EQ(sorted.size(), 2U);
    EXPECT_EQ(sorted[0].severity, common::Severity::kWarning);
    EXPECT_EQ(sorted[1].severity, common::Severity::kError);
    EXPECT_EQ(sorted[1].loc.line, 5);
    ASSERT_EQ(sorted[1].notes.size(), 1U);
    EXPECT_EQ(sorted[1].notes[0].loc.line, 1);
    EXPECT_EQ(sorted[1].notes[0].loc.col, 5);
    EXPECT_EQ(sorted[1].notes[0].message, "previous declaration is here");
}
