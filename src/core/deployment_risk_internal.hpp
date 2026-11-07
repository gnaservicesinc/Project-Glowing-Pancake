#pragma once
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <map>

namespace pgp::core::deployment_risk_detail {
[[noreturn]] void invalid(const char*);
template<class Rows> const auto& named(const Rows& rows,std::string_view id) {
    const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.id==id;});
    if(row==rows.end()) invalid("Deployment incident refers to a missing source, release, report or response");
    return *row;
}
struct Release { const DeploymentRecord* record; bool staged; };
[[nodiscard]] std::map<std::string_view,Release,std::less<>> releases(const SimulationState&);
[[nodiscard]] const DeploymentRiskRules& rules(const SimulationState&,std::string_view project);
[[nodiscard]] std::map<std::string,std::uint64_t,std::less<>> previous_attempts(const SimulationState&,OrdinalDay);
[[nodiscard]] DeploymentRiskSample draw_sample(const SimulationState&,std::string_view,OrdinalDay,std::uint64_t,std::uint64_t);
[[nodiscard]] std::string claim_id(std::string_view);
[[nodiscard]] DeploymentResponsePreview response_terms(const SimulationState&,const DeploymentIncident&,MilliCUPerDay,std::uint32_t);
[[nodiscard]] std::vector<ProjectPause> expected_disruptions(const DeploymentRiskState&,std::string_view);
} // namespace pgp::core::deployment_risk_detail
