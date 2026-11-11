#pragma once
#include <pgp/core/simulation.hpp>
#include <pgp/content/world_setup.hpp>
#include <pgp/runtime/heavyweight.hpp>
#include <pgp/runtime/mergers.hpp>
#include <pgp/runtime/wholesale.hpp>
#include <pgp/runtime/collaboration.hpp>
#include <map>

namespace pgp::runtime {
struct WorldState;
struct CompiledProgram;
struct RivalObservation {
    core::OrdinalDay day{0};
    std::uint32_t public_providers=0;
    core::Cents public_price{0};
    std::uint64_t demand_lower=0,demand_upper=0;
    auto operator<=>(const RivalObservation&) const = default;
};
struct RivalDecision {
    core::OrdinalDay day{0};
    std::string action,subject,explanation;
    bool accepted=false;
    std::uint64_t stream_before=0,stream_after=0;
    RivalObservation observation;
    auto operator<=>(const RivalDecision&) const = default;
};
// Difficulty profiles permit one bounded strategy review every three days.
inline constexpr std::size_t rival_decision_limit = (core::retained_campaign_days + 2) / 3;
struct RivalPlanState {
    std::optional<core::OrdinalDay> entered_on;
    core::OrdinalDay next_review{0};
    core::PagedHistory<RivalDecision, 16> decisions;
    auto operator<=>(const RivalPlanState&) const = default;
};
struct RivalCompany {
    std::string definition_id;
    core::SimulationState simulation;
    bool attempted = false;
    std::optional<core::OrdinalDay> warned_on, published_on;
    std::optional<RivalPlanState> planning{};
    std::optional<HeavyweightState> response{};
    std::string parent_company{};
    auto operator<=>(const RivalCompany&) const = default;
};
struct RivalNotice {
    std::string company_id, kind;
    core::OrdinalDay day{0};
    std::optional<core::OrdinalDay> response_due;
    std::string subject{};
    auto operator<=>(const RivalNotice&) const = default;
};
struct PublicProductRelease {
    std::string company_id,product_id;
    core::OrdinalDay day{0};
    auto operator<=>(const PublicProductRelease&) const = default;
};
struct CompetitionState {
    std::vector<RivalCompany> companies;
    std::vector<RivalNotice> notices;
    std::optional<content::CompetitionSetup> setup{};
    std::vector<PublicProductRelease> releases{};
    core::MarketAdoptionHistory adoptions{};
    std::vector<CompetitiveOffer> offers{};
    std::vector<AcquisitionRecord> acquisitions{};
    std::vector<WholesaleContract> wholesale{};
    std::vector<ResearchCollaboration> collaborations{};
    auto operator<=>(const CompetitionState&) const = default;
};
[[nodiscard]] const content::RivalDefinition& rival_definition(const CompiledProgram& program,std::string_view id);
using IndustryAllocations=std::map<std::string,core::AllocationResult,std::less<>>;
void initialize_competition(WorldState& world,const CompiledProgram& program,const std::vector<content::RivalDefinition>& definitions,
    const std::optional<content::CompetitionSetup>& setup = {});
[[nodiscard]] IndustryAllocations prepare_competition_day(WorldState& world,const CompiledProgram& program);
[[nodiscard]] std::vector<core::MarketAdoption> industry_adoption_snapshot(const WorldState& world,
    std::optional<core::OrdinalDay> work_day = {});
void advance_competition(WorldState& world,const CompiledProgram& program,core::OrdinalDay prior_day,const IndustryAllocations& assignments = {});
void synchronize_industry_people(WorldState&,const CompiledProgram&);
void validate_industry_people(const WorldState&,const CompiledProgram&);
void validate_public_insight(const WorldState& world,const core::ResearchInsight& insight);
void validate_competition(const WorldState& world,const CompiledProgram& program,bool deep);
}
