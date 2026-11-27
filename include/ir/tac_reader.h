#pragma once

#include <optional>
#include <string_view>

#include "common/diagnostic.h"
#include "ir/tac.h"

namespace ir {
// Returns no program if the TAC is malformed. Diagnostics refer to the TAC text.
[[nodiscard]] std::optional<Program> ReadTac(std::string_view         source,
                                             common::DiagnosisEngine& diag);
}  // namespace ir
