#pragma once
#include <pgp/core/lab.hpp>
#include <pgp/core/market.hpp>

namespace pgp::core {
struct SimulationState;
struct FinancingOffer {
    std::string id,name;
    Cents proceeds{0};
    BasisPoints acquired{0},monthly_interest{0};
    Days term{0}; // zero = equity, otherwise debt
    auto operator<=>(const FinancingOffer&) const = default;
};
struct SpecialistOffer {
    std::string id,name;
    Cents signing_cost{0},monthly_salary{0};
    auto operator<=>(const SpecialistOffer&) const = default;
};
struct CapacitySetback {
    std::string id,name,source_id;
    OrdinalDay announced_on{0},starts_on{0},ends_before{0};
    MilliCUPerDay reduced_capacity{0};
    auto operator<=>(const CapacitySetback&) const = default;
};
struct SliceScoring {
    std::uint64_t task_target=0;
    std::array<std::uint32_t,5> weights{}; // survival, work, profit, evidence, founder stake
    std::string founder;
    BasisPoints opening_founder_share{0};
    auto operator<=>(const SliceScoring&) const = default;
};
struct ManagementCatalog {
    std::vector<FinancingOffer> financing;
    std::vector<SpecialistOffer> specialists;
    std::vector<CapacitySetback> setbacks;
    SliceScoring scoring;
    Days minimum_evaluation_interval{30};
    std::uint32_t maximum_policy_runs=12;
    Cents default_policy_budget{0},default_reserve_floor{0},default_monthly_ceiling{0};
    std::uint32_t default_policy_runs=1;
    auto operator<=>(const ManagementCatalog&) const = default;
};
struct ManagementAcceptance {
    std::string offer_id;
    OrdinalDay day{0};
    auto operator<=>(const ManagementAcceptance&) const = default;
};
enum class DelegationKind { evaluation, lease_renewal };
struct DelegationPolicy {
    std::string id,target_id,plan_id;
    DelegationKind kind=DelegationKind::evaluation;
    LabAllocation allocation;
    Cents budget{0},reserve_floor{0},monthly_ceiling{0};
    Days interval{30};
    std::uint32_t maximum_runs=1;
    bool enabled=true;
    OrdinalDay created_on{0},next_review{0};
    std::uint32_t runs=0;
    Cents committed{0};
    std::string blocked;
    auto operator<=>(const DelegationPolicy&) const = default;
};
struct DelegationDecision {
    std::string policy_id,result_id,explanation;
    OrdinalDay day{0};
    Cents commitment{0};
    bool accepted=false;
    auto operator<=>(const DelegationDecision&) const = default;
};
struct ManagementNotice {
    std::string id,kind,subject;
    OrdinalDay day{0};
    std::optional<OrdinalDay> deadline;
    bool required=false,acknowledged=false;
    auto operator<=>(const ManagementNotice&) const = default;
};
struct ManagementState {
    ManagementCatalog catalog;
    std::vector<ManagementAcceptance> financing,specialists;
    std::vector<DelegationPolicy> policies;
    std::vector<DelegationDecision> decisions;
    std::vector<ManagementNotice> notices;
    std::optional<OrdinalDay> reviewed_on;
    auto operator<=>(const ManagementState&) const = default;
};
struct TakeFinancing { std::string id; };
struct HireSpecialist { std::string id; };
struct AddDelegationPolicy { DelegationPolicy policy; };
struct SetDelegationEnabled { std::string id;bool enabled; };
struct ReviewDelegation {};
struct AcknowledgeManagementNotice { std::string id; };
struct SliceScore {
    std::array<std::uint32_t,5> points{};
    std::uint32_t total=0;
    bool final=false;
    Cents profit{0};
    std::uint64_t delivered_tasks=0;
    BasisPoints founder_share{0};
};
void validate_management_catalog(const ManagementCatalog& catalog);
void validate_management(const SimulationState& state,bool deep=true);
void take_financing(SimulationState& state,std::string_view id);
void hire_specialist(SimulationState& state,std::string_view id,Cents protected_cash);
void add_delegation_policy(SimulationState& state,DelegationPolicy policy);
void set_delegation_enabled(SimulationState& state,std::string_view id,bool enabled);
void review_delegation(SimulationState& state,Cents protected_cash);
void prepare_management(SimulationState& state);
void acknowledge_management_notice(SimulationState& state,std::string_view id);
[[nodiscard]] bool required_management_notice(const SimulationState& state);
[[nodiscard]] SliceScore slice_score(const SimulationState& state);
}
