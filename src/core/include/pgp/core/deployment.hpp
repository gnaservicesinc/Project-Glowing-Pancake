#pragma once
#include <pgp/core/lab.hpp>
#include <pgp/core/market.hpp>

namespace pgp::core {
struct ServicePreference {
    std::string segment_id;
    MarketPreference preference;
    auto operator<=>(const ServicePreference&) const = default;
};
struct ServiceDefinition {
    MarketProduct product;
    CapacityProject serving;
    LabProfile required = empty_lab_profile;
    LabProfile review_credit = empty_lab_profile;
    std::uint32_t review_staff = 0;
    Cents review_monthly_cost{0};
    std::uint64_t review_tasks_per_day = 0;
    std::vector<ServicePreference> preferences;
    std::string required_recipe{};
    std::vector<std::string> required_research{};
    auto operator<=>(const ServiceDefinition&) const = default;
};
struct DeploymentRecord {
    std::string service_id, model_id, evidence_id, project_id;
    bool human_review = false;
    OrdinalDay deployed_on{0};
    std::string region_id{};
    std::uint64_t restored_release=0;
    auto operator<=>(const DeploymentRecord&) const = default;
};
struct ServiceRollout {
    DeploymentRecord candidate;
    std::uint64_t incumbent_release=0;
    std::optional<OrdinalDay> resolved_on{};
    std::uint64_t promoted_release=0;
    auto operator<=>(const ServiceRollout&) const = default;
};
struct DeploymentState {
    std::vector<ServiceDefinition> catalog;
    std::vector<DeploymentRecord> records;
    std::vector<ServiceRollout> rollouts{};
    auto operator<=>(const DeploymentState&) const = default;
};
struct DeployModel {
    std::string service_id, model_id, evidence_id, source_id;
    bool human_review = false;
    std::string region_id{};
};
struct StageServiceRollout {
    DeployModel candidate;
    BasisPoints share{2000};
    Days term{14};
};
struct FinishServiceRollout { std::string service_id; bool promote=false; };
struct RollbackService { std::string service_id; };
struct CommitServiceCompatibility { std::string service_id; Days term{90}; };
struct AnnounceServiceRetirement { std::string service_id; Days notice{90}; };
struct CompatibilityProfile {
    LabProfile quality=empty_lab_profile;
    bool human_review=false;
    std::uint64_t tasks_per_day=0;
    std::uint64_t customer_memory_mib=0;
    std::string customer_interconnect;
    MilliCU customer_compute_per_task{0};
};
struct SimulationState;

[[nodiscard]] ServiceCompatibility preview_compatibility(const SimulationState&,const CommitServiceCompatibility&);
[[nodiscard]] CompatibilityProfile compatibility_profile(const SimulationState&,const ServiceCompatibility&);
[[nodiscard]] ProductRetirement preview_retirement(const SimulationState&,const AnnounceServiceRetirement&);
void commit_service_compatibility(SimulationState&,const CommitServiceCompatibility&);
void announce_service_retirement(SimulationState&,const AnnounceServiceRetirement&);
[[nodiscard]] std::size_t retire_due_services(SimulationState&);
[[nodiscard]] const ServiceRollout* pending_rollout(const DeploymentState&,std::string_view service);
[[nodiscard]] const DeploymentRecord* trial_release(const DeploymentState&,const MarketTrial&);
[[nodiscard]] DeployModel release_request(const DeploymentRecord&,const CapacityState&);
[[nodiscard]] std::uint64_t rollback_release(const DeploymentState&,const MarketState&,std::string_view service);
[[nodiscard]] CapacityProject preview_rollout(const SimulationState&,const StageServiceRollout&);
void stage_service_rollout(SimulationState&,const StageServiceRollout&);
void finish_service_rollout(SimulationState&,const FinishServiceRollout&);
void rollback_service(SimulationState&,const RollbackService&);
void expire_service_rollouts(SimulationState&);
void discard_service_rollouts(SimulationState&);
void validate_service_rollouts(const DeploymentState&,const LabState&,const CapacityState&,const MarketState&);
void validate_service_support(const DeploymentState&,const LabState&,const CapacityState&,const MarketState&);
void validate_service_catalog(const std::vector<ServiceDefinition>& catalog);
void validate_deployments(const DeploymentState& state, const LabState& lab,
    const CapacityState& capacity, const MarketState& market);
// All modules live in the enclosing copied Simulation transaction.
void deploy_model(DeploymentState& state, const LabState& lab, AccountsState& accounts,
    CapacityState& capacity, MarketState& market, OrdinalDay day, const DeployModel& request);
}
