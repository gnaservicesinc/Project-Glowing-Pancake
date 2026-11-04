#include <pgp/content/collection.hpp>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
using Json = nlohmann::json;
int checks = 0;
void check(bool value, const std::string& message) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
Json& definition(Json& collection, const std::string& name) {
    for (auto& item : collection["definitions"]) if (item["id"] == name) return item;
    throw std::runtime_error("Test definition missing");
}
const std::string company_flow = "flow.hidden.compute_review";
void rejects(const std::string& document, const std::string& expected) {
    try {
        (void)pgp::content::load_collection(document, pgp::content::foundation_registry());
        check(false, "Expected rejection: " + expected);
    } catch (const pgp::content::ValidationError& error) {
        check(error.code() == expected, "Expected " + expected + ", got " + error.what());
        check(!error.path().empty(), "Rejection has a location");
    }
}
void mutated(const Json& source, const std::string& code, const std::function<void(Json&)>& mutate) {
    auto changed = source; mutate(changed); rejects(changed.dump(), code);
}
Json simple_flow() {
    return {{"id", "flow.test"}, {"kind", "flow"}, {"revision", 1}, {"scope", "company"},
            {"trigger", {{"fact", "company.is_operating"}, {"op", "eq"}, {"value", true}}},
            {"lifecycle", {{"mode", "once"}, {"max_occurrences_per_scope", 1}}}, {"entry_node", "end"},
            {"nodes", {{"end", {{"kind", "end"}}}}}};
}
void registry_rejects(const std::function<void(pgp::content::Registry&)>& action) {
    auto registry = pgp::content::foundation_registry();
    try { action(registry); check(false, "Invalid registration must fail"); }
    catch (const std::invalid_argument&) { check(true, "Invalid registration rejected"); }
}
} // namespace

int main() {
    using namespace pgp::content;
    std::ifstream stream(PGP_CONTENT_FIXTURE_PATH);
    std::ostringstream buffer; buffer << stream.rdbuf();
    check(stream.good() || stream.eof(), "Engineering fixture readable");
    const auto document = buffer.str();
    const auto source = Json::parse(document);
    const auto registry = foundation_registry();
    const auto loaded = load_collection(document, registry);
    check(loaded.schema == 1 && loaded.content_version == "0.0.0-dev", "Separate schema/content identity");
    check(loaded.definitions.size() == 11, "All normalized example definitions loaded");
    const auto& scenario = std::get<ScenarioDefinition>(loaded.find("scenario.foundation")->data);
    check(scenario.cash.value() == 120'000'000 && scenario.leased_capacity.value() == 100'000, "Opening units preserve exact values");
    check(scenario.ownership.at("founder_trust").value() == 5500 && scenario.team_people == 6, "Ownership and staff typed");
    check(scenario.monthly_commitments.at("compute_lease").value() == 1'800'000 && scenario.lease_energy_included, "Recurring costs separate from prepaid capacity");
    const auto& sequence = std::get<FlowDefinition>(loaded.find("flow.hidden.sequence_resume")->data);
    check(sequence.nodes.at("outer_sequence").children.at(0) == "inner_sequence", "Ordered branches retained");
    check(sequence.nodes.at("wait").delay_days == 14, "Positive delay typed");
    const auto& continuous = std::get<FlowDefinition>(loaded.find("flow.hidden.sustained_workload")->data);
    check(continuous.lifecycle.exit_when.has_value() && !continuous.exit_node.empty(), "Continuous exit retained");
    check(registry.fact("company.cash_cents")->unit == Unit::constant_2014_cents, "Money metadata explicit");
    check(registry.effect("open_negotiation")->arguments.at(0).reference_kind == "deal", "Effect reference kind explicit");
    check(registry.fact("company.cash_cents")->ports.size() == 2 && !registry.fact("company.cash_cents")->help.empty(), "Shared ports/help available");
    check(registry.fact("company.cash_cents")->boundary == Boundary::committed_result &&
          registry.fact("company.cash_cents")->visibility == Visibility::player_known, "Phase and knowledge explicit");

    rejects("{", "json");
    rejects("{\"content_schema\":1,\"content_schema\":1}", "duplicate_key");
    rejects(std::string(maximum_document_bytes + 1, ' '), "budget");
    auto exponent = document; const auto seed = exponent.find("20140101"); exponent.replace(seed, 8, "20140101e0"); rejects(exponent, "integer_token");
    auto fractional = document; fractional.replace(fractional.find("20140101"), 8, "20140101.0"); rejects(fractional, "integer_token");
    mutated(source, "unsupported_schema", [](auto& value) { value["content_schema"] = 2; });
    mutated(source, "type", [](auto& value) { value["content_schema"] = true; });
    mutated(source, "version", [](auto& value) { value["content_version"] = "1.02.0"; });
    mutated(source, "version", [](auto& value) { value["content_version"] = "1.0.0-01"; });
    mutated(source, "unit", [](auto& value) { value["currency_basis"] = "nominal_cents"; });
    mutated(source, "unknown_field", [](auto& value) { value["script"] = "run()"; });
    mutated(source, "unknown_field", [](auto& value) { value["remote"] = {{"url", "https://example.invalid/data"}}; });
    mutated(source, "duplicate_id", [](auto& value) { value["definitions"].push_back(value["definitions"][0]); });
    mutated(source, "range", [](auto& value) { value["definitions"][0]["revision"] = 0; });
    mutated(source, "unsupported_kind", [](auto& value) { value["definitions"][0]["kind"] = "python"; });
    mutated(source, "ownership_total", [](auto& value) { value["definitions"][0]["ownership_basis_points"]["founder_trust"] = 5499; });
    mutated(source, "type", [](auto& value) { value["definitions"][0]["cash_cents"] = true; });
    mutated(source, "range", [](auto& value) { value["definitions"][0]["cash_cents"] = std::numeric_limits<std::uint64_t>::max(); });
    mutated(source, "range", [](auto& value) { value["definitions"][0]["capacity_milli_CU_per_day"] = -1; });
    mutated(source, "range", [](auto& value) { value["definitions"][0]["ends_before"] = "2014-01-01"; });
    mutated(source, "date", [](auto& value) { value["definitions"][0]["starts_on"] = "2100-02-29"; });
    mutated(source, "dangling_reference", [](auto& value) { value["definitions"][0]["research"] = {"research.missing"}; });
    mutated(source, "reference_kind", [](auto& value) { value["definitions"][0]["research"] = {"deal.compute_exclusivity"}; });
    mutated(source, "prerequisite_cycle", [](auto& value) { definition(value, "research.foundation")["prerequisites"]["all_of"] = {"research.foundation"}; });
    mutated(source, "unknown_fact", [](auto& value) { definition(value, company_flow)["trigger"]["all_of"][0]["fact"] = "company.unknown"; });
    mutated(source, "scope", [](auto& value) { definition(value, company_flow)["trigger"]["all_of"][0]["fact"] = "world.public_breakthrough_count"; });
    mutated(source, "type", [](auto& value) { definition(value, company_flow)["trigger"]["all_of"][0]["value"] = 1; });
    mutated(source, "type", [](auto& value) { definition(value, company_flow)["trigger"]["all_of"][0]["op"] = "gte"; });
    mutated(source, "unit", [](auto& value) { definition(value, company_flow)["trigger"]["all_of"][0]["unit"] = "count"; });
    mutated(source, "range", [](auto& value) { definition(value, "flow.hidden.sustained_workload")["trigger"]["all_of"][0]["value"] = 10001; });
    mutated(source, "range", [](auto& value) { definition(value, company_flow)["lifecycle"]["cooldown_days"] = 0; });
    mutated(source, "missing_field", [](auto& value) { definition(value, company_flow)["lifecycle"].erase("max_occurrences_per_scope"); });
    mutated(source, "lifecycle", [](auto& value) { definition(value, "flow.hidden.sustained_workload").erase("exit_node"); });
    mutated(source, "missing_field", [](auto& value) { definition(value, company_flow)["nodes"]["record_review"]["effects"][0].erase("target"); });
    mutated(source, "unknown_effect", [](auto& value) { definition(value, company_flow)["nodes"]["record_review"]["effects"][0]["op"] = "execute"; });
    mutated(source, "target", [](auto& value) { definition(value, company_flow)["nodes"]["record_review"]["effects"][0]["target"] = "file:///tmp/state"; });
    mutated(source, "scope", [](auto& value) { definition(value, company_flow)["nodes"]["record_review"]["effects"][0]["target"] = "world"; });
    mutated(source, "unknown_field", [](auto& value) { definition(value, company_flow)["nodes"]["record_review"]["effects"][0]["script"] = "exec"; });
    mutated(source, "unit", [](auto& value) { definition(value, "flow.hidden.sustained_workload")["nodes"]["enter"]["effects"][0]["units"] = {{"add_basis_points", "constant_2014_cents"}}; });
    mutated(source, "operation", [](auto& value) { definition(value, "flow.hidden.sustained_workload")["nodes"]["enter"]["effects"][0]["field"] = "arbitrary_memory"; });
    mutated(source, "reference_kind", [](auto& value) { definition(value, "flow.supplier.exclusivity_offer")["nodes"]["review_effect"]["effects"][0]["template_id"] = "research.foundation"; });
    mutated(source, "dangling_reference", [](auto& value) { definition(value, "flow.supplier.exclusivity_offer")["nodes"]["review_effect"]["effects"][0]["template_id"] = "deal.missing"; });
    mutated(source, "dangling_reference", [](auto& value) { definition(value, company_flow)["nodes"]["record_review"]["next"] = "missing"; });
    mutated(source, "immediate_cycle", [](auto& value) { definition(value, company_flow)["nodes"]["record_review"]["next"] = "record_review"; });
    mutated(source, "unreachable_node", [](auto& value) { definition(value, company_flow)["nodes"]["unused"] = {{"kind", "end"}}; });
    mutated(source, "dangling_reference", [](auto& value) { definition(value, "flow.supplier.exclusivity_offer")["nodes"]["offer"]["default_choice_if_dismissed"] = "missing"; });
    mutated(source, "duplicate_id", [](auto& value) { auto& choices = definition(value, "flow.supplier.exclusivity_offer")["nodes"]["offer"]["choices"]; choices.push_back(choices[0]); });

    Json loop = source; auto delayed = simple_flow(); delayed["entry_node"] = "wait";
    delayed["nodes"] = {{"wait", {{"kind", "delay"}, {"duration_days", 1}, {"next", "wait"}}}};
    loop["definitions"].push_back(delayed); rejects(loop.dump(), "unbounded_cycle");
    loop["definitions"].back()["max_node_visits"] = 8;
    check(load_collection(loop.dump(), registry).definitions.size() == 12, "Bounded positive-delay loop validates");
    loop["definitions"].back()["nodes"]["wait"]["duration_days"] = 0; rejects(loop.dump(), "range");
    loop["definitions"].back() = delayed; loop["definitions"].back()["max_node_visits"] = 8;
    auto& sequence_loop = loop["definitions"].back(); sequence_loop["entry_node"] = "sequence";
    sequence_loop["nodes"]["sequence"] = {{"kind", "sequence"}, {"children", {"wait"}}, {"next", "end"}};
    sequence_loop["nodes"]["wait"]["next"] = "sequence"; sequence_loop["nodes"]["end"] = {{"kind", "end"}};
    rejects(loop.dump(), "budget");

    auto recursive = source; auto subflow = simple_flow(); subflow["entry_node"] = "call";
    subflow["nodes"]["call"] = {{"kind", "subflow"}, {"flow_id", "flow.test"}, {"next", "end"}};
    recursive["definitions"].push_back(subflow); rejects(recursive.dump(), "subflow_cycle");

    auto random = source; auto random_flow = simple_flow(); random_flow["entry_node"] = "draw";
    random_flow["nodes"]["draw"] = {{"kind", "random"}, {"stream", "narrative"},
        {"branches", {{{"id", "incident"}, {"weight", 3}, {"next", "end"}},
                      {{"id", "no_event"}, {"weight", 7}, {"next", "end"}}}}};
    random["definitions"].push_back(random_flow);
    const auto random_collection = load_collection(random.dump(), registry);
    const auto& draw = std::get<FlowDefinition>(random_collection.find("flow.test")->data).nodes.at("draw");
    check(draw.kind == NodeKind::random && draw.random_stream == "narrative" && draw.branches.size() == 2 &&
          draw.branches[0].id == "incident" && draw.branches[0].weight == 3 && draw.branches[1].id == "no_event",
          "Weighted narrative branches preserve ordered outcome IDs and integer weights");
    mutated(random, "missing_field", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"].erase("stream"); });
    mutated(random, "operation", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["stream"] = "research"; });
    mutated(random, "unknown_field", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["next"] = "end"; });
    mutated(random, "budget", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["branches"] = Json::array(); });
    mutated(random, "budget", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["branches"].erase(1); });
    mutated(random, "budget", [](auto& value) {
        auto& branches = definition(value, "flow.test")["nodes"]["draw"]["branches"];
        for (int i = 2; i < 33; ++i) branches.push_back({{"id", "outcome" + std::to_string(i)}, {"weight", 1}, {"next", "end"}});
    });
    for (const auto weight : {-1, 0, 1'000'001})
        mutated(random, "range", [weight](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["branches"][0]["weight"] = weight; });
    mutated(random, "type", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["branches"][0]["weight"] = true; });
    mutated(random, "duplicate_id", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["branches"][1]["id"] = "incident"; });
    mutated(random, "identifier", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["branches"][1]["id"] = "../bad"; });
    mutated(random, "unknown_field", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["branches"][0]["script"] = "run()"; });
    mutated(random, "dangling_reference", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["branches"][0]["next"] = "missing"; });
    mutated(random, "immediate_cycle", [](auto& value) { definition(value, "flow.test")["nodes"]["draw"]["branches"][0]["next"] = "draw"; });
    auto random_loop = random;
    definition(random_loop, "flow.test")["nodes"]["draw"]["branches"][0]["next"] = "wait";
    definition(random_loop, "flow.test")["nodes"]["wait"] = {{"kind", "delay"}, {"duration_days", 1}, {"next", "draw"}};
    rejects(random_loop.dump(), "unbounded_cycle");
    definition(random_loop, "flow.test")["max_node_visits"] = 16;
    check(load_collection(random_loop.dump(), registry).definitions.size() == 12, "Weighted delayed recurrence requires and accepts a finite visit budget");
    auto nested = source; auto branch = simple_flow(); branch["entry_node"] = "sequence0";
    for (int i = 0; i < 9; ++i) branch["nodes"]["sequence" + std::to_string(i)] =
        {{"kind", "sequence"}, {"children", {i == 8 ? "end" : "sequence" + std::to_string(i + 1)}}, {"next", "end"}};
    nested["definitions"].push_back(branch); rejects(nested.dump(), "budget");

    // Formerly exponential path enumeration: 12 nodes x 8 branches, all
    // converging on the next node. Validation must scale with graph edges.
    auto diamond = source; auto branching = simple_flow(); branching["entry_node"] = "dialogue0";
    for (int i = 0; i < 12; ++i) {
        auto choices = Json::array();
        for (int j = 0; j < 8; ++j) choices.push_back({{"id", "choice" + std::to_string(j)}, {"label_key", "test.label"},
            {"next", i == 11 ? "end" : "dialogue" + std::to_string(i + 1)}});
        branching["nodes"]["dialogue" + std::to_string(i)] = {{"kind", "dialogue"}, {"title_key", "test.title"},
            {"text_key", "test.text"}, {"choices", choices}, {"default_choice_if_dismissed", "choice0"}};
    }
    diamond["definitions"].push_back(branching);
    check(load_collection(diamond.dump(), registry).definitions.size() == 12, "Converging branches validate in bounded work");
    auto subflows = source;
    for (int i = 0; i < 9; ++i) {
        auto called = simple_flow(); called["id"] = "flow.depth" + std::to_string(i);
        if (i != 8) {
            called["entry_node"] = "call";
            called["nodes"]["call"] = {{"kind", "subflow"}, {"flow_id", "flow.depth" + std::to_string(i + 1)}, {"next", "end"}};
        }
        subflows["definitions"].push_back(called);
    }
    check(load_collection(subflows.dump(), registry).definitions.size() == 20, "Exactly eight subflow frames accepted");
    auto ninth = simple_flow(); ninth["id"] = "flow.depth9"; subflows["definitions"].push_back(ninth);
    auto& depth8 = definition(subflows, "flow.depth8"); depth8["entry_node"] = "call";
    depth8["nodes"]["call"] = {{"kind", "subflow"}, {"flow_id", "flow.depth9"}, {"next", "end"}};
    rejects(subflows.dump(), "budget");
    auto month = source; definition(month, "flow.news.monthly_operations")["lifecycle"]["anchor"]["kind"] = "month_start";
    check(load_collection(month.dump(), registry).definitions.size() == 11, "Calendar month-start anchor supported");
    auto large_seed = source; large_seed["definitions"][0]["seed"] = std::numeric_limits<std::uint64_t>::max();
    check(std::get<ScenarioDefinition>(load_collection(large_seed.dump(), registry).find("scenario.foundation")->data).seed ==
          std::numeric_limits<std::uint64_t>::max(), "Unsigned seed remains lossless");
    for (const auto& version : {"1.2.3", "1.2.3-rc.1", "1.2.3+build.01", "1.2.3-rc.1+build-tag"}) {
        auto versioned = source; versioned["content_version"] = version;
        check(load_collection(versioned.dump(), registry).content_version == version, "Valid SemVer accepted");
    }
    auto deep = source; auto& trigger = definition(deep, company_flow)["trigger"];
    for (int i = 0; i < 50; ++i) trigger = Json{{"not", trigger}};
    rejects(deep.dump(), "budget");

    registry_rejects([](auto& value) { value.register_fact(*value.fact("company.cash_cents")); });
    registry_rejects([](auto& value) { auto meta = *value.fact("company.cash_cents"); meta.id = "test.money"; meta.unit = Unit::days; value.register_fact(meta); });
    registry_rejects([](auto& value) { auto meta = *value.fact("company.cash_cents"); meta.id = "test.money"; meta.provider = "missing"; value.register_fact(meta); });
    registry_rejects([](auto& value) { auto meta = *value.effect("record_decision"); meta.id = "test.effect"; meta.ports[0].scope = Scope::world; value.register_effect(meta); });
    registry_rejects([](auto& value) { value.register_system("new_system", {"missing_system"}); });
    registry_rejects([](auto& value) { auto meta = *value.fact("company.workload_stress_basis_points"); meta.id = "test.stress"; meta.minimum = -1; value.register_fact(meta); });
    registry_rejects([](auto& value) { auto meta = *value.fact("company.is_operating"); meta.id = "test.operating"; meta.maximum = 5; value.register_fact(meta); });
    registry_rejects([](auto& value) { auto meta = *value.effect("register_owned_modifier"); meta.id = "test.modifier"; meta.arguments[2].maximum = 20000; value.register_effect(meta); });
    auto extension = registry; extension.register_system("test_system", {"foundation"});
    auto fact = *extension.fact("company.cash_cents"); fact.id = "test.cash_cents"; fact.provider = "test_system";
    fact.dependencies = {"foundation"}; fact.visibility = Visibility::hidden_truth; extension.register_fact(fact);
    auto ext_source = source; definition(ext_source, company_flow)["trigger"]["all_of"][0] = {{"fact", "test.cash_cents"}, {"op", "gte"}, {"value", 0}};
    check(load_collection(ext_source.dump(), extension).definitions.size() == 11, "Module fact extension requires no loader/UI switch");
    auto effect_meta = *extension.effect("adjust_cash"); effect_meta.id = "test.cash_change"; effect_meta.provider = "test_system";
    extension.register_effect(effect_meta);
    definition(ext_source, company_flow)["nodes"]["record_review"]["effects"] = {{{"op", "test.cash_change"}, {"target", "self"}, {"amount_cents", -10}}};
    check(load_collection(ext_source.dump(), extension).definitions.size() == 11, "Module effect extension shares typed argument validation");
    std::cout << checks << " content registry/loader checks passed\n";
}
