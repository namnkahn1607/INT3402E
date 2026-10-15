#pragma once

#include <gtest/gtest.h>

#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ast/translation_unit.h"
#include "common/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/expr_parser.h"
#include "parser/parser.h"

namespace parser::test {

struct ParseResult {
    std::unique_ptr<ast::TranslationUnit> translation_unit;
    std::vector<common::Diagnostic>       syntax_errors;
    std::vector<common::Diagnostic>       lexical_errors;
    std::vector<common::Diagnostic>       all_diagnostics;
    std::size_t                           token_count = 0;
    bool                                  has_errors  = false;

    [[nodiscard]] bool has_syntax_errors() const noexcept {
        return !syntax_errors.empty();
    }
};

inline ParseResult ParseSource(
    const std::string&  source,
    std::optional<bool> expected_lexical_errors = std::nullopt) {
    common::DiagnosisEngine diagnostics;
    lexer::Lexer            lexer{diagnostics};
    const auto              tokens = lexer.Tokenize(source);

    if (expected_lexical_errors.has_value()) {
        EXPECT_EQ(diagnostics.HasErrors(), *expected_lexical_errors);
    }

    std::vector<common::Diagnostic> lex_errors(diagnostics.Collected().begin(),
                                               diagnostics.Collected().end());

    parser::Parser parser{source, tokens, diagnostics};
    auto           unit = parser.ParseTranslationUnit();

    std::vector<common::Diagnostic> syn_errors;
    const auto                      all = diagnostics.Collected();
    if (all.size() > lex_errors.size()) {
        syn_errors.assign(all.begin() + static_cast<long>(lex_errors.size()),
                          all.end());
    }

    return ParseResult{
        .translation_unit = std::move(unit),
        .syntax_errors    = std::move(syn_errors),
        .lexical_errors   = std::move(lex_errors),
        .all_diagnostics =
            std::vector<common::Diagnostic>(all.begin(), all.end()),
        .token_count = tokens.size(),
        .has_errors  = diagnostics.HasErrors(),
    };
}

inline ParseResult Parse(const std::string& source) {
    return ParseSource(source);
}

inline ParseResult ParseBody(std::string_view body) {
    return Parse("{ " + std::string{body} + " }");
}

inline std::string DumpAstToString(const ast::TranslationUnit& unit) {
    std::ostringstream ss;
    ast::DumpAst(unit, ss);
    return ss.str();
}

inline void ExpectAst(const ast::TranslationUnit& unit,
                      std::string_view            expected_dump) {
    std::string actual = DumpAstToString(unit);
    if (!expected_dump.empty() && expected_dump.front() == '\n') {
        expected_dump.remove_prefix(1);
    }
    while (!actual.empty() && (actual.back() == '\n' || actual.back() == ' ' ||
                               actual.back() == '\r')) {
        actual.pop_back();
    }
    while (!expected_dump.empty() &&
           (expected_dump.back() == '\n' || expected_dump.back() == ' ' ||
            expected_dump.back() == '\r')) {
        expected_dump.remove_suffix(1);
    }
    EXPECT_EQ(actual, expected_dump);
}

inline void ExpectLocation(common::Location actual, int line, int column) {
    EXPECT_EQ(actual.line, line);
    EXPECT_EQ(actual.col, column);
}

inline std::vector<common::Location> ErrorLocations(
    std::span<const common::Diagnostic> diagnostics) {
    std::vector<common::Location> locations;
    locations.reserve(diagnostics.size());
    for (const auto& d : diagnostics) {
        if (d.severity == common::Severity::kError) {
            locations.push_back(d.loc);
        }
    }
    return locations;
}

inline std::vector<common::Location> ErrorLocations(const ParseResult& result) {
    return ErrorLocations(result.all_diagnostics);
}

inline void ExpectValidPartialTree(const ParseResult& result) {
    ASSERT_NE(result.translation_unit, nullptr);
    std::ostringstream ss;
    ast::DumpAst(*result.translation_unit, ss);
    EXPECT_FALSE(ss.str().empty());
}

}  // namespace parser::test
