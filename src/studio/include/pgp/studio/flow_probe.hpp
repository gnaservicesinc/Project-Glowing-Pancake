#pragma once

#include <pgp/runtime/runtime.hpp>

namespace pgp::studio {
struct ProbeFixture {
    std::string scenario_id;
    std::string headquarters;
    // An empty ID uses the scenario's authored roots.
    std::string isolated_flow;
    std::optional<std::uint64_t> seed;
};

struct ProbeStep {
    runtime::RuntimeResult trace;
    core::OrdinalDay before_date;
    core::OrdinalDay after_date;
    core::Cents before_cash;
    core::Cents after_cash;
    std::vector<runtime::EffectReceipt> effects;
    std::vector<core::Posting> postings;
    std::vector<runtime::OwnedModifier> active_modifiers;
};
struct BoundarySample {
    std::string path;
    runtime::ScopeBinding binding;
    std::string fact;
    content::Unit unit;
    std::string label;
    std::optional<std::int64_t> value;
    std::optional<bool> result;
    std::string error;
};

// Owns a copy of the validated program and fixture. No live session,
// campaign storage or mutable caller-owned checkpoint is accepted.
class FlowProbe {
public:
    FlowProbe(const runtime::CompiledProgram& program, runtime::Checkpoint fixture);
    FlowProbe(const runtime::CompiledProgram& program, const ProbeFixture& fixture);
    [[nodiscard]] const runtime::Checkpoint& state() const noexcept { return engine_.current_state(); }
    [[nodiscard]] runtime::FlowInspection inspect(std::string_view flow_id, runtime::FlowPredicate predicate) const;
    // Individual numeric literal clauses on overridden facts, without execution.
    [[nodiscard]] std::vector<BoundarySample> boundaries(std::string_view flow_id) const;
    [[nodiscard]] ProbeStep prepare();
    [[nodiscard]] ProbeStep submit(core::CommandAction action);
    [[nodiscard]] ProbeStep choose(std::uint64_t presentation_id, std::string_view choice_id);
    // Run separately from the natural fixture timeline, leaving it intact.
    [[nodiscard]] ProbeStep synthetic_cleanup(std::uint64_t activation_id) const;
    // Save and restore the exact isolated state through the game codec in memory.
    void reload();
private:
    runtime::Runtime engine_;
};
} // namespace pgp::studio
