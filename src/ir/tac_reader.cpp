#include "ir/tac_reader.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ir {
namespace {
constexpr std::string_view kWhitespace = " \t";
constexpr std::string_view kDigits     = "0123456789";
constexpr std::string_view kLetters    = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

struct Token {
    std::string_view text;
    common::Location loc;
};

struct LocatedLabel {
    Label label;
    Token token;
};

bool IsDigits(std::string_view text) {
    return !text.empty() && text.find_first_not_of(kDigits) == std::string_view::npos;
}

bool IsVariableName(std::string_view text) {
    const auto suffix = text.find_first_not_of(kLetters);
    return !text.empty() && suffix != 0 &&
           (suffix == std::string_view::npos || IsDigits(text.substr(suffix)));
}

void Error(common::DiagnosisEngine& diag, common::Location loc, std::string message) {
    diag.Record({loc, common::Severity::kError, std::move(message), {}});
}

std::optional<BinOp> ReadBinOp(std::string_view text) {
    if (text == "add") return BinOp::Add;
    if (text == "mul") return BinOp::Mul;
    if (text == "eq") return BinOp::Eq;
    if (text == "gt") return BinOp::Gt;
    if (text == "ge") return BinOp::Ge;
    return std::nullopt;
}

class LineReader {
public:
    LineReader(std::string_view line, int line_number, common::DiagnosisEngine& diag)
        : line_{line}, line_number_{line_number}, diag_{diag} {}

    std::optional<Instruction> Read() {
        const Token first = Take();
        if (first.text.empty()) return std::nullopt;

        auto instruction = ReadItem(first);
        if (!instruction) return std::nullopt;
        const Token extra = Take();
        if (!extra.text.empty()) {
            Error(diag_, extra.loc, "unexpected token after TAC instruction");
            return std::nullopt;
        }
        return instruction;
    }

    [[nodiscard]] const std::optional<LocatedLabel>& LabelMention() const { return label_; }

private:
    Token Take() {
        const auto start = line_.find_first_not_of(kWhitespace, pos_);
        if (start == std::string_view::npos) {
            pos_ = line_.size();
            return {{}, {line_number_, static_cast<int>(pos_) + 1}};
        }

        // Keeping each word whole enforces the spacing rules and keeps t0.1 distinct from t0.
        const auto end = line_.find_first_of(kWhitespace, start);
        pos_           = end == std::string_view::npos ? line_.size() : end;
        return {line_.substr(start, pos_ - start), {line_number_, static_cast<int>(start) + 1}};
    }

    bool Expect(std::string_view expected) {
        const Token token = Take();
        if (token.text == expected) return true;
        Error(diag_, token.loc, "expected '" + std::string{expected} + "' in TAC instruction");
        return false;
    }

    std::optional<std::uint32_t> ReadId(std::string_view digits, common::Location loc) {
        std::uint32_t id{};
        const auto    result = std::from_chars(digits.data(), digits.data() + digits.size(), id);
        if (result.ec == std::errc{}) return id;
        Error(diag_, loc, "TAC identifier number is outside the uint32 range");
        return std::nullopt;
    }

    std::optional<Label> ReadLabel(Token token) {
        if (!token.text.starts_with('L') || !IsDigits(token.text.substr(1))) {
            Error(diag_, token.loc, "expected a TAC label");
            return std::nullopt;
        }
        const auto id = ReadId(token.text.substr(1), token.loc);
        if (!id) return std::nullopt;
        label_ = LocatedLabel{Label{*id}, token};
        return label_->label;
    }

    std::optional<Place> ReadPlace(Token token, std::string_view expected) {
        const auto dot = token.text.find('.');
        if (dot != std::string_view::npos && IsVariableName(token.text.substr(0, dot)) &&
            IsDigits(token.text.substr(dot + 1))) {
            const auto id = ReadId(token.text.substr(dot + 1), token.loc);
            if (!id) return std::nullopt;
            return Var{std::string{token.text.substr(0, dot)}, *id};
        }
        if (token.text.starts_with('t') && IsDigits(token.text.substr(1))) {
            const auto id = ReadId(token.text.substr(1), token.loc);
            if (!id) return std::nullopt;
            return Temp{*id};
        }
        Error(diag_, token.loc, std::string{expected});
        return std::nullopt;
    }

    std::optional<Operand> ReadOperand(Token token) {
        auto digits = token.text;
        if (digits.starts_with('-')) digits.remove_prefix(1);
        if (IsDigits(digits)) {
            std::int64_t value{};
            const auto   result =
                std::from_chars(token.text.data(), token.text.data() + token.text.size(), value);
            if (result.ec == std::errc{} && std::in_range<std::int32_t>(value)) {
                return Int{static_cast<std::int32_t>(value)};
            }
            Error(diag_, token.loc, "TAC integer literal is outside the int32 range");
            return std::nullopt;
        }
        auto place = ReadPlace(token, "expected a TAC operand");
        if (!place) return std::nullopt;
        return ToOperand(std::move(*place));
    }

    std::optional<Instruction> ReadAssignment(Token first) {
        auto dst = ReadPlace(first, "expected a TAC instruction or assignment destination");
        if (!dst || !Expect("=")) return std::nullopt;

        const Token rhs_start = Take();
        if (auto op = ReadBinOp(rhs_start.text)) {
            auto lhs = ReadOperand(Take());
            if (!lhs) return std::nullopt;
            auto rhs = ReadOperand(Take());
            if (!rhs) return std::nullopt;
            return BinOpInst{std::move(*dst), *op, std::move(*lhs), std::move(*rhs)};
        }
        auto src = ReadOperand(rhs_start);
        if (!src) return std::nullopt;
        return CopyInst{std::move(*dst), std::move(*src)};
    }

    std::optional<Instruction> ReadItem(Token first) {
        if (first.text.ends_with(':')) {
            first.text.remove_suffix(1);
            const auto label = ReadLabel(first);
            if (!label) return std::nullopt;
            return LabelInst{*label};
        }
        if (first.text == "goto") {
            const auto label = ReadLabel(Take());
            if (!label) return std::nullopt;
            return GotoInst{*label};
        }
        if (first.text == "ifFalse" || first.text == "ifTrue") {
            const auto cond  = first.text == "ifFalse" ? BranchCond::IfFalse : BranchCond::IfTrue;
            auto       value = ReadOperand(Take());
            if (!value || !Expect("goto")) return std::nullopt;
            const auto label = ReadLabel(Take());
            if (!label) return std::nullopt;
            return CondGotoInst{cond, std::move(*value), *label};
        }
        if (first.text == "print") {
            auto value = ReadOperand(Take());
            if (!value) return std::nullopt;
            return PrintInst{std::move(*value)};
        }
        return ReadAssignment(first);
    }

    std::string_view            line_;
    int                         line_number_;
    common::DiagnosisEngine&    diag_;
    std::size_t                 pos_ = 0;
    std::optional<LocatedLabel> label_;
};
}  // namespace

std::optional<Program> ReadTac(std::string_view source, common::DiagnosisEngine& diag) {
    const auto previous_diagnostics = diag.Collected().size();
    Program    program;
    std::unordered_map<std::uint32_t, common::Location> definitions;
    std::vector<LocatedLabel>                           targets;

    for (int line_number = 1; !source.empty(); ++line_number) {
        const auto newline = source.find('\n');
        auto       line    = source.substr(0, newline);
        if (newline == std::string_view::npos) {
            source = {};
        } else {
            source.remove_prefix(newline + 1);
            if (line.ends_with('\r')) line.remove_suffix(1);
        }

        LineReader reader{line.substr(0, line.find('#')), line_number, diag};
        auto       instruction = reader.Read();
        if (!instruction) continue;
        if (const auto& mention = reader.LabelMention()) {
            const auto [label, token] = *mention;
            if (std::holds_alternative<LabelInst>(*instruction)) {
                const auto [first, inserted] = definitions.try_emplace(label.id, token.loc);
                if (!inserted) {
                    diag.Record({token.loc,
                                 common::Severity::kError,
                                 "duplicate TAC label '" + std::string{token.text} + "'",
                                 {{first->second, "first defined here"}}});
                }
            } else {
                targets.push_back(*mention);
            }
        }
        program.instructions.push_back(std::move(*instruction));
    }

    for (const auto& [label, token] : targets) {
        if (!definitions.contains(label.id)) {
            Error(diag, token.loc, "undefined TAC label '" + std::string{token.text} + "'");
        }
    }
    if (diag.Collected().size() != previous_diagnostics) return std::nullopt;
    return program;
}
}  // namespace ir
