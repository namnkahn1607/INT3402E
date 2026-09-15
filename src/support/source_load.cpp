#include "support/source_load.h"

#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <utility>

#include "support/diagnostic.h"

namespace support {

std::optional<SourceBuf> LoadSourceFile(
    const std::string& path, DiagnosisEngine& diag
) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        diag.Report({0, 0, Severity::kError, "cannot open source: " + path});
        return std::nullopt;
    }

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content(static_cast<size_t>(size), '\0');
    if (!file.read(content.data(), size)) {
        diag.Report({0, 0, Severity::kError, "failed reading source: " + path});
        return std::nullopt;
    }

    return SourceBuf{path, std::move(content)};
}

}  // namespace support
