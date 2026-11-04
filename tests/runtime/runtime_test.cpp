#include <pgp/runtime/runtime.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace pgp;
namespace {
int checks = 0;
void check(bool condition, const char* detail) {
    ++checks;
    if (!condition) throw std::runtime_error(detail);
}
void accepted(const runtime::RuntimeResult& result) {
    if (!result.accepted()) throw std::runtime_error(std::string(runtime::to_string(result.reason)) + ": " + result.explanation);
    ++checks;
}
runtime::CompiledProgram program() {
    std::ifstream input(PGP_RUNTIME_FIXTURE_PATH);
    const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
    return runtime::compile_collection(content::load_collection(bytes, content::foundation_registry()), content::foundation_registry(), runtime::supported_effects());
}
std::uint64_t revision(const runtime::Runtime& instance) { return instance.checkpoint().world.simulation.revision; }
void advance(runtime::Runtime& instance, int days) {
    for (int day = 0; day < days; ++day)
        accepted(instance.submit({revision(instance), core::AdvanceDay{}}));
}
runtime::Checkpoint initial(std::vector<std::string> enabled, std::int64_t cash = 1000) {
    runtime::Checkpoint checkpoint;
    checkpoint.world = runtime::foundation_world(core::Cents{cash});
    checkpoint.world.enabled_flows = std::move(enabled);
    return checkpoint;
}
runtime::Activation activation(const runtime::Checkpoint& checkpoint, std::string_view id,
                               runtime::ActivationStatus status) {
    const auto found = std::find_if(checkpoint.runtime.activations.begin(), checkpoint.runtime.activations.end(), [&](const auto& item) {
        return item.definition_id == id && item.status == status;
    });
    if (found == checkpoint.runtime.activations.end()) throw std::runtime_error("Expected activation not found");
    return *found;
}
std::uint64_t pending(const runtime::Runtime& instance) {
    for (const auto& presentation : instance.checkpoint().runtime.presentations)
        if (presentation.kind == runtime::PresentationKind::dialogue && !presentation.resolved) return presentation.id;
    throw std::runtime_error("Expected dialogue absent");
}
template<class Change> void malformed(const runtime::CompiledProgram& compiled,
                                      const runtime::Checkpoint& original, Change change) {
    auto corrupted = original;
    change(corrupted);
    try { runtime::validate_checkpoint(compiled, corrupted); }
    catch (const std::exception&) { ++checks; return; }
    throw std::runtime_error("Malformed checkpoint was accepted");
}
content::FlowDefinition simple_flow(std::vector<content::Effect> effects) {
    content::FlowDefinition value{};
    value.scope = content::Scope::company;
    value.trigger = {content::ConditionKind::clause,
        content::FactClause{"company.is_operating", content::Comparison::equal, true, content::Unit::boolean}, {}};
    value.lifecycle = {content::LifecycleMode::once, content::Scope::company, 1, 0, {}, {}, {}, {}, false};
    value.entry_node = "start";
    content::Node start{}; start.kind = content::NodeKind::effect; start.effects = std::move(effects); start.next = "end";
    content::Node end{}; end.kind = content::NodeKind::end;
    value.nodes.emplace("start", std::move(start)); value.nodes.emplace("end", std::move(end));
    return value;
}
content::Effect cash(std::int64_t amount) {
    return {"adjust_cash", "self", content::Scope::company, {{"amount_cents", amount}}};
}
void full_fixture_and_replay() {
    const auto compiled = program();
    runtime::Checkpoint checkpoint;
    checkpoint.world = runtime::foundation_world(core::Cents{100000});
    checkpoint.world.facts["company.has_compute_lease"] = true;
    checkpoint.world.facts["company.monthly_compute_bill_cents"] = std::int64_t{2000000};
    checkpoint.world.facts["company.workload_stress_basis_points"] = std::int64_t{8000};
    checkpoint.world.facts["world.public_breakthrough_count"] = std::int64_t{1};
    checkpoint.world.facts["company.has_research_partner"] = true;
    const auto random = checkpoint.world.simulation.random;
    runtime::Runtime instance(compiled, checkpoint);
    accepted(instance.prepare_opening(0));
    const auto waiting = instance.checkpoint();
    check(!waiting.runtime.prepared_day && waiting.world.simulation.paused, "Required choice suspends prepared morning");
    const auto delayed = activation(waiting, "flow.hidden.sequence_resume", runtime::ActivationStatus::delayed);
    check(delayed.frames.size() == 2 && delayed.current_node == "wait" && delayed.due == core::OrdinalDay{14}, "Two nested frames save exact delay boundary");
    check(runtime::modifier_total(waiting.runtime, {content::Scope::company, waiting.world.company_id}, "research_coordination_basis_points") == -500,
          "Continuous entry owns one modifier");
    accepted(instance.prepare_opening(revision(instance)));
    check(instance.checkpoint() == waiting, "Reopening a suspended morning is exactly idempotent");
    check(instance.submit({revision(instance), core::AdvanceDay{}}).reason == runtime::RuntimeReason::morning_not_ready,
          "Required flow choice gates ordinary day commands");
    check(instance.submit({revision(instance), core::ReserveFunds{"Forbidden", core::Cents{10}}}).reason == runtime::RuntimeReason::morning_not_ready,
          "Core work_ready cannot bypass content choice");
    check(instance.choose(0, pending(instance), "review").reason == runtime::RuntimeReason::stale_revision, "Choice revalidates revision");
    check(instance.choose(revision(instance), pending(instance), "missing").reason == runtime::RuntimeReason::invalid_choice, "Unknown choice refused");
    check(instance.checkpoint() == waiting, "Rejected choice and work preserve all state");
    runtime::Runtime restored(compiled, waiting);
    accepted(instance.choose(revision(instance), pending(instance), "review"));
    accepted(restored.choose(revision(restored), pending(restored), "review"));
    check(instance.checkpoint() == restored.checkpoint(), "Choice restore matches uninterrupted continuation");
    check(instance.checkpoint().world.negotiations == std::vector<std::string>{"deal.compute_exclusivity"}, "Negotiation starts once without signed terms/payment");
    check(instance.checkpoint().runtime.prepared_day == core::OrdinalDay{0}, "Remaining opening flows finish after choice");
    for (int day = 0; day < 14; ++day) {
        runtime::Runtime daily_restore(compiled, instance.checkpoint());
        advance(instance, 1); advance(daily_restore, 1);
        check(instance.checkpoint() == daily_restore.checkpoint(), "Every delayed continuation step matches a fresh restore");
    }
    const auto& decisions = instance.checkpoint().world.decisions;
    check(std::count(decisions.begin(), decisions.end(), "inner_done") == 1 &&
          std::count(decisions.begin(), decisions.end(), "outer_done") == 1, "Delayed numbered siblings each execute once");
    const auto inner = std::find(decisions.begin(), decisions.end(), "inner_done");
    const auto outer = std::find(decisions.begin(), decisions.end(), "outer_done");
    check(inner < outer, "Nested sequence preserves inner then outer effect order");
    check(instance.checkpoint().world.simulation.random == random, "Inspection and deterministic flow execution consume no future RNG");
    check(instance.checkpoint().world.simulation.cash == core::Cents{100000}, "News and negotiations cannot invent financial consequences");
    malformed(compiled, waiting, [](auto& copy) { copy.runtime.activations[1].frames[0].next_child = 2; });
    malformed(compiled, waiting, [](auto& copy) { copy.world.facts["company.cash_cents"] = std::int64_t{1}; });
    malformed(compiled, waiting, [](auto& copy) { copy.runtime.activations[1].due = core::OrdinalDay{15}; });
    malformed(compiled, waiting, [](auto& copy) { copy.runtime.activations[1].current_node = "wait_return"; });
    malformed(compiled, waiting, [](auto& copy) { copy.runtime.prepared_day = copy.world.simulation.date; });
    malformed(compiled, waiting, [](auto& copy) { copy.runtime.lifecycles[1].active_activation.reset(); });
    malformed(compiled, waiting, [](auto& copy) { copy.runtime.receipts[0].after = true; });
}
void lifecycle_boundaries() {
    auto compiled = program();
    auto recurring = initial({"flow.hidden.compute_review"});
    recurring.world.facts["company.has_compute_lease"] = true;
    runtime::Runtime loop(compiled, recurring);
    accepted(loop.prepare_opening(0));
    advance(loop, 360);
    check(loop.checkpoint().world.decisions.size() == 4, "Recurring cooldown and occurrence cap both apply");

    auto monthly = initial({"flow.news.monthly_operations"});
    monthly.world.simulation.date = core::to_ordinal({2014, 1, 31});
    monthly.world.scope_created = monthly.world.simulation.date;
    runtime::Runtime calendar(compiled, monthly);
    accepted(calendar.prepare_opening(0));
    advance(calendar, 28);
    check(calendar.checkpoint().runtime.presentations.size() == 1, "First month clamps Jan31 to Feb28");
    advance(calendar, 31);
    check(calendar.checkpoint().world.simulation.date == core::to_ordinal({2014, 3, 31}) &&
          calendar.checkpoint().runtime.presentations.size() == 2, "Saved preferred day returns to Mar31 without clamp drift");
    const auto month_state = calendar.checkpoint().runtime.lifecycles.front();
    check(month_state.calendar_day == 31, "Interval checkpoint retains original calendar day");

    auto scheduled = initial({"flow.news.scheduled_partner_review"});
    scheduled.world.simulation.date = core::to_ordinal({2014, 6, 1});
    runtime::Runtime skipped(compiled, scheduled);
    accepted(skipped.prepare_opening(0));
    auto changed = skipped.checkpoint(); changed.world.facts["company.has_research_partner"] = true;
    runtime::Runtime eligible_later(compiled, changed);
    advance(eligible_later, 1);
    check(eligible_later.checkpoint().runtime.presentations.empty(), "Scheduled skip never backfills later eligibility");
    scheduled.world.facts["company.has_research_partner"] = true;
    runtime::Runtime due(compiled, scheduled); accepted(due.prepare_opening(0));
    check(due.checkpoint().runtime.presentations.size() == 1, "Scheduled event fires at its eligible due boundary");
}
void continuous_and_cleanup() {
    auto source = program();
    auto& original = std::get<content::FlowDefinition>(source.collection.definitions.at("flow.hidden.sustained_workload").data);
    auto second = original;
    second.nodes.at("enter").effects[0].arguments["key"] = std::string{"parallel_modifier"};
    second.nodes.at("enter").effects[0].arguments["add_basis_points"] = std::int64_t{-100};
    second.nodes.at("exit").effects[0].arguments["key"] = std::string{"parallel_modifier"};
    second.lifecycle.exit_when->children[0].clause->value = std::int64_t{3000};
    source.collection.definitions.emplace("flow.hidden.parallel_workload", content::Definition{"flow.hidden.parallel_workload", 1, second});
    const auto compiled = runtime::compile_collection(source.collection, source.registry);
    auto state = initial({"flow.hidden.sustained_workload", "flow.hidden.parallel_workload"});
    state.world.facts["company.workload_stress_basis_points"] = std::int64_t{8000};
    runtime::Runtime active(compiled, state); accepted(active.prepare_opening(0));
    const runtime::ScopeBinding company{content::Scope::company, state.world.company_id};
    check(runtime::modifier_total(active.checkpoint().runtime, company, "research_coordination_basis_points") == -600, "Concurrent owners add bounded separate contributions");
    const auto receipts = active.checkpoint().runtime.receipts.size();
    advance(active, 1);
    check(active.checkpoint().runtime.receipts.size() == receipts, "Unchanged active tick never reapplies modifiers");
    auto relieved = active.checkpoint(); relieved.world.facts["company.workload_stress_basis_points"] = std::int64_t{4000};
    runtime::Runtime natural(compiled, relieved); advance(natural, 1);
    check(runtime::modifier_total(natural.checkpoint().runtime, company, "research_coordination_basis_points") == -100,
          "Natural exit removes only eligible owner's contribution");
    check(std::get<std::int64_t>(natural.checkpoint().world.facts.at("company.workload_stress_basis_points")) == 4000,
          "Exit preserves unrelated base-state transition");
    const auto snapshot = natural.checkpoint();
    malformed(compiled, snapshot, [](auto& copy) { copy.runtime.modifiers.clear(); });
    malformed(compiled, snapshot, [](auto& copy) {
        for (auto& item : copy.runtime.activations)
            if (item.status == runtime::ActivationStatus::continuous_active) item.current_node = "enter";
    });
    runtime::Runtime synthetic(compiled, snapshot);
    const auto id = activation(snapshot, "flow.hidden.parallel_workload", runtime::ActivationStatus::continuous_active).id;
    const auto cleanup = synthetic.synthetic_cleanup(revision(synthetic), id); accepted(cleanup);
    check(cleanup.paths.size() == 1 && cleanup.paths[0].synthetic_cleanup, "Forced probe is explicitly labeled synthetic cleanup");
    check(runtime::modifier_total(synthetic.checkpoint().runtime, company, "research_coordination_basis_points") == 0 && natural.checkpoint() == snapshot,
          "Synthetic copied-state cleanup cannot mutate live state or claim natural exit");
    auto reenter = natural.checkpoint(); reenter.world.facts["company.workload_stress_basis_points"] = std::int64_t{8000};
    runtime::Runtime again(compiled, reenter); advance(again, 1);
    check(runtime::modifier_total(again.checkpoint().runtime, company, "research_coordination_basis_points") == -600,
          "Natural exit permits one clean re-entry without duplicate surviving ownership");
}
void cash_atomicity_and_budgets() {
    auto source = program();
    source.collection.definitions.emplace("flow.cash.atomic", content::Definition{"flow.cash.atomic", 1,
        simple_flow({cash(-30), cash(500)})});
    auto compiled = runtime::compile_collection(source.collection, source.registry);
    runtime::Runtime rejected(compiled, initial({"flow.cash.atomic"}, 100));
    const auto before = rejected.checkpoint();
    const auto failed = rejected.prepare_opening(0);
    check(failed.reason == runtime::RuntimeReason::effect_rejected, "Unfunded second effect refuses the segment");
    check(!failed.committed && !failed.paths.empty() && !failed.paths[0].committed &&
          failed.paths[0].detail.starts_with("Rolled-back candidate:"), "Failed operation traces distinguish candidates from committed work");
    check(rejected.checkpoint() == before, "Earlier cash effect, receipts, occurrence and cursor all roll back atomically");
    auto& cash_flow = std::get<content::FlowDefinition>(source.collection.definitions.at("flow.cash.atomic").data);
    cash_flow.nodes.at("start").effects = {cash(30)};
    compiled = runtime::compile_collection(source.collection, source.registry);
    auto funded = initial({"flow.cash.atomic"}, 100); funded.world.counterparty_cash = core::Cents{100};
    runtime::Runtime transferred(compiled, funded); accepted(transferred.prepare_opening(0));
    check(transferred.checkpoint().world.simulation.cash == core::Cents{130} &&
          transferred.checkpoint().world.counterparty_cash == core::Cents{70}, "Authorized cash moves conserve both sides");
    check(transferred.checkpoint().runtime.receipts[0].counterparty_delta == core::Cents{-30}, "Cash receipt retains balanced counterparty delta");
    auto copied = transferred.checkpoint();
    malformed(compiled, copied, [](auto& value) { value.runtime.receipts[0].counterparty_delta = core::Cents{0}; });
    cash_flow.nodes.at("start").effects = {cash(-20)};
    compiled = runtime::compile_collection(source.collection, source.registry);
    auto reserved = initial({"flow.cash.atomic"}, 100);
    core::Simulation core_state(reserved.world.simulation);
    check(core_state.submit({0, core::ReserveFunds{"Protected", core::Cents{90}}}).accepted(), "Reserve fixture funds");
    reserved.world.simulation = core_state.checkpoint();
    runtime::Runtime protected_cash(compiled, reserved);
    check(protected_cash.prepare_opening(revision(protected_cash)).reason == runtime::RuntimeReason::effect_rejected,
          "Effects cannot spend committed reservation cash");

    auto loop = simple_flow({{"record_decision", "self", content::Scope::company, {{"value", std::string{"loop_tick"}}}}});
    loop.entry_node = "wait"; loop.max_node_visits = 5;
    content::Node wait{}; wait.kind = content::NodeKind::delay; wait.delay_days = 1; wait.next = "start";
    loop.nodes.emplace("wait", wait); loop.nodes.erase("end"); loop.nodes.at("start").next = "wait";
    source.collection.definitions.emplace("flow.loop.bounded", content::Definition{"flow.loop.bounded", 1, loop});
    compiled = runtime::compile_collection(source.collection, source.registry);
    runtime::Runtime bounded(compiled, initial({"flow.loop.bounded"})); accepted(bounded.prepare_opening(0));
    advance(bounded, 2);
    const auto limit = bounded.checkpoint();
    check(bounded.submit({revision(bounded), core::AdvanceDay{}}).reason == runtime::RuntimeReason::budget_exceeded,
          "Positive-delay loop obeys authored lifetime visit bound");
    check(bounded.checkpoint() == limit, "Budget failure cannot advance date or lose required continuation");
}
void raw_opening_restore() {
    const auto compiled = program();
    auto checkpoint = initial({}, 100);
    checkpoint.world.simulation.phase = core::DayPhase::opening;
    checkpoint.world.simulation.next_reservation_id = 2;
    checkpoint.world.simulation.reservations = {{1, "Due work", core::Cents{60}, core::Cents{0}, false, core::ReservationStatus::active}};
    checkpoint.world.simulation.opening_actions = {{1, core::OrdinalDay{0}, 1, core::Cents{20}, false, "Delivery"},
                                                  {2, core::OrdinalDay{0}, 1, core::Cents{30}, true, "Opening decision"}};
    runtime::Runtime initial_opening(compiled, checkpoint);
    accepted(initial_opening.prepare_opening(0));
    const auto suspended = initial_opening.checkpoint();
    check(suspended.world.simulation.cash == core::Cents{80} && suspended.world.simulation.opening_cursor == 1 &&
          suspended.world.simulation.phase == core::DayPhase::awaiting_choice, "Raw opening save resumes deliveries before required decision");
    accepted(initial_opening.prepare_opening(revision(initial_opening)));
    check(initial_opening.checkpoint() == suspended, "Reopening core choice does not repeat delivered consumption");
    runtime::Runtime restored(compiled, suspended);
    accepted(restored.submit({revision(restored), core::ResolveOpeningChoice{2, true}}));
    check(restored.checkpoint().world.simulation.cash == core::Cents{50} &&
          restored.checkpoint().runtime.prepared_day == core::OrdinalDay{0}, "Core choice resolution prepares flow morning exactly once");
    runtime::Runtime again(compiled, restored.checkpoint());
    check(again.checkpoint() == restored.checkpoint(), "Constructing prepared runtime executes no additional work");
}
void subflow_resume() {
    auto source = program();
    auto caller = simple_flow({{"record_decision", "self", content::Scope::company,
                               {{"value", std::string{"caller_done"}}}}});
    auto callee = simple_flow({{"record_decision", "self", content::Scope::company,
                               {{"value", std::string{"callee_done"}}}}});
    content::Node call{}; call.kind = content::NodeKind::subflow; call.subflow_id = "flow.call.child"; call.next = "start";
    caller.nodes.emplace("call", call); caller.entry_node = "call";
    content::Node wait{}; wait.kind = content::NodeKind::delay; wait.delay_days = 1; wait.next = "start";
    callee.nodes.emplace("wait", wait); callee.entry_node = "wait";
    source.collection.definitions.emplace("flow.call.root", content::Definition{"flow.call.root", 1, caller});
    source.collection.definitions.emplace("flow.call.child", content::Definition{"flow.call.child", 1, callee});
    const auto compiled = runtime::compile_collection(source.collection, source.registry);
    runtime::Runtime uninterrupted(compiled, initial({"flow.call.root"}));
    accepted(uninterrupted.prepare_opening(0));
    const auto waiting = uninterrupted.checkpoint();
    check(waiting.runtime.activations.size() == 1 && waiting.runtime.lifecycles.size() == 1 &&
          waiting.runtime.activations[0].frames.size() == 1 &&
          waiting.runtime.activations[0].frames[0].kind == runtime::FrameKind::subflow &&
          waiting.runtime.activations[0].current_flow == "flow.call.child", "Reusable child executes only through its saved call frame");
    malformed(compiled, waiting, [](auto& checkpoint) {
        checkpoint.runtime.activations[0].frames[0].flow_id = "flow.call.child";
    });
    runtime::Runtime restored(compiled, waiting);
    advance(uninterrupted, 1); advance(restored, 1);
    check(uninterrupted.checkpoint() == restored.checkpoint(), "Saved subflow delay resumes exactly like uninterrupted execution");
    check(uninterrupted.checkpoint().world.decisions == std::vector<std::string>{"callee_done", "caller_done"},
          "Subflow returns once to its caller's later effects in order");
}
} // namespace
int main() {
    try {
        full_fixture_and_replay(); lifecycle_boundaries(); continuous_and_cleanup(); cash_atomicity_and_budgets(); raw_opening_restore(); subflow_resume();
        std::cout << checks << " runtime lifecycle, continuation, transaction and restore checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
