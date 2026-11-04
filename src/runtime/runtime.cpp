#include <pgp/runtime/runtime.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace pgp::runtime {
namespace {
struct Failure { RuntimeReason reason; std::string detail; };
[[noreturn]] void fail(RuntimeReason reason, std::string detail) { throw Failure{reason, std::move(detail)}; }
std::uint64_t identifier(std::uint64_t& next) {
    if (next == 0 || next == std::numeric_limits<std::uint64_t>::max())
        fail(RuntimeReason::budget_exceeded, "The stable runtime identifier range is exhausted.");
    return next++;
}
void record_budget(std::size_t count) {
    if (count >= maximum_records) fail(RuntimeReason::budget_exceeded, "The runtime record budget is full; no cause or choice was discarded.");
}
const content::FlowDefinition& flow(const CompiledProgram& program, std::string_view id) {
    const auto* definition = program.collection.find(id);
    if (!definition || !std::holds_alternative<content::FlowDefinition>(definition->data))
        fail(RuntimeReason::missing_definition, "A required flow definition is unavailable.");
    return std::get<content::FlowDefinition>(definition->data);
}
const content::Node& node(const CompiledProgram& program, const Activation& activation) {
    const auto& definition = flow(program, activation.current_flow);
    const auto found = definition.nodes.find(activation.current_node);
    if (found == definition.nodes.end()) fail(RuntimeReason::invalid_state, "A saved flow node no longer exists.");
    return found->second;
}
ScopeBinding binding_for(const WorldState& world, content::Scope scope) {
    return {scope, scope == content::Scope::company ? world.company_id :
                   scope == content::Scope::world ? world.world_id : world.state_id};
}
LifecycleState& lifecycle(RuntimeState& state, const Activation& activation) {
    const auto found = std::find_if(state.lifecycles.begin(), state.lifecycles.end(), [&](const auto& entry) {
        return entry.definition_id == activation.definition_id && entry.binding == activation.binding;
    });
    if (found == state.lifecycles.end()) fail(RuntimeReason::invalid_state, "Activation has no lifecycle state.");
    return *found;
}
Activation& activation_by_id(RuntimeState& state, std::uint64_t id) {
    const auto found = std::find_if(state.activations.begin(), state.activations.end(), [id](const auto& entry) { return entry.id == id; });
    if (found == state.activations.end()) fail(RuntimeReason::invalid_state, "A required activation no longer exists.");
    return *found;
}
bool has_choice(const RuntimeState& state) {
    return std::any_of(state.activations.begin(), state.activations.end(), [](const auto& activation) {
        return activation.status == ActivationStatus::awaiting_choice;
    });
}
void append_trace(RuntimeResult& result, const Activation& activation, std::string detail, bool synthetic = false) {
    if (result.paths.size() >= maximum_steps_per_operation)
        fail(RuntimeReason::budget_exceeded, "The ordered execution trace budget was exceeded.");
    result.paths.push_back({activation.id, activation.current_flow, activation.current_node,
        "flows/" + activation.current_flow + "/nodes/" + activation.current_node,
        activation.binding, std::move(detail), synthetic});
}
void add_receipt(Checkpoint& checkpoint, const Activation& activation, std::size_t effect_index,
                 std::string operation, content::Scalar before, content::Scalar after,
                 core::Cents counterparty_delta = core::Cents{0}) {
    auto& state = checkpoint.runtime;
    record_budget(state.receipts.size());
    state.receipts.push_back({identifier(state.next_receipt_id), activation.id,
        checkpoint.world.simulation.revision, checkpoint.world.simulation.date,
        activation.current_flow, activation.current_node, effect_index, std::move(operation),
        activation.binding, std::move(before), std::move(after), counterparty_delta});
}
void remove_owned(Checkpoint& checkpoint, const Activation& activation) {
    auto& modifiers = checkpoint.runtime.modifiers;
    for (std::size_t i = modifiers.size(); i != 0; --i) {
        const auto index = i - 1;
        if (modifiers[index].activation_id == activation.id) {
            add_receipt(checkpoint, activation, 0, "lifecycle_cleanup", modifiers[index].add_basis_points, std::int64_t{0});
            modifiers.erase(modifiers.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
}
void complete(Checkpoint& checkpoint, Activation& activation, const CompiledProgram& program) {
    activation.frames.clear(); activation.due.reset(); activation.wait_started.reset(); activation.presentation_id.reset();
    if (flow(program, activation.definition_id).lifecycle.mode == content::LifecycleMode::continuous && !activation.exiting) {
        activation.status = ActivationStatus::continuous_active;
    } else {
        remove_owned(checkpoint, activation);
        activation.status = ActivationStatus::completed;
        lifecycle(checkpoint.runtime, activation).active_activation.reset();
    }
}
void apply_effect(Checkpoint& checkpoint, Activation& activation, const content::Effect& effect,
                  std::size_t index, const CompiledProgram& program) {
    if (effect.target != "self" || effect.target_scope != activation.binding.scope ||
        activation.binding.scope != content::Scope::company)
        fail(RuntimeReason::effect_rejected, "The foundation operation requires its explicit company self target.");
    auto& world = checkpoint.world;
    auto& state = checkpoint.runtime;
    if (effect.operation == "adjust_cash") {
        const auto amount = std::get<std::int64_t>(effect.arguments.at("amount_cents"));
        const auto before = world.simulation.cash.value();
        const auto after = core::checked_add(before, amount);
        const auto pool = core::checked_subtract(world.counterparty_cash.value(), amount);
        std::int64_t reserved = 0;
        for (const auto& reservation : world.simulation.reservations)
            reserved = core::checked_add(reserved, reservation.reserved.value());
        if (after < reserved || pool < 0)
            fail(RuntimeReason::effect_rejected, "A cash transfer lacks unreserved funds or an authorized counterparty balance.");
        world.simulation.cash = core::Cents{after};
        world.counterparty_cash = core::Cents{pool};
        add_receipt(checkpoint, activation, index, effect.operation, before, after,
                    core::Cents{core::checked_subtract(0, amount)});
    } else if (effect.operation == "record_decision") {
        record_budget(world.decisions.size());
        const auto& marker = std::get<std::string>(effect.arguments.at("value"));
        const auto before = static_cast<std::int64_t>(world.decisions.size());
        world.decisions.push_back(marker);
        add_receipt(checkpoint, activation, index, effect.operation, before, marker);
    } else if (effect.operation == "open_negotiation") {
        record_budget(world.negotiations.size());
        const auto& id = std::get<std::string>(effect.arguments.at("template_id"));
        const auto* definition = program.collection.find(id);
        if (!definition || !std::holds_alternative<content::DealDefinition>(definition->data))
            fail(RuntimeReason::missing_definition, "The negotiation template is unavailable.");
        const auto before = static_cast<std::int64_t>(world.negotiations.size());
        world.negotiations.push_back(id);
        add_receipt(checkpoint, activation, index, effect.operation, before, id);
    } else if (effect.operation == "register_owned_modifier") {
        if (flow(program, activation.definition_id).lifecycle.mode != content::LifecycleMode::continuous)
            fail(RuntimeReason::effect_rejected, "Owned persistent modifiers require a continuous activation.");
        const auto& key = std::get<std::string>(effect.arguments.at("key"));
        const auto& field = std::get<std::string>(effect.arguments.at("field"));
        const auto delta = std::get<std::int64_t>(effect.arguments.at("add_basis_points"));
        const auto found = std::find_if(state.modifiers.begin(), state.modifiers.end(), [&](const auto& modifier) {
            return modifier.activation_id == activation.id && modifier.binding == activation.binding && modifier.key == key;
        });
        const auto before = found == state.modifiers.end() ? 0 : found->add_basis_points;
        if (found == state.modifiers.end()) {
            if (state.modifiers.size() >= maximum_activations) fail(RuntimeReason::budget_exceeded, "Owned modifier count exceeded.");
            state.modifiers.push_back({activation.id, activation.binding, key, field, delta});
        } else {
            if (found->field != field) fail(RuntimeReason::effect_rejected, "An owned modifier key cannot change fields.");
            found->add_basis_points = delta;
        }
        add_receipt(checkpoint, activation, index, effect.operation, before, delta);
    } else if (effect.operation == "remove_owned_modifier") {
        const auto& key = std::get<std::string>(effect.arguments.at("key"));
        const auto found = std::find_if(state.modifiers.begin(), state.modifiers.end(), [&](const auto& modifier) {
            return modifier.activation_id == activation.id && modifier.binding == activation.binding && modifier.key == key;
        });
        const auto before = found == state.modifiers.end() ? 0 : found->add_basis_points;
        if (found != state.modifiers.end()) state.modifiers.erase(found);
        add_receipt(checkpoint, activation, index, effect.operation, before, std::int64_t{0});
    } else fail(RuntimeReason::effect_rejected, "The registered operation has no installed runtime implementation.");
}
void present(Checkpoint& checkpoint, Activation& activation, const content::Node& entry) {
    auto& state = checkpoint.runtime;
    record_budget(state.presentations.size());
    Presentation presentation;
    presentation.id = identifier(state.next_presentation_id);
    presentation.activation_id = activation.id;
    presentation.day = checkpoint.world.simulation.date;
    presentation.flow_id = activation.current_flow;
    presentation.node_id = activation.current_node;
    presentation.title_key = entry.title_key;
    presentation.text_key = entry.text_key;
    presentation.kind = entry.kind == content::NodeKind::dialogue ? PresentationKind::dialogue :
                        entry.kind == content::NodeKind::news ? PresentationKind::news : PresentationKind::result;
    presentation.resolved = entry.kind != content::NodeKind::dialogue;
    for (const auto& choice : entry.choices) presentation.choices.push_back({choice.id, choice.label_key});
    if (entry.kind == content::NodeKind::dialogue) activation.presentation_id = presentation.id;
    state.presentations.push_back(std::move(presentation));
}
void execute(Checkpoint& checkpoint, Activation& activation, const CompiledProgram& program,
             RuntimeResult& result, std::size_t& steps) {
    while (activation.status == ActivationStatus::ready) {
        const auto& root = flow(program, activation.definition_id);
        const auto visits_limit = root.max_node_visits ? root.max_node_visits : maximum_steps_per_operation;
        if (++steps > maximum_steps_per_operation || activation.node_visits >= visits_limit)
            fail(RuntimeReason::budget_exceeded, "The deterministic flow step/visit budget was exceeded.");
        ++activation.node_visits;
        const auto& entry = node(program, activation);
        append_trace(result, activation, "Visit ordered node");
        switch (entry.kind) {
        case content::NodeKind::effect:
            for (std::size_t index = 0; index < entry.effects.size(); ++index)
                apply_effect(checkpoint, activation, entry.effects[index], index, program);
            activation.current_node = entry.next;
            break;
        case content::NodeKind::news: case content::NodeKind::result:
            present(checkpoint, activation, entry);
            activation.current_node = entry.next;
            break;
        case content::NodeKind::dialogue:
            present(checkpoint, activation, entry);
            activation.status = ActivationStatus::awaiting_choice;
            checkpoint.world.simulation.paused = true;
            break;
        case content::NodeKind::delay:
            activation.due = core::advance_days(checkpoint.world.simulation.date, core::Days{entry.delay_days});
            activation.wait_started = checkpoint.world.simulation.date;
            activation.status = ActivationStatus::delayed;
            break;
        case content::NodeKind::random: {
            record_budget(checkpoint.runtime.random_resolutions.size());
            std::uint64_t total = 0;
            std::string weights;
            for (const auto& branch : entry.branches) {
                // Compiler bounds 32 positive weights to one million each.
                total += branch.weight;
                if (!weights.empty()) weights += ",";
                weights += branch.id + "=" + std::to_string(branch.weight);
            }
            auto& saved = checkpoint.world.simulation.random[static_cast<std::size_t>(core::RandomStream::narrative)];
            const auto before = saved;
            if (!checkpoint.runtime.narrative_origin) checkpoint.runtime.narrative_origin = before;
            core::SplitMix64 stream{before};
            const auto sample = stream.bounded(total);
            auto remaining = sample;
            const content::WeightedBranch* selected = nullptr;
            for (const auto& branch : entry.branches) {
                if (remaining < branch.weight) { selected = &branch; break; }
                remaining -= branch.weight;
            }
            if (!selected) fail(RuntimeReason::invalid_state, "The weighted branch has no selected outcome.");
            saved = stream.state();
            checkpoint.runtime.random_resolutions.push_back({activation.id, activation.node_visits,
                checkpoint.world.simulation.date, activation.current_flow, activation.current_node,
                selected->id, sample, before, saved});
            append_trace(result, activation, "Resolved narrative branch " + selected->id +
                "; weights [" + weights + "]; sample " + std::to_string(sample) + "/" + std::to_string(total));
            activation.current_node = selected->next;
            break;
        }
        case content::NodeKind::sequence:
            if (activation.frames.size() >= maximum_continuation_frames)
                fail(RuntimeReason::budget_exceeded, "The continuation stack exceeds eight frames.");
            activation.frames.push_back({FrameKind::sequence, activation.current_flow, activation.current_node,
                                         1, entry.next, activation.binding});
            activation.current_node = entry.children.at(0);
            break;
        case content::NodeKind::subflow:
            if (activation.frames.size() >= maximum_continuation_frames)
                fail(RuntimeReason::budget_exceeded, "The continuation stack exceeds eight frames.");
            activation.frames.push_back({FrameKind::subflow, activation.current_flow, activation.current_node,
                                         0, entry.next, activation.binding});
            activation.current_flow = entry.subflow_id;
            activation.current_node = flow(program, entry.subflow_id).entry_node;
            break;
        case content::NodeKind::end:
            if (activation.frames.empty()) { complete(checkpoint, activation, program); break; }
            {
                auto& frame = activation.frames.back();
                activation.current_flow = frame.flow_id;
                const auto& sequence = flow(program, frame.flow_id).nodes.at(frame.sequence_node);
                if (frame.kind == FrameKind::sequence && frame.next_child < sequence.children.size()) {
                    activation.current_node = sequence.children[frame.next_child++];
                } else {
                    activation.current_node = frame.return_node;
                    activation.frames.pop_back();
                }
            }
            break;
        }
    }
}
bool condition_met(const content::Condition& condition, const ScopeBinding& binding,
                   const CompiledProgram& program, const WorldState& world,
                   RuntimeResult& result, const std::string& path) {
    const auto reader = [&](const auto& metadata, const auto& scope) { return read_fact(world, metadata, scope); };
    const auto authoritative = evaluate_condition(condition, binding, program.registry, reader,
                                                   InspectionMode::author_fixture, path);
    // Runtime result traces are safe to show to players. Full fixture traces
    // remain available by explicitly invoking the pure author inspector.
    auto visible = evaluate_condition(condition, binding, program.registry, reader, InspectionMode::player_known, path);
    result.clauses.insert(result.clauses.end(), std::make_move_iterator(visible.clauses.begin()),
                          std::make_move_iterator(visible.clauses.end()));
    return authoritative.result.value();
}
void ensure_lifecycles(Checkpoint& checkpoint, const CompiledProgram& program) {
    auto& state = checkpoint.runtime;
    const auto& world = checkpoint.world;
    for (const auto& [id, definition] : program.collection.definitions) {
        const auto* current = std::get_if<content::FlowDefinition>(&definition.data);
        if (!current) continue;
        if (world.enabled_flows && std::find(world.enabled_flows->begin(), world.enabled_flows->end(), id) == world.enabled_flows->end()) continue;
        const auto binding = binding_for(world, current->scope);
        const auto found = std::find_if(state.lifecycles.begin(), state.lifecycles.end(), [&](const auto& entry) {
            return entry.definition_id == id && entry.binding == binding;
        });
        if (found != state.lifecycles.end()) continue;
        LifecycleState entry;
        entry.definition_id = id; entry.binding = binding;
        if (current->lifecycle.mode == content::LifecycleMode::interval) {
            const auto created = current->scope == content::Scope::world ? world.campaign_start : world.scope_created;
            const auto anchor = resolve_anchor(*current->lifecycle.anchor, world.campaign_start, created);
            entry.first_due = anchor.first_due; entry.calendar_day = anchor.calendar_day;
        }
        state.lifecycles.push_back(std::move(entry));
    }
    std::sort(state.lifecycles.begin(), state.lifecycles.end(), [](const auto& left, const auto& right) {
        return std::tie(left.binding.id, left.definition_id) < std::tie(right.binding.id, right.definition_id);
    });
}
bool interval_ready(LifecycleState& state, const content::Lifecycle& lifecycle_definition,
                     core::OrdinalDay today, std::size_t& steps) {
    const ResolvedAnchor anchor{*state.first_due, state.calendar_day};
    auto due = interval_due(anchor, *lifecycle_definition.interval, state.interval_index);
    if (due < today) {
        if (lifecycle_definition.interval->unit == content::IntervalUnit::days) {
            const auto difference = core::checked_subtract(today.value(), anchor.first_due.value());
            state.interval_index = static_cast<std::uint64_t>(difference) / lifecycle_definition.interval->value;
        } else {
            const auto first = core::from_ordinal(anchor.first_due);
            const auto current = core::from_ordinal(today);
            const auto months = static_cast<std::uint64_t>((current.year - first.year) * 12 +
                static_cast<int>(current.month) - static_cast<int>(first.month));
            state.interval_index = months / lifecycle_definition.interval->value;
        }
        due = interval_due(anchor, *lifecycle_definition.interval, state.interval_index);
        while (due < today) {
            if (++steps > maximum_steps_per_operation || state.interval_index == std::numeric_limits<std::uint64_t>::max())
                fail(RuntimeReason::budget_exceeded, "Interval catch-up budget exceeded.");
            due = interval_due(anchor, *lifecycle_definition.interval, ++state.interval_index);
        }
    }
    if (due != today) return false;
    if (state.interval_index == std::numeric_limits<std::uint64_t>::max()) fail(RuntimeReason::budget_exceeded, "Interval index exhausted.");
    ++state.interval_index;
    return true;
}
void prepare(Checkpoint& checkpoint, const CompiledProgram& program, RuntimeResult& result, std::size_t& steps) {
    auto& state = checkpoint.runtime;
    auto& world = checkpoint.world;
    const auto today = world.simulation.date;
    if (world.simulation.phase == core::DayPhase::opening)
        world.simulation = core::Simulation{world.simulation}.checkpoint();
    if (world.simulation.phase == core::DayPhase::finished) return;
    if (world.simulation.phase == core::DayPhase::awaiting_choice) return;
    if (world.simulation.phase != core::DayPhase::work_ready)
        fail(RuntimeReason::morning_not_ready, "Resolve the core opening work before running content.");
    if (state.prepared_day == today || has_choice(state)) return;
    ensure_lifecycles(checkpoint, program);
    // Continuous exits resolve before the new day's capacity or cost queries.
    for (auto& entry : state.lifecycles) {
        const auto& definition = flow(program, entry.definition_id);
        if (definition.lifecycle.mode != content::LifecycleMode::continuous || !entry.active_activation || entry.last_checked == today) continue;
        auto& active = activation_by_id(state, *entry.active_activation);
        entry.last_checked = today;
        if (!active.exiting && condition_met(*definition.lifecycle.exit_when, entry.binding, program, world,
                                             result, "flows/" + entry.definition_id + "/exit_when")) {
            active.current_flow = active.definition_id;
            active.current_node = definition.exit_node;
            active.frames.clear(); active.due.reset(); active.wait_started.reset(); active.presentation_id.reset();
            active.status = ActivationStatus::ready; active.exiting = true;
            execute(checkpoint, active, program, result, steps);
            if (has_choice(state)) return;
        }
    }
    std::vector<std::uint64_t> due;
    for (const auto& entry : state.activations)
        if (entry.status == ActivationStatus::ready ||
            (entry.status == ActivationStatus::delayed && entry.due && *entry.due <= today)) due.push_back(entry.id);
    std::sort(due.begin(), due.end(), [&](auto left, auto right) {
        const auto& a = activation_by_id(state, left);
        const auto& b = activation_by_id(state, right);
        return std::tuple{a.due.value_or(today), a.binding.id, a.definition_id, a.id} <
               std::tuple{b.due.value_or(today), b.binding.id, b.definition_id, b.id};
    });
    for (auto id : due) {
        auto& active = activation_by_id(state, id);
        if (active.status == ActivationStatus::delayed) active.current_node = node(program, active).next;
        active.due.reset(); active.wait_started.reset(); active.status = ActivationStatus::ready;
        execute(checkpoint, active, program, result, steps);
        if (has_choice(state)) return;
    }
    for (auto& entry : state.lifecycles) {
        if (++steps > maximum_steps_per_operation) fail(RuntimeReason::budget_exceeded, "Lifecycle evaluation budget exceeded.");
        if (entry.last_checked == today) continue;
        const auto& definition = flow(program, entry.definition_id);
        entry.last_checked = today;
        const bool eligible = condition_met(definition.trigger, entry.binding, program, world, result,
                                             "flows/" + entry.definition_id + "/trigger");
        entry.was_eligible = eligible;
        bool due_now = true;
        if (definition.lifecycle.mode == content::LifecycleMode::interval)
            due_now = interval_ready(entry, definition.lifecycle, today, steps);
        else if (definition.lifecycle.mode == content::LifecycleMode::scheduled)
            due_now = definition.lifecycle.due_date == today;
        else if (definition.lifecycle.mode == content::LifecycleMode::recurring && entry.last_started)
            due_now = core::checked_subtract(today.value(), entry.last_started->value()) >= definition.lifecycle.cooldown_days;
        const bool continuous = definition.lifecycle.mode == content::LifecycleMode::continuous;
        const bool permitted = continuous ? (entry.occurrences == 0 || definition.lifecycle.allow_reentry) :
                                            entry.occurrences < definition.lifecycle.max_occurrences;
        if (!eligible || !due_now || !permitted || entry.active_activation) continue;
        if (state.activations.size() >= maximum_activations || entry.occurrences == std::numeric_limits<std::uint32_t>::max())
            fail(RuntimeReason::budget_exceeded, "Activation/occurrence count budget exceeded.");
        Activation active;
        active.id = identifier(state.next_activation_id); active.definition_id = entry.definition_id;
        active.definition_revision = program.collection.find(entry.definition_id)->revision;
        active.started_on = today;
        active.binding = entry.binding; active.current_flow = entry.definition_id;
        active.current_node = definition.entry_node;
        entry.active_activation = active.id; entry.last_started = today; ++entry.occurrences;
        state.activations.push_back(std::move(active));
        execute(checkpoint, state.activations.back(), program, result, steps);
        if (has_choice(state)) return;
    }
    state.prepared_day = today;
}
template<class Function>
RuntimeResult transaction(const CompiledProgram& program, Checkpoint& original, std::uint64_t expected, Function action) {
    RuntimeResult result;
    result.revision = original.world.simulation.revision;
    if (expected != result.revision) {
        result.reason = RuntimeReason::stale_revision;
        result.explanation = "State changed after this action was prepared.";
        return result;
    }
    try {
        auto next = original;
        if (next.world.simulation.revision == std::numeric_limits<std::uint64_t>::max())
            fail(RuntimeReason::arithmetic_overflow, "The authoritative revision range is exhausted.");
        ++next.world.simulation.revision;
        std::size_t steps = 0;
        action(next, result, steps);
        validate_checkpoint(program, next);
        original = std::move(next);
        result.revision = original.world.simulation.revision;
        result.committed = true;
        for (auto& path : result.paths) path.committed = true;
        result.explanation = "The content operation was committed.";
    } catch (const Failure& failure) {
        result.reason = failure.reason; result.explanation = failure.detail;
    } catch (const std::overflow_error&) {
        result.reason = RuntimeReason::arithmetic_overflow; result.explanation = "Checked arithmetic refused the operation; nothing changed.";
    } catch (const std::exception& error) {
        result.reason = RuntimeReason::invalid_state; result.explanation = error.what();
    }
    if (!result.accepted()) {
        result.explanation += " The candidate operation was rolled back; no state changed.";
        for (auto& path : result.paths) path.detail = "Rolled-back candidate: " + path.detail;
        for (auto& clause : result.clauses) clause.path = "rolled_back_candidate/" + clause.path;
    }
    return result;
}
} // namespace

std::string_view to_string(RuntimeReason reason) noexcept {
    switch (reason) {
    case RuntimeReason::accepted: return "accepted";
    case RuntimeReason::stale_revision: return "stale_revision";
    case RuntimeReason::invalid_state: return "invalid_state";
    case RuntimeReason::missing_definition: return "missing_definition";
    case RuntimeReason::invalid_choice: return "invalid_choice";
    case RuntimeReason::morning_not_ready: return "morning_not_ready";
    case RuntimeReason::effect_rejected: return "effect_rejected";
    case RuntimeReason::budget_exceeded: return "budget_exceeded";
    case RuntimeReason::arithmetic_overflow: return "arithmetic_overflow";
    case RuntimeReason::campaign_finished: return "campaign_finished";
    }
    return "invalid";
}
Runtime::Runtime(CompiledProgram program, Checkpoint checkpoint)
    : program_{compile_collection(program.collection, program.registry, supported_effects())}, checkpoint_{std::move(checkpoint)} {
    validate_checkpoint(program_, checkpoint_);
}
RuntimeResult Runtime::prepare_opening(std::uint64_t expected_revision) {
    if (expected_revision == checkpoint_.world.simulation.revision &&
        (checkpoint_.runtime.prepared_day == checkpoint_.world.simulation.date || has_choice(checkpoint_.runtime) ||
         checkpoint_.world.simulation.phase == core::DayPhase::awaiting_choice))
        return {RuntimeReason::accepted, expected_revision, "This morning is already prepared or suspended at its existing choice.", {}, {}};
    return transaction(program_, checkpoint_, expected_revision, [&](auto& next, auto& result, auto& steps) {
        if (next.world.simulation.phase == core::DayPhase::finished) fail(RuntimeReason::campaign_finished, "The campaign is finished.");
        prepare(next, program_, result, steps);
    });
}
RuntimeResult Runtime::choose(std::uint64_t expected_revision, std::uint64_t presentation_id, std::string_view choice_id) {
    return transaction(program_, checkpoint_, expected_revision, [&](auto& next, auto& result, auto& steps) {
        const auto found = std::find_if(next.runtime.presentations.begin(), next.runtime.presentations.end(),
            [presentation_id](const auto& presentation) { return presentation.id == presentation_id; });
        if (found == next.runtime.presentations.end() || found->kind != PresentationKind::dialogue || found->resolved)
            fail(RuntimeReason::invalid_choice, "This dialogue is absent or was already resolved.");
        auto& active = activation_by_id(next.runtime, found->activation_id);
        if (active.status != ActivationStatus::awaiting_choice || active.presentation_id != presentation_id)
            fail(RuntimeReason::invalid_choice, "The dialogue no longer owns the waiting activation.");
        const auto& entry = node(program_, active);
        const auto selected_id = choice_id.empty() ? std::string_view{entry.default_choice} : choice_id;
        const auto selected = std::find_if(entry.choices.begin(), entry.choices.end(), [&](const auto& choice) { return choice.id == selected_id; });
        if (selected == entry.choices.end()) fail(RuntimeReason::invalid_choice, "The selected choice is not available.");
        found->resolved = true; found->selected_choice = selected->id;
        active.presentation_id.reset(); active.status = ActivationStatus::ready;
        active.current_node = selected->next;
        execute(next, active, program_, result, steps);
        prepare(next, program_, result, steps);
    });
}
RuntimeResult Runtime::submit(const core::Command& command) {
    if (command.expected_revision != checkpoint_.world.simulation.revision)
        return {RuntimeReason::stale_revision, checkpoint_.world.simulation.revision, "The command's observed revision is stale.", {}, {}};
    // Core submit increments once itself; transaction's reservation is removed
    // inside the copied state so the whole command/opening shares one revision.
    return transaction(program_, checkpoint_, command.expected_revision, [&](auto& next, auto& result, auto& steps) {
        --next.world.simulation.revision;
        const bool pause_only = std::holds_alternative<core::SetPaused>(command.action) && std::get<core::SetPaused>(command.action).paused;
        const bool core_choice = std::holds_alternative<core::ResolveOpeningChoice>(command.action);
        if (!pause_only && !core_choice &&
            (has_choice(next.runtime) || next.runtime.prepared_day != next.world.simulation.date))
            fail(RuntimeReason::morning_not_ready, "Resolve required content choices and prepare the morning before work.");
        core::Simulation core_state{next.world.simulation};
        const auto outcome = core_state.submit(command);
        if (!outcome.accepted()) {
            if (outcome.reason == core::CommandReason::campaign_finished) fail(RuntimeReason::campaign_finished, outcome.explanation);
            if (outcome.reason == core::CommandReason::arithmetic_overflow) fail(RuntimeReason::arithmetic_overflow, outcome.explanation);
            if (outcome.reason == core::CommandReason::limit_exceeded) fail(RuntimeReason::budget_exceeded, outcome.explanation);
            fail(RuntimeReason::effect_rejected, outcome.explanation);
        }
        const auto prior_day = next.world.simulation.date;
        next.world.simulation = core_state.checkpoint();
        if (next.world.simulation.date != prior_day) next.runtime.prepared_day.reset();
        if (next.world.simulation.phase == core::DayPhase::work_ready &&
            next.runtime.prepared_day != next.world.simulation.date) prepare(next, program_, result, steps);
    });
}
RuntimeResult Runtime::synthetic_cleanup(std::uint64_t expected_revision, std::uint64_t activation_id) {
    return transaction(program_, checkpoint_, expected_revision, [&](auto& next, auto& result, auto&) {
        auto& active = activation_by_id(next.runtime, activation_id);
        if (active.status != ActivationStatus::continuous_active)
            fail(RuntimeReason::invalid_state, "Synthetic cleanup requires an active completed continuous entry.");
        append_trace(result, active, "Synthetic cleanup only: this does not establish natural exit eligibility.", true);
        remove_owned(next, active);
        active.status = ActivationStatus::completed;
        lifecycle(next.runtime, active).active_activation.reset();
    });
}
} // namespace pgp::runtime
