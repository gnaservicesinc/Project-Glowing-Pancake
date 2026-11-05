#pragma once
#include <pgp/core/simulation.hpp>
#include <pgp/content/world_setup.hpp>

namespace pgp::runtime {
struct WorldState;
struct CompiledProgram;
struct RivalCompany {
    std::string definition_id;
    core::SimulationState simulation;
    bool attempted = false;
    std::optional<core::OrdinalDay> warned_on, published_on;
    auto operator<=>(const RivalCompany&) const = default;
};
struct RivalNotice {
    std::string company_id, kind;
    core::OrdinalDay day{0};
    std::optional<core::OrdinalDay> response_due;
    auto operator<=>(const RivalNotice&) const = default;
};
struct CompetitionState {
    std::vector<RivalCompany> companies;
    std::vector<RivalNotice> notices;
    auto operator<=>(const CompetitionState&) const = default;
};
[[nodiscard]] const content::RivalDefinition& rival_definition(const CompiledProgram& program,std::string_view id);
void initialize_competition(WorldState& world,const CompiledProgram& program,const std::vector<content::RivalDefinition>& definitions);
void advance_competition(WorldState& world,const CompiledProgram& program,core::OrdinalDay prior_day);
void validate_competition(const WorldState& world,const CompiledProgram& program,bool deep);
}
