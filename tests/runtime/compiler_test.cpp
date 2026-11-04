#include <pgp/runtime/compiler.hpp>
#include <pgp/core/random.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace pgp;
using namespace pgp::content;
using namespace pgp::runtime;
namespace {
int checks = 0;
void require(bool condition, std::string_view message) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string(message));
}
template <class Action> void rejects(Action action, std::string_view code) {
    ++checks;
    try { action(); }
    catch (const CompileError& error) {
        if (error.code() == code) return;
        throw std::runtime_error("Expected " + std::string(code) + "; received " + error.what());
    }
    throw std::runtime_error("Expected compilation/inspection rejection: " + std::string(code));
}
FlowDefinition& flow(Collection& collection, std::string_view name) {
    return std::get<FlowDefinition>(collection.definitions.at(std::string(name)).data);
}
Condition cash_condition(Comparison comparison = Comparison::greater_equal, std::int64_t value = 1000) {
    return {ConditionKind::clause, FactClause{"company.cash_cents", comparison, value, Unit::constant_2014_cents}, {}};
}
Condition boolean_condition(bool value = true) {
    return {ConditionKind::clause, FactClause{"company.is_operating", Comparison::equal, value, Unit::boolean}, {}};
}
Collection chain(const Collection& original, unsigned frames) {
    auto result = original;
    for (unsigned frame = 0; frame <= frames; ++frame) {
        const auto name = "flow.compiler_frame_" + std::to_string(frame);
        auto payload = flow(result, "flow.hidden.compute_review");
        payload.lifecycle = Lifecycle{};
        payload.lifecycle.occurrence_scope = Scope::company;
        payload.nodes.clear();
        payload.entry_node = frame == frames ? "done" : "call";
        payload.nodes.emplace("done", Node{.kind = NodeKind::end});
        if (frame != frames) {
            Node call{};
            call.kind = NodeKind::subflow; call.next = "done";
            call.subflow_id = "flow.compiler_frame_" + std::to_string(frame + 1);
            payload.nodes.emplace("call", std::move(call));
        }
        result.definitions.emplace(name, Definition{name, 1, std::move(payload)});
    }
    return result;
}
void compilation(const Collection& fixture, const Registry& registry, const std::string& source) {
    const auto program = compile_collection(fixture, registry);
    require(program.collection.definitions.size() == 11, "All eleven normalized definitions compile");
    require(std::any_of(program.lints.begin(), program.lints.end(), [](const auto& lint) {
        return lint.code == "waiting_sibling" && lint.path.ends_with("outer_sequence/children/0");
    }), "Outer sibling wait is explicitly linted");
    require(std::any_of(program.lints.begin(), program.lints.end(), [](const auto& lint) {
        return lint.code == "waiting_sibling" && lint.path.ends_with("inner_sequence/children/0");
    }), "Nested sibling wait is explicitly linted");

    auto reordered = nlohmann::json::parse(source);
    std::reverse(reordered["definitions"].begin(), reordered["definitions"].end());
    require(compile_collection(load_collection(reordered.dump(4), registry), registry).canonical_content_bytes == program.canonical_content_bytes,
            "Whitespace and definition-array order do not change canonical semantics");
    require(compile_collection(load_collection(program.canonical_content_bytes, registry), registry).canonical_content_bytes == program.canonical_content_bytes,
            "Canonical content is stable through its own normalized reader");
    auto changed = fixture;
    auto& children = flow(changed, "flow.hidden.sequence_resume").nodes.at("outer_sequence").children;
    std::reverse(children.begin(), children.end());
    require(compile_collection(changed, registry).canonical_content_bytes != program.canonical_content_bytes,
            "Ordered child changes affect content identity");
    changed = fixture;
    ++flow(changed, "flow.hidden.sequence_resume").nodes.at("wait").delay_days;
    require(compile_collection(changed, registry).canonical_content_bytes != program.canonical_content_bytes,
            "Delay changes affect content identity");
    changed = fixture;
    flow(changed, "flow.news.first_public_breakthrough").nodes.at("news").text_key = "compiler.changed_text";
    require(compile_collection(changed, registry).canonical_content_bytes != program.canonical_content_bytes,
            "Narrative key changes affect content identity");
    changed = fixture;
    auto& effects = flow(changed, "flow.hidden.compute_review").nodes.at("record_review").effects;
    effects.push_back(effects.front()); effects.back().arguments["value"] = std::string{"compiler_second"};
    const auto first_order = compile_collection(changed, registry).canonical_content_bytes;
    std::reverse(effects.begin(), effects.end());
    require(compile_collection(changed, registry).canonical_content_bytes != first_order,
            "Effect order is part of canonical semantics");

    changed = fixture; flow(changed, "flow.hidden.compute_review").nodes.at("record_review").next = "missing";
    rejects([&] { (void)compile_collection(changed, registry); }, "dangling_reference");
    changed = fixture; flow(changed, "flow.hidden.compute_review").nodes.at("record_review").next = "record_review";
    rejects([&] { (void)compile_collection(changed, registry); }, "immediate_cycle");
    changed = fixture; flow(changed, "flow.hidden.compute_review").nodes.at("end").effects.push_back(
        flow(changed, "flow.hidden.compute_review").nodes.at("record_review").effects.front());
    rejects([&] { (void)compile_collection(changed, registry); }, "typed_shape");
    changed = fixture; flow(changed, "flow.hidden.compute_review").nodes.at("record_review").effects.front().target_scope = Scope::world;
    rejects([&] { (void)compile_collection(changed, registry); }, "typed_shape");
    changed = fixture; flow(changed, "flow.hidden.compute_review").trigger = cash_condition();
    flow(changed, "flow.hidden.compute_review").trigger.clause->value = true;
    rejects([&] { (void)compile_collection(changed, registry); }, "type");
    changed = fixture; changed.definitions.at("flow.hidden.compute_review").id = "flow.wrong_key";
    rejects([&] { (void)compile_collection(changed, registry); }, "typed_shape");
    changed = fixture; flow(changed, "flow.hidden.compute_review").lifecycle.anchor = Anchor{AnchorKind::campaign_start, {}, {IntervalUnit::days, 0}};
    rejects([&] { (void)compile_collection(changed, registry); }, "typed_shape");
    changed = fixture;
    flow(changed, "flow.hidden.sequence_resume").nodes.at("record_inner").next = "record_outer";
    flow(changed, "flow.hidden.sequence_resume").nodes.erase("inner_branch_return");
    rejects([&] { (void)compile_collection(changed, registry); }, "cross_sibling");
    changed = fixture;
    auto& boundary = flow(changed, "flow.hidden.sequence_resume");
    boundary.nodes.at("outer_sequence").next = "record_outer";
    boundary.nodes.erase("complete");
    rejects([&] { (void)compile_collection(changed, registry); }, "sequence_boundary");
    const std::vector<std::string> operations{"record_decision", "open_negotiation", "register_owned_modifier", "remove_owned_modifier", "adjust_cash"};
    require(compile_collection(fixture, registry, operations).collection.definitions.size() == 11,
            "An explicit complete executor vocabulary compiles");
    rejects([&] { (void)compile_collection(fixture, registry, std::vector<std::string>{"record_decision"}); }, "provider");
    rejects([&] { (void)compile_collection(fixture, registry, std::vector<std::string>{"missing.operation"}); }, "provider");
    require(compile_collection(chain(fixture, 8), registry).collection.definitions.size() == 20,
            "Eight nested subflow frames are accepted");
    rejects([&] { (void)compile_collection(chain(fixture, 9), registry); }, "budget");
}
void inspection(Registry registry) {
    const ScopeBinding company{Scope::company, "company.player"};
    int reads = 0;
    core::RandomStreams random{42};
    const auto before = random.snapshot();
    FactReader read = [&](const FactMetadata& metadata, const ScopeBinding& binding) -> Scalar {
        ++reads;
        require(binding == company, "Reader receives the exact matched scope");
        if (metadata.type == ValueType::boolean) return true;
        return std::int64_t{1000};
    };
    const auto equal = evaluate_condition(cash_condition(), company, registry, read);
    require(equal.result == true && equal.clauses.size() == 1 && equal.clauses[0].actual == Scalar{std::int64_t{1000}} &&
                equal.clauses[0].expected == Scalar{std::int64_t{1000}} && equal.clauses[0].path == "trigger",
            "Threshold equality exposes ordered actual/expected truth trace");
    require(evaluate_condition(cash_condition(Comparison::greater_equal, 1001), company, registry, read).result == false,
            "Threshold direction is not inverted");
    Condition nested{ConditionKind::all_of, {}, {cash_condition(), {ConditionKind::not_condition, {}, {boolean_condition(false)}}}};
    const auto traced = evaluate_condition(nested, company, registry, read);
    require(traced.result == true && traced.clauses[0].path == "trigger/all_of/0" &&
                traced.clauses[1].path == "trigger/all_of/1/not", "Nested clause paths and negation are inspectable");
    reads = 0;
    const auto all = evaluate_condition({ConditionKind::all_of, {}, {cash_condition(Comparison::greater, 1000), boolean_condition()}}, company, registry, read);
    require(all.result == false && reads == 2 && all.clauses.size() == 2, "Inspector does not hide later clauses behind short circuiting");
    require(random.snapshot() == before, "Trigger inspection leaves independent simulation streams untouched");
    rejects([&] { (void)evaluate_condition(cash_condition(), {Scope::world, "world.scenario"}, registry, read); }, "scope");
    auto wrong = cash_condition(); wrong.clause->unit = Unit::basis_points;
    rejects([&] { (void)evaluate_condition(wrong, company, registry, read); }, "unit");
    wrong = cash_condition(); wrong.clause->value = true;
    rejects([&] { (void)evaluate_condition(wrong, company, registry, read); }, "type");
    rejects([&] { (void)evaluate_condition(cash_condition(), company, registry,
        [](const FactMetadata&, const ScopeBinding&) -> Scalar { return true; }); }, "type");
    auto stress = Condition{ConditionKind::clause,
        FactClause{"company.workload_stress_basis_points", Comparison::greater, std::int64_t{500}, Unit::basis_points}, {}};
    rejects([&] { (void)evaluate_condition(stress, company, registry,
        [](const FactMetadata&, const ScopeBinding&) -> Scalar { return std::int64_t{10001}; }); }, "range");
    rejects([&] { (void)evaluate_condition({ConditionKind::all_of, {}, {}}, company, registry, read); }, "typed_shape");
    auto boolean_order = boolean_condition(); boolean_order.clause->comparison = Comparison::greater;
    rejects([&] { (void)evaluate_condition(boolean_order, company, registry, read); }, "type");

    auto hidden = *registry.fact("company.cash_cents"); hidden.id = "test.hidden_cash";
    hidden.visibility = Visibility::hidden_truth; registry.register_fact(hidden);
    auto hidden_condition = cash_condition(); hidden_condition.clause->fact = hidden.id;
    reads = 0;
    const auto redacted = evaluate_condition(hidden_condition, company, registry, read, InspectionMode::player_known);
    require(!redacted.result.has_value() && reads == 0 && redacted.clauses[0].redacted &&
                !redacted.clauses[0].actual && !redacted.clauses[0].expected && !redacted.clauses[0].result,
            "Player inspection neither reads nor reveals hidden values or truth");
    const auto hidden_not = evaluate_condition({ConditionKind::not_condition, {}, {hidden_condition}}, company, registry, read, InspectionMode::player_known);
    require(!hidden_not.result, "Negation cannot reveal unknown hidden truth");
    require(evaluate_condition({ConditionKind::all_of, {}, {hidden_condition, boolean_condition(false)}}, company, registry, read,
                               InspectionMode::player_known).result == false,
            "A publicly false all-condition can be explained without exposing a hidden clause");
    require(evaluate_condition({ConditionKind::any_of, {}, {hidden_condition, boolean_condition()}}, company, registry, read,
                               InspectionMode::player_known).result == true,
            "A publicly true any-condition can be explained without exposing hidden truth");
    reads = 0;
    require(evaluate_condition(hidden_condition, company, registry, read).result == true && reads == 1,
            "Author-fixture inspection deliberately uses full copied-state knowledge");
}
core::OrdinalDay date(int year, unsigned month, unsigned day) { return core::to_ordinal({year, month, day}); }
void calendars() {
    const auto january = date(2014, 1, 31);
    const auto anchor = resolve_anchor({AnchorKind::fixed_date, january, {IntervalUnit::calendar_months, 1}}, date(2014, 1, 1), date(2014, 1, 2));
    require(anchor.first_due == date(2014, 2, 28) && anchor.calendar_day == 31, "First-due offset clamps but retains preferred day");
    require(interval_due(anchor, {IntervalUnit::calendar_months, 1}, 1) == date(2014, 3, 31) &&
                interval_due(anchor, {IntervalUnit::calendar_months, 1}, 2) == date(2014, 4, 30),
            "Monthly recurrence derives from anchor rather than drifting after February");
    const auto leap = resolve_anchor({AnchorKind::fixed_date, date(2000, 1, 31), {IntervalUnit::calendar_months, 1}}, january, january);
    const auto century = resolve_anchor({AnchorKind::fixed_date, date(2100, 1, 31), {IntervalUnit::calendar_months, 1}}, january, january);
    require(leap.first_due == date(2000, 2, 29) && century.first_due == date(2100, 2, 28), "Gregorian century/leap clamping");
    const auto first_boundary = resolve_anchor({AnchorKind::month_start, {}, {IntervalUnit::days, 0}}, january, date(2014, 6, 1));
    const auto next_boundary = resolve_anchor({AnchorKind::month_start, {}, {IntervalUnit::calendar_months, 1}}, january, date(2014, 6, 15));
    require(first_boundary.first_due == date(2014, 6, 1) && next_boundary.first_due == date(2014, 8, 1),
            "Month-start is first boundary on/after scope creation, then its offset");
    const auto campaign = resolve_anchor({AnchorKind::campaign_start, {}, {IntervalUnit::days, 7}}, date(2014, 1, 1), january);
    const auto created = resolve_anchor({AnchorKind::scope_created, {}, {IntervalUnit::days, 1}}, date(2014, 1, 1), january);
    require(campaign.first_due == date(2014, 1, 8) && created.first_due == date(2014, 2, 1), "Relative campaign/scope anchors remain distinct");
    require(interval_due(anchor, {IntervalUnit::calendar_months, 1}, 0) == anchor.first_due, "Index zero is the explicit first due date");
    require(interval_due({january, 31}, {IntervalUnit::days, 1}, 31) == date(2014, 3, 3), "Day recurrence uses integer elapsed days");
    rejects([&] { (void)interval_due(anchor, {IntervalUnit::days, 0}, 1); }, "interval");
    rejects([&] { (void)interval_due({january, 0}, {IntervalUnit::days, 1}, 1); }, "interval");
    rejects([&] { (void)interval_due({date(2014, 3, 1), 31}, {IntervalUnit::days, 1}, 1); }, "interval");
    rejects([&] { (void)interval_due(anchor, {IntervalUnit::days, 1000}, std::numeric_limits<std::uint64_t>::max()); }, "calendar_range");
    rejects([&] { (void)resolve_anchor({AnchorKind::fixed_date, {}, {IntervalUnit::days, 0}}, january, january); }, "typed_shape");
    rejects([&] { (void)resolve_anchor({AnchorKind::month_start, {}, {IntervalUnit::days, 0}}, january, date(9999, 12, 31)); }, "calendar_range");
}
} // namespace
int main() {
    try {
        std::ifstream input(PGP_RUNTIME_FIXTURE_PATH);
        if (!input) throw std::runtime_error("Cannot open runtime compiler fixture");
        const std::string source{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        const auto registry = foundation_registry();
        compilation(load_collection(source, registry), registry, source);
        inspection(registry); calendars();
        std::cout << checks << " compiler, trace, canonical-identity and calendar checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
