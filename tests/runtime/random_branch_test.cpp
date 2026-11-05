#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace pgp;
namespace {
using Json = nlohmann::json;
constexpr auto narrative = static_cast<std::size_t>(core::RandomStream::narrative);
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void accepted(const runtime::RuntimeResult& result) {
    if (!result.accepted()) throw std::runtime_error(result.explanation);
    ++checks;
}
std::uint64_t revision(const runtime::Runtime& engine) {
    return engine.checkpoint().world.simulation.revision;
}
void prepare(runtime::Runtime& engine) { accepted(engine.prepare_opening(revision(engine))); }
void step(runtime::Runtime& engine) { accepted(engine.submit({revision(engine), core::AdvanceDay{}})); }
content::Node terminal() {
    content::Node result{};
    result.kind = content::NodeKind::end;
    return result;
}
content::Node marker(std::string value, std::string next) {
    content::Node result{};
    result.kind = content::NodeKind::effect;
    result.effects = {{"record_decision", "self", content::Scope::company, {{"value", std::move(value)}}}};
    result.next = std::move(next);
    return result;
}
content::Node weighted(std::string first, std::string second, std::string third) {
    content::Node result{};
    result.kind = content::NodeKind::random;
    result.random_stream = "narrative";
    result.branches = {{"small", 1, std::move(first)}, {"medium", 3, std::move(second)},
                       {"large", 6, std::move(third)}};
    return result;
}
content::FlowDefinition flow(std::string entry) {
    content::FlowDefinition result{};
    result.scope = content::Scope::company;
    result.trigger = {content::ConditionKind::clause,
        content::FactClause{"company.is_operating", content::Comparison::equal, true, content::Unit::boolean}, {}};
    result.lifecycle.mode = content::LifecycleMode::once;
    result.lifecycle.occurrence_scope = content::Scope::company;
    result.entry_node = std::move(entry);
    return result;
}
runtime::CompiledProgram compile(std::map<std::string, content::FlowDefinition, std::less<>> flows) {
    content::Collection collection{content::content_schema, "0.0.0-dev", {}};
    for (auto& [id, definition] : flows)
        collection.definitions.emplace(id, content::Definition{id, 1, std::move(definition)});
    return runtime::compile_collection(collection, content::foundation_registry(), runtime::supported_effects());
}
runtime::Checkpoint initial(std::string root = "flow.random", std::uint64_t seed = 0) {
    runtime::Checkpoint result;
    result.world = runtime::foundation_world(core::Cents{100});
    result.world.enabled_flows = std::vector<std::string>{std::move(root)};
    result.world.root_seed = seed;
    result.world.simulation.random = core::RandomStreams{seed}.snapshot();
    return result;
}
runtime::CompiledProgram simple_program() {
    auto definition = flow("draw");
    definition.nodes.emplace("draw", weighted("small", "medium", "large"));
    for (const auto* name : {"small", "medium", "large"})
        definition.nodes.emplace(name, marker(name, "end"));
    definition.nodes.emplace("end", terminal());
    return compile({{"flow.random", std::move(definition)}});
}
runtime::Checkpoint reload(const runtime::CompiledProgram& program, const runtime::Checkpoint& state) {
    const auto loaded = persistence::decode_save(program, persistence::encode_save(program, state)).checkpoint;
    check(loaded == state, "Save/reload must preserve every random and continuation field");
    return loaded;
}
template<class Change>
void corrupt_save(const runtime::CompiledProgram& program, const runtime::Checkpoint& state,
                  Change change, const char* message) {
    auto body = Json::parse(persistence::encode_save(program, state));
    change(body["checkpoint"]["runtime"]);
    body.erase("sha256");
    body["sha256"] = persistence::sha256(body.dump());
    bool rejected = false;
    try { (void)persistence::decode_save(program, body.dump()); }
    catch (const persistence::PersistenceError&) { rejected = true; }
    check(rejected, message);
}
void frozen_seed_oracle_and_inspection() {
    const auto program = simple_program();
    // Independent integer oracle, frozen for SplitMix64's narrative domain.
    // The ten seeds cover every sample slot and both sides of each branch boundary.
    struct Expected { std::uint64_t seed; std::uint64_t before; std::uint64_t after; };
    const std::array<Expected, 10> expected{{
        {3, 2493001065868230072ULL, 13893715885191428557ULL},
        {8, 17600687008453613099ULL, 10554657754067259968ULL},
        {4, 16983547147878014814ULL, 9937517893491661683ULL},
        {31, 7263281830530456924ULL, 217252576144103793ULL},
        {14, 8755608413366195ULL, 11409470427736564680ULL},
        {13, 13390128619908158103ULL, 6344099365521804972ULL},
        {39, 3089967241967845098ULL, 14490682061291043583ULL},
        {0, 3207296026000306913ULL, 14608010845323505398ULL},
        {2, 13398859234004329862ULL, 6352829979617976731ULL},
        {1, 16184226688143867045ULL, 9138197433757513914ULL}
    }};
    for (std::size_t sample = 0; sample < expected.size(); ++sample) {
        const auto& oracle = expected[sample];
        const auto state = initial("flow.random", oracle.seed);
        const auto& definition = std::get<content::FlowDefinition>(program.collection.definitions.at("flow.random").data);
        const auto read = [&](const auto& metadata, const auto& binding) {
            return runtime::read_fact(state.world, metadata, binding);
        };
        const auto inspected = runtime::evaluate_condition(definition.trigger,
            {content::Scope::company, state.world.company_id}, program.registry, read);
        check(inspected.result == true && state == initial("flow.random", oracle.seed),
              "Condition inspection has no random or mutation side effects");
        runtime::Runtime engine{program, state};
        runtime::Runtime restored_before{program, reload(program, state)};
        const auto outcome = engine.prepare_opening(revision(engine));
        accepted(outcome);
        prepare(restored_before);
        const auto after = engine.checkpoint();
        check(after == restored_before.checkpoint(), "Save before selection retains the exact seeded outcome");
        check(after.runtime.random_resolutions.size() == 1, "One random node visit records one outcome");
        const auto& record = after.runtime.random_resolutions.front();
        const std::string selected = sample == 0 ? "small" : sample < 4 ? "medium" : "large";
        check(record.sample == sample && record.branch_id == selected && record.node_visit == 1 &&
                  record.stream_before == oracle.before && record.stream_after == oracle.after,
              "Frozen seed oracle and weighted boundaries match the saved draw");
        check(record.flow_id == "flow.random" && record.node_id == "draw" &&
                  record.activation_id == after.runtime.activations.front().id && record.day == core::OrdinalDay{0},
              "Random outcome retains its activation, path, visit and date");
        check(after.world.decisions == std::vector<std::string>{selected}, "Only the selected branch effect executes");
        for (std::size_t stream = 0; stream < core::stream_count; ++stream)
            check(after.world.simulation.random[stream] ==
                      (stream == narrative ? oracle.after : state.world.simulation.random[stream]),
                  "Random branches consume only the narrative stream");
        const auto traced = std::find_if(outcome.paths.begin(), outcome.paths.end(), [](const auto& path) {
            return path.detail.find("narrative") != std::string::npos &&
                   path.detail.find("sample") != std::string::npos;
        });
        check(traced != outcome.paths.end() && traced->committed && traced->detail.find(selected) != std::string::npos,
              "Committed random trace identifies its stream, sample and selected branch");
        (void)reload(program, after);
        prepare(engine);
        check(engine.checkpoint() == after, "Reopening a prepared morning never rerolls");
    }
}
void previously_advanced_narrative_origin() {
    const auto program = simple_program();
    // A provider may have consumed narrative draws before the first flow branch.
    // The branch history starts at its actual stream position, independent of root_seed.
    auto state = initial("flow.random", 42);
    state.world.simulation.random[narrative] = std::numeric_limits<std::uint64_t>::max();
    runtime::Runtime engine{program, state};
    prepare(engine);
    const auto selected = engine.checkpoint();
    const auto& record = selected.runtime.random_resolutions.front();
    check(record.stream_before == std::numeric_limits<std::uint64_t>::max() &&
              record.stream_after == 11400714819323198484ULL && record.sample == 6 &&
              record.branch_id == "large",
          "Previously advanced narrative state defines its own saved branch-history origin");
    (void)reload(program, selected);

    // The next raw SplitMix64 draw from this state is zero. Uniform bounded
    // sampling rejects it for a bound of ten and consumes the following draw.
    state.world.simulation.random[narrative] = 7046029254386353131ULL;
    runtime::Runtime rejection_sample{program, state};
    prepare(rejection_sample);
    const auto sampled = rejection_sample.checkpoint();
    check(sampled.runtime.random_resolutions.front().sample == 5 &&
              sampled.runtime.random_resolutions.front().stream_after == 11400714819323198485ULL,
          "Bounded sampling and saved stream replay account for a rejected raw draw");
    (void)reload(program, sampled);
}
runtime::CompiledProgram nested_program() {
    auto root = flow("ordered");
    content::Node ordered{};
    ordered.kind = content::NodeKind::sequence;
    ordered.children = {"call", "outer"}; ordered.next = "end";
    root.nodes.emplace("ordered", std::move(ordered));
    content::Node call{};
    call.kind = content::NodeKind::subflow; call.subflow_id = "flow.nested"; call.next = "child_end";
    root.nodes.emplace("call", std::move(call));
    root.nodes.emplace("outer", marker("outer_done", "child_end"));
    root.nodes.emplace("child_end", terminal()); root.nodes.emplace("end", terminal());
    auto nested = flow("draw");
    nested.nodes.emplace("draw", weighted("wait_small", "wait_medium", "wait_large"));
    for (const auto* name : {"wait_small", "wait_medium", "wait_large"}) {
        content::Node wait{};
        wait.kind = content::NodeKind::delay; wait.delay_days = 2; wait.next = "choice";
        nested.nodes.emplace(name, std::move(wait));
    }
    content::Node choice{};
    choice.kind = content::NodeKind::dialogue; choice.title_key = "random.choice.title";
    choice.text_key = "random.choice.text"; choice.default_choice = "continue";
    choice.choices = {{"continue", "random.choice.continue", "inner"}};
    nested.nodes.emplace("choice", std::move(choice));
    nested.nodes.emplace("inner", marker("inner_done", "end")); nested.nodes.emplace("end", terminal());
    return compile({{"flow.random", std::move(root)}, {"flow.nested", std::move(nested)}});
}
void nested_saved_continuations() {
    const auto program = nested_program();
    runtime::Runtime engine{program, initial()};
    prepare(engine);
    const auto delayed = engine.checkpoint();
    check(delayed.runtime.random_resolutions.size() == 1 && delayed.runtime.activations.size() == 1 &&
              delayed.runtime.activations.front().frames.size() == 2 &&
              delayed.runtime.activations.front().status == runtime::ActivationStatus::delayed &&
              delayed.runtime.random_resolutions.front().flow_id == "flow.nested",
          "Random selection survives a delay inside both sequence and subflow frames");
    corrupt_save(program, delayed, [](auto& runtime_state) {
        runtime_state["activations"][0]["current_node"] = "wait_small";
    }, "Saved continuation cannot resume an unselected random branch");
    corrupt_save(program, delayed, [](auto& runtime_state) {
        runtime_state["random_resolutions"] = Json::array();
    }, "Saved selected path requires the record of its consumed random outcome");
    runtime::Runtime resumed{program, reload(program, delayed)};
    for (unsigned day = 0; day < 2; ++day) {
        step(engine); step(resumed);
        check(engine.checkpoint() == resumed.checkpoint(), "Nested delayed continuation matches after reload each day");
    }
    const auto waiting = engine.checkpoint();
    check(waiting.runtime.activations.front().status == runtime::ActivationStatus::awaiting_choice &&
              waiting.runtime.random_resolutions == delayed.runtime.random_resolutions &&
              waiting.world.simulation.random == delayed.world.simulation.random,
          "Resuming a selected random path reaches dialogue without another draw");
    resumed = runtime::Runtime{program, reload(program, waiting)};
    const auto presentation = waiting.runtime.presentations.back().id;
    prepare(resumed);
    check(resumed.checkpoint() == waiting, "Reopening a saved dialogue never rerolls its branch");
    accepted(engine.choose(revision(engine), presentation, "continue"));
    accepted(resumed.choose(revision(resumed), presentation, "continue"));
    const auto finished = engine.checkpoint();
    check(finished == resumed.checkpoint() &&
              finished.world.decisions == std::vector<std::string>{"inner_done", "outer_done"} &&
              finished.runtime.random_resolutions == delayed.runtime.random_resolutions &&
              finished.world.simulation.random == delayed.world.simulation.random,
          "Saved choice returns once through nested siblings without reroll or duplicate effects");
    corrupt_save(program, finished, [](auto& runtime_state) {
        runtime_state["random_resolutions"] = Json::array();
    }, "Completed nested sequences retain the history of their consumed random outcomes");
    const auto before_invalid = engine.checkpoint();
    check(engine.choose(revision(engine), presentation, "continue").reason == runtime::RuntimeReason::invalid_choice &&
              engine.checkpoint() == before_invalid, "Reusing a resolved choice preserves the selected outcome");
    step(engine); step(resumed);
    check(engine.checkpoint() == resumed.checkpoint(), "The next whole checkpoint remains exact after nested choice reload");
}
void rejected_operations_rollback_random() {
    const auto program = simple_program();
    const auto start = initial();
    runtime::Runtime engine{program, start};
    check(engine.prepare_opening(1).reason == runtime::RuntimeReason::stale_revision && engine.checkpoint() == start,
          "Stale operations consume neither random state nor resolution records");
    check(engine.choose(0, 1, "missing").reason == runtime::RuntimeReason::invalid_choice && engine.checkpoint() == start,
          "Invalid operations consume neither random state nor resolution records");
    corrupt_save(program, start, [](auto& runtime_state) { runtime_state.erase("random_resolutions"); },
                 "Random-bearing programs require an explicit outcome list before the first draw");
    auto failing = flow("draw");
    failing.nodes.emplace("draw", weighted("unfunded", "unfunded", "unfunded"));
    content::Node unfunded{};
    unfunded.kind = content::NodeKind::effect; unfunded.next = "end";
    unfunded.effects = {{"adjust_cash", "self", content::Scope::company, {{"amount_cents", std::int64_t{1}}}}};
    failing.nodes.emplace("unfunded", std::move(unfunded)); failing.nodes.emplace("end", terminal());
    const auto failure_program = compile({{"flow.random", std::move(failing)}});
    runtime::Runtime rejected{failure_program, start};
    const auto outcome = rejected.prepare_opening(0);
    check(outcome.reason == runtime::RuntimeReason::effect_rejected && rejected.checkpoint() == start,
          "Failure after a branch rolls back its random draw, selected outcome and earlier state changes");
    check(!outcome.committed && !outcome.paths.empty() &&
              std::all_of(outcome.paths.begin(), outcome.paths.end(), [](const auto& path) { return !path.committed; }),
          "Rolled-back candidate branch traces never claim a committed outcome");
}
void repeated_node_visits_and_record_validation() {
    auto repeating = flow("draw");
    repeating.max_node_visits = 8;
    repeating.nodes.emplace("draw", weighted("wait", "wait", "wait"));
    content::Node wait{};
    wait.kind = content::NodeKind::delay; wait.delay_days = 1; wait.next = "draw";
    repeating.nodes.emplace("wait", std::move(wait));
    const auto program = compile({{"flow.random", std::move(repeating)}});
    runtime::Runtime engine{program, initial()};
    prepare(engine); step(engine);
    runtime::Runtime resumed{program, reload(program, engine.checkpoint())};
    step(engine); step(resumed);
    check(engine.checkpoint() == resumed.checkpoint(), "Saved delayed loop resumes with one new outcome per visit");
    step(engine); step(resumed);
    const auto state = engine.checkpoint();
    check(state == resumed.checkpoint() && state.runtime.random_resolutions.size() == 4,
          "Four executions of one random node retain four distinct outcomes");
    const auto& records = state.runtime.random_resolutions;
    for (std::size_t index = 0; index < records.size(); ++index) {
        check(records[index].node_visit == 1 + 2 * index && records[index].day == core::OrdinalDay{static_cast<std::int64_t>(index)},
              "Repeated random visits retain their exact visit counters and dates");
        if (index != 0)
            check(records[index].stream_before == records[index - 1].stream_after,
                  "Repeated outcomes form one continuous narrative stream");
    }
    const auto failed = engine.submit({revision(engine), core::AdvanceDay{}});
    check(failed.reason == runtime::RuntimeReason::budget_exceeded && engine.checkpoint() == state,
          "Visit-budget failure preserves the last committed random selection and RNG");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["sample"] = 10; },
                 "Out-of-range saved random sample rejects even with valid integrity checksum");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["branch_id"] = "missing"; },
                 "Unknown selected branch rejects even with valid integrity checksum");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["branch_id"] = "small"; },
                 "Saved branch must match the weighted interval of its saved sample");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["stream_after"] = 0; },
                 "Saved random result must reproduce the recorded post-draw stream state");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["stream_before"] = 0; },
                 "Saved random result must reproduce the recorded sample from its pre-draw state");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["node_visit"] = 0; },
                 "Zero random-node visit identity rejects");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"].push_back(runtime_state["random_resolutions"][0]); },
                 "Duplicate random-node visit identities reject");
    corrupt_save(program, state, [](auto& runtime_state) {
        auto& resolutions = runtime_state["random_resolutions"];
        resolutions.erase(resolutions.begin());
    }, "Truncating the first random outcome cannot detach the remaining history from its saved origin");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["activation_id"] = 999; },
                 "Orphan random resolution rejects");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["node_id"] = "wait"; },
                 "Random resolution cannot refer to a non-random node");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["day"] = -1; },
                 "Random outcome before activation start rejects");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["extra"] = true; },
                 "Unknown random-record fields reject");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0].erase("sample"); },
                 "Missing random-record fields reject");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["random_resolutions"][0]["sample"] = 1.5; },
                 "Fractional saved random numbers reject without truncation");
    corrupt_save(program, state, [](auto& runtime_state) {
        runtime_state["random_resolutions"][0]["node_visit"] = std::uint64_t{1} << 32U;
    }, "Oversized saved random visit counter rejects without narrowing");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state.erase("random_resolutions"); },
                 "Removing consumed random outcomes cannot turn a stochastic save into a legacy save");
    check(engine.checkpoint() == state, "All failed corrupt-load probes preserve the live campaign");
}
void deterministic_legacy_save() {
    auto deterministic = flow("record");
    deterministic.nodes.emplace("record", marker("deterministic", "end"));
    deterministic.nodes.emplace("end", terminal());
    const auto program = compile({{"flow.random", std::move(deterministic)}});
    runtime::Runtime engine{program, initial()};
    prepare(engine);
    const auto state = engine.checkpoint();
    auto body = Json::parse(persistence::encode_save(program, state));
    body["checkpoint"]["runtime"].erase("random_resolutions");
    body["checkpoint"]["runtime"].erase("narrative_origin");
    body.erase("sha256");
    body["sha256"] = persistence::sha256(body.dump());
    check(persistence::decode_save(program, body.dump()).checkpoint == state,
          "Existing deterministic format-one saves remain readable without the added outcome list");
}
content::FlowDefinition incident(std::string stream, std::string outcome) {
    auto definition = flow("sample");
    content::Node draw{};
    draw.kind = content::NodeKind::random;
    draw.random_stream = std::move(stream);
    draw.branches = {{"no_event", 1, "end"}, {"incident", 1, outcome}};
    definition.nodes.emplace("sample", std::move(draw));
    definition.nodes.emplace("end", terminal());
    return definition;
}
void seeded_openings_and_reload() {
    auto market = incident("markets", "wait");
    market.lifecycle.mode = content::LifecycleMode::interval;
    market.lifecycle.anchor = content::Anchor{content::AnchorKind::campaign_start, std::nullopt, {content::IntervalUnit::days, 2}};
    market.lifecycle.interval = content::Interval{content::IntervalUnit::days, 3};
    market.lifecycle.max_occurrences = 2;
    content::Node delay{};
    delay.kind = content::NodeKind::delay;
    delay.delay_days = 1;
    delay.next = "news";
    market.nodes.emplace("wait", std::move(delay));
    content::Node news{};
    news.kind = content::NodeKind::news;
    news.text_key = "incident.market.news";
    news.next = "choose";
    market.nodes.emplace("news", std::move(news));
    content::Node dialogue{};
    dialogue.kind = content::NodeKind::dialogue;
    dialogue.title_key = "incident.market.title";
    dialogue.text_key = "incident.market.prompt";
    dialogue.default_choice = "continue";
    dialogue.choices.push_back({"continue", "incident.continue", "record"});
    market.nodes.emplace("choose", std::move(dialogue));
    market.nodes.emplace("record", marker("market.reviewed", "end"));

    auto hazard = incident("hazards", "record");
    hazard.lifecycle.mode = content::LifecycleMode::scheduled;
    hazard.lifecycle.due_date = core::OrdinalDay{5};
    hazard.nodes.emplace("record", marker("hazard.observed", "end"));
    const auto narrative_program = simple_program();
    auto story = std::get<content::FlowDefinition>(narrative_program.collection.definitions.at("flow.random").data);
    const auto program = compile({{"flow.market", std::move(market)}, {"flow.hazard", std::move(hazard)}, {"flow.random", std::move(story)}});
    auto start = initial("flow.market", 6);
    start.world.enabled_flows = std::vector<std::string>{"flow.market", "flow.random", "flow.hazard"};
    constexpr auto markets = static_cast<std::size_t>(core::RandomStream::markets);
    constexpr auto hazards = static_cast<std::size_t>(core::RandomStream::hazards);
    // Frozen independently from the SplitMix64 algorithm and stream ordinals.
    constexpr std::uint64_t first_market = 12440057887101070171ULL;
    constexpr std::uint64_t next_market = 5394028632714717040ULL;
    constexpr std::uint64_t next_hazard = 8148332893662455429ULL;

    runtime::Runtime live{program, start};
    prepare(live);
    const auto story_only = live.checkpoint();
    check(story_only.runtime.random_resolutions.size() == 1 && story_only.runtime.incident_origins.empty() &&
          story_only.world.simulation.random[markets] == start.world.simulation.random[markets],
          "A narrative draw must leave both system streams untouched before any incident date");
    step(live);
    check(live.checkpoint().runtime.random_resolutions.size() == 1, "No system sample before its anchor");
    step(live);
    const auto quiet = live.checkpoint();
    const auto& no_event = quiet.runtime.random_resolutions.back();
    check(no_event.day == core::OrdinalDay{2} && no_event.sample == 0 && no_event.branch_id == "no_event" &&
          quiet.world.simulation.random[markets] == first_market && quiet.runtime.presentations.empty() &&
          quiet.world.decisions == story_only.world.decisions, "A quiet review consumes one real draw without creating an effect or news");
    check(quiet.runtime.incident_origins.at(core::RandomStream::markets) == start.world.simulation.random[markets],
          "The first incident draw retains the stream's starting cursor");
    runtime::Runtime resumed{program, reload(program, quiet)};
    for (int day = 3; day <= 5; ++day) {
        step(live);
        step(resumed);
        check(live.checkpoint() == resumed.checkpoint(), "Matched seeds and reload must agree through both sampling boundaries");
        if (day < 5) check(live.checkpoint().runtime.random_resolutions.size() == 2, "Quiet interval days consume no draw");
    }
    const auto pending = live.checkpoint();
    check(pending.runtime.random_resolutions.size() == 4 && pending.runtime.incident_origins.size() == 2 &&
          pending.world.simulation.random[markets] == next_market && pending.world.simulation.random[hazards] == next_hazard &&
          pending.world.simulation.random[narrative] == story_only.world.simulation.random[narrative],
          "Interleaved market, hazard and story histories must keep separate exact stream cursors");
    check(pending.runtime.activations.back().status == runtime::ActivationStatus::delayed &&
          pending.runtime.random_resolutions.back().branch_id == "incident" && pending.runtime.presentations.empty(),
          "A selected incident retains its delayed continuation before presentation");
    corrupt_save(program, pending, [](auto& state) { state["incident_origins"].erase("markets"); }, "Missing market stream origin must fail");
    corrupt_save(program, pending, [](auto& state) {
        state["random_resolutions"].erase(state["random_resolutions"].begin() + 1);
    }, "Dropping the quiet sample cannot detach the later market draw");
    corrupt_save(program, pending, [](auto& state) {
        state["random_resolutions"].back()["branch_id"] = "no_event";
    }, "Save cannot switch a selected incident to a quiet outcome");
    resumed = runtime::Runtime{program, reload(program, pending)};
    step(live);
    step(resumed);
    const auto choice = resumed.checkpoint();
    check(choice == live.checkpoint() && choice.runtime.presentations.size() == 2 &&
          choice.runtime.presentations.back().kind == runtime::PresentationKind::dialogue &&
          choice.world.simulation.random == pending.world.simulation.random,
          "Delayed news and required choice reuse the saved outcome without a draw");
    resumed = runtime::Runtime{program, reload(program, choice)};
    const auto presentation = choice.runtime.presentations.back().id;
    accepted(live.choose(revision(live), presentation, "continue"));
    accepted(resumed.choose(revision(resumed), presentation, "continue"));
    for (int day = 7; day <= 9; ++day) {
        step(live);
        step(resumed);
    }
    const auto final = resumed.checkpoint();
    check(final == live.checkpoint() && final.world.simulation.random == pending.world.simulation.random &&
          final.world.decisions.back() == "market.reviewed",
          "Review cap includes quiet draws and prevents a third sample at the next due date");

    auto ineligible = start;
    ineligible.world.enabled_flows = std::vector<std::string>{"flow.market"};
    ineligible.world.facts.at("company.is_operating") = false;
    runtime::Runtime guard{program, ineligible};
    prepare(guard);
    step(guard);
    step(guard);
    check(guard.checkpoint().runtime.random_resolutions.empty() && guard.checkpoint().world.simulation.random == start.world.simulation.random,
          "An ineligible due root consumes no randomness");
}
void invalid_incident_content() {
    auto definition = incident("hazards", "record");
    definition.nodes.emplace("record", marker("incident", "end"));
    const auto invalid = [&](const content::FlowDefinition& value, std::string_view code) {
        try { (void)compile({{"flow.incident", value}}); }
        catch (const runtime::CompileError& error) {
            check(error.code() == code, error.what());
            return;
        }
        throw std::runtime_error("Invalid incident content was accepted");
    };
    invalid(definition, "incident_boundary");
    definition.lifecycle.mode = content::LifecycleMode::scheduled;
    definition.lifecycle.due_date = core::OrdinalDay{0};
    auto missing = definition;
    missing.nodes.at("sample").branches.front().id = "silence";
    invalid(missing, "incident_outcome");
    auto loop = definition;
    loop.nodes.at("record").next = "sample";
    invalid(loop, "incident_boundary");
    auto quiet_has_effect = definition;
    quiet_has_effect.nodes.at("sample").branches.front().next = "record";
    invalid(quiet_has_effect, "incident_outcome");
}
} // namespace
int main() {
    try {
        frozen_seed_oracle_and_inspection();
        previously_advanced_narrative_origin();
        nested_saved_continuations();
        rejected_operations_rollback_random();
        repeated_node_visits_and_record_validation();
        deterministic_legacy_save();
        seeded_openings_and_reload();
        invalid_incident_content();
        std::cout << checks << " weighted branch, exact reload and rollback checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
