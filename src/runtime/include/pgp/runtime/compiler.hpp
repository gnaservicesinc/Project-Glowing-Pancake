#pragma once

#include <pgp/content/collection.hpp>

#include <compare>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace pgp::runtime {

struct ScopeBinding {
    content::Scope scope;
    std::string id;
    auto operator<=>(const ScopeBinding&) const = default;
};

class CompileError : public std::runtime_error {
public:
    CompileError(std::string code, std::string path, std::string detail);
    [[nodiscard]] const std::string& code() const noexcept { return code_; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }
private:
    std::string code_;
    std::string path_;
};

struct CompileDiagnostic {
    std::string code;
    std::string path;
    std::string explanation;
};

struct CompiledProgram {
    content::Collection collection;
    content::Registry registry;
    std::vector<CompileDiagnostic> lints;
    // Canonical definition collection only; ordered effects/children/choices
    // retain their order. This is not an asset/localization payload hash.
    std::string canonical_content_bytes;
};

// Revalidates manually assembled typed input too. By default every registered
// effect is available; an executor supplies its actual supported-operation list
// to reject missing providers before starting a campaign.
[[nodiscard]] CompiledProgram compile_collection(
    const content::Collection& collection, const content::Registry& registry,
    std::optional<std::vector<std::string>> available_effects = std::nullopt);

enum class InspectionMode { author_fixture, player_known };
struct ClauseTrace {
    std::string path;
    std::string fact;
    ScopeBinding binding;
    content::Unit unit;
    content::Comparison comparison;
    std::optional<content::Scalar> actual;
    std::optional<content::Scalar> expected;
    std::optional<bool> result;
    bool redacted;
};
struct DurationObservation {
    core::OrdinalDay observed_on;
    std::optional<core::OrdinalDay> true_since;
};
using DurationReader = std::function<std::optional<DurationObservation>(std::string_view, const ScopeBinding&)>;
struct DurationTrace {
    std::string path;
    std::string predicate_id;
    ScopeBinding binding;
    std::uint32_t required_days;
    std::optional<core::OrdinalDay> observed_on;
    std::optional<core::OrdinalDay> true_since;
    std::optional<std::int64_t> elapsed_days;
    std::optional<bool> result;
    bool redacted = false;
};
struct RelatedMatch {
    ScopeBinding binding;
    std::optional<bool> result;
};
struct SelectionTrace {
    std::string path;
    std::string relation;
    ScopeBinding owner;
    content::QuantifierKind kind;
    std::optional<content::CountComparison> count;
    std::vector<RelatedMatch> members{};
    std::optional<std::size_t> minimum_matches{};
    std::optional<std::size_t> maximum_matches{};
    std::optional<bool> result{};
    bool redacted = false;
};
struct ConditionTrace {
    // Unknown in player mode when hidden clauses determine the outcome.
    // Temporal predicates also need a current saved observation to be known.
    std::optional<bool> result;
    std::vector<ClauseTrace> clauses;
    std::vector<DurationTrace> durations;
    std::vector<SelectionTrace> selections;
    std::size_t inspected_nodes = 0;
};
using FactReader = std::function<content::Scalar(const content::FactMetadata&, const ScopeBinding&)>;
using RelationReader = std::function<std::vector<ScopeBinding>(const content::RelationMetadata&, const ScopeBinding&)>;
inline constexpr std::size_t maximum_condition_inspection_nodes = 16'384;
struct ExpressionType { content::ValueType type; content::Unit unit; };
[[nodiscard]] ExpressionType validate_expression(const content::Expression& expression,
                                                content::Scope scope, const content::Registry& registry);
[[nodiscard]] std::optional<content::Scalar> evaluate_expression(
    const content::Expression& expression, const ScopeBinding& binding,
    const content::Registry& registry, const FactReader& read,
    InspectionMode mode = InspectionMode::author_fixture);

// Pure inspection: no random-stream or mutable-state handle is accepted.
// Every clause is inspected in authored order, including unselected clauses.
[[nodiscard]] ConditionTrace evaluate_condition(
    const content::Condition& condition, const ScopeBinding& binding,
    const content::Registry& registry, const FactReader& read,
    InspectionMode mode = InspectionMode::author_fixture,
    std::string root_path = "trigger",
    const DurationReader& durations = {},
    std::optional<core::OrdinalDay> inspection_day = std::nullopt,
    const RelationReader& relations = {});

struct ResolvedAnchor {
    core::OrdinalDay first_due;
    unsigned calendar_day;
    auto operator<=>(const ResolvedAnchor&) const = default;
};
// Calendar-day preference survives a short-month clamp. Runtime saves both
// fields and derives each later due date directly, never from the last clamp.
[[nodiscard]] ResolvedAnchor resolve_anchor(const content::Anchor& anchor,
                                           core::OrdinalDay campaign_start,
                                           core::OrdinalDay scope_created);
[[nodiscard]] core::OrdinalDay interval_due(const ResolvedAnchor& anchor,
                                          content::Interval interval,
                                          std::uint64_t occurrence_index);

} // namespace pgp::runtime
