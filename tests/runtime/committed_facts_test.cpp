#include "integration_fixture.hpp"

#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>

#include <iostream>

using namespace pgp;
namespace {
using Json = nlohmann::json;
constexpr auto project_flow = "flow.a_project";
constexpr auto contract_flow = "flow.b_contract";
std::size_t checks = 0;
void check(bool valid, const char* detail) {
    ++checks;
    if (!valid) throw std::runtime_error(detail);
}
Json flow(const char* id, const char* event, bool choice) {
    auto nodes = Json{
        {"news", {{"kind", "news"}, {"text_key", "fixture.settlement"}, {"next", choice ? "choice" : "end"}}},
        {"end", {{"kind", "end"}}}
    };
    if (choice) {
        nodes["choice"] = {{"kind", "dialogue"}, {"title_key", "fixture.review"}, {"text_key", "fixture.review.completed"},
            {"default_choice_if_dismissed", "continue"}, {"choices", Json::array({
                {{"id", "continue"}, {"label_key", "fixture.continue"}, {"next", "record"}}
            })}};
        nodes["record"] = {{"kind", "effect"}, {"next", "end"}, {"effects", Json::array({
            {{"op", "record_decision"}, {"target", "self"}, {"value", "project.reviewed"}}
        })}};
    }
    return {{"id", id}, {"kind", "flow"}, {"revision", 1}, {"scope", "company"}, {"on_committed", event},
        {"trigger", {{"fact", "company.is_operating"}, {"op", "eq"}, {"value", true}}},
        {"lifecycle", {{"mode", "recurring"}, {"max_occurrences_per_scope", 3}, {"cooldown_days", 2},
            {"repeat_policy", "after_cooldown_while_eligible"}}},
        {"entry_node", "news"}, {"nodes", std::move(nodes)}};
}
Json document() {
    std::ifstream input(PGP_MARKET_FIXTURE_PATH);
    auto data = Json::parse(input);
    auto& scenario = data["definitions"][0];
    scenario["flows"] = {project_flow, contract_flow};
    auto& contract = scenario["market"]["contracts"][0];
    contract["start_immediately"] = false;
    contract["start_by"] = "2014-01-01";
    data["definitions"].push_back(flow(project_flow, "capacity.project_completed", true));
    data["definitions"].push_back(flow(contract_flow, "market.contract_missed_start", false));
    return data;
}
runtime::CompiledProgram compile(const Json& data) {
    const auto registry = content::foundation_registry();
    return runtime::compile_collection(content::load_collection(data.dump(), registry), registry, runtime::supported_effects());
}
void invalid_document(Json data, std::string_view code) {
    try { (void)compile(data); }
    catch (const content::ValidationError& error) {
        check(error.code() == code, error.what());
        return;
    }
    throw std::runtime_error("Invalid subscription was accepted");
}
void invalid_save(const runtime::CompiledProgram& program, Json data) {
    data.erase("sha256");
    data["sha256"] = persistence::sha256(data.dump());
    try { (void)persistence::decode_save(program, data.dump()); }
    catch (const persistence::PersistenceError& error) {
        check(error.code() == "invalid_state", error.what());
        return;
    }
    throw std::runtime_error("Invalid committed source save was accepted");
}
std::uint64_t revision(const runtime::Runtime& runtime) { return runtime.checkpoint().world.simulation.revision; }
void reserve_one_day(runtime::Runtime& engine, std::string id) {
    const auto checkpoint = engine.checkpoint();
    core::CapacityProject row;
    row.id = std::move(id);
    row.source_id = "lease.initial";
    row.staff = 1;
    row.starts_on = checkpoint.world.simulation.date;
    row.total_work = core::MilliCU{1000};
    row.requested = core::MilliCUPerDay{1000};
    row.cause = "Measured finite work";
    integration_fixture::require_accepted(engine.submit({checkpoint.world.simulation.revision, core::ReserveProject{row}}));
}
void causal_dispatch_and_pending_reload() {
    const auto program = compile(document());
    check(program.canonical_content_bytes.find("\"on_committed\":\"capacity.project_completed\"") != std::string::npos,
          "Canonical content must retain the committed source subscription");
    const auto initial = runtime::begin_scenario(program, "scenario.enterprise", "US-CA");
    runtime::Runtime live{program, initial};
    integration_fixture::prepare(live);
    check(live.checkpoint().runtime.activations.empty(), "A true guard without a source must remain quiet");
    reserve_one_day(live, "Zeta");
    reserve_one_day(live, "Alpha");
    check(live.checkpoint().runtime.activations.empty(), "Reserving work cannot announce completion");
    const auto before = live.checkpoint();
    runtime::Runtime preview{program, before};
    integration_fixture::step(preview);
    check(live.checkpoint() == before, "Copied inspection must not publish live events");
    integration_fixture::step(live);
    const auto waiting = live.checkpoint();
    check(waiting == preview.checkpoint() && !waiting.runtime.prepared_day && waiting.runtime.activations.size() == 1,
          "A required choice must suspend the opening before the other committed occurrence");
    const auto& cause = *waiting.runtime.activations[0].committed_source;
    check(cause.fact_id == "capacity.project_completed" && cause.subject.id == "Alpha" &&
          cause.settled_on == core::OrdinalDay{0} && cause.unit == content::Unit::milli_CU &&
          cause.result == content::Scalar{std::int64_t{1000}} && cause.cause == "Measured finite work",
          "Source must name the first canonical project and its actual work and date");
    check(waiting.runtime.lifecycles[0].committed_cursor == 1 && waiting.runtime.presentations.size() == 2,
          "Only the first processed project may publish before its required choice");
    const auto bytes = persistence::encode_save(program, waiting);
    auto forged = Json::parse(bytes);
    forged["checkpoint"]["runtime"]["activations"][0]["committed_source"]["result"]["value"] = 999;
    invalid_save(program, forged);
    forged = Json::parse(bytes);
    forged["checkpoint"]["runtime"]["activations"][0].erase("committed_source");
    invalid_save(program, forged);
    forged = Json::parse(bytes);
    forged["checkpoint"]["runtime"]["lifecycles"][0]["committed_cursor"] = 2;
    invalid_save(program, forged);
    runtime::Runtime resumed{program, persistence::decode_save(program, bytes).checkpoint};
    check(resumed.checkpoint() == waiting, "Pending source, choice and cursor must round-trip exactly");
    const auto id = waiting.runtime.presentations[1].id;
    const auto try_step = resumed.submit({revision(resumed), core::AdvanceDay{}});
    check(try_step.reason == runtime::RuntimeReason::morning_not_ready && resumed.checkpoint() == waiting,
          "Work cannot skip a pending committed-source choice");
    integration_fixture::require_accepted(live.choose(revision(live), id, "continue"));
    integration_fixture::require_accepted(resumed.choose(revision(resumed), id, "continue"));
    const auto prepared = resumed.checkpoint();
    check(prepared == live.checkpoint() && prepared.runtime.prepared_day == core::OrdinalDay{1} &&
          prepared.runtime.activations.size() == 2 && prepared.runtime.lifecycles[0].committed_cursor == 2,
          "Resume must consume the remaining prefix and dispatch the other source exactly once");
    const auto& missed = *prepared.runtime.activations[1].committed_source;
    check(missed.fact_id == "market.contract_missed_start" && missed.subject.id == "contract.documents" &&
          missed.result == content::Scalar{true} && missed.settled_on == core::OrdinalDay{0},
          "Missed contract news must reference the actual terminal deadline result");
    check(prepared.world.decisions == std::vector<std::string>{"project.reviewed"} &&
          prepared.world.simulation.random == initial.world.simulation.random,
          "News must not duplicate the underlying operation or consume RNG");
    check(!resumed.choose(revision(resumed), id, "continue").accepted() && resumed.checkpoint() == prepared,
          "Resolved source dialogue cannot commit its effect twice");
    reserve_one_day(resumed, "cooldown");
    integration_fixture::step(resumed);
    check(resumed.checkpoint().runtime.activations.size() == 2, "Fresh occurrences during the authored cooldown are skipped");
    reserve_one_day(resumed, "boundary");
    integration_fixture::step(resumed);
    const auto boundary = resumed.checkpoint();
    check(boundary.runtime.activations.size() == 3 && boundary.runtime.activations.back().committed_source->subject.id == "boundary",
          "A fresh source at the exact cooldown boundary can activate");
}
void refuse_bad_subscriptions_and_rollback_effects() {
    auto data = document();
    data["definitions"][2]["on_committed"] = "capacity.fake";
    invalid_document(data, "unknown_committed_fact");
    data = document();
    data["definitions"][3]["on_committed"] = "capacity.project_completed";
    invalid_document(data, "duplicate_subscription");
    data = document();
    auto& broken = data["definitions"][2];
    broken["entry_node"] = "record";
    broken["nodes"].erase("choice");
    broken["nodes"]["record"] = {{"kind", "effect"}, {"next", "news"}, {"effects", Json::array({
        {{"op", "adjust_cash"}, {"target", "self"}, {"amount_cents", 1}}
    })}};
    broken["nodes"]["news"]["next"] = "end";
    const auto program = compile(data);
    runtime::Runtime engine{program, runtime::begin_scenario(program, "scenario.enterprise", "US-CA")};
    integration_fixture::prepare(engine);
    reserve_one_day(engine, "project");
    const auto before = engine.checkpoint();
    const auto rejected = engine.submit({revision(engine), core::AdvanceDay{}});
    check(rejected.reason == runtime::RuntimeReason::effect_rejected && !rejected.committed && engine.checkpoint() == before,
          "Refused flow effect must roll back settlement, consumption cursor and candidate news together");
    check(!rejected.paths.empty() && !rejected.paths.front().committed, "Rejection retains a candidate causal trace");
}
} // namespace

int main() {
    try {
        causal_dispatch_and_pending_reload();
        refuse_bad_subscriptions_and_rollback_effects();
        std::cout << checks << " committed-fact checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
