#pragma once

#include <pgp/runtime/runtime.hpp>

namespace pgp::runtime::detail {

// Pending committed events are a projection of immutable settlement rows.
// The lifecycle saves the processed prefix so reopening cannot replay or
// re-evaluate an already-consumed source after a required choice.
[[nodiscard]] std::vector<CommittedSource> committed_sources(const WorldState& world,
    const content::Registry& registry, std::string_view fact_id, const ScopeBinding& owner,
    core::OrdinalDay opening_day);
void validate_committed_sources(const CompiledProgram& program, const Checkpoint& checkpoint);

} // namespace pgp::runtime::detail
