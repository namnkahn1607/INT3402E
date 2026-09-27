#include "common/source_load.h"

#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <utility>

#include "common/diagnostic.h"

namespace common {

std::optional<SourceBuf> LoadSourceFile(
    const std::string& path, DiagnosisEngine& diag
) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        diag.Report(
            {.line     = 0,
             .col      = 0,
             .severity = Severity::kError,
             .message  = "cannot open source: " + path}
        );
        return std::nullopt;
    }

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    if (size < 0) {
        diag.Report(
            {.line     = 0,
             .col      = 0,
             .severity = Severity::kError,
             .message  = "cannot determine source size: " + path}
        );
        return std::nullopt;
    }
    file.seekg(0, std::ios::beg);

    std::string content(static_cast<size_t>(size), '\0');
    if (!file.read(content.data(), size)) {
        diag.Report(
            {.line     = 0,
             .col      = 0,
             .severity = Severity::kError,
             .message  = "failed reading source: " + path}
        );
        return std::nullopt;
    }

    return SourceBuf{.path = path, .text = std::move(content)};
}

}  // namespace common
