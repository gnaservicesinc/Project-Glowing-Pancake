#include "duration.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace pgp::runtime::detail {
namespace {
using Key = std::tuple<std::string, content::Scope, std::string, std::string>;
Key key(const DurationState& value) {
    return {value.binding.id, value.binding.scope, value.definition_id, value.predicate_id};
}
Key key(const LifecycleState& value, std::string_view predicate_id) {
    return {value.binding.id, value.binding.scope, value.definition_id, std::string(predicate_id)};
}
const content::FlowDefinition& flow(const CompiledProgram& program, std::string_view id) {
    const auto* definition = program.collection.find(id);
    if (!definition || !std::holds_alternative<content::FlowDefinition>(definition->data))
        throw std::invalid_argument("Duration state references a missing flow");
    return std::get<content::FlowDefinition>(definition->data);
}
void charge(std::size_t& steps, std::size_t amount = 1) {
    if (amount > maximum_steps_per_operation || steps > maximum_steps_per_operation - amount)
        throw DurationBudgetExceeded("The deterministic duration observation budget was exceeded.");
    steps += amount;
}
void sample_tree(const content::Condition& condition, const LifecycleState& lifecycle,
                 Checkpoint& checkpoint, const CompiledProgram& program,
                 std::map<Key, std::size_t>& index, std::size_t& steps) {
    // Postorder gives nested predicates the current day's inner sample before
    // evaluating an outer timer. No boolean short-circuit can suppress resets.
    for (const auto& child : condition.children)
        sample_tree(child, lifecycle, checkpoint, program, index, steps);
    if (condition.kind != content::ConditionKind::holds_for_days) return;
    const auto identity = key(lifecycle, condition.predicate_id);
    const auto found = index.find(identity);
    auto& states = checkpoint.runtime.duration_states;
    const auto today = checkpoint.world.simulation.date;
    if (found != index.end() && states[found->second].observed_on == today) return;
    charge(steps);
    const auto facts = [&](const auto& metadata, const auto& binding) {
        return read_fact(checkpoint.world, metadata, binding);
    };
    const auto relations = [&](const auto& metadata, const auto& owner) {
        return read_relation(checkpoint.world, metadata, owner);
    };
    const DurationReader durations = [&](std::string_view predicate_id, const ScopeBinding& binding)
        -> std::optional<DurationObservation> {
        if (binding != lifecycle.binding) throw std::invalid_argument("Duration query changed scope");
        const auto sample = index.find(key(lifecycle, predicate_id));
        if (sample == index.end()) return std::nullopt;
        const auto& state = states[sample->second];
        return DurationObservation{state.observed_on, state.true_since};
    };
    const auto inspected = evaluate_condition(condition.children.front(), lifecycle.binding,
        program.registry, facts, InspectionMode::author_fixture, "duration_sample", durations, today, relations);
    charge(steps, inspected.selections.empty() ? inspected.clauses.size() + inspected.durations.size() : inspected.inspected_nodes);
    if (!inspected.result) throw std::invalid_argument("A daily duration sample lacks its nested observation");
    if (found == index.end()) {
        if (states.size() >= maximum_records)
            throw DurationBudgetExceeded("The saved duration state budget was exceeded.");
        index.emplace(identity, states.size());
        states.push_back({lifecycle.definition_id, program.collection.find(lifecycle.definition_id)->revision,
            condition.predicate_id, lifecycle.binding, today,
            *inspected.result ? std::optional<core::OrdinalDay>{today} : std::nullopt});
    } else {
        auto& state = states[found->second];
        const bool consecutive = today.value() - state.observed_on.value() == 1;
        if (!*inspected.result) state.true_since.reset();
        else if (!consecutive || !state.true_since) state.true_since = today;
        state.observed_on = today;
    }
}
void collect(const content::Condition& condition, const LifecycleState& lifecycle, std::set<Key>& expected) {
    if (condition.kind == content::ConditionKind::holds_for_days)
        expected.insert(key(lifecycle, condition.predicate_id));
    for (const auto& child : condition.children) collect(child, lifecycle, expected);
}
} // namespace

DurationReader saved_duration_reader(const RuntimeState& state, std::string_view definition_id) {
    return [&state, id = std::string(definition_id)](std::string_view predicate_id, const ScopeBinding& binding)
        -> std::optional<DurationObservation> {
        const auto found = std::find_if(state.duration_states.begin(), state.duration_states.end(), [&](const auto& value) {
            return value.definition_id == id && value.predicate_id == predicate_id && value.binding == binding;
        });
        if (found == state.duration_states.end()) return std::nullopt;
        return DurationObservation{found->observed_on, found->true_since};
    };
}
void observe_durations(Checkpoint& checkpoint, const CompiledProgram& program, std::size_t& steps) {
    if (checkpoint.runtime.duration_sampled_day == checkpoint.world.simulation.date) return;
    std::map<Key, std::size_t> index;
    auto& states = checkpoint.runtime.duration_states;
    for (std::size_t i = 0; i < states.size(); ++i) index.emplace(key(states[i]), i);
    // The world is unchanged throughout this prepass. Exit timers measure the
    // scope condition's history, including mornings before activation/reentry.
    for (const auto& lifecycle : checkpoint.runtime.lifecycles) {
        const auto& definition = flow(program, lifecycle.definition_id);
        sample_tree(definition.trigger, lifecycle, checkpoint, program, index, steps);
        if (definition.lifecycle.exit_when)
            sample_tree(*definition.lifecycle.exit_when, lifecycle, checkpoint, program, index, steps);
    }
    std::sort(states.begin(), states.end(), [](const auto& left, const auto& right) { return key(left) < key(right); });
    if (!states.empty()) checkpoint.runtime.duration_sampled_day = checkpoint.world.simulation.date;
}
void validate_duration_states(const Checkpoint& checkpoint, const CompiledProgram& program) {
    const auto& states = checkpoint.runtime.duration_states;
    if (states.size() > maximum_records) throw std::invalid_argument("Duration state count budget exceeded");
    std::set<Key> expected;
    std::map<std::pair<std::string, ScopeBinding>, const LifecycleState*> owners;
    for (const auto& lifecycle : checkpoint.runtime.lifecycles)
        owners.emplace(std::pair{lifecycle.definition_id, lifecycle.binding}, &lifecycle);
    std::set<std::pair<std::string, ScopeBinding>> timed_roots;
    for (const auto& [id, entry] : program.collection.definitions) {
        const auto* definition = std::get_if<content::FlowDefinition>(&entry.data);
        if (!definition || (checkpoint.world.enabled_flows &&
            std::find(checkpoint.world.enabled_flows->begin(), checkpoint.world.enabled_flows->end(), id) ==
                checkpoint.world.enabled_flows->end())) continue;
        LifecycleState lifecycle;
        lifecycle.definition_id = id;
        lifecycle.binding = {definition->scope, definition->scope == content::Scope::company ? checkpoint.world.company_id :
            definition->scope == content::Scope::world ? checkpoint.world.world_id : checkpoint.world.state_id};
        const auto count = expected.size();
        collect(definition->trigger, lifecycle, expected);
        if (definition->lifecycle.exit_when) collect(*definition->lifecycle.exit_when, lifecycle, expected);
        if (expected.size() != count) timed_roots.emplace(id, lifecycle.binding);
    }
    const auto sampled_day = checkpoint.runtime.duration_sampled_day;
    const bool waiting = std::any_of(checkpoint.runtime.activations.begin(), checkpoint.runtime.activations.end(), [](const auto& active) {
        return active.status == ActivationStatus::awaiting_choice;
    });
    if (sampled_day.has_value() != !states.empty())
        throw std::invalid_argument("Duration histories and their daily observation cursor must be retained together");
    if (!sampled_day) {
        const bool has_owner = std::any_of(timed_roots.begin(), timed_roots.end(), [&](const auto& identity) { return owners.contains(identity); });
        if (!expected.empty() && (has_owner || checkpoint.runtime.prepared_day || waiting))
            throw std::invalid_argument("Observed duration roots cannot lose their daily histories");
        return;
    }
    (void)core::from_ordinal(*sampled_day);
    if (*sampled_day < checkpoint.world.campaign_start || *sampled_day > checkpoint.world.simulation.date ||
        ((checkpoint.runtime.prepared_day || waiting || checkpoint.world.simulation.phase == core::DayPhase::work_ready) &&
            *sampled_day != checkpoint.world.simulation.date))
        throw std::invalid_argument("Duration observation cursor differs from its prepared or suspended morning");
    for (const auto& identity : timed_roots)
        if (!owners.contains(identity)) throw std::invalid_argument("Observed duration root lost its lifecycle state");
    if (expected.size() != states.size())
        throw std::invalid_argument("Duration state must retain every observed root predicate");
    std::optional<Key> previous;
    for (const auto& state : states) {
        const auto identity = key(state);
        if ((previous && identity <= *previous) || !expected.contains(identity))
            throw std::invalid_argument("Duration states must be unique, canonical and owned by enabled root predicates");
        previous = identity;
        const auto* definition = program.collection.find(state.definition_id);
        if (!definition || state.definition_revision != definition->revision)
            throw std::invalid_argument("Duration state definition revision mismatch");
        (void)core::from_ordinal(state.observed_on);
        if (state.true_since) (void)core::from_ordinal(*state.true_since);
        const auto earliest = state.binding.scope == content::Scope::world ?
            checkpoint.world.campaign_start : checkpoint.world.scope_created;
        const auto& owner = *owners.at({state.definition_id, state.binding});
        if (state.observed_on < earliest || state.observed_on > checkpoint.world.simulation.date ||
            (state.true_since && (*state.true_since < earliest || *state.true_since > state.observed_on)) ||
            state.observed_on != *sampled_day ||
            (owner.last_checked && *owner.last_checked > state.observed_on) ||
            (checkpoint.runtime.prepared_day && *checkpoint.runtime.prepared_day != state.observed_on))
            throw std::invalid_argument("Duration start/sample dates differ from their daily boundary");
    }
}
} // namespace pgp::runtime::detail
