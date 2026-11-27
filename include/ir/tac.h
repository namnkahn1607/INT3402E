#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ir {

// Alpha-renamed source variable `<name>.<id>`, where `id` is globally unique
// across all declarations in a program, erasing lexical scope.
struct Var {
    std::string   name;
    std::uint32_t id;

    auto operator<=>(const Var&) const = default;
};

struct Temp {
    std::uint32_t id;

    auto operator<=>(const Temp&) const = default;
};

struct Int {
    std::int32_t value;

    auto operator<=>(const Int&) const = default;
};

struct Label {
    std::uint32_t id;

    auto operator<=>(const Label&) const = default;
};

// Assignable destination `<dst>`; excludes `Int` so writing to a constant is
// unrepresentable at compile time.
using Place = std::variant<Var, Temp>;

// Read operand `<val>`.
using Operand = std::variant<Var, Temp, Int>;

[[nodiscard]] inline Operand ToOperand(Place place) {
    return std::visit([](auto&& slot) -> Operand { return std::forward<decltype(slot)>(slot); },
                      std::move(place));
}

enum class BinOp : std::uint8_t { Add, Mul, Eq, Gt, Ge };

enum class BranchCond : std::uint8_t { IfFalse, IfTrue };

struct LabelInst {
    Label label;

    bool operator==(const LabelInst&) const = default;
};

struct CopyInst {
    Place   dst;
    Operand src;

    bool operator==(const CopyInst&) const = default;
};

struct BinOpInst {
    Place   dst;
    BinOp   op;
    Operand lhs;
    Operand rhs;

    bool operator==(const BinOpInst&) const = default;
};

struct GotoInst {
    Label target;

    bool operator==(const GotoInst&) const = default;
};

struct CondGotoInst {
    BranchCond cond;
    Operand    value;
    Label      target;

    bool operator==(const CondGotoInst&) const = default;
};

struct PrintInst {
    Operand value;

    bool operator==(const PrintInst&) const = default;
};

using Instruction = std::variant<LabelInst, CopyInst, BinOpInst, GotoInst, CondGotoInst, PrintInst>;

struct Program {
    std::vector<Instruction> instructions;

    bool operator==(const Program&) const = default;
};

}  // namespace ir
