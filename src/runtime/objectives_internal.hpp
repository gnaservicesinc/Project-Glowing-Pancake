#pragma once

#include <pgp/runtime/runtime.hpp>

namespace pgp::runtime::objective_detail {
[[nodiscard]] const core::ObjectiveCharter& charter(const WorldState&,const CompiledProgram&);
[[nodiscard]] std::vector<const core::SimulationState*> institutions(const WorldState&);
[[nodiscard]] bool controlled_on(const WorldState&,std::string_view institution,core::OrdinalDay day);
[[nodiscard]] core::ObjectiveMonth month(const WorldState&,core::OrdinalDay day);
[[nodiscard]] std::array<std::uint64_t,core::society_regions> remaining_delivery(const WorldState&,core::OrdinalDay settled);
[[nodiscard]] std::optional<AsiRaceResult> race_through(const WorldState&,core::OrdinalDay through);
[[nodiscard]] bool retains_control(const WorldState&);
[[nodiscard]] std::vector<LegacyValuation> terminal_profit(const WorldState&,const core::ObjectiveCharter&);
[[nodiscard]] SurvivalEvidence terminal_survival(const WorldState&);
[[nodiscard]] core::LiquidityExposure recorded_exposure(const core::AccountsState&,
    std::size_t posting_cursor,core::OrdinalDay due_through);
[[nodiscard]] bool survival_met(const SurvivalEvidence&) noexcept;
void finish(WorldState&,CampaignEndCause,std::string source = {});
} // namespace pgp::runtime::objective_detail
