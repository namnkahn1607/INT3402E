#include "support/diagnostic.h"

#include <iostream>

namespace support {

bool DiagnosisEngine::Report(const Diagnostic& d) noexcept {
    const char* tag;
    if (d.severity == Severity::kError) {
        tag        = "error";
        err_found_ = true;
    } else {
        tag = "warning";
        ++warning_count_;
    }

    std::cerr << d.line << ':' << d.col << ": " << tag << ": " << d.message
              << '\n';
    return err_found_;
}

void DiagnosisEngine::Summarize() const noexcept {
    std::cerr << warning_count_ << " warning(s) generated";
    if (err_found_) {
        std::cerr << ", compilation failed";
    }
    std::cerr << "\n";
}

}  // namespace support
