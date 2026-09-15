#pragma once

#include <string>

namespace common {

enum class Severity {
    kWarning,
    kError,
};

struct Diagnostic {
    int         line, col;
    Severity    severity;
    std::string message;
};

class DiagnosisEngine {
public:
    DiagnosisEngine()  = default;
    ~DiagnosisEngine() = default;

    // Reports either a warning or error into standard error.
    // Returns `true` on error found, `false` otherwise.
    bool Report(const Diagnostic& d) noexcept;

    // Summarizes compilation process. Only invoke upon halting.
    void Summarize() const noexcept;

    bool HasErrors() const noexcept { return err_found_; }

private:
    int  warning_count_ = 0;
    bool err_found_     = false;
};

}  // namespace common
