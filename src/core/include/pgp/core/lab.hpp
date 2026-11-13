#pragma once

#include <pgp/core/accounts.hpp>
#include <pgp/core/capacity.hpp>
#include <pgp/core/random.hpp>

#include <array>
#include <map>
#include <span>

namespace pgp::core {
// Keep the introductory axes first in authored profiles, saves and old evidence streams.
inline constexpr std::size_t introductory_lab_domains = 2;
inline constexpr std::array<std::string_view,6> lab_domain_names{
    "documents", "scanned_tables", "software_reasoning", "science_industry", "social_interaction", "autonomous_workflows"};
inline constexpr std::size_t lab_domain_count = lab_domain_names.size();
inline constexpr std::array<std::string_view,lab_domain_count> lab_domain_labels{
    "Documents", "Scanned tables", "Software and reasoning", "Science and industry", "Social interaction", "Autonomous workflows"};
using LabProfile = std::array<BasisPoints, lab_domain_count>;
inline constexpr LabProfile empty_lab_profile{
    BasisPoints{0}, BasisPoints{0}, BasisPoints{0}, BasisPoints{0}, BasisPoints{0}, BasisPoints{0}};
[[nodiscard]] constexpr std::size_t stored_lab_domains(const LabProfile& profile) {
    auto count=profile.size();
    while (count>introductory_lab_domains && profile[count-1].value()==0) --count;
    return count;
}
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
struct ResearchSetback {
    BasisPoints failure_chance{0}, retry_credit{0};
    std::string safeguard, finding;
    auto operator<=>(const ResearchSetback&) const = default;
};
struct ResearchInnovation {
    std::string previous;
    std::map<std::string,BasisPoints> inference_savings;
    auto operator<=>(const ResearchInnovation&) const = default;
};
struct LabResearch {
    std::string id;
    std::vector<std::string> all_of, any_of;
    Cents cash{0};
    Days staff_days{0};
    MilliCU experiment_work{0};
    std::uint32_t repeat_limit = 1;
    std::optional<ResearchSetback> setback{};
    std::optional<ResearchInnovation> innovation{};
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
    std::uint64_t sample_budget=0;
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
struct SyntheticRecipe {
    std::string id,name;
    std::vector<std::string> requires_research;
    std::size_t domain=0;
    Cents fee{0};
    MilliCU verification_per_unit{0};
    Days staff_days{1};
    std::uint64_t maximum_units=0;
    BasisPoints diversity{0},minimum_teacher{0};
    auto operator<=>(const SyntheticRecipe&) const = default;
};
struct FeedbackConsent {
    std::string product_id, segment_id;
    BasisPoints opted_in_share{0};
    auto operator<=>(const FeedbackConsent&) const = default;
};
struct DataAcquisitionOffer {
    // Price is the commissioning fee. For feedback, unique_units is a cap.
    DataOffer dataset;
    Days collection_days{1}, staff_days{1};
    MilliCU review_work{0};
    std::optional<FeedbackConsent> feedback{};
    auto operator<=>(const DataAcquisitionOffer&) const = default;
};
struct CreativeResearchRule {
    int earliest_year=2015;
    std::uint32_t difficulty=1,coordination_bonus_bp=0;
    bool replaces_frontier_calendar=false;
    auto operator<=>(const CreativeResearchRule&) const = default;
};
struct ResearchPacing {
    std::uint32_t threshold=0, missing_groups=0, prerequisite_groups=0;
    std::uint32_t expected_offset=0, early_multiplier=1;
};
struct LabCatalog {
    std::vector<DataOffer> data;
    std::vector<LabResearch> research;
    std::vector<TrainingRecipe> recipes;
    std::vector<EvaluationPlan> evaluations;
    std::vector<SyntheticRecipe> synthesis{};
    std::vector<DataAcquisitionOffer> acquisition{};
    // When present, all research uses named equipped staff. Listed topics
    // additionally require the specified equipment together in a room.
    std::optional<std::map<std::string,std::vector<std::string>>> research_workplaces{};
    std::map<std::string,CreativeResearchRule> creative_research{};
    auto operator<=>(const LabCatalog&) const = default;
};
struct DataLicense {
    std::string offer_id;
    OrdinalDay acquired_on{0};
    auto operator<=>(const DataLicense&) const = default;
};
struct ResearchCooperation {
    std::string agreement,partner;
    BasisPoints credit{0};
    auto operator<=>(const ResearchCooperation&) const=default;
};
struct ResearchRun {
    std::string research_id, project_id;
    bool completed = false;
    BasisPoints insight_credit{0};
    Cents sponsored_fee{0};
    bool failed = false;
    BasisPoints retry_credit{0};
    std::vector<std::string> researchers{};
    std::uint32_t creativity=0,compatibility_bp=0;
    std::optional<ResearchCooperation> cooperation{};
    // Preserve the launch team's evidence when a person's live traits change.
    std::map<std::string,std::vector<std::string>,std::less<>> original_traits{};
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
using DataMixWeights=std::map<std::string,std::uint32_t>;
struct LabModel {
    std::string id, recipe_id, parent_id, project_id;
    std::vector<std::string> data_ids;
    OrdinalDay created_on{0};
    MilliCU inherited_work{0}, consumed_work{0};
    // Authoritative truth stays out of LabReport and all player previews.
    LabProfile capability = empty_lab_profile;
    // Immutable snapshot of the parent's settled work at this capacity-day cursor.
    std::optional<std::uint64_t> checkpoint_cursor{};
    DataMixWeights data_weights{};
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
struct GeneratedDataset {
    std::string id,recipe_id,teacher_id,evidence_id,verification_id,project_id;
    OrdinalDay started_on{0};
    MilliCU teacher_work{0};
    std::uint64_t attempted_units=0;
    std::optional<DataOffer> output{};
    auto operator<=>(const GeneratedDataset&) const = default;
};
struct DataAcquisition {
    std::string offer_id, project_id;
    std::optional<std::uint64_t> settled_deliveries{};
    std::optional<DataOffer> output{};
    auto operator<=>(const DataAcquisition&) const = default;
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
    std::vector<GeneratedDataset> generated{};
    std::vector<DataAcquisition> acquisitions{};
    BasisPoints setback_risk_scale_bp{10000};
    BasisPoints research_work_scale_bp{10000};
    auto operator<=>(const LabState&) const = default;
};
struct TrainingRequest {
    std::string model_id, recipe_id, parent_id;
    std::vector<std::string> data_ids;
    std::string source_id;
    MilliCUPerDay daily_compute{0};
    std::uint32_t staff = 0;
    MilliCU work{0};
    DataMixWeights data_weights{};
};
struct LabAllocation {
    std::string source_id;
    MilliCUPerDay daily_compute{0};
    std::uint32_t staff = 0;
    std::vector<std::string> researchers{};
    auto operator<=>(const LabAllocation&) const = default;
};
struct GenerateDataset {
    std::string id,recipe_id,teacher_id,evidence_id,verification_id;
    std::uint64_t attempted_units=0;
    LabAllocation allocation;
};
struct StartDataAcquisition {
    std::string offer_id;
    LabAllocation allocation;
};
struct DataAcquisitionPreview {
    Cents fee{0};
    MilliCU work{0};
    Days minimum_days{0};
    OrdinalDay collection_ends_before{0};
};
struct SyntheticPreview {
    Cents fee{0};
    MilliCU work{0};
    Days minimum_days{0};
    std::uint64_t retained_lower=0,retained_upper=0,diversity_remaining=0;
    bool commercial_use=false,open_weight_release=false;
};
struct DataMixSlice {
    std::string id,name;
    std::uint32_t weight=1;
    std::uint64_t planned=0,retained=0;
};
struct DataMixPreview {
    std::uint64_t budget=0,retained=0;
    std::vector<DataMixSlice> sources;
    std::array<std::uint64_t,lab_domain_count> effective_units{};
    LabProfile saturation=empty_lab_profile;
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
void validate_synthetic_catalog(const LabCatalog&);
void validate_acquisition_catalog(const LabCatalog&);
[[nodiscard]] const DataOffer& lab_data(const LabState&,std::string_view id);
[[nodiscard]] std::optional<OrdinalDay> data_available_on(const LabState&,const CapacityState&,std::string_view id);
struct SimulationState;
[[nodiscard]] DataAcquisitionPreview preview_data_acquisition(const SimulationState&,const StartDataAcquisition&);
void start_data_acquisition(SimulationState&,const StartDataAcquisition&,Cents protected_cash);
void close_data_acquisitions(SimulationState&);
void validate_data_acquisitions(const SimulationState&,bool deep);
[[nodiscard]] SyntheticPreview preview_synthesis(const LabState&,const CapacityState&,OrdinalDay,const GenerateDataset&);
void generate_dataset(LabState&,AccountsState&,CapacityState&,OrdinalDay,const GenerateDataset&,Cents protected_cash);
void close_synthetic_data(LabState&,const CapacityState&,OrdinalDay);
void validate_synthetic_data(const LabState&,const CapacityState&,const AccountsState&,bool deep);
void checkpoint_model(LabState&,const CapacityState&,OrdinalDay,std::string_view source_id,std::string id);
void validate_lab(const LabState& state, const CapacityState& capacity, const AccountsState& accounts,
                  const RandomSnapshot& random, bool deep = true);
[[nodiscard]] LabProfile model_capability(const LabCatalog& catalog, const LabModel& model,
    std::span<const GeneratedDataset> generated={},std::span<const DataAcquisition> acquired={});
[[nodiscard]] DataMixPreview preview_data_mix(const LabState&,const TrainingRequest&);
[[nodiscard]] TrainingPreview preview_training(const LabState& state, const TrainingRequest& request);
[[nodiscard]] LabReport lab_report(const LabState& state, const CapacityState& capacity);
// Mutators operate inside Simulation's copied transaction; costs and capacity
// are committed together or the whole command is refused.
void buy_data(LabState& state, AccountsState& accounts, OrdinalDay day, std::string_view offer_id, Cents protected_cash);
void start_research(LabState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day,
                    std::string_view research_id, const LabAllocation& allocation, Cents protected_cash,
                    Cents sponsored_fee=Cents{0},std::uint32_t creativity=0,std::uint32_t compatibility_bp=0,
                    std::optional<ResearchCooperation> cooperation={});
[[nodiscard]] std::uint32_t research_creativity_threshold(const LabState&,const CapacityState&,std::string_view,OrdinalDay);
[[nodiscard]] ResearchPacing research_pacing(const LabState&,const CapacityState&,std::string_view,OrdinalDay);
[[nodiscard]] MilliCU research_inference_cost(const LabState&,const CapacityState&,const TrainingRecipe&,OrdinalDay);
[[nodiscard]] std::uint32_t coordinated_staff_days(const LabCatalog&,const LabResearch&,std::uint32_t compatibility_bp,BasisPoints cooperation=BasisPoints{0});
void publish_research(LabState& state, const CapacityState& capacity, OrdinalDay day, std::string_view research_id);
void acquire_research_insight(LabState& state, OrdinalDay day, ResearchInsight insight);
void start_training(LabState& state, CapacityState& capacity, OrdinalDay day, const TrainingRequest& request);
void clone_model(LabState& state, const CapacityState& capacity, OrdinalDay day,
                 std::string_view source_id, std::string id);
void start_evaluation(LabState& state, AccountsState& accounts, CapacityState& capacity, OrdinalDay day,
                      std::string id, std::string_view model_id, std::string_view plan_id,
                      const LabAllocation& allocation, Cents protected_cash, std::string specialist_id={});
void close_lab_day(LabState& state, const CapacityState& capacity, OrdinalDay day, RandomSnapshot& random);
} // namespace pgp::core
