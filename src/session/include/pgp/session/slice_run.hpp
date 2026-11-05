#pragma once
#include <pgp/runtime/runtime.hpp>

namespace pgp::session {
enum class SliceStop { finished, requested_boundary, liquidity_warning, closed };
struct SliceRun {
    SliceStop stop;
    runtime::Checkpoint checkpoint;
};
// Reproducible engineering strategy through ordinary commands, not a player
// automation policy. It uses the authored service/offer catalog and actual
// independent evidence. Optional story roots are disabled; all base roots stay.
[[nodiscard]] SliceRun run_slice_baseline(const runtime::CompiledProgram& program,
    std::optional<core::OrdinalDay> stop_before={},std::optional<runtime::Checkpoint> resume={});
[[nodiscard]] SliceRun run_slice_specialist(const runtime::CompiledProgram& program,
    std::optional<core::OrdinalDay> stop_before={},std::optional<runtime::Checkpoint> resume={});
[[nodiscard]] SliceRun run_competition_specialist(const runtime::CompiledProgram& program,std::string_view scenario_id,
    std::optional<core::OrdinalDay> stop_before={},std::optional<runtime::Checkpoint> resume={});
}
