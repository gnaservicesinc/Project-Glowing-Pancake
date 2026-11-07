#pragma once
#include <pgp/core/agent_service.hpp>
#include <pgp/core/calendar.hpp>
#include <pgp/core/paged_history.hpp>
#include <optional>
#include <string>
#include <vector>

namespace pgp::core {
struct SimulationState;
struct DeploymentRecord;
struct DeploymentRiskCell {
    bool large=false;
    AgentTools autonomy=AgentTools::draft;
    bool mismatch=false,supervised=false,staged=false;
    BasisPoints probability{0};
    auto operator<=>(const DeploymentRiskCell&) const = default;
};
struct DeploymentRiskRules {
    std::string assumptions,exposure,severity;
    std::size_t domain=0;
    BasisPoints control_threshold{0};
    std::uint64_t large_daily_tasks=0;
    BasisPoints elevated_from{100},high_from{500};
    // All 48 scale/autonomy/mismatch/supervision/staging combinations.
    // Exposure and task severity select this service's authored table.
    std::vector<DeploymentRiskCell> lookup;
    Cents compensation{0},response_fee{0};
    MilliCU response_work{0};
    Days response_staff_days{1};
    auto operator<=>(const DeploymentRiskRules&) const = default;
};
struct DeploymentRiskSample {
    std::string project_id;
    OrdinalDay sampled_on{0},event_on{0};
    std::uint64_t prior_attempts=0,stream_before=0,stream_after=0;
    std::uint32_t cell=0;
    bool incident=false;
    auto operator<=>(const DeploymentRiskSample&) const = default;
};
struct DeploymentIncident {
    std::string id,project_id;
    std::uint64_t sample_index=0;
    OrdinalDay discovered_on{0};
    std::optional<OrdinalDay> recovered_on{};
    std::optional<std::uint64_t> last_response{};
    auto operator<=>(const DeploymentIncident&) const = default;
};
struct DeploymentIncidentResponse {
    std::string incident_id,project_id;
    OrdinalDay started_on{0};
    auto operator<=>(const DeploymentIncidentResponse&) const = default;
};
struct DeploymentIncidentAlert {
    std::string id,incident_id;
    OrdinalDay day{0};
    bool recovery=false,acknowledged=false;
    auto operator<=>(const DeploymentIncidentAlert&) const = default;
};
struct DeploymentRiskState {
    OrdinalDay starts_on{0};
    std::uint64_t stream_origin=0;
    PagedHistory<DeploymentRiskSample,64> samples;
    std::vector<DeploymentIncident> incidents;
    std::vector<DeploymentIncidentResponse> responses;
    std::vector<DeploymentIncidentAlert> alerts;
    std::vector<std::uint64_t> open_incidents;
    std::optional<OrdinalDay> sampled_on{},prepared_on{};
    std::size_t alert_cursor=0;
    auto operator<=>(const DeploymentRiskState&) const = default;
};
inline constexpr std::size_t deployment_risk_sample_limit=300000,deployment_incident_limit=8192,
    deployment_response_limit=16384,deployment_incident_alert_limit=16384;
struct RespondDeploymentIncident {
    std::string id,source_id;
    MilliCUPerDay daily_compute{20000};
    std::uint32_t staff=2;
};
struct AcknowledgeDeploymentIncident {std::string id;};
struct DeploymentResponsePreview {
    Cents fee{0},compensation_due{0},cash_required{0};
    MilliCU work{0};
    Days minimum_days{0};
};
struct DeploymentRiskReport {
    std::string band,exposure,severity,reasons;
    bool staged=false;
};
void validate_deployment_risk_rules(const DeploymentRiskRules&);
void initialize_deployment_risk(SimulationState&);
void sample_deployment_risk(SimulationState&);
void prepare_deployment_risk(SimulationState&);
void validate_deployment_risk(const SimulationState&,bool deep=true);
[[nodiscard]] DeploymentRiskReport deployment_risk_report(const SimulationState&,const DeploymentRecord&,bool staged=false);
[[nodiscard]] DeploymentResponsePreview preview_deployment_response(const SimulationState&,const RespondDeploymentIncident&);
void respond_deployment_incident(SimulationState&,const RespondDeploymentIncident&,Cents protected_cash);
[[nodiscard]] const DeploymentIncidentAlert* required_deployment_incident(const SimulationState&);
[[nodiscard]] std::string deployment_incident_description(const SimulationState&,const DeploymentIncidentAlert&);
void acknowledge_deployment_incident(SimulationState&,std::string_view);
} // namespace pgp::core
