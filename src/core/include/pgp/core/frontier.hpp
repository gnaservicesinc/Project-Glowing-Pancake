#pragma once

#include <pgp/core/lab.hpp>
#include <functional>

namespace pgp::core {
struct SimulationState;
struct DeployModel;

inline constexpr std::size_t future_condition_count = 5;
inline constexpr std::array<std::string_view, future_condition_count> future_condition_names{
    "Progress", "Energy", "Social adaptation", "Cooperation", "Research"};
using FutureConditions = std::array<std::uint32_t, future_condition_count>;

inline constexpr std::size_t frontier_domain_count = 6;
inline constexpr std::size_t frontier_metric_count = 11;
inline constexpr std::array<std::string_view, frontier_metric_count> frontier_metric_names{
    "Documents", "Structured data", "Perception", "Software", "Reasoning", "Science",
    "Held-out transfer", "Reliability", "Autonomous workflows", "Novel science and engineering", "Control"};
// Independent Frontier qualification uses the two introductory task scores
// alongside its separately authored transfer, breadth and control metrics.
inline constexpr std::size_t frontier_extra_count = frontier_metric_count - introductory_lab_domains;
using FrontierScores = std::array<std::uint32_t, frontier_metric_count>;
using FrontierExtraScores = std::array<std::uint32_t, frontier_extra_count>;
inline constexpr std::size_t frontier_system_limit = 64;
inline constexpr std::size_t frontier_trial_limit = 512;
inline constexpr std::size_t frontier_bundle_limit = 4;
inline constexpr std::size_t frontier_policy_limit = 64;
inline constexpr std::size_t frontier_active_policy_limit = 8;
inline constexpr std::size_t frontier_queue_limit = 8;
inline constexpr std::uint32_t frontier_policy_run_limit = 12;
inline constexpr std::string_view frontier_refresh_prefix = "auto.refresh.";
inline constexpr Days frontier_assessment_span{90};
inline constexpr Days frontier_replication_span{180};
inline constexpr std::uint32_t frontier_agi_domain=7500, frontier_agi_transfer=7000,
    frontier_agi_reliability=8500, frontier_agi_workflows=8000, frontier_asi_domain=9000, frontier_asi_novelty=9500;

enum class FrontierTechnology { data, research, recipe, hardware };
enum class FrontierTrialKind { assessment, experiment, replication };
[[nodiscard]] std::string_view frontier_key(FrontierTechnology);
[[nodiscard]] std::string_view frontier_key(FrontierTrialKind);

struct FuturePath {
    std::string id, name, description;
    FutureConditions minimum{}, maximum{};
    BasisPoints ceiling{10000};
    auto operator<=>(const FuturePath&) const = default;
};
struct FrontierGate {
    FrontierTechnology kind = FrontierTechnology::research;
    std::string target;
    FutureConditions minimum{};
    auto operator<=>(const FrontierGate&) const = default;
};
struct FrontierCorpus {
    std::string id;
    FrontierExtraScores coverage{};
    auto operator<=>(const FrontierCorpus&) const = default;
};
struct FrontierRecipeProfile {
    std::string id;
    FrontierExtraScores baseline{}, gain{}, ceiling{};
    std::uint64_t data_scale = 1;
    auto operator<=>(const FrontierRecipeProfile&) const = default;
};
struct FrontierWorkTerms {
    Cents fee{0};
    MilliCU work{0};
    Days minimum_days{1};
    std::uint32_t minimum_staff = 1;
    auto operator<=>(const FrontierWorkTerms&) const = default;
};
struct FrontierPanel {
    std::string id, name, reviewer, holdout;
    FrontierWorkTerms terms;
    BasisPoints margin{0};
    auto operator<=>(const FrontierPanel&) const = default;
};
struct FrontierExperiment {
    std::string id, name, research, reviewer, verification;
    FrontierWorkTerms pilot, replication;
    BasisPoints baseline{0}, possible_gain{0}, required_gain{0}, margin{0};
    auto operator<=>(const FrontierExperiment&) const = default;
};
struct FrontierCatalog {
    std::string name, assumptions;
    OrdinalDay branches_after{0};
    bool asi_route = false;
    std::vector<FuturePath> paths;
    std::vector<FrontierGate> gates;
    std::vector<FrontierCorpus> corpora;
    std::vector<FrontierRecipeProfile> recipes;
    std::array<FrontierPanel, 3> panels{};
    std::array<FrontierExperiment, 3> experiments{};
    std::uint64_t minimum_field_tasks_per_day = 1;
    BasisPoints integration_loss{100};
    std::string refresh_research;
    Days refresh_review_age{180};
    auto operator<=>(const FrontierCatalog&) const = default;
};
struct FrontierSystem {
    std::uint64_t id = 0;
    std::string name;
    OrdinalDay registered_on{0};
    // One-based immutable deployment record positions, sorted and unique.
    std::vector<std::uint64_t> deployments;
    auto operator<=>(const FrontierSystem&) const = default;
};
struct FrontierResult {
    OrdinalDay observed_on{0};
    FrontierScores lower{};
    std::uint64_t field_tasks = 0;
    BasisPoints gain{0};
    bool valid = false;
    auto operator<=>(const FrontierResult&) const = default;
};
struct FrontierTrial {
    std::uint64_t id = 0, system = 0;
    FrontierTrialKind kind = FrontierTrialKind::assessment;
    std::string protocol, project;
    OrdinalDay starts_on{0};
    std::optional<FrontierResult> result;
    auto operator<=>(const FrontierTrial&) const = default;
};
struct FrontierBudget {
    LabAllocation allocation;
    Cents limit{0}, reserve{0};
    Days interval{1};
    std::uint32_t runs = 1;
    auto operator<=>(const FrontierBudget&) const = default;
};
struct FrontierResearchPlan {
    std::vector<std::string> topics;
    FrontierBudget budget;
    bool standing = false;
    auto operator<=>(const FrontierResearchPlan&) const = default;
};
struct FrontierResearchDecision {
    OrdinalDay day{0};
    // Empty only when a queued topic was completed outside the program.
    std::string project;
    Cents paid{0};
    auto operator<=>(const FrontierResearchDecision&) const = default;
};
struct FrontierResearchProgram {
    std::uint64_t id = 0;
    FrontierResearchPlan plan;
    OrdinalDay authorized_on{0};
    std::optional<OrdinalDay> cancelled_on;
    std::vector<FrontierResearchDecision> decisions;
    std::string waiting;
    auto operator<=>(const FrontierResearchProgram&) const = default;
};
struct FrontierRefreshPlan {
    std::string service, recipe, evaluation;
    std::vector<std::string> data;
    FrontierBudget budget;
    MilliCU work{0};
    BasisPoints improvement{100};
    auto operator<=>(const FrontierRefreshPlan&) const = default;
};
struct FrontierRefreshRun {
    std::uint64_t previous_deployment = 0, rollout_deployment = 0;
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> evaluated_on, finished_on;
    std::string outcome;
    auto operator<=>(const FrontierRefreshRun&) const = default;
};
struct FrontierRefreshProgram {
    std::uint64_t id = 0;
    FrontierRefreshPlan plan;
    OrdinalDay authorized_on{0};
    std::optional<OrdinalDay> cancelled_on;
    std::vector<FrontierRefreshRun> runs;
    std::string waiting;
    auto operator<=>(const FrontierRefreshProgram&) const = default;
};
struct FrontierState {
    FrontierCatalog catalog;
    std::uint64_t seed = 0;
    std::string path;
    FutureConditions conditions{};
    std::vector<FrontierSystem> systems;
    std::vector<FrontierTrial> trials;
    std::vector<FrontierResearchProgram> research_programs;
    std::vector<FrontierRefreshProgram> refresh_programs;
    std::optional<OrdinalDay> policies_reviewed_on;
    auto operator<=>(const FrontierState&) const = default;
};

struct RegisterFrontierSystem {
    std::string name;
    std::vector<std::string> services;
};
struct StartFrontierTrial {
    std::uint64_t system = 0;
    FrontierTrialKind kind = FrontierTrialKind::assessment;
    std::string protocol;
    LabAllocation allocation;
};
struct AuthorizeFrontierResearch { FrontierResearchPlan plan; };
struct AuthorizeFrontierRefresh { FrontierRefreshPlan plan; };
struct CancelFrontierPolicy { std::uint64_t id = 0; bool refresh = false; };
struct FrontierTrialQuote {
    Cents fee{0};
    MilliCU work{0};
    OrdinalDay earliest_result{0};
};
struct FrontierQualification {
    std::optional<OrdinalDay> agi_on, asi_on;
    std::uint32_t assessments = 0, improvements = 0;
    FrontierScores conservative{};
};

void validate_frontier_catalog(const FrontierCatalog&);
void validate_frontier_links(const FrontierCatalog&, const LabCatalog&, const std::vector<std::string>& hardware);
[[nodiscard]] FrontierState opening_frontier(const FrontierCatalog&, std::uint64_t seed);
[[nodiscard]] const FuturePath& future_path(const FrontierState&);
[[nodiscard]] std::string frontier_technology_block(const SimulationState&, FrontierTechnology, std::string_view, OrdinalDay);
void validate_frontier(const SimulationState&, bool deep);
void register_frontier_system(SimulationState&, const RegisterFrontierSystem&);
[[nodiscard]] FrontierTrialQuote quote_frontier_trial(const SimulationState&, const StartFrontierTrial&, Cents protected_cash);
void start_frontier_trial(SimulationState&, const StartFrontierTrial&, Cents protected_cash);
void prepare_frontier(SimulationState&);
[[nodiscard]] FrontierQualification frontier_qualification(const FrontierState&, std::uint64_t system,std::optional<OrdinalDay> through={});
[[nodiscard]] std::optional<OrdinalDay> earliest_frontier_qualification(const FrontierState&, bool asi);
void authorize_frontier_research(SimulationState&, FrontierResearchPlan);
void authorize_frontier_refresh(SimulationState&, FrontierRefreshPlan);
void cancel_frontier_policy(SimulationState&, const CancelFrontierPolicy&);
using FrontierLaunchGuard = std::function<std::string(const SimulationState&,const DeployModel&)>;
[[nodiscard]] bool review_frontier_policies(SimulationState&, Cents protected_cash, const FrontierLaunchGuard& = {});
[[nodiscard]] bool frontier_research_active(const FrontierResearchProgram&);
[[nodiscard]] bool frontier_refresh_active(const FrontierRefreshProgram&);
[[nodiscard]] std::string frontier_refresh_id(std::uint64_t program, std::size_t attempt);
} // namespace pgp::core
