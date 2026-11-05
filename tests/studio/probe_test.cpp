#include <pgp/studio/flow_probe.hpp>

#include <pgp/persistence/storage.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pgp;
std::size_t checks = 0;
void check(bool valid, const char* detail) {
    ++checks;
    if (!valid) throw std::runtime_error(detail);
}
void accepted(const studio::ProbeStep& step) {
    check(step.trace.accepted(), step.trace.explanation.c_str());
}
runtime::CompiledProgram foundation() {
    const auto registry = content::foundation_registry();
    return runtime::compile_collection(content::load_collection(
        persistence::read_bounded(PGP_STUDIO_FIXTURE_PATH), registry), registry);
}
content::FlowDefinition& flow(content::Collection& collection, const std::string& id) {
    return std::get<content::FlowDefinition>(collection.definitions.at(id).data);
}
content::Condition cash_condition(content::Comparison comparison, std::int64_t amount) {
    return {content::ConditionKind::clause,
        content::FactClause{"company.cash_cents", comparison, amount, content::Unit::constant_2014_cents}, {}};
}
void ordered_impacts_and_boundaries() {
    auto source = foundation();
    auto& current = flow(source.collection, "flow.hidden.compute_review");
    current.trigger = cash_condition(content::Comparison::greater_equal, 120'000'000);
    current.nodes.at("record_review").effects = {
        {"adjust_cash", "self", content::Scope::company, {{"amount_cents", std::int64_t{-200}}}},
        {"adjust_cash", "self", content::Scope::company, {{"amount_cents", std::int64_t{50}}}}};
    const auto compiled = runtime::compile_collection(source.collection, source.registry);
    const studio::ProbeFixture fixture{"scenario.foundation", "US-CA", "flow.hidden.compute_review", 42};
    studio::FlowProbe probe{compiled, fixture};
    const auto original = probe.state();
    const auto samples = probe.boundaries("flow.hidden.compute_review");
    check(samples.size() == 5 && samples[0].result == false && samples[1].result == true &&
        samples[2].result == true && samples[3].result == false && !samples[4].result &&
        !samples[4].error.empty() && probe.state() == original,
        "Automatic single-clause boundaries include zero and an erroneous absent binding without state mutation");
    for (const std::int64_t delta : {-1, 0, 1}) {
        auto boundary = original;
        if (delta != 0) {
            core::record_cash(*boundary.world.simulation.accounts, boundary.world.simulation.date,
                core::Cents{delta}, core::Account::equity, "probe_boundary", "probe_boundary");
            boundary.world.simulation.cash = core::Cents{120'000'000 + delta};
        }
        studio::FlowProbe candidate{compiled, boundary};
        const auto result = candidate.inspect("flow.hidden.compute_review", runtime::FlowPredicate::trigger);
        check(result.trace.result == (delta >= 0) && result.trace.clauses.front().actual ==
            content::Scalar{120'000'000 + delta}, "Threshold inspection preserves below/equal/above values");
        check(candidate.state() == boundary, "Pure inspection preserves fixture and RNG");
    }
    const auto step = probe.prepare();
    accepted(step);
    check(step.effects.size() == 2 && step.postings.size() == 2 &&
        step.effects[0].before == content::Scalar{std::int64_t{120'000'000}} &&
        step.effects[0].after == content::Scalar{std::int64_t{119'999'800}} &&
        step.effects[1].after == content::Scalar{std::int64_t{119'999'850}},
        "Ordered effects expose actual before and after values");
    check(step.effects[0].target.id == original.world.company_id && step.effects[1].effect_index == 1 &&
        step.postings[0].credit == core::Account::cash && step.postings[1].debit == core::Account::cash,
        "Resolved target, authored order and cash direction share the game ledger");
    check(step.trace.condition_checks.size() == 1 && step.trace.condition_checks[0].result == true &&
        step.trace.paths[0].path == "flows/flow.hidden.compute_review/nodes/record_review",
        "Author trace contains exact condition truth and execution path");
    accepted(probe.prepare());
    check(original.world.simulation.cash == core::Cents{120'000'000} &&
        original.world.simulation.random == probe.state().world.simulation.random,
        "The caller-owned fixture and RNG are isolated");
    current.nodes.at("record_review").effects[1].arguments["amount_cents"] = std::int64_t{201};
    const auto rejected_program = runtime::compile_collection(source.collection, source.registry);
    studio::FlowProbe invalid{rejected_program, fixture};
    const auto before = invalid.state();
    const auto failed = invalid.prepare();
    check(!failed.trace.accepted() && failed.effects.empty() && failed.postings.empty() && invalid.state() == before,
        "A rejected downstream effect leaves no committed delta or posting");
    check(!failed.trace.paths.empty() && !failed.trace.paths.front().committed &&
        failed.trace.condition_checks.front().path.starts_with("rolled_back_candidate/"),
        "Rolled-back candidate traces retain their label");
}

void nested_wait_choice_and_random_reload() {
    auto source = foundation();
    auto& current = flow(source.collection, "flow.hidden.sequence_resume");
    current.nodes.at("wait").delay_days = 2;
    current.nodes.at("record_inner").next = "draw";
    content::Node draw{};
    draw.kind = content::NodeKind::random; draw.random_stream = "narrative";
    draw.branches = {{"first", 1, "call"}, {"second", 1, "call"}};
    current.nodes.emplace("draw", draw);
    content::Node call{};
    call.kind = content::NodeKind::subflow; call.subflow_id = "flow.supplier.exclusivity_offer";
    call.next = "inner_branch_return";
    current.nodes.emplace("call", call);
    const auto compiled = runtime::compile_collection(source.collection, source.registry);
    const studio::ProbeFixture fixture{"scenario.foundation", "US-TX", "flow.hidden.sequence_resume", 9001};
    studio::FlowProbe uninterrupted{compiled, fixture}, resumed{compiled, fixture};
    accepted(uninterrupted.prepare()); accepted(resumed.prepare());
    check(resumed.state().runtime.activations.front().frames.size() == 2 &&
        resumed.state().runtime.activations.front().due == core::OrdinalDay{2},
        "Inspection retains nested sibling and due-date state");
    resumed.reload();
    check(resumed.state() == uninterrupted.state(), "Waiting checkpoint round-trips exactly");
    for (int day = 0; day < 2; ++day) {
        accepted(uninterrupted.submit(core::AdvanceDay{})); accepted(resumed.submit(core::AdvanceDay{}));
    }
    check(resumed.state() == uninterrupted.state() && resumed.state().runtime.random_resolutions.size() == 1,
        "A restored wait draws the same one branch before the nested dialogue");
    const auto pending = resumed.state().runtime.presentations.front();
    check(pending.kind == runtime::PresentationKind::dialogue && pending.choices.size() == 3 &&
        resumed.state().runtime.activations.front().frames.size() == 3,
        "The probe reuses real player choices and continuation frames");
    resumed.reload();
    accepted(resumed.prepare());
    check(resumed.state() == uninterrupted.state(), "Reopening a choice neither replays effects nor rerolls");
    const auto result = resumed.choose(pending.id, "decline");
    accepted(result); accepted(uninterrupted.choose(pending.id, "decline"));
    check(resumed.state() == uninterrupted.state() && result.effects.size() == 2 &&
        resumed.state().runtime.presentations.back().kind == runtime::PresentationKind::result,
        "Choice effects, result and outer sibling resume identically");
}

void natural_lifecycle_and_isolated_cleanup() {
    auto source = foundation();
    auto& original = flow(source.collection, "flow.hidden.sustained_workload");
    original.trigger = cash_condition(content::Comparison::greater_equal, 119'000'000);
    original.lifecycle.exit_when = cash_condition(content::Comparison::less_equal, 119'000'000);
    auto parallel = original;
    parallel.nodes.at("enter").effects[0].arguments["add_basis_points"] = std::int64_t{-100};
    parallel.lifecycle.exit_when = cash_condition(content::Comparison::less_equal, 100'000'000);
    source.collection.definitions.emplace("flow.inspection.parallel",
        content::Definition{"flow.inspection.parallel", 1, parallel});
    auto drop = flow(source.collection, "flow.hidden.compute_review");
    drop.lifecycle = {content::LifecycleMode::scheduled, content::Scope::company, 1, 0, {}, {}, {}, {}, false};
    drop.lifecycle.due_date = core::OrdinalDay{1};
    drop.nodes.at("record_review").effects = {
        {"adjust_cash", "self", content::Scope::company, {{"amount_cents", std::int64_t{-10'000'000}}}}};
    source.collection.definitions.emplace("flow.inspection.cash_change",
        content::Definition{"flow.inspection.cash_change", 1, drop});
    std::get<content::ScenarioDefinition>(source.collection.definitions.at("scenario.foundation").data).flows =
        {"flow.hidden.sustained_workload", "flow.inspection.parallel", "flow.inspection.cash_change"};
    const auto compiled = runtime::compile_collection(source.collection, source.registry);
    studio::FlowProbe natural{compiled, studio::ProbeFixture{"scenario.foundation", "US-CA", "", 12}};
    const auto entry = natural.prepare(); accepted(entry);
    check(entry.active_modifiers.size() == 2, "Natural entry retains both modifier owners");
    const auto first = natural.state().runtime.activations.front().id;
    const auto before_cleanup = natural.state();
    const auto cleanup = natural.synthetic_cleanup(first); accepted(cleanup);
    check(cleanup.trace.paths.front().synthetic_cleanup && cleanup.active_modifiers.size() == 1 &&
        natural.state() == before_cleanup, "Synthetic cleanup is labeled and leaves the natural timeline intact");
    const auto active = natural.submit(core::AdvanceDay{}); accepted(active);
    check(active.active_modifiers.size() == 2 && active.effects.size() == 1 &&
        active.effects.front().operation == "adjust_cash", "Active tick preserves owners while the fixture changes real cash");
    const auto exit = natural.submit(core::AdvanceDay{}); accepted(exit);
    check(exit.active_modifiers.size() == 1 &&
        exit.active_modifiers.front().add_basis_points == -100 &&
        std::any_of(exit.trace.condition_checks.begin(), exit.trace.condition_checks.end(), [](const auto& row) {
            return row.path == "flows/flow.hidden.sustained_workload/exit_when" && row.result == true;
        }), "Natural exit removes one owner and preserves the surviving modifier");
    natural.reload();
}
void duration_and_member_inspection() {
    auto source = foundation();
    auto& current = flow(source.collection, "flow.hidden.compute_review");
    content::Condition member{content::ConditionKind::clause,
        content::FactClause{"capacity_source.effective_milli_cu_per_day", content::Comparison::greater_equal,
            std::int64_t{100'000}, content::Unit::milli_CU_per_day}, {}};
    content::Condition related{content::ConditionKind::quantified, {}, {member}};
    related.quantifier = content::Quantifier{content::QuantifierKind::any, "company.capacity_sources", {}};
    current.trigger = {content::ConditionKind::holds_for_days, {}, {related}, "probe_elapsed", 1};
    const auto compiled = runtime::compile_collection(source.collection, source.registry);
    studio::FlowProbe preview{compiled,
        studio::ProbeFixture{"scenario.foundation", "US-WA", "flow.hidden.compute_review", 2}};
    const auto initial = preview.inspect("flow.hidden.compute_review", runtime::FlowPredicate::trigger);
    check(initial.trace.result == false && !initial.trace.durations.front().observed_on &&
        initial.trace.selections.front().members.front().binding.id == "lease.initial" &&
        initial.trace.clauses.front().binding.scope == content::Scope::capacity_source,
        "Missing duration history is unsatisfied while actual related members and their facts remain inspectable");
    accepted(preview.prepare());
    check(preview.inspect("flow.hidden.compute_review", runtime::FlowPredicate::trigger).trace.result == false,
        "The saved first observation has zero elapsed days");
    preview.reload();
    const auto due = preview.submit(core::AdvanceDay{}); accepted(due);
    check(due.trace.durations.front().elapsed_days == 1 && due.trace.selections.front().members.front().result == true &&
        due.effects.size() == 1, "Author duration and selector traces use the same saved morning samples as execution");
}
} // namespace

void check_flow_probe() {
    ordered_impacts_and_boundaries();
    nested_wait_choice_and_random_reload();
    natural_lifecycle_and_isolated_cleanup();
    duration_and_member_inspection();
    std::cout << checks << " copied-fixture probe checks passed\n";
}
