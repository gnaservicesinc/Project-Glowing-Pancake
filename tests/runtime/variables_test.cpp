#include <pgp/persistence/save.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>

using namespace pgp;
namespace {
using Json = nlohmann::json;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
Json literal(content::Scalar value, const char* unit) {
    return {{"literal", std::visit([](const auto& scalar) { return Json(scalar); }, value)}, {"unit", unit}};
}
Json assignment(const char* variable, Json value) {
    return {{"op", "set_variable"}, {"target", "self"}, {"variable", variable}, {"value", std::move(value)}};
}
Json fixture() {
    const Json offers{{"fact", "story.offers"}};
    const Json once{{"mode", "once"}, {"max_occurrences_per_scope", 1}};
    const Json initial{{"fact", "story.offers"}, {"op", "eq"}, {"value", 0}};
    const Json effects = Json::array({
        assignment("story.offers", {{"add", Json::array({offers, literal(std::int64_t{2}, "count")})}}),
        assignment("story.ready", literal(true, "boolean")),
        assignment("story.offers", {{"scale", {{"value", offers}, {"numerator", 3}, {"denominator", 2}}}})});
    return {{"content_schema", 1}, {"content_version", "0.0.0-dev"}, {"currency_basis", "constant_2014_cents"},
        {"variables", Json::array({
            {{"id", "story.offers"}, {"scope", "company"}, {"type", "integer"}, {"unit", "count"},
             {"minimum", 0}, {"maximum", 10}, {"default", 0}, {"help", "Offers made."}},
            {{"id", "story.threshold"}, {"scope", "company"}, {"type", "integer"}, {"unit", "count"},
             {"minimum", 0}, {"maximum", 10}, {"default", 2}, {"help", "Offer threshold."}},
            {{"id", "story.ready"}, {"scope", "company"}, {"type", "boolean"}, {"unit", "boolean"},
             {"minimum", 0}, {"maximum", 1}, {"default", false}, {"help", "Offer preparation flag."}}})},
        {"definitions", Json::array({
            {{"id", "flow.a.assign"}, {"kind", "flow"}, {"revision", 1}, {"scope", "company"},
             {"trigger", initial}, {"lifecycle", once}, {"entry_node", "assign"},
             {"nodes", {{"assign", {{"kind", "effect"}, {"effects", effects}, {"next", "wait"}}},
                        {"wait", {{"kind", "delay"}, {"duration_days", 1}, {"next", "finish"}}},
                        {"finish", {{"kind", "effect"}, {"effects", Json::array({assignment("story.offers",
                            {{"subtract", Json::array({offers, literal(std::int64_t{1}, "count")})}})})}, {"next", "end"}}},
                        {"end", {{"kind", "end"}}}}}},
            {{"id", "flow.z.observe"}, {"kind", "flow"}, {"revision", 1}, {"scope", "company"},
             {"trigger", {{"all_of", Json::array({
                 {{"fact", "story.ready"}, {"op", "eq"}, {"value", true}},
                 {{"fact", "story.offers"}, {"op", "eq"}, {"value", {{"add", Json::array({
                     Json{{"fact", "story.threshold"}}, literal(std::int64_t{1}, "count")})}}}}})}}},
             {"lifecycle", once}, {"entry_node", "record"}, {"nodes", {
                 {"record", {{"kind", "effect"}, {"effects", Json::array({{{"op", "record_decision"},
                     {"target", "self"}, {"value", "offers_observed"}}})}, {"next", "end"}}},
                 {"end", {{"kind", "end"}}}}}}})}};
}
runtime::CompiledProgram compile(const Json& document) {
    auto registry = content::foundation_registry();
    return runtime::compile_collection(content::load_collection(document.dump(), registry), registry, runtime::supported_effects());
}
runtime::Checkpoint initial() {
    runtime::Checkpoint state; state.world = runtime::foundation_world(core::Cents{100});
    state.world.enabled_flows = std::vector<std::string>{"flow.a.assign", "flow.z.observe"}; return state;
}
content::Scalar read(const runtime::CompiledProgram& program, const runtime::Checkpoint& state, const char* id) {
    return runtime::read_fact(state.world, *program.registry.fact(id), {content::Scope::company, state.world.company_id});
}
void behavior_and_reload() {
    const auto program = compile(fixture()); const auto start = initial();
    check(std::get<std::int64_t>(read(program, start, "story.threshold")) == 2 &&
          std::get<bool>(read(program, start, "story.ready")) == false && start.world.variables.empty(), "Registered defaults are scoped and sparse");
    runtime::Runtime engine{program, start}; check(engine.prepare_opening(0).accepted(), "Assignments execute");
    const auto waiting = engine.checkpoint();
    check(std::get<std::int64_t>(read(program, waiting, "story.offers")) == 3 &&
          std::get<bool>(read(program, waiting, "story.ready")) && waiting.world.variables.size() == 2 &&
          waiting.world.decisions == std::vector<std::string>{"offers_observed"}, "Ordered expressions and flags feed another flow's RHS arithmetic trigger");
    check(waiting.runtime.receipts.size() == 4 && waiting.runtime.receipts.front().before == content::Scalar{std::int64_t{0}} &&
          waiting.runtime.receipts.front().after == content::Scalar{std::int64_t{2}}, "Variable receipts retain actual ordered before/after values");
    runtime::Runtime resumed{program, persistence::decode_save(program, persistence::encode_save(program, waiting)).checkpoint};
    check(resumed.checkpoint() == waiting, "Delayed save preserves scoped overrides and untouched defaults");
    check(engine.submit({waiting.world.simulation.revision, core::AdvanceDay{}}).accepted() &&
          resumed.submit({waiting.world.simulation.revision, core::AdvanceDay{}}).accepted(), "Saved delayed expressions resume");
    check(engine.checkpoint() == resumed.checkpoint() && std::get<std::int64_t>(read(program, engine.checkpoint(), "story.offers")) == 2 &&
          std::get<std::int64_t>(read(program, engine.checkpoint(), "story.threshold")) == 2 &&
          engine.checkpoint().world.simulation.random == start.world.simulation.random, "Subtraction resumes exactly without consuming RNG or duplicating the observer");
}
void rejection_and_inspection() {
    for (const bool overflow : {false, true}) {
        auto document = fixture();
        const auto value = overflow ? literal(std::numeric_limits<std::int64_t>::max(), "count") : Json{{"fact", "story.offers"}};
        document["definitions"][0]["nodes"]["assign"]["effects"][2] = assignment("story.offers",
            {{"add", Json::array({value, literal(std::int64_t{11}, "count")})}});
        const auto program = compile(document); const auto start = initial(); runtime::Runtime engine{program, start};
        const auto result = engine.prepare_opening(0);
        check(!result.accepted() && engine.checkpoint() == start, "Range/overflow refusal rolls back earlier counter and flag assignments");
    }
    for (const bool wrong_scope : {false, true}) {
        auto document = fixture();
        if (wrong_scope) document["variables"][0]["scope"] = "world";
        else document["definitions"][0]["nodes"]["assign"]["effects"][0]["value"]["add"][1]["unit"] = "days";
        bool rejected = false; try { (void)compile(document); } catch (const std::exception&) { rejected = true; }
        check(rejected, "Compilation rejects expression unit or variable scope mismatches");
    }
    auto registry = content::foundation_registry(); auto hidden = *registry.fact("company.cash_cents");
    hidden.id = "company.hidden_cash"; hidden.visibility = content::Visibility::hidden_truth; registry.register_fact(hidden);
    content::Condition condition{}; condition.kind = content::ConditionKind::clause;
    content::Expression rhs{}; rhs.kind = content::ExpressionKind::fact; rhs.fact = hidden.id;
    condition.clause = content::FactClause{"company.cash_cents", content::Comparison::equal, std::int64_t{0}, content::Unit::constant_2014_cents, rhs};
    unsigned hidden_reads = 0;
    const auto inspected = runtime::evaluate_condition(condition, {content::Scope::company, "company.player"}, registry,
        [&](const auto& metadata, const auto&) -> content::Scalar { if (metadata.visibility == content::Visibility::hidden_truth) ++hidden_reads; return std::int64_t{100}; },
        runtime::InspectionMode::player_known);
    check(!inspected.result && hidden_reads == 0 && !inspected.clauses.front().expected, "Player inspection never reads or reveals a hidden RHS expression");
}
}
int main() {
    try { behavior_and_reload(); rejection_and_inspection(); std::cout << "Scoped variables, expressions and delayed reload checks passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
