#pragma once

#include <cstdint>
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

    // Reports a `Diagnostic` of any severity level into standard error.
    void Report(const Diagnostic& d) noexcept;

    // Summarizes compilation process. Only invoke upon halting.
    void Summarize() const noexcept;

    // Collection of all `Diagnostic` reported so far.
    [[nodiscard]] std::vector<Diagnostic> Collected() const noexcept {
        return collections_;
    }

    [[nodiscard]] bool HasErrors() const noexcept { return err_count_ > 0; }

private:
    int warn_count_ = 0;
    int err_count_  = 0;

    std::vector<Diagnostic> collections_;
};

}  // namespace common
