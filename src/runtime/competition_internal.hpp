#pragma once
#include <pgp/runtime/runtime.hpp>
namespace pgp::runtime::competition_detail {
void initialize_planning(WorldState&,const CompiledProgram&,const std::vector<content::RivalDefinition>&,const content::CompetitionSetup&);
void advance_planning(WorldState&,const CompiledProgram&,core::OrdinalDay,const IndustryAllocations&);
void validate_planning(const WorldState&,const CompiledProgram&,bool);
[[nodiscard]] const core::SimulationState* company(const WorldState&,std::string_view);
[[nodiscard]] core::SimulationState* company(WorldState&,std::string_view);
[[nodiscard]] core::SimulationState dormant_company(const WorldState&,const content::RivalDefinition&);
[[nodiscard]] std::uint64_t rival_seed(const WorldState&,const content::RivalDefinition&);
[[nodiscard]] core::SimulationState opening_rival(const WorldState&,const CompiledProgram&,const content::RivalDefinition&,core::OrdinalDay);
void review_rival(WorldState&,const CompiledProgram&,RivalCompany&);
[[nodiscard]] RivalObservation public_observation(const WorldState&,core::OrdinalDay,std::uint64_t);
void prepare_rival_morning(WorldState&,const CompiledProgram&,RivalCompany&);
void collect_public_results(WorldState&,core::OrdinalDay);
void append_industry_adoption(WorldState&,core::OrdinalDay);
void validate_industry(const WorldState&,bool);
}
