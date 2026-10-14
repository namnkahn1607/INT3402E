#include "common/diagnostic.h"

#include <iostream>

namespace common {

void DiagnosisEngine::Report(const Diagnostic& d) noexcept {
    const char* tag;
    if (d.severity == Severity::kError) {
        tag = "error";
        collections_.push_back(d);
        ++err_count_;
    } else {
        tag = "warning";
        ++warn_count_;
    }

    std::cerr << d.loc.line << ':' << d.loc.col << ": " << tag << ": "
              << d.message << '\n';
}

void DiagnosisEngine::Summarize() const noexcept {
    if (warn_count_ > 0) {
        std::cerr << warn_count_ << " warning(s) generated";
    }
    if (err_count_ > 0) {
        std::cerr << err_count_ << " error(s) found, compilation failed";
    }
    std::cerr << "\n";
}

}  // namespace common
