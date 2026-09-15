#pragma once

#include <optional>
#include <string>

#include "support/diagnostic.h"

namespace support {

struct SourceBuf {
    std::string path;
    std::string text;
};

// Stream data from source file into memory buffer.
std::optional<SourceBuf> LoadSourceFile(
    const std::string& path, DiagnosisEngine& diag
);

}  // namespace support
