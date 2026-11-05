#include <pgp/studio/flow_probe.hpp>

#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>

#include <algorithm>
#include <array>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace pgp::studio {
namespace {
runtime::Checkpoint start_fixture(const runtime::CompiledProgram& program, const ProbeFixture& fixture) {
    auto state = runtime::begin_scenario(program, fixture.scenario_id, fixture.headquarters);
    if (!fixture.isolated_flow.empty()) {
        const auto* definition = program.collection.find(fixture.isolated_flow);
        if (!definition || !std::holds_alternative<content::FlowDefinition>(definition->data))
            throw std::invalid_argument("Select a flow from this collection to isolate.");
        state.world.enabled_flows = std::vector{fixture.isolated_flow};
    }
    if (fixture.seed) {
        state.world.root_seed = *fixture.seed;
        state.world.simulation.random = core::RandomStreams{*fixture.seed}.snapshot();
        if (state.world.simulation.lab) state.world.simulation.lab->research_stream_origin=
            state.world.simulation.random[static_cast<std::size_t>(core::RandomStream::research)];
    }
    return state;
}

template<class Record> std::vector<Record> since(const std::vector<Record>& records, std::uint64_t next_id) {
    const auto first = std::lower_bound(records.begin(), records.end(), next_id,
        [](const auto& row, auto id) { return row.id < id; });
    return {first, records.end()};
}

template<class Operation> ProbeStep run(runtime::Runtime& engine, Operation operation) {
    const auto& before = engine.current_state();
    const auto date = before.world.simulation.date;
    const auto cash = before.world.simulation.cash;
    const auto next_effect = before.runtime.next_receipt_id;
    const auto& accounts = before.world.simulation.accounts;
    const auto next_posting = accounts ? accounts->next_posting_id : 1;
    auto trace = operation(before.world.simulation.revision);
    const auto& after = engine.current_state();
    const auto& journal = after.world.simulation.accounts;
    return {std::move(trace), date, after.world.simulation.date, cash, after.world.simulation.cash,
            since(after.runtime.receipts, next_effect),
            journal ? since(journal->postings, next_posting) : std::vector<core::Posting>{},
            after.runtime.modifiers};
}
} // namespace

FlowProbe::FlowProbe(const runtime::CompiledProgram& program, runtime::Checkpoint fixture)
    : engine_{program, std::move(fixture), runtime::InspectionMode::author_fixture} {}

FlowProbe::FlowProbe(const runtime::CompiledProgram& program, const ProbeFixture& fixture)
    : FlowProbe(program, start_fixture(program, fixture)) {}

runtime::FlowInspection FlowProbe::inspect(std::string_view flow_id, runtime::FlowPredicate predicate) const {
    return runtime::inspect_flow_predicate(engine_.program(), state(), flow_id, predicate);
}

std::vector<BoundarySample> FlowProbe::boundaries(std::string_view flow_id) const {
    const auto inspected = inspect(flow_id, runtime::FlowPredicate::trigger);
    const auto& flow = std::get<content::FlowDefinition>(engine_.program().collection.find(flow_id)->data);
    std::vector<const content::FactClause*> literals;
    const auto collect = [&](const auto& self, const content::Condition& condition) -> void {
        if (condition.clause && !condition.clause->expression &&
            std::holds_alternative<std::int64_t>(condition.clause->value)) literals.push_back(&*condition.clause);
        for (const auto& child : condition.children) self(self, child);
    };
    collect(collect, flow.trigger);
    std::vector<BoundarySample> samples;
    for (const auto& observed : inspected.trace.clauses) {
        const auto source = std::find_if(literals.begin(), literals.end(), [&](const auto* clause) {
            return clause->fact == observed.fact && clause->comparison == observed.comparison && observed.expected == clause->value;
        });
        if (source == literals.end()) continue;
        const auto& clause = **source;
        const auto threshold = std::get<std::int64_t>(clause.value);
        const auto* metadata = engine_.program().registry.fact(clause.fact);
        const std::array labels{"below", "equal", "above", "zero", "missing target"};
        for (std::size_t index = 0; index < labels.size(); ++index) {
            BoundarySample sample{observed.path, observed.binding, observed.fact, observed.unit, labels[index], {}, {}, {}};
            try {
                if (index < 4) {
                    sample.value = index == 3 ? 0 : core::checked_add(threshold, static_cast<std::int64_t>(index) - 1);
                    if (*sample.value < metadata->minimum || *sample.value > metadata->maximum)
                        throw std::invalid_argument("The candidate is outside the fact's registered range.");
                } else sample.binding.id += ".absent";
                const runtime::FactReader reader = [&](const auto& field, const auto& binding) {
                    if (sample.value && field.id == observed.fact && binding == observed.binding)
                        return content::Scalar{*sample.value};
                    return runtime::read_fact(state().world, field, binding);
                };
                const content::Condition condition{content::ConditionKind::clause, clause, {}};
                sample.result = runtime::evaluate_condition(condition, sample.binding, engine_.program().registry,
                    reader, runtime::InspectionMode::author_fixture, observed.path).result;
            } catch (const std::exception& error) {
                sample.error = error.what();
            }
            samples.push_back(std::move(sample));
        }
    }
    return samples;
}

ProbeStep FlowProbe::prepare() {
    return run(engine_, [&](auto revision) { return engine_.prepare_opening(revision); });
}

ProbeStep FlowProbe::submit(core::CommandAction action) {
    return run(engine_, [&](auto revision) { return engine_.submit({revision, std::move(action)}); });
}

ProbeStep FlowProbe::choose(std::uint64_t presentation_id, std::string_view choice_id) {
    return run(engine_, [&](auto revision) { return engine_.choose(revision, presentation_id, choice_id); });
}

ProbeStep FlowProbe::synthetic_cleanup(std::uint64_t activation_id) const {
    auto isolated = engine_;
    return run(isolated, [&](auto revision) { return isolated.synthetic_cleanup(revision, activation_id); });
}

void FlowProbe::reload() {
    const auto& program = engine_.program();
    const auto bytes = persistence::encode_save(program, state());
    auto restored = persistence::decode_save(program, bytes);
    if (restored.checkpoint != state()) throw std::runtime_error("The isolated checkpoint changed after reload.");
    engine_ = runtime::Runtime{program, std::move(restored.checkpoint), runtime::InspectionMode::author_fixture};
}
} // namespace pgp::studio
