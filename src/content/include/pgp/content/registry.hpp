#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pgp::content {

enum class Scope { world, company, state, capacity_source, project, contract };
enum class Unit { none, boolean, constant_2014_cents, basis_points, basis_point_delta,
                  days, calendar_months, people, milli_CU, milli_CU_per_day, count };
enum class ValueType { integer, boolean, text, reference };
enum class Boundary { opening, prepared_morning, work, settlement, committed_result };
enum class Visibility { player_known, hidden_truth };
enum class PortType { scope, condition, execution, value };
using Scalar = std::variant<std::int64_t, bool, std::string>;

struct PortMetadata {
    std::string name;
    PortType type;
    Scope scope;
    Unit unit;
    bool output;
};
struct ArgumentMetadata {
    std::string name;
    ValueType type;
    Unit unit;
    std::int64_t minimum = 0;
    std::int64_t maximum = 0;
    bool required = true;
    std::string reference_kind;
    std::string help;
    std::vector<std::string> choices;
};
struct FactMetadata {
    std::string id;
    Scope scope;
    ValueType type;
    Unit unit;
    std::int64_t minimum;
    std::int64_t maximum;
    std::string help;
    std::vector<PortMetadata> ports;
    Boundary boundary = Boundary::committed_result;
    Visibility visibility = Visibility::player_known;
    std::string provider = "foundation";
    std::vector<std::string> dependencies;
    std::optional<Scalar> default_value{};
};
struct EffectMetadata {
    std::string id;
    std::vector<Scope> target_scopes;
    std::vector<ArgumentMetadata> arguments;
    std::string help;
    std::vector<PortMetadata> ports;
    Boundary boundary = Boundary::committed_result;
    Visibility visibility = Visibility::player_known;
    std::string provider = "foundation";
    std::vector<std::string> dependencies;
};
struct RelationMetadata {
    std::string id;
    Scope owner_scope;
    Scope member_scope;
    std::size_t maximum_members;
    std::string help;
    Visibility visibility = Visibility::player_known;
    std::string provider = "foundation";
};
inline constexpr std::size_t maximum_relation_members = 4096;
struct CommittedFactMetadata {
    std::string id;
    Scope owner_scope;
    Scope subject_scope;
    std::string result_fact;
    std::string help;
    std::string provider = "foundation";
};

// Modules explicitly register vocabulary. Unknown operations never become an
// executable expression or a filesystem/network escape hatch.
class Registry {
public:
    Registry();
    void register_system(std::string id, std::vector<std::string> dependencies);
    void register_fact(FactMetadata metadata);
    void register_variable(FactMetadata metadata, Scalar default_value);
    void register_effect(EffectMetadata metadata);
    void register_relation(RelationMetadata metadata);
    void register_committed_fact(CommittedFactMetadata metadata);
    [[nodiscard]] const FactMetadata* fact(std::string_view id) const;
    [[nodiscard]] const EffectMetadata* effect(std::string_view id) const;
    [[nodiscard]] const RelationMetadata* relation(std::string_view id) const;
    [[nodiscard]] const CommittedFactMetadata* committed_fact(std::string_view id) const;
    [[nodiscard]] const auto& facts() const noexcept { return facts_; }
    [[nodiscard]] const auto& effects() const noexcept { return effects_; }
    [[nodiscard]] const auto& relations() const noexcept { return relations_; }
    [[nodiscard]] const auto& committed_facts() const noexcept { return committed_facts_; }
private:
    void validate_provider(std::string_view provider, const std::vector<std::string>& dependencies) const;
    std::map<std::string, std::vector<std::string>, std::less<>> systems_;
    std::map<std::string, FactMetadata, std::less<>> facts_;
    std::map<std::string, EffectMetadata, std::less<>> effects_;
    std::map<std::string, RelationMetadata, std::less<>> relations_;
    std::map<std::string, CommittedFactMetadata, std::less<>> committed_facts_;
};
[[nodiscard]] Registry foundation_registry();
[[nodiscard]] std::string_view scope_name(Scope scope);
[[nodiscard]] std::string_view unit_name(Unit unit);
[[nodiscard]] bool valid_id(std::string_view id);

} // namespace pgp::content
