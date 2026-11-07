#pragma once

#include <pgp/core/accounts.hpp>
#include <pgp/core/capacity.hpp>
#include <pgp/core/random.hpp>

#include <array>

namespace pgp::core {
// The slice has one customer task and a deliberately separate weak subgroup.
inline constexpr std::size_t lab_domain_count = 2;
inline constexpr std::array<std::string_view, lab_domain_count> lab_domain_names{"documents", "scanned_tables"};
using LabProfile = std::array<BasisPoints, lab_domain_count>;
inline constexpr LabProfile empty_lab_profile{BasisPoints{0}, BasisPoints{0}};
inline constexpr std::size_t lab_catalog_limit = 64;
inline constexpr std::size_t lab_record_limit = 1024;

struct DataOffer {
    std::string id, name, provenance, rights;
    Cents price{0};
    std::uint64_t unique_units = 0;
    BasisPoints quality{0};
    LabProfile relevance = empty_lab_profile;
    LabProfile coverage = empty_lab_profile;
    BasisPoints contamination{0};
    bool commercial_use = true;
    bool open_weight_release = false;
    auto operator<=>(const DataOffer&) const = default;
};
struct LabResearch {
    std::string id;
    std::vector<std::string> all_of, any_of;
    Cents cash{0};
    Days staff_days{0};
    MilliCU experiment_work{0};
    std::uint32_t repeat_limit = 1;
    auto operator<=>(const LabResearch&) const = default;
};
struct TrainingRecipe {
    std::string id, name;
    std::vector<std::string> requires_research;
    LabProfile baseline = empty_lab_profile, gain = empty_lab_profile, ceiling = empty_lab_profile;
    LabProfile fit = empty_lab_profile;
    std::array<std::uint64_t, lab_domain_count> data_scale{};
    MilliCU compute_scale{0}, minimum_work{0}, maximum_work{0};
    BasisPoints preview_uncertainty{0};
    MilliCU inference_per_task{0};
    std::uint64_t minimum_memory_mib = 0;
    std::string interconnect = "general";
    auto operator<=>(const TrainingRecipe&) const = default;
};
struct EvaluationPlan {
    std::string id, name;
    std::vector<std::string> requires_research;
    LabProfile coverage = empty_lab_profile;
    LabProfile required = empty_lab_profile;
    BasisPoints uncertainty{0};
    bool independent = true;
    MilliCU work{0};
    Cents cash{0};
    auto operator<=>(const EvaluationPlan&) const = default;
};
struct LabCatalog {
    std::vector<DataOffer> data;
    std::vector<LabResearch> research;
    std::vector<TrainingRecipe> recipes;
    std::vector<EvaluationPlan> evaluations;
    auto operator<=>(const LabCatalog&) const = default;
};
struct DataLicense {
    std::string offer_id;
    OrdinalDay acquired_on{0};
    auto operator<=>(const DataLicense&) const = default;
};
struct ResearchRun {
    std::string research_id, project_id;
    bool completed = false;
    BasisPoints insight_credit{0};
    Cents sponsored_fee{0};
    auto operator<=>(const ResearchRun&) const = default;
};
struct ResearchPublication {
    std::string research_id;
    OrdinalDay day{0};
    auto operator<=>(const ResearchPublication&) const = default;
};
struct ResearchInsight {
    std::string research_id,publisher;
    OrdinalDay published_on{0},observed_on{0};
    BasisPoints credit{0};
    auto operator<=>(const ResearchInsight&) const = default;
};
struct LabModel {
    std::string id, recipe_id, parent_id, project_id;
    std::vector<std::string> data_ids;
    OrdinalDay created_on{0};
    MilliCU inherited_work{0}, consumed_work{0};
    // Authoritative truth stays out of LabReport and all player previews.
    LabProfile capability = empty_lab_profile;
    // Immutable snapshot of the parent's settled work at this capacity-day cursor.
    std::optional<std::uint64_t> checkpoint_cursor{};
    auto operator<=>(const LabModel&) const = default;
};
struct EvaluationEvidence {
    std::string id, model_id, plan_id, project_id;
    MilliCU model_work{0};
    bool completed = false;
    LabProfile measured = empty_lab_profile, lower = empty_lab_profile, upper = empty_lab_profile;
    std::vector<std::string> findings;
    std::uint64_t stream_before = 0, stream_after = 0;
    auto operator<=>(const EvaluationEvidence&) const = default;
};
struct LabState {
    std::string owner = "company.player";
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> closed_through;
    std::uint64_t research_stream_origin = 0;
    LabCatalog catalog;
    std::vector<DataLicense> licenses;
    std::vector<ResearchRun> research;
    std::vector<LabModel> models;
    std::vector<EvaluationEvidence> evaluations;
    std::vector<ResearchPublication> publications{};
    std::vector<ResearchInsight> insights{};
    auto operator<=>(const LabState&) const = default;
};
struct TrainingRequest {
    std::string model_id, recipe_id, parent_id;
    std::vector<std::string> data_ids;
    std::string source_id;
    MilliCUPerDay daily_compute{0};
    std::uint32_t staff = 0;
    MilliCU work{0};
};
struct LabAllocation {
    std::string source_id;
    MilliCUPerDay daily_compute{0};
    std::uint32_t staff = 0;
    auto operator<=>(const LabAllocation&) const = default;
};
struct TrainingPreview {
    Days minimum_days{0};
    Cents unowned_data_cost{0};
    LabProfile lower = empty_lab_profile, upper = empty_lab_profile;
    std::array<std::string, lab_domain_count> bottlenecks;
    bool licensed = false, prerequisites_met = false;
    std::string explanation;
};
struct LabModelView {
    std::string id, recipe_id, parent_id;
    MilliCU consumed_work{0};
    bool training = false;
    bool paused = false;
};
struct LabReport {
    std::vector<DataOffer> data_offers;
    std::vector<DataLicense> licenses;
    std::vector<ResearchRun> research;
    std::vector<LabModelView> models;
    struct EvidenceView {
        std::string id, model_id, plan_id;
        bool completed;
        LabProfile measured, lower, upper, coverage;
        std::vector<std::string> findings;
        bool independent;
    };
    std::vector<EvidenceView> evidence;
};

void validate_lab_catalog(const LabCatalog& catalog);
void checkpoint_model(LabState&,const CapacityState&,OrdinalDay,std::string_view source_id,std::string id);
void validate_lab(const LabState& state, const CapacityState& capacity, const AccountsState& accounts,
                  const RandomSnapshot& random, bool deep = true);
[[nodiscard]] LabProfile model_capability(const LabCatalog& catalog, const LabModel& model);
[[nodiscard]] TrainingPreview preview_training(const LabState& state, const TrainingRequest& request);
[[nodiscard]] LabReport lab_report(const LabState& state, const CapacityState& capacity);
// Mutators operate inside Simulation's copied transaction; costs and capacity
// are committed together or the whole command is refused.
void buy_data(LabState& state, AccountsState& accounts, OrdinalDay day, std::string_view offer_id, Cents protected_cash);
void start_research(LabState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day,
                    std::string_view research_id, const LabAllocation& allocation, Cents protected_cash,
                    Cents sponsored_fee=Cents{0});
void publish_research(LabState& state, const CapacityState& capacity, OrdinalDay day, std::string_view research_id);
void acquire_research_insight(LabState& state, OrdinalDay day, ResearchInsight insight);
void start_training(LabState& state, CapacityState& capacity, OrdinalDay day, const TrainingRequest& request);
void clone_model(LabState& state, const CapacityState& capacity, OrdinalDay day,
                 std::string_view source_id, std::string id);
void start_evaluation(LabState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day,
                      std::string id, std::string_view model_id, std::string_view plan_id,
                      const LabAllocation& allocation, Cents protected_cash);
void close_lab_day(LabState& state, const CapacityState& capacity, OrdinalDay day, RandomSnapshot& random);
} // namespace pgp::core
