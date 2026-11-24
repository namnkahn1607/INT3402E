#pragma once

#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <vector>

#include "common/location.h"

namespace common {

enum class Severity : std::uint8_t {
    kWarning,
    kError,
};

struct Diagnostic {
    Location    loc;
    Severity    severity;
    std::string message;
};

class DiagnosisEngine {
public:
    DiagnosisEngine()  = default;
    ~DiagnosisEngine() = default;

    // Records a `Diagnostic` of any severity level.
    void Record(Diagnostic d);

    // Prints all collected diagnostics to `out` (defaults to `std::cerr`).
    void Print(std::ostream& out) const;
    void Print() const;

    // Summarizes compilation process to `out` (defaults to `std::cerr`).
    // Only invoke upon halting.
    void Summarize(std::ostream& out) const;
    void Summarize() const;

    // Collection of all `Diagnostic` reported so far.
    [[nodiscard]] std::span<const Diagnostic> Collected() const noexcept {
        return collections_;
    }

    [[nodiscard]] bool HasErrors() const noexcept { return err_count_ > 0; }

private:
    int warn_count_ = 0;
    int err_count_  = 0;

    std::vector<Diagnostic> collections_;
};

}  // namespace common
