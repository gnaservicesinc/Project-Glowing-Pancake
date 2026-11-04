#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace pgp;
namespace {
using Json = nlohmann::json;
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void accepted(const runtime::RuntimeResult& result) {
    if (!result.accepted()) throw std::runtime_error(result.explanation);
    ++checks;
}
std::uint64_t revision(const runtime::Runtime& engine) { return engine.checkpoint().world.simulation.revision; }
void prepare(runtime::Runtime& engine) { accepted(engine.prepare_opening(revision(engine))); }
void step(runtime::Runtime& engine) { accepted(engine.submit({revision(engine), core::AdvanceDay{}})); }
void days(runtime::Runtime& engine, unsigned count) { for (unsigned day = 0; day < count; ++day) step(engine); }
content::Condition fact(std::string id = "company.is_operating", content::Scalar value = true,
                        content::Unit unit = content::Unit::boolean) {
    content::Condition result{};
    result.kind = content::ConditionKind::clause;
    result.clause = content::FactClause{std::move(id), content::Comparison::equal, std::move(value), unit};
    return result;
}
content::Condition duration(std::string id, std::uint32_t count, content::Condition condition = fact()) {
    content::Condition result{};
    result.kind = content::ConditionKind::holds_for_days;
    result.predicate_id = std::move(id); result.duration_days = count;
    result.children.push_back(std::move(condition));
    return result;
}
content::Node terminal() {
    content::Node result{}; result.kind = content::NodeKind::end; return result;
}
content::Node effect(std::vector<content::Effect> effects, std::string next = "end") {
    content::Node result{};
    result.kind = content::NodeKind::effect; result.effects = std::move(effects); result.next = std::move(next);
    return result;
}
content::FlowDefinition flow(content::Condition trigger, std::string marker = "duration_fired") {
    content::FlowDefinition result{};
    result.scope = content::Scope::company;
    result.trigger = std::move(trigger);
    result.lifecycle.mode = content::LifecycleMode::once;
    result.lifecycle.occurrence_scope = content::Scope::company;
    result.entry_node = "record";
    result.nodes.emplace("record", effect({{"record_decision", "self", content::Scope::company, {{"value", std::move(marker)}}}}));
    result.nodes.emplace("end", terminal());
    return result;
}
runtime::CompiledProgram compile(std::map<std::string, content::FlowDefinition, std::less<>> flows) {
    content::Collection collection{content::content_schema, "0.0.0-dev", {}};
    for (auto& [id, definition] : flows)
        collection.definitions.emplace(id, content::Definition{id, 1, std::move(definition)});
    return runtime::compile_collection(collection, content::foundation_registry(), runtime::supported_effects());
}
runtime::Checkpoint initial(std::vector<std::string> roots = {"flow.duration"}) {
    runtime::Checkpoint result;
    result.world = runtime::foundation_world(core::Cents{100});
    result.world.enabled_flows = std::move(roots);
    return result;
}
runtime::DurationState timer(const runtime::Checkpoint& state, std::string_view id,
                             std::string_view owner = "flow.duration") {
    const auto found = std::find_if(state.runtime.duration_states.begin(), state.runtime.duration_states.end(),
        [&](const auto& row) { return row.predicate_id == id && row.definition_id == owner; });
    if (found == state.runtime.duration_states.end()) throw std::runtime_error("Expected duration sample is absent");
    return *found;
}
runtime::Checkpoint reload(const runtime::CompiledProgram& program, const runtime::Checkpoint& state) {
    const auto loaded = persistence::decode_save(program, persistence::encode_save(program, state)).checkpoint;
    check(loaded == state, "Duration save/reload preserves the entire checkpoint");
    return loaded;
}
void fixture_fact(runtime::Runtime& engine, const char* id, content::Scalar value) {
    auto state = engine.checkpoint();
    state.world.facts.at(id) = std::move(value);
    engine = runtime::Runtime{engine.program(), std::move(state)};
}
template<class Change>
void corrupt_save(const runtime::CompiledProgram& program, const runtime::Checkpoint& state,
                  Change change, const char* message) {
    auto body = Json::parse(persistence::encode_save(program, state));
    change(body["checkpoint"]["runtime"]);
    body.erase("sha256"); body["sha256"] = persistence::sha256(body.dump());
    bool rejected = false;
    try { (void)persistence::decode_save(program, body.dump()); }
    catch (const persistence::PersistenceError&) { rejected = true; }
    check(rejected, message);
}
void boundary_reset_and_saved_threshold() {
    const auto program = compile({{"flow.duration", flow(duration("operating", 2))}});
    const auto start = initial();
    runtime::Runtime engine{program, start};
    check(engine.checkpoint().runtime.duration_states.empty(), "Construction does not initialize temporal history");
    prepare(engine);
    auto state = engine.checkpoint();
    check(timer(state, "operating").true_since == core::OrdinalDay{0} && state.world.decisions.empty(),
          "First true observation starts at elapsed zero without firing");
    prepare(engine);
    check(engine.checkpoint() == state, "Repeated same-day preparation never changes temporal state");
    step(engine);
    state = engine.checkpoint();
    check(state.world.decisions.empty() && timer(state, "operating").observed_on == core::OrdinalDay{1},
          "N-day condition remains false just before its elapsed-day boundary");
    runtime::Runtime resumed{program, reload(program, state)};
    step(engine); step(resumed);
    check(engine.checkpoint() == resumed.checkpoint() &&
              engine.checkpoint().world.decisions == std::vector<std::string>{"duration_fired"},
          "Save immediately before D+N fires once on the identical next morning");
    check(engine.checkpoint().world.simulation.random == start.world.simulation.random,
          "Duration sampling and effects consume no random stream");

    runtime::Runtime reset{program, start};
    prepare(reset);
    fixture_fact(reset, "company.is_operating", false);
    step(reset);
    check(!timer(reset.checkpoint(), "operating").true_since && reset.checkpoint().world.decisions.empty(),
          "A false morning clears the previous run instead of preserving partial credit");
    fixture_fact(reset, "company.is_operating", true);
    step(reset);
    check(timer(reset.checkpoint(), "operating").true_since == core::OrdinalDay{2},
          "The next true morning starts a fresh run");
    step(reset);
    check(reset.checkpoint().world.decisions.empty(), "Fresh true run must complete its own whole elapsed duration");
    step(reset);
    check(reset.checkpoint().world.decisions.size() == 1 && reset.checkpoint().world.simulation.date == core::OrdinalDay{4},
          "False reset moves the eventual activation to the new D+N boundary");
}
void nested_postorder_and_observation_gap() {
    const auto nested = compile({{"flow.duration", flow(duration("outer", 2, duration("inner", 1)))}});
    runtime::Runtime engine{nested, initial()};
    prepare(engine);
    check(timer(engine.checkpoint(), "inner").true_since == core::OrdinalDay{0} &&
              !timer(engine.checkpoint(), "outer").true_since,
          "Outer duration starts only once its nested duration becomes true");
    step(engine);
    check(timer(engine.checkpoint(), "outer").true_since == core::OrdinalDay{1},
          "Postorder sampling starts the parent on the child's threshold morning");
    step(engine);
    check(engine.checkpoint().world.decisions.empty(), "Nested durations do not collapse into the shorter threshold");
    step(engine);
    check(engine.checkpoint().world.decisions.size() == 1 && engine.checkpoint().world.simulation.date == core::OrdinalDay{3},
          "Nested elapsed durations compose without an extra sampling day");

    const auto program = compile({{"flow.duration", flow(duration("observed", 2))}});
    runtime::Runtime continuous{program, initial()};
    prepare(continuous); step(continuous);
    auto skipped = continuous.checkpoint();
    skipped.world.simulation.date = core::OrdinalDay{5};
    skipped.world.simulation.phase = core::DayPhase::opening;
    skipped.runtime.prepared_day.reset();
    runtime::Runtime gap{program, skipped};
    prepare(gap);
    check(timer(gap.checkpoint(), "observed").true_since == core::OrdinalDay{5} && gap.checkpoint().world.decisions.empty(),
          "Missing daily observations restart a true run instead of crediting unobserved days");
    days(gap, 2);
    check(gap.checkpoint().world.decisions.size() == 1 && gap.checkpoint().world.simulation.date == core::OrdinalDay{7},
          "A restarted run fires only after its new observed D+N boundary");
}
void sample_before_effects_and_choice_resume() {
    auto cash_condition = fact("company.cash_cents", std::int64_t{100}, content::Unit::constant_2014_cents);
    const auto observer = flow(duration("cash_held", 2, std::move(cash_condition)), "cash_stayed_high");
    auto debit = flow(fact(), "unused");
    debit.lifecycle.mode = content::LifecycleMode::scheduled;
    debit.lifecycle.due_date = core::OrdinalDay{1};
    debit.nodes.at("record") = effect({{"adjust_cash", "self", content::Scope::company,
                                        {{"amount_cents", std::int64_t{-50}}}}}, "choice");
    content::Node choice{};
    choice.kind = content::NodeKind::dialogue; choice.title_key = "duration.choice.title";
    choice.text_key = "duration.choice.text"; choice.default_choice = "continue";
    choice.choices = {{"continue", "duration.choice.continue", "end"}};
    debit.nodes.emplace("choice", std::move(choice));
    const auto program = compile({{"flow.a.debit", std::move(debit)}, {"flow.z.observer", observer}});
    runtime::Runtime engine{program, initial({"flow.a.debit", "flow.z.observer"})};
    prepare(engine); step(engine);
    const auto waiting = engine.checkpoint();
    check(waiting.world.simulation.cash == core::Cents{50} &&
              timer(waiting, "cash_held", "flow.z.observer").true_since == core::OrdinalDay{0} &&
              waiting.runtime.presentations.size() == 1 && !waiting.runtime.prepared_day,
          "All roots sample the common opening snapshot before an earlier flow spends cash and waits");
    corrupt_save(program, waiting, [](auto& runtime_state) {
        for (auto& row : runtime_state["duration_states"]) row["observed_on"] = 0;
    }, "Saved content dialogue cannot carry yesterday's temporal rows under today's sample cursor");
    corrupt_save(program, waiting, [](auto& runtime_state) { runtime_state["duration_sampled_day"] = nullptr; },
                 "Saved content dialogue must retain its daily temporal sample cursor");
    runtime::Runtime resumed{program, reload(program, waiting)};
    const auto presentation = waiting.runtime.presentations.front().id;
    accepted(engine.choose(revision(engine), presentation, "continue"));
    accepted(resumed.choose(revision(resumed), presentation, "continue"));
    check(engine.checkpoint() == resumed.checkpoint() &&
              engine.checkpoint().runtime.duration_states == waiting.runtime.duration_states,
          "Choice resume reuses the frozen daily duration sample even after a cash change");
    step(engine); step(resumed);
    check(engine.checkpoint() == resumed.checkpoint() &&
              !timer(engine.checkpoint(), "cash_held", "flow.z.observer").true_since &&
              engine.checkpoint().world.decisions.empty(),
          "The next morning observes the spent cash and resets instead of firing on stale truth");
}
void cooldown_interval_and_disabled_roots() {
    auto recurring = flow(duration("cooldown", 1));
    recurring.lifecycle.mode = content::LifecycleMode::recurring;
    recurring.lifecycle.max_occurrences = 3; recurring.lifecycle.cooldown_days = 3;
    auto interval = flow(duration("not_due", 2), "interval_fired");
    interval.lifecycle.mode = content::LifecycleMode::interval;
    interval.lifecycle.max_occurrences = 3;
    interval.lifecycle.interval = content::Interval{content::IntervalUnit::days, 3};
    interval.lifecycle.anchor = content::Anchor{content::AnchorKind::campaign_start, {}, {content::IntervalUnit::days, 0}};
    auto disabled = flow(duration("disabled", 1), "disabled_fired");
    const auto program = compile({{"flow.duration", std::move(recurring)}, {"flow.interval", std::move(interval)},
                                  {"flow.disabled", std::move(disabled)}});
    runtime::Runtime engine{program, initial({"flow.duration", "flow.interval"})};
    prepare(engine); step(engine);
    check(engine.checkpoint().world.decisions == std::vector<std::string>{"duration_fired"},
          "Recurring duration activates at its elapsed boundary");
    fixture_fact(engine, "company.is_operating", false);
    step(engine);
    check(!timer(engine.checkpoint(), "cooldown").true_since &&
              !timer(engine.checkpoint(), "not_due", "flow.interval").true_since,
          "Cooldown and off-interval roots still observe false resets");
    fixture_fact(engine, "company.is_operating", true);
    step(engine);
    check(timer(engine.checkpoint(), "not_due", "flow.interval").true_since == core::OrdinalDay{3} &&
              engine.checkpoint().world.decisions.size() == 1,
          "An interval due date skips an incomplete duration without backfilling");
    step(engine);
    check(engine.checkpoint().world.decisions.size() == 2 &&
              timer(engine.checkpoint(), "cooldown").observed_on == core::OrdinalDay{4},
          "Recurring root resumes after both cooldown and restarted duration permit it");
    days(engine, 2);
    check(engine.checkpoint().world.decisions.back() == "interval_fired" &&
              timer(engine.checkpoint(), "not_due", "flow.interval").observed_on == core::OrdinalDay{6},
          "Off-interval daily history can satisfy the next due interval");
    const auto observed = engine.checkpoint();
    check(observed.runtime.duration_states.size() == 2 &&
              std::none_of(observed.runtime.duration_states.begin(), observed.runtime.duration_states.end(),
                  [](const auto& row) { return row.definition_id == "flow.disabled"; }),
          "Disabled reusable root triggers do not acquire autonomous duration observations");
}
void decisive_siblings_still_sample_and_reset() {
    content::Condition any{};
    any.kind = content::ConditionKind::any_of;
    any.children = {fact(), duration("unselected", 1, fact("company.has_research_partner"))};
    content::Condition all{};
    all.kind = content::ConditionKind::all_of;
    all.children = {fact("company.is_operating", false),
                    duration("unselected", 1, fact("company.has_research_partner"))};
    const auto program = compile({{"flow.any", flow(std::move(any), "decisive_any")},
                                  {"flow.all", flow(std::move(all), "blocked_all")}});
    auto start = initial({"flow.any", "flow.all"});
    start.world.facts.at("company.has_research_partner") = true;
    runtime::Runtime engine{program, start};
    prepare(engine);
    check(engine.checkpoint().world.decisions == std::vector<std::string>{"decisive_any"} &&
              timer(engine.checkpoint(), "unselected", "flow.any").true_since == core::OrdinalDay{0} &&
              timer(engine.checkpoint(), "unselected", "flow.all").true_since == core::OrdinalDay{0},
          "Publicly decisive any/all siblings never suppress duration starts");
    fixture_fact(engine, "company.has_research_partner", false);
    step(engine);
    check(!timer(engine.checkpoint(), "unselected", "flow.any").true_since &&
              !timer(engine.checkpoint(), "unselected", "flow.all").true_since,
          "Unselected duration children still reset even after a once-only root has fired");
}
void inverted_condition_and_sampling_budget() {
    content::Condition inverted{};
    inverted.kind = content::ConditionKind::not_condition;
    inverted.children.push_back(fact());
    const auto program = compile({{"flow.duration", flow(duration("not_operating", 1, std::move(inverted)))}});
    runtime::Runtime engine{program, initial()};
    prepare(engine);
    check(!timer(engine.checkpoint(), "not_operating").true_since,
          "Negation inside a duration observes its typed child's inverted truth");
    fixture_fact(engine, "company.is_operating", false);
    step(engine);
    check(timer(engine.checkpoint(), "not_operating").true_since == core::OrdinalDay{1} &&
              engine.checkpoint().world.decisions.empty(),
          "A negated condition starts its own run when the child becomes false");
    step(engine);
    check(engine.checkpoint().world.decisions.size() == 1, "Negated duration activates on its own elapsed threshold");

    std::map<std::string, content::FlowDefinition, std::less<>> definitions;
    std::vector<std::string> roots;
    for (unsigned index = 0; index < 33; ++index) {
        content::Condition all{};
        all.kind = content::ConditionKind::all_of;
        for (unsigned predicate = 0; predicate < 63; ++predicate)
            all.children.push_back(duration("predicate_" + std::to_string(predicate), 1));
        const auto id = "flow.budget_" + std::to_string(index);
        definitions.emplace(id, flow(std::move(all)));
        roots.push_back(id);
    }
    const auto bounded = compile(std::move(definitions));
    const auto before = initial(std::move(roots));
    runtime::Runtime refused{bounded, before};
    const auto result = refused.prepare_opening(revision(refused));
    check(result.reason == runtime::RuntimeReason::budget_exceeded && !result.committed && refused.checkpoint() == before,
          "A bounded but overly expensive temporal prepass refuses atomically without partial samples or lost causes");
}
void pure_saved_inspection_and_hidden_history() {
    const auto program = compile({{"flow.duration", flow(duration("inspect", 2))}});
    runtime::Runtime engine{program, initial()}; prepare(engine); days(engine, 2);
    auto state = engine.checkpoint();
    state.world.facts.at("company.is_operating") = false;
    const auto before = state;
    const auto& condition = std::get<content::FlowDefinition>(program.collection.definitions.at("flow.duration").data).trigger;
    const runtime::ScopeBinding binding{content::Scope::company, state.world.company_id};
    const auto read = [&](const auto& metadata, const auto& selected) {
        return runtime::read_fact(state.world, metadata, selected);
    };
    const runtime::DurationReader saved = [&](std::string_view id, const runtime::ScopeBinding& selected)
        -> std::optional<runtime::DurationObservation> {
        for (const auto& row : state.runtime.duration_states)
            if (row.predicate_id == id && row.binding == selected)
                return runtime::DurationObservation{row.observed_on, row.true_since};
        return std::nullopt;
    };
    const auto inspected = runtime::evaluate_condition(condition, binding, program.registry, read,
        runtime::InspectionMode::author_fixture, "trigger", saved, state.world.simulation.date);
    check(inspected.result == true && inspected.durations.size() == 1 &&
              inspected.durations.front().elapsed_days == 2 && inspected.clauses.front().result == false && state == before,
          "Pure inspection shows frozen duration truth alongside current facts without resampling either");
    const auto absent_reader = runtime::evaluate_condition(condition, binding, program.registry, read);
    check(!absent_reader.result && !absent_reader.durations.front().observed_on && state == before,
          "Without a temporal reader, pure inspection reports unknown and initializes no history");
    const runtime::DurationReader no_history = [](std::string_view, const auto&) -> std::optional<runtime::DurationObservation> {
        return std::nullopt;
    };
    const auto absent_history = runtime::evaluate_condition(condition, binding, program.registry, read,
        runtime::InspectionMode::author_fixture, "trigger", no_history, state.world.simulation.date);
    check(absent_history.result == false && state == before, "Explicitly missing history is unsatisfied and remains absent after inspection");

    auto registry = content::foundation_registry();
    auto hidden = *registry.fact("company.is_operating");
    hidden.id = "company.hidden_ready"; hidden.visibility = content::Visibility::hidden_truth;
    registry.register_fact(hidden);
    const auto hidden_condition = duration("hidden", 1, fact("company.hidden_ready"));
    unsigned fact_reads = 0, duration_reads = 0;
    const runtime::FactReader hidden_fact = [&](const auto&, const auto&) -> content::Scalar { ++fact_reads; return true; };
    const runtime::DurationReader hidden_history = [&](std::string_view, const auto&) -> std::optional<runtime::DurationObservation> {
        ++duration_reads;
        return runtime::DurationObservation{core::OrdinalDay{2}, core::OrdinalDay{0}};
    };
    const auto redacted = runtime::evaluate_condition(hidden_condition, binding, registry, hidden_fact,
        runtime::InspectionMode::player_known, "trigger", hidden_history, core::OrdinalDay{2});
    check(!redacted.result && fact_reads == 0 && duration_reads == 0 &&
              redacted.durations.front().redacted && !redacted.durations.front().true_since &&
              !redacted.durations.front().elapsed_days && !redacted.durations.front().result &&
              redacted.clauses.front().redacted,
          "Player inspection neither reads nor exposes hidden fact history through a duration trace");
    const auto author = runtime::evaluate_condition(hidden_condition, binding, registry, hidden_fact,
        runtime::InspectionMode::author_fixture, "trigger", hidden_history, core::OrdinalDay{2});
    check(author.result == true && author.durations.front().elapsed_days == 2 && fact_reads == 1 && duration_reads == 1,
          "Author inspection can explicitly inspect the complete temporal fixture");
}
void continuous_scope_history_and_reentry() {
    auto continuous = flow(duration("enter", 2));
    continuous.lifecycle.mode = content::LifecycleMode::continuous;
    continuous.lifecycle.allow_reentry = true;
    continuous.lifecycle.exit_when = duration("leave", 3, fact("company.has_research_partner"));
    continuous.exit_node = "exit";
    continuous.nodes.at("record") = effect({{"register_owned_modifier", "self", content::Scope::company,
        {{"key", std::string{"duration_owner"}}, {"field", std::string{"research_coordination_basis_points"}},
         {"add_basis_points", std::int64_t{150}}}}});
    continuous.nodes.emplace("exit", effect({{"remove_owned_modifier", "self", content::Scope::company,
        {{"key", std::string{"duration_owner"}}}}}, "exit_end"));
    continuous.nodes.emplace("exit_end", terminal());
    const auto program = compile({{"flow.duration", std::move(continuous)}});
    auto start = initial(); start.world.facts.at("company.has_research_partner") = true;
    runtime::Runtime engine{program, start};
    const runtime::ScopeBinding binding{content::Scope::company, start.world.company_id};
    prepare(engine);
    check(timer(engine.checkpoint(), "leave").true_since == core::OrdinalDay{0} &&
              engine.checkpoint().runtime.activations.empty(),
          "Scope-history exit duration begins before the first activation");
    days(engine, 2);
    check(runtime::modifier_total(engine.checkpoint().runtime, binding, "research_coordination_basis_points") == 150 &&
              timer(engine.checkpoint(), "enter").observed_on == core::OrdinalDay{2},
          "Continuous entry registers one modifier on its D+N morning");
    runtime::Runtime resumed{program, reload(program, engine.checkpoint())};
    step(engine); step(resumed);
    check(engine.checkpoint() == resumed.checkpoint() &&
              runtime::modifier_total(engine.checkpoint().runtime, binding, "research_coordination_basis_points") == 0 &&
              timer(engine.checkpoint(), "enter").observed_on == core::OrdinalDay{3},
          "Natural duration exit removes its owned modifier and active trigger history still updates");
    step(engine);
    check(engine.checkpoint().runtime.activations.size() == 2 &&
              runtime::modifier_total(engine.checkpoint().runtime, binding, "research_coordination_basis_points") == 150,
          "Explicit reentry reuses satisfied scope history without duplicate ownership");
    step(engine);
    check(runtime::modifier_total(engine.checkpoint().runtime, binding, "research_coordination_basis_points") == 0 &&
              timer(engine.checkpoint(), "leave").true_since == core::OrdinalDay{0},
          "Scope-history exit remains satisfied across reentry and exits on the next daily evaluation");
}
void transaction_rollback_and_corrupt_history() {
    auto duration_flow = flow(duration("observed", 2));
    auto failure = flow(fact(), "unused");
    failure.lifecycle.mode = content::LifecycleMode::scheduled;
    failure.lifecycle.due_date = core::OrdinalDay{1};
    failure.nodes.at("record") = effect({{"adjust_cash", "self", content::Scope::company,
                                          {{"amount_cents", std::int64_t{1}}}}});
    const auto failing_program = compile({{"flow.duration", duration_flow}, {"flow.failure", std::move(failure)}});
    runtime::Runtime failing{failing_program, initial({"flow.duration", "flow.failure"})};
    prepare(failing);
    const auto before_failure = failing.checkpoint();
    const auto refused = failing.submit({revision(failing), core::AdvanceDay{}});
    check(refused.reason == runtime::RuntimeReason::effect_rejected && failing.checkpoint() == before_failure,
          "Failure after daily sampling rolls back date, timer update, effects and RNG together");

    const auto program = compile({{"flow.duration", std::move(duration_flow)}});
    runtime::Runtime engine{program, initial()}; prepare(engine); step(engine);
    const auto state = engine.checkpoint();
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0]["definition_id"] = "flow.missing"; },
                 "Duration history requires an existing owning definition");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0]["definition_revision"] = 2; },
                 "Duration history cannot silently bind to another definition revision");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0]["predicate_id"] = "missing"; },
                 "Duration history requires an existing predicate ID");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0]["binding"]["id"] = "company.other"; },
                 "Duration history cannot move into another scope entity");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0]["binding"]["scope"] = "world"; },
                 "Duration history cannot change its typed scope");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"].push_back(runtime_state["duration_states"][0]); },
                 "Duplicate duration predicate histories reject");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0]["observed_on"] = 2; },
                 "Future duration observation rejects");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0]["true_since"] = 2; },
                 "Duration true-run start cannot follow its last observation");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0]["true_since"] = -1; },
                 "Duration true-run start cannot predate its campaign");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"] = Json::array(); },
                 "Prepared duration content must retain all daily predicate observations");
    corrupt_save(program, state, [](auto& runtime_state) {
        runtime_state["duration_states"] = Json::array(); runtime_state["lifecycles"] = Json::array();
    }, "Deleting a not-yet-fired lifecycle and all its rows cannot erase prepared temporal history");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state.erase("duration_sampled_day"); },
                 "Sampled temporal state must retain its explicit daily cursor");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_sampled_day"] = nullptr; },
                 "The daily temporal cursor cannot be cleared after the prepass");
    corrupt_save(program, state, [](auto& runtime_state) {
        runtime_state["duration_sampled_day"] = 0; runtime_state["prepared_day"] = nullptr;
        for (auto& row : runtime_state["duration_states"]) row["observed_on"] = 0;
        for (auto& lifecycle : runtime_state["lifecycles"]) lifecycle["last_checked"] = nullptr;
    }, "Backdating all observations and clearing preparation cannot forge a stale work-ready temporal snapshot");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state.erase("duration_states"); },
                 "Duration-bearing content requires an explicit saved history collection");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0].erase("observed_on"); },
                 "Missing duration row fields reject");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0]["extra"] = true; },
                 "Unknown duration row fields reject");
    corrupt_save(program, state, [](auto& runtime_state) { runtime_state["duration_states"][0]["observed_on"] = 0.5; },
                 "Fractional duration dates reject without truncation");
    corrupt_save(program, state, [](auto& runtime_state) {
        runtime_state["duration_states"][0]["definition_revision"] = std::uint64_t{1} << 32U;
    }, "Oversized duration revision rejects without narrowing");
    check(engine.checkpoint() == state, "Corrupt temporal-save probes preserve the live campaign");

    const auto deterministic = compile({{"flow.duration", flow(fact())}});
    runtime::Runtime legacy{deterministic, initial()}; prepare(legacy);
    auto body = Json::parse(persistence::encode_save(deterministic, legacy.checkpoint()));
    body["checkpoint"]["runtime"].erase("duration_states");
    body["checkpoint"]["runtime"].erase("duration_sampled_day");
    body.erase("sha256"); body["sha256"] = persistence::sha256(body.dump());
    check(persistence::decode_save(deterministic, body.dump()).checkpoint == legacy.checkpoint(),
          "Earlier saves with no duration-bearing definitions remain readable without temporal fields");
}
} // namespace
int main() {
    try {
        boundary_reset_and_saved_threshold();
        nested_postorder_and_observation_gap();
        sample_before_effects_and_choice_resume();
        cooldown_interval_and_disabled_roots();
        decisive_siblings_still_sample_and_reset();
        inverted_condition_and_sampling_budget();
        pure_saved_inspection_and_hidden_history();
        continuous_scope_history_and_reentry();
        transaction_rollback_and_corrupt_history();
        std::cout << checks << " elapsed-duration, daily sampling and reload checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
