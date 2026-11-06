#pragma once

#include <pgp/content/registry.hpp>
#include <pgp/content/market_setup.hpp>
#include <pgp/content/business_plan.hpp>
#include <pgp/content/lab_catalog.hpp>
#include <pgp/content/world_setup.hpp>
#include <pgp/content/management_catalog.hpp>
#include <pgp/content/infrastructure_catalog.hpp>
#include <pgp/content/resilience_catalog.hpp>
#include <pgp/content/civic_catalog.hpp>
#include <pgp/content/society_catalog.hpp>
#include <pgp/content/authority_catalog.hpp>
#include <pgp/core/calendar.hpp>
#include <pgp/core/random.hpp>
#include <pgp/core/units.hpp>

#include <memory>

namespace pgp::content {

inline constexpr std::uint32_t content_schema = 1;
inline constexpr std::size_t maximum_document_bytes = 1'048'576;

class ValidationError : public std::runtime_error {
public:
    ValidationError(std::string code, std::string path, std::string detail);
    [[nodiscard]] const std::string& code() const noexcept { return code_; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }
private:
    std::string code_;
    std::string path_;
};

enum class Comparison { equal, not_equal, less, less_equal, greater, greater_equal };
enum class ExpressionKind { literal, fact, add, subtract, scale };
struct Expression {
    ExpressionKind kind = ExpressionKind::literal;
    Scalar value{std::int64_t{0}};
    Unit unit = Unit::none;
    std::string fact{};
    std::vector<Expression> children{};
    std::uint32_t numerator = 0;
    std::uint32_t denominator = 1;
};
struct FactClause {
    std::string fact;
    Comparison comparison;
    Scalar value;
    Unit unit;
    std::optional<Expression> expression{};
};
enum class QuantifierKind { any, all, count };
struct CountComparison {
    Comparison comparison;
    std::uint32_t value;
};
struct Quantifier {
    QuantifierKind kind;
    std::string relation;
    std::optional<CountComparison> count{};
};
enum class ConditionKind { clause, all_of, any_of, not_condition, holds_for_days, quantified };
struct Condition {
    ConditionKind kind;
    std::optional<FactClause> clause;
    std::vector<Condition> children;
    std::string predicate_id{};
    std::uint32_t duration_days = 0;
    std::optional<Quantifier> quantifier{};
};
struct Effect {
    std::string operation;
    std::string target; // Explicit self/world; resolution retains its typed scope.
    Scope target_scope;
    std::map<std::string, Scalar, std::less<>> arguments;
    std::optional<Expression> value_expression{};
};

enum class LifecycleMode { once, recurring, interval, scheduled, continuous };
enum class IntervalUnit { days, calendar_months };
enum class AnchorKind { campaign_start, scope_created, month_start, fixed_date };
struct Interval { IntervalUnit unit; std::uint32_t value; };
struct Anchor {
    AnchorKind kind;
    std::optional<pgp::core::OrdinalDay> date;
    Interval offset{IntervalUnit::days, 0};
};
struct Lifecycle {
    LifecycleMode mode;
    Scope occurrence_scope;
    std::uint32_t max_occurrences = 1;
    std::uint32_t cooldown_days = 0;
    std::optional<Interval> interval;
    std::optional<Anchor> anchor;
    std::optional<pgp::core::OrdinalDay> due_date;
    std::optional<Condition> exit_when;
    bool allow_reentry = false;
};
struct Choice { std::string id; std::string label_key; std::string next; };
struct WeightedBranch { std::string id; std::uint32_t weight; std::string next; };
struct GroupPort {
    std::string name;
    std::string variable;
    bool output = false;
};
enum class NodeKind { end, effect, news, dialogue, result, delay, sequence, subflow, random };
struct Node {
    NodeKind kind;
    std::string next;
    std::string text_key;
    std::string title_key;
    std::vector<Effect> effects;
    std::vector<Choice> choices;
    std::string default_choice;
    std::vector<std::string> children;
    std::uint32_t delay_days = 0;
    std::string subflow_id;
    std::vector<WeightedBranch> branches;
    std::string random_stream;
    std::map<std::string, std::string, std::less<>> group_bindings{};
};
struct FlowDefinition {
    Scope scope;
    Condition trigger;
    Lifecycle lifecycle;
    std::string entry_node;
    std::string exit_node;
    std::map<std::string, Node, std::less<>> nodes;
    // Positive-delay graph cycles need an explicit finite execution budget.
    std::uint32_t max_node_visits = 0;
    std::vector<std::string> tags;
    std::string on_committed{};
    // A group can only be called. Its named ports alias explicitly bound
    // scoped variables for the duration of the saved subflow call.
    std::optional<std::vector<GroupPort>> group{};
};
[[nodiscard]] std::optional<core::RandomStream> random_branch_stream(std::string_view name);
[[nodiscard]] bool is_incident_stream(core::RandomStream stream) noexcept;
struct ScenarioDefinition {
    pgp::core::OrdinalDay starts_on;
    pgp::core::OrdinalDay ends_before;
    std::uint64_t seed;
    pgp::core::Cents cash;
    pgp::core::MilliCUPerDay leased_capacity;
    bool lease_energy_included;
    std::uint32_t team_people;
    std::vector<std::string> headquarters_choices;
    std::map<std::string, pgp::core::Cents, std::less<>> monthly_commitments;
    std::map<std::string, pgp::core::BasisPoints, std::less<>> ownership;
    std::vector<std::string> research;
    std::vector<std::string> flows;
    std::optional<MarketSetup> market{};
    std::optional<BusinessPlan> business_plan{};
    std::optional<core::LabCatalog> lab{};
    std::vector<core::BusinessOffer> business_offers{};
    std::vector<RivalDefinition> rivals{};
    // Deployable templates, not active products or prepaid serving workloads.
    std::optional<MarketSetup> service_catalog{};
    std::optional<core::ManagementCatalog> management{};
    std::optional<CompetitionSetup> competition{};
    std::optional<core::InfrastructureCatalog> infrastructure{};
    std::string regional_markets{};
    std::string resilience{};
    std::string civic{};
    std::string society{};
    std::string authority{};
};
struct ResearchDefinition {
    std::vector<std::string> all_of;
    std::vector<std::string> any_of;
    pgp::core::Cents cash;
    pgp::core::Days staff_days;
    pgp::core::MilliCU work;
};
struct DealDefinition { pgp::core::Cents minimum_payment; pgp::core::Days minimum_duration; };
using DefinitionData = std::variant<ScenarioDefinition, ResearchDefinition, FlowDefinition, DealDefinition,
    RegionalMarkets, core::ResilienceCatalog, core::CivicCatalog, core::SocietyCatalog, core::AuthorityCatalog>;
struct Definition {
    std::string id;
    std::uint32_t revision;
    DefinitionData data;
    bool deprecated = false;
};
using MessageTable = std::map<std::string,std::string,std::less<>>;
struct Collection {
    std::uint32_t schema;
    std::string content_version;
    std::map<std::string, Definition, std::less<>> definitions;
    std::vector<FactMetadata> variables{};
    MessageTable messages{};
    std::map<std::string,MessageTable,std::less<>> translations{};
    std::map<std::string,std::string,std::less<>> asset_records{};
    [[nodiscard]] const Definition* find(std::string_view id) const;
    [[nodiscard]] const std::string* message(std::string_view key, std::string_view locale = "en") const;
};

// The parser owns no files or sockets. Caller supplies a bounded local byte
// string. Success returns a fully validated typed collection, never partial data.
[[nodiscard]] Collection load_collection(std::string_view document, const Registry& registry);
// Shared static expression contract used by both the loader and compiler.
[[nodiscard]] std::pair<ValueType, Unit> expression_type(const Expression& expression, Scope scope, const Registry& registry);
[[nodiscard]] Registry registry_with_variables(const Registry& registry, const std::vector<FactMetadata>& variables);
[[nodiscard]] std::string_view definition_kind(const Definition& definition);
[[nodiscard]] const RegionalMarkets* regional_markets(const Collection& collection, const ScenarioDefinition& scenario);
[[nodiscard]] const core::ResilienceCatalog* resilience_catalog(const Collection&,const ScenarioDefinition&);
[[nodiscard]] const core::CivicCatalog* civic_catalog(const Collection&, const ScenarioDefinition&);
[[nodiscard]] const core::SocietyCatalog* society_catalog(const Collection&, const ScenarioDefinition&);
[[nodiscard]] const core::AuthorityCatalog* authority_catalog(const Collection&,const ScenarioDefinition&);
[[nodiscard]] MarketSetup scenario_market_setup(const Collection& collection, const ScenarioDefinition& scenario);
[[nodiscard]] std::vector<core::ServiceDefinition> scenario_services(const Collection& collection, const ScenarioDefinition& scenario);

} // namespace pgp::content
