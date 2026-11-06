#pragma once

#include <pgp/runtime/runtime.hpp>

#include <string_view>

namespace pgp::runtime {

struct ScenarioOptions {
    std::optional<CampaignIdentity> identity;
    std::optional<std::uint64_t> seed;
    std::string background;
    std::string charter;
    std::string difficulty;
};

// Initialize implemented foundation fields from validated authored data. No
// ledger/work/business result is inferred before its owning system exists.
[[nodiscard]] Checkpoint begin_scenario(const CompiledProgram& program,
                                       std::string_view scenario_id,
                                       std::string_view headquarters,
                                       std::optional<core::ObjectiveSelection> objectives = {},
                                       ScenarioOptions options = {});
[[nodiscard]] core::TrainingPreview preview_campaign_training(const CompiledProgram&,const WorldState&,const core::TrainingRequest&);
[[nodiscard]] core::OrdinalDay scripted_rival_start(const CompiledProgram&,const WorldState&,const content::RivalDefinition&);
} // namespace pgp::runtime
