#include "integration_fixture.hpp"

#include <pgp/persistence/save.hpp>
#include <nlohmann/json.hpp>

#include <iostream>

using namespace pgp;
namespace {
using Json = nlohmann::json;
using content::Scope;
using runtime::ScopeBinding;
const ScopeBinding company{Scope::company, "company.player"};
std::size_t checks = 0;
void check(bool valid, const char* detail) {
    ++checks;
    if (!valid) throw std::runtime_error(detail);
}
template<class Error, class Action> void refused(std::string_view code, Action action) {
    try { action(); }
    catch (const Error& error) {
        check(error.code() == code, error.what());
        return;
    }
    throw std::runtime_error("Expected refusal: " + std::string(code));
}
Json clause(const char* fact, Json value = true) { return {{"fact", fact}, {"op", "eq"}, {"value", std::move(value)}}; }
Json related(const char* relation, const char* match, Json where) {
    return {{"related", {{"relation", relation}, {"match", match}, {"where", std::move(where)}}}};
}
Json count(const char* relation, const char* operation, int value, Json where) {
    auto result = related(relation, "count", std::move(where));
    result["related"]["op"] = operation;
    result["related"]["value"] = value;
    return result;
}
Json document(const Json& trigger) {
    std::ifstream input(PGP_MARKET_FIXTURE_PATH);
    auto value = Json::parse(input);
    value["definitions"][0]["flows"] = {"flow.selection"};
    value["definitions"].push_back({
        {"id", "flow.selection"}, {"kind", "flow"}, {"revision", 1}, {"scope", "company"},
        {"trigger", trigger}, {"lifecycle", {{"mode", "once"}, {"max_occurrences_per_scope", 1}}},
        {"entry_node", "record"}, {"nodes", {
            {"record", {{"kind", "effect"}, {"next", "end"},
                {"effects", Json::array({{{"op", "record_decision"}, {"target", "self"}, {"value", "selection.fired"}}})}}},
            {"end", {{"kind", "end"}}}}}});
    return value;
}
runtime::CompiledProgram program(const Json& trigger, const content::Registry& registry) {
    const auto loaded = content::load_collection(document(trigger).dump(), registry);
    return runtime::compile_collection(loaded, registry, runtime::supported_effects());
}
const content::Condition& predicate(const runtime::CompiledProgram& compiled) {
    return std::get<content::FlowDefinition>(compiled.collection.definitions.at("flow.selection").data).trigger;
}
runtime::ConditionTrace inspect(const Json& trigger, const content::Registry& registry, const runtime::FactReader& facts,
                               const runtime::RelationReader& relations, runtime::InspectionMode mode = runtime::InspectionMode::author_fixture) {
    const auto compiled = program(trigger, registry);
    return runtime::evaluate_condition(predicate(compiled), company, compiled.registry, facts, mode, "trigger", {}, {}, relations);
}
void quantifier_semantics() {
    auto registry = content::foundation_registry();
    registry.register_relation({"company.private_projects", Scope::company, Scope::project, 8, "Private list.",
        content::Visibility::hidden_truth});
    registry.register_fact({"project.private_match", Scope::project, content::ValueType::boolean, content::Unit::boolean,
        0, 1, "Hidden match.", {{"value", content::PortType::value, Scope::project, content::Unit::boolean, true}},
        content::Boundary::committed_result, content::Visibility::hidden_truth, "foundation", {}});
    const runtime::RelationReader members = [](const auto&, const auto&) {
        return std::vector<ScopeBinding>{{Scope::project, "Zeta project"}, {Scope::project, "Alpha project"}};
    };
    unsigned hidden_reads = 0;
    const runtime::FactReader facts = [&](const auto& metadata, const auto& binding) -> content::Scalar {
        if (metadata.id == "project.private_match") ++hidden_reads;
        return binding.id == "Alpha project" || metadata.id == "project.private_match";
    };
    const auto active = clause("project.is_active");
    const auto any = inspect(related("company.projects", "any", active), registry, facts, members);
    check(any.result == true && any.selections[0].minimum_matches == 1 && any.selections[0].maximum_matches == 1,
          "Any must retain exact match counts");
    check(any.selections[0].members[0].binding.id == "Alpha project" && any.clauses[1].binding.id == "Zeta project",
          "Every member must be inspected in canonical ID order");
    check(inspect(related("company.projects", "all", active), registry, facts, members).result == false, "All must include false members");
    check(inspect(count("company.projects", "eq", 1, active), registry, facts, members).result == true, "Count equality boundary");
    check(inspect(count("company.projects", "gt", 1, active), registry, facts, members).result == false, "Count strict boundary");
    const runtime::RelationReader empty = [](const auto&, const auto&) { return std::vector<ScopeBinding>{}; };
    check(inspect(related("company.projects", "any", active), registry, facts, empty).result == false, "Empty any is false");
    check(inspect(related("company.projects", "all", active), registry, facts, empty).result == true, "Empty all is true");
    check(inspect(count("company.projects", "eq", 0, active), registry, facts, empty).result == true, "Empty count is zero");

    const auto hidden = clause("project.private_match");
    const auto partial = Json{{"any_of", Json::array({active, hidden})}};
    const auto mode = runtime::InspectionMode::player_known;
    const auto mixed = inspect(count("company.projects", "eq", 1, partial), registry, facts, members, mode);
    check(!mixed.result && mixed.selections[0].minimum_matches == 1 && mixed.selections[0].maximum_matches == 2,
          "Hidden facts leave exact counts unknown but retain public bounds");
    check(inspect(count("company.projects", "gte", 1, partial), registry, facts, members, mode).result == true,
          "A public lower bound can settle the comparison");
    check(!inspect(count("company.projects", "ne", 1, hidden), registry, facts, members, mode).result,
          "An unknown interior value must keep inequality unknown even if both endpoints agree");
    check(hidden_reads == 0, "Player inspection must skip all hidden fact reads");
    const auto private_list = inspect(related("company.private_projects", "all", active), registry, facts,
        [](const auto&, const auto&) -> std::vector<ScopeBinding> { throw std::runtime_error("Hidden relation read"); }, mode);
    check(!private_list.result && private_list.selections[0].redacted && private_list.selections[0].members.empty() &&
          !private_list.selections[0].maximum_matches && private_list.clauses.empty(), "Hidden membership must be redacted");

    const auto invalid_members = [&](std::vector<ScopeBinding> bindings, std::string_view code) {
        refused<runtime::CompileError>(code, [&] {
            (void)inspect(related("company.private_projects", "any", active), registry, facts,
                [&](const auto&, const auto&) { return bindings; });
        });
    };
    invalid_members({{Scope::project, "same"}, {Scope::project, "same"}}, "scope");
    invalid_members({{Scope::contract, "wrong"}}, "scope");
    invalid_members(std::vector<ScopeBinding>(9, {Scope::project, "excess"}), "budget");
    const runtime::RelationReader many = [](const auto&, const auto&) {
        std::vector<ScopeBinding> result;
        for (std::size_t i = 0; i < content::maximum_relation_members; ++i)
            result.push_back({Scope::project, "project." + std::to_string(i)});
        return result;
    };
    refused<runtime::CompileError>("budget", [&] {
        (void)inspect(related("company.projects", "any", {{"all_of", Json::array({active, active, active, active})}}),
                      registry, facts, many);
    });
}
void loader_validation() {
    const auto registry = content::foundation_registry();
    const auto invalid = [&](const Json& trigger, std::string_view code) {
        refused<content::ValidationError>(code, [&] { (void)content::load_collection(document(trigger).dump(), registry); });
    };
    const auto active = clause("project.is_active");
    invalid(related("company.unregistered", "any", active), "unknown_relation");
    invalid(related("capacity_source.projects", "any", active), "scope");
    invalid(related("company.projects", "all", clause("company.is_operating")), "scope");
    invalid(related("company.projects", "count", active), "missing_field");
    invalid(count("company.projects", "eq", -1, active), "range");
    invalid(count("company.contracts", "eq", 257, clause("contract.is_active")), "range");
    invalid(related("company.projects", "any", {{"holds_for_days", {{"id", "member"}, {"days", 1}, {"condition", active}}}}), "scope");
    auto invalid_payload = program(related("company.projects", "any", active), registry).collection;
    auto& root = std::get<content::FlowDefinition>(invalid_payload.definitions.at("flow.selection").data).trigger;
    root.quantifier->count = content::CountComparison{content::Comparison::equal, 0};
    refused<runtime::CompileError>("typed_shape", [&] { (void)runtime::compile_collection(invalid_payload, registry); });
}
void live_save_and_resume() {
    const auto registry = content::foundation_registry();
    const auto completed = related("company.capacity_sources", "any",
        count("capacity_source.projects", "eq", 1, clause("project.is_completed")));
    const auto trigger = Json{{"all_of", Json::array({
        related("company.contracts", "all", clause("contract.is_active")),
        {{"holds_for_days", {{"id", "completed_project"}, {"days", 1}, {"condition", completed}}}}
    })}};
    const auto compiled = program(trigger, registry);
    check(runtime::compile_collection(content::load_collection(compiled.canonical_content_bytes, registry), registry)
              .canonical_content_bytes == compiled.canonical_content_bytes, "Canonical relation and threshold survive reopen");
    auto initial = runtime::begin_scenario(compiled, "scenario.enterprise", "US-CA");
    // A second empty source must not inherit the first source's project.
    initial.world.simulation.capacity->sources.push_back({"empty.source", "empty.physical", company.id, "supplier",
        core::CapacitySourceKind::leased, core::MilliCUPerDay{1}, core::MilliCUPerDay{1}, 0, "general", true});
    runtime::Runtime live{compiled, initial};
    integration_fixture::prepare(live);
    core::CapacityProject row;
    row.id = "Train model";
    row.source_id = "lease.initial";
    row.staff = 1;
    row.total_work = core::MilliCU{1};
    row.requested = core::MilliCUPerDay{1};
    row.cause = "One-day training fixture";
    integration_fixture::require_accepted(live.submit({live.checkpoint().world.simulation.revision, core::ReserveProject{row}}));
    integration_fixture::step(live);
    const auto saved = live.checkpoint();
    check(saved.world.decisions.empty() && saved.runtime.duration_states.at(0).true_since == saved.world.simulation.date,
          "Actual completed work begins the aggregate timer at zero elapsed days");
    const auto bytes = persistence::encode_save(compiled, saved);
    runtime::Runtime resumed{compiled, persistence::decode_save(compiled, bytes).checkpoint};
    check(resumed.checkpoint() == saved, "Aggregate timer and owning business rows reload exactly");
    integration_fixture::step(live);
    const auto result = resumed.submit({saved.world.simulation.revision, core::AdvanceDay{}});
    integration_fixture::require_accepted(result);
    const auto final = resumed.checkpoint();
    check(final == live.checkpoint() && final.world.decisions == std::vector<std::string>{"selection.fired"},
          "Loaded and uninterrupted selectors fire once at the same boundary");
    check(final.world.simulation.random == initial.world.simulation.random, "Selecting members must not consume RNG");
    check(result.selections.size() == 4 && result.selections[0].members[0].binding.id == "contract.documents" &&
          result.selections[2].owner.id == "empty.source" && result.selections[2].members.empty() &&
          result.selections[3].minimum_matches == 1 && result.selections[3].members.size() == 2,
          "Runtime reports the actual contract, isolated source membership and matching completed project");
    integration_fixture::step(resumed);
    check(resumed.checkpoint().world.decisions.size() == 1, "Later inspection must not duplicate a once-only effect");
}
} // namespace

int main() {
    try {
        quantifier_semantics();
        loader_validation();
        live_save_and_resume();
        std::cout << checks << " selector checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
