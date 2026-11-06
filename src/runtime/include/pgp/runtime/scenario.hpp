#pragma once

#include <pgp/runtime/runtime.hpp>

#include <string_view>

namespace pgp::runtime {

// Initialize implemented foundation fields from validated authored data. No
// ledger/work/business result is inferred before its owning system exists.
[[nodiscard]] Checkpoint begin_scenario(const CompiledProgram& program,
                                       std::string_view scenario_id,
                                       std::string_view headquarters,
                                       std::optional<core::ObjectiveSelection> objectives = {});
} // namespace pgp::runtime
