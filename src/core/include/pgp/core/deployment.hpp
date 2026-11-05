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
    auto operator<=>(const DeploymentRecord&) const = default;
};
struct DeploymentState {
    std::vector<ServiceDefinition> catalog;
    std::vector<DeploymentRecord> records;
    auto operator<=>(const DeploymentState&) const = default;
};
struct DeployModel {
    std::string service_id, model_id, evidence_id, source_id;
    bool human_review = false;
    std::string region_id{};
};
void validate_service_catalog(const std::vector<ServiceDefinition>& catalog);
void validate_deployments(const DeploymentState& state, const LabState& lab,
    const CapacityState& capacity, const MarketState& market);
// All modules live in the enclosing copied Simulation transaction.
void deploy_model(DeploymentState& state, const LabState& lab, AccountsState& accounts,
    CapacityState& capacity, MarketState& market, OrdinalDay day, const DeployModel& request);
}
