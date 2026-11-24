#include "common/diagnostic.h"

#include <cassert>
#include <iostream>
#include <string_view>
#include <utility>

namespace common {

namespace {

std::string_view SeverityTag(Severity severity) {
    switch (severity) {
        case Severity::kWarning: return "warning";
        case Severity::kError: return "error";
    }
    assert(false && "unhandled Severity");
    return "?";
}

}  // namespace

void DiagnosisEngine::Record(Diagnostic d) {
    const auto severity = d.severity;
    collections_.push_back(std::move(d));
    switch (severity) {
        case Severity::kError: ++err_count_; break;
        case Severity::kWarning: ++warn_count_; break;
    }
}

void DiagnosisEngine::Print(std::ostream& out) const {
    for (const auto& d : collections_) {
        out << d.loc.line << ':' << d.loc.col << ": " << SeverityTag(d.severity) << ": "
            << d.message << '\n';
    }
}

void DiagnosisEngine::Print() const { Print(std::cerr); }

void DiagnosisEngine::Summarize(std::ostream& out) const {
    if (warn_count_ == 0 && err_count_ == 0) return;
    if (warn_count_ > 0) {
        out << warn_count_ << " warning(s) generated";
        if (err_count_ > 0) out << ", ";
    }
    if (err_count_ > 0) out << err_count_ << " error(s) found, compilation failed";

    out << '\n';
}

void DiagnosisEngine::Summarize() const { Summarize(std::cerr); }

}  // namespace common
