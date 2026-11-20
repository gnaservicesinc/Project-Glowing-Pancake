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
    std::optional<content::FoundingSelection> founding{};
    std::vector<core::PeopleContact> opening_contacts{};
#if !PGP_DISTRIBUTION
    std::optional<core::OrdinalDay> playtest_start{};
#endif
};

// Reproduce the opening offers and profiles from this campaign's saved seed.
void generate_rival_roster(WorldState&, const CompiledProgram&);
[[nodiscard]] core::LabCatalog campaign_lab_catalog(const content::Collection&, const content::ScenarioDefinition&,
                                                    const WorldState&);
[[nodiscard]] std::optional<core::ManagementCatalog> campaign_management(const content::ScenarioDefinition&,
                                                                        const WorldState&);

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
