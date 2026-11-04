#pragma once

#include <pgp/runtime/runtime.hpp>

namespace pgp::runtime::detail {
class DurationBudgetExceeded : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
// The returned query has no mutable state or RNG handle.
[[nodiscard]] DurationReader saved_duration_reader(const RuntimeState& state, std::string_view definition_id);
void observe_durations(Checkpoint& checkpoint, const CompiledProgram& program, std::size_t& steps);
void validate_duration_states(const Checkpoint& checkpoint, const CompiledProgram& program);
} // namespace pgp::runtime::detail
