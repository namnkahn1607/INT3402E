#pragma once

#include <optional>
#include <string>

#include "common/diagnostic.h"

namespace common {

struct SourceBuf {
    std::string path;
    std::string text;
};

// Stream data from source file into memory buffer.
std::optional<SourceBuf> LoadSourceFile(
    const std::string& path, DiagnosisEngine& diag
);

}  // namespace common
