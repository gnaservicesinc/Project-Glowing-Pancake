#include <pgp/runtime/runtime.hpp>
#include <pgp/runtime/society.hpp>
#include <pgp/runtime/authority.hpp>

#include "duration.hpp"
#include "committed.hpp"
#include "competition_internal.hpp"
#include "group_bindings.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace pgp::runtime {
void validate_checkpoint_with_simulation(const CompiledProgram& program, const Checkpoint& checkpoint,
                                         bool validate_core_simulation);
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
    if (effect.operation == "set_variable") {
        const auto& declared = std::get<std::string>(effect.arguments.at("variable"));
        const auto& executing = flow(program, activation.current_flow);
        std::vector<std::string> calls;
        if (executing.group) for (const auto& frame : activation.frames)
            if (frame.kind == FrameKind::subflow) calls.push_back(frame.sequence_node);
        const auto bound = [&](std::string_view id, bool write) {
            if (!detail::group_port(executing, id)) return id;
            return detail::resolve_group_variable(program, activation.definition_id, activation.current_flow, calls, id, write);
        };
        const std::string variable_id{bound(declared, true)};
        const auto* metadata = program.registry.fact(variable_id);
        if (!metadata || !metadata->default_value || metadata->scope != activation.binding.scope ||
            effect.target != "self" || effect.target_scope != activation.binding.scope || !effect.value_expression)
            fail(RuntimeReason::effect_rejected, "Assignment requires a registered variable in the explicit self scope.");
        const auto facts = [&](const auto& field, const auto& binding) {
            return read_fact(checkpoint.world, *program.registry.fact(bound(field.id, false)), binding);
        };
        const auto evaluated = evaluate_expression(*effect.value_expression, activation.binding, program.registry, facts);
        if (!evaluated) fail(RuntimeReason::effect_rejected, "Assignment value is unavailable.");
        const auto* number = std::get_if<std::int64_t>(&*evaluated);
        const auto* text = std::get_if<std::string>(&*evaluated);
        const bool valid = metadata->type == content::ValueType::integer ?
            (number && *number >= metadata->minimum && *number <= metadata->maximum) :
            metadata->type == content::ValueType::boolean ? std::holds_alternative<bool>(*evaluated) :
            (text && text->size() <= 4096 && text->find('\0') == std::string::npos);
        if (!valid) fail(RuntimeReason::effect_rejected, "Assignment exceeds the variable's registered type or range.");
        const auto before = read_fact(checkpoint.world, *metadata, activation.binding);
        auto& values = checkpoint.world.variables;
        const auto found = std::find_if(values.begin(), values.end(), [&](const auto& value) {
            return value.binding == activation.binding && value.variable_id == variable_id;
        });
        if (found == values.end()) {
            record_budget(values.size());
            values.push_back({activation.binding, variable_id, *evaluated});
            std::sort(values.begin(), values.end(), [](const auto& left, const auto& right) {
                return std::tie(left.binding, left.variable_id) < std::tie(right.binding, right.variable_id);
            });
        } else found->value = *evaluated;
        add_receipt(checkpoint, activation, index, effect.operation, before, *evaluated);
        if (detail::group_port(executing, declared)) {
            auto& receipt = checkpoint.runtime.receipts.back();
            receipt.bound_variable = variable_id;
            receipt.group_calls = std::move(calls);
        }
        return;
    }
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
        // The foundation transfer has no business revenue semantics. Keep it
        // separate from customer invoices and explicit costs in the journal.
        if (world.simulation.accounts && amount != 0)
            core::record_cash(*world.simulation.accounts, world.simulation.date, core::Cents{amount},
                core::Account::equity, "foundation_cash_transfer",
                "flow.transfer:" + std::to_string(state.next_receipt_id));
        world.simulation.cash = core::Cents{after};
        world.counterparty_cash = core::Cents{pool};
        add_receipt(checkpoint, activation, index, effect.operation, before, after,
                    core::Cents{core::checked_subtract(0, amount)});
    } else if (effect.operation == "accept_business_offer") {
        auto& simulation=world.simulation;
        if (!simulation.business_offers || !simulation.accounts || !simulation.capacity)
            fail(RuntimeReason::effect_rejected,"This scenario has no business offer accounts or capacity.");
        core::Cents protected_cash{0}; for (const auto& row:simulation.reservations) protected_cash=core::checked_unit_add(protected_cash,row.reserved);
        const auto before=simulation.cash.value();
        core::accept_business_offer(*simulation.business_offers,*simulation.accounts,*simulation.capacity,simulation.market,simulation.date,
            std::get<std::string>(effect.arguments.at("offer_id")),protected_cash);
        simulation.cash=core::accounts_report(*simulation.accounts).cash;
        add_receipt(checkpoint,activation,index,effect.operation,before,simulation.cash.value());
    } else if (effect.operation == "temporary_demand_change") {
        auto& simulation=world.simulation;
        if (!simulation.market) fail(RuntimeReason::effect_rejected,"This scenario has no customer market.");
        const auto& segment_id=std::get<std::string>(effect.arguments.at("segment_id"));
        const auto segment=std::find_if(simulation.market->segments.begin(),simulation.market->segments.end(),[&](const auto& row){return row.id==segment_id;});
        if (segment==simulation.market->segments.end()) fail(RuntimeReason::effect_rejected,"Demand incident refers to an unavailable segment.");
        const auto customer=*segment;
        const auto before=core::effective_market_demand(*simulation.market,customer,simulation.date);
        const auto duration=std::get<std::int64_t>(effect.arguments.at("duration_days"));
        core::add_market_demand_change(*simulation.market,simulation.date,{"occurrence."+std::to_string(activation.id)+"."+std::to_string(index),segment_id,
            simulation.date,core::advance_days(simulation.date,core::Days{duration}),core::BasisPointDelta{std::get<std::int64_t>(effect.arguments.at("delta_bp"))}});
        add_receipt(checkpoint,activation,index,effect.operation,static_cast<std::int64_t>(before),
            static_cast<std::int64_t>(core::effective_market_demand(*simulation.market,customer,simulation.date)));
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
            const auto domain = content::random_branch_stream(entry.random_stream);
            if (!domain) fail(RuntimeReason::invalid_state, "The random node has no owning stream.");
            auto& saved = checkpoint.world.simulation.random[static_cast<std::size_t>(*domain)];
            const auto before = saved;
            if (*domain == core::RandomStream::narrative) {
                if (!checkpoint.runtime.narrative_origin) checkpoint.runtime.narrative_origin = before;
            } else checkpoint.runtime.incident_origins.try_emplace(*domain, before);
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
            append_trace(result, activation, "Resolved " + entry.random_stream + " branch " + selected->id +
                " with sample " + std::to_string(sample) + "/" + std::to_string(total) + " and weights [" + weights + "]");
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
ConditionTrace inspect(const content::Condition& condition, const ScopeBinding& binding,
                       const CompiledProgram& program, const Checkpoint& checkpoint,
                       const std::string& path, std::string_view definition_id, InspectionMode mode) {
    const auto& world = checkpoint.world;
    const auto reader = [&](const auto& metadata, const auto& scope) { return read_fact(world, metadata, scope); };
    const auto relations = [&](const auto& metadata, const auto& owner) { return read_relation(world, metadata, owner); };
    const auto durations = detail::saved_duration_reader(checkpoint.runtime, definition_id);
    return evaluate_condition(condition, binding, program.registry, reader,
        mode, path, durations, world.simulation.date, relations);
}
bool condition_met(const content::Condition& condition, const ScopeBinding& binding,
                   const CompiledProgram& program, const Checkpoint& checkpoint,
                   RuntimeResult& result, const std::string& path, std::string_view definition_id,
                   std::size_t& condition_work, InspectionMode trace_mode) {
    auto authoritative = inspect(condition, binding, program, checkpoint, path,
                                 definition_id, InspectionMode::author_fixture);
    condition_work += authoritative.inspected_nodes;
    if (condition_work > 2 * maximum_records)
        fail(RuntimeReason::budget_exceeded, "The prepared-morning condition inspection budget was exceeded.");
    const auto eligible = authoritative.result.value();
    auto visible = trace_mode == InspectionMode::author_fixture ? std::move(authoritative)
        : inspect(condition, binding, program, checkpoint, path, definition_id, trace_mode);
    if (trace_mode == InspectionMode::author_fixture)
        result.condition_checks.push_back({path, binding, visible.result});
    result.clauses.insert(result.clauses.end(), std::make_move_iterator(visible.clauses.begin()),
                          std::make_move_iterator(visible.clauses.end()));
    result.durations.insert(result.durations.end(), std::make_move_iterator(visible.durations.begin()),
                            std::make_move_iterator(visible.durations.end()));
    result.selections.insert(result.selections.end(), std::make_move_iterator(visible.selections.begin()),
                             std::make_move_iterator(visible.selections.end()));
    return eligible;
}
void ensure_lifecycles(Checkpoint& checkpoint, const CompiledProgram& program) {
    auto& state = checkpoint.runtime;
    const auto& world = checkpoint.world;
    for (const auto& [id, definition] : program.collection.definitions) {
        const auto* current = std::get_if<content::FlowDefinition>(&definition.data);
        if (!current || current->group) continue;
        if (world.enabled_flows && std::find(world.enabled_flows->begin(), world.enabled_flows->end(), id) == world.enabled_flows->end()) continue;
        const auto binding = binding_for(world, current->scope);
        const auto found = std::find_if(state.lifecycles.begin(), state.lifecycles.end(), [&](const auto& entry) {
            return entry.definition_id == id && entry.binding == binding;
        });
        if (found != state.lifecycles.end()) continue;
        LifecycleState entry;
        entry.definition_id = id; entry.binding = binding;
        if (!current->on_committed.empty()) entry.committed_cursor = 0;
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
void start_activation(Checkpoint& checkpoint, LifecycleState& entry, const content::FlowDefinition& definition,
                      const CompiledProgram& program, RuntimeResult& result, std::size_t& steps,
                      std::optional<CommittedSource> source = std::nullopt) {
    auto& state = checkpoint.runtime;
    if (state.activations.size() >= maximum_activations || entry.occurrences == std::numeric_limits<std::uint32_t>::max())
        fail(RuntimeReason::budget_exceeded, "Activation/occurrence count budget exceeded.");
    Activation active;
    active.id = identifier(state.next_activation_id);
    active.definition_id = entry.definition_id;
    active.definition_revision = program.collection.find(entry.definition_id)->revision;
    active.started_on = checkpoint.world.simulation.date;
    active.binding = entry.binding;
    active.current_flow = entry.definition_id;
    active.current_node = definition.entry_node;
    active.committed_source = std::move(source);
    entry.active_activation = active.id;
    entry.last_started = active.started_on;
    ++entry.occurrences;
    if (active.committed_source)
        append_trace(result, active, "Committed " + active.committed_source->fact_id + " for " + active.committed_source->subject.id);
    state.activations.push_back(std::move(active));
    execute(checkpoint, state.activations.back(), program, result, steps);
}
void dispatch_committed(Checkpoint& checkpoint, LifecycleState& entry, const content::FlowDefinition& definition,
                        const CompiledProgram& program, RuntimeResult& result, std::size_t& steps,
                        std::size_t& condition_work, InspectionMode trace_mode) {
    const auto today = checkpoint.world.simulation.date;
    if (entry.last_checked != today) {
        entry.last_checked = today;
        entry.committed_cursor = 0;
        entry.was_eligible = false;
    }
    const auto sources = detail::committed_sources(checkpoint.world, program.registry, definition.on_committed, entry.binding, today);
    while (*entry.committed_cursor < sources.size()) {
        if (++steps > maximum_steps_per_operation) fail(RuntimeReason::budget_exceeded, "Committed occurrence dispatch budget exceeded.");
        const auto& source = sources[(*entry.committed_cursor)++];
        const auto& rule = definition.lifecycle;
        const bool cooling_down = entry.last_started && today.value() - entry.last_started->value() < rule.cooldown_days;
        if (entry.active_activation || entry.occurrences >= rule.max_occurrences || cooling_down) continue;
        const auto path = "flows/" + entry.definition_id + "/committed/" + std::to_string(*entry.committed_cursor - 1);
        entry.was_eligible = condition_met(definition.trigger, entry.binding, program, checkpoint,
                                           result, path, entry.definition_id, condition_work, trace_mode);
        if (!entry.was_eligible) continue;
        start_activation(checkpoint, entry, definition, program, result, steps, source);
        if (has_choice(checkpoint.runtime)) return;
    }
}
void prepare(Checkpoint& checkpoint, const CompiledProgram& program, RuntimeResult& result,
             std::size_t& steps, InspectionMode trace_mode) {
    auto& state = checkpoint.runtime;
    auto& world = checkpoint.world;
    const auto today = world.simulation.date;
    if (world.simulation.phase == core::DayPhase::opening)
        world.simulation = core::Simulation{world.simulation}.checkpoint();
    if (world.simulation.phase == core::DayPhase::finished || world.simulation.phase == core::DayPhase::closed) return;
    if (world.simulation.phase == core::DayPhase::awaiting_choice) return;
    if (world.simulation.phase != core::DayPhase::work_ready)
        fail(RuntimeReason::morning_not_ready, "Resolve the core opening work before running content.");
    if (state.prepared_day == today || has_choice(state)) return;
    ensure_lifecycles(checkpoint, program);
    detail::observe_durations(checkpoint, program, steps);
    std::size_t condition_work = 0;
    // Continuous exits resolve before the new day's capacity or cost queries.
    for (auto& entry : state.lifecycles) {
        const auto& definition = flow(program, entry.definition_id);
        if (definition.lifecycle.mode != content::LifecycleMode::continuous || !entry.active_activation || entry.last_checked == today) continue;
        auto& active = activation_by_id(state, *entry.active_activation);
        entry.last_checked = today;
        if (!active.exiting && condition_met(*definition.lifecycle.exit_when, entry.binding, program, checkpoint,
                result, "flows/" + entry.definition_id + "/exit_when", entry.definition_id, condition_work, trace_mode)) {
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
        const auto& definition = flow(program, entry.definition_id);
        if (entry.committed_cursor) {
            dispatch_committed(checkpoint, entry, definition, program, result, steps, condition_work, trace_mode);
            if (has_choice(state)) return;
            continue;
        }
        if (entry.last_checked == today) continue;
        entry.last_checked = today;
        const bool eligible = condition_met(definition.trigger, entry.binding, program, checkpoint, result,
                                             "flows/" + entry.definition_id + "/trigger", entry.definition_id, condition_work, trace_mode);
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
        start_activation(checkpoint, entry, definition, program, result, steps);
        if (has_choice(state)) return;
    }
    state.prepared_day = today;
    core::Cents protected_cash{0};for (const auto& reservation:checkpoint.world.simulation.reservations) protected_cash=core::checked_unit_add(protected_cash,reservation.reserved);
    core::review_delegation(checkpoint.world.simulation,protected_cash);
}
bool review_frontier_world(Checkpoint& checkpoint) {
    auto& world=checkpoint.world;
    auto& state=world.simulation;
    if (!state.frontier || checkpoint.runtime.prepared_day!=state.date || has_choice(checkpoint.runtime) ||
        required_public_case(world)) return false;
    core::Cents protected_cash{0};
    for (const auto& reservation:state.reservations)
        protected_cash=core::checked_unit_add(protected_cash,reservation.reserved);
    return core::review_frontier_policies(state,protected_cash,[&](const auto& company,const auto& launch){
        return society_launch_block(world.society?&*world.society:nullptr,company,launch,company.date);
    });
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
        bool core_already_validated = false;
        action(next, result, steps, core_already_validated);
        validate_checkpoint_with_simulation(program, next, !core_already_validated);
        original = std::move(next);
        result.revision = original.world.simulation.revision;
        result.committed = true;
        for (auto& path : result.paths) path.committed = true;
        result.explanation = "The content operation was committed.";
    } catch (const Failure& failure) {
        result.reason = failure.reason; result.explanation = failure.detail;
    } catch (const detail::DurationBudgetExceeded& error) {
        result.reason = RuntimeReason::budget_exceeded; result.explanation = error.what();
    } catch (const CompileError& error) {
        result.reason = error.code() == "budget" ? RuntimeReason::budget_exceeded : RuntimeReason::invalid_state;
        result.explanation = error.what();
    } catch (const std::overflow_error&) {
        result.reason = RuntimeReason::arithmetic_overflow; result.explanation = "Checked arithmetic refused the operation; nothing changed.";
    } catch (const std::exception& error) {
        result.reason = RuntimeReason::invalid_state; result.explanation = error.what();
    }
    if (!result.accepted()) {
        result.explanation += " The candidate operation was rolled back; no state changed.";
        for (auto& path : result.paths) path.detail = "Rolled-back candidate: " + path.detail;
        for (auto& clause : result.clauses) clause.path = "rolled_back_candidate/" + clause.path;
        for (auto& duration : result.durations) duration.path = "rolled_back_candidate/" + duration.path;
        for (auto& selection : result.selections) selection.path = "rolled_back_candidate/" + selection.path;
        for (auto& check : result.condition_checks) check.path = "rolled_back_candidate/" + check.path;
    }
    return result;
}
} // namespace

FlowInspection inspect_flow_predicate(const CompiledProgram& program, const Checkpoint& checkpoint,
                                      std::string_view flow_id, FlowPredicate predicate) {
    const auto* definition = program.collection.find(flow_id);
    const auto* data = definition ? std::get_if<content::FlowDefinition>(&definition->data) : nullptr;
    if (!data) throw std::invalid_argument("Select an existing flow to inspect.");
    const bool exit = predicate == FlowPredicate::exit_when;
    if (exit && !data->lifecycle.exit_when)
        throw std::invalid_argument("This flow has no continuous exit condition.");
    const auto binding = binding_for(checkpoint.world, data->scope);
    const auto path = "flows/" + std::string(flow_id) + (exit ? "/exit_when" : "/trigger");
    return {binding, inspect(exit ? *data->lifecycle.exit_when : data->trigger, binding,
                             program, checkpoint, path, flow_id, InspectionMode::author_fixture)};
}

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
Runtime::Runtime(CompiledProgram program, Checkpoint checkpoint, InspectionMode trace_mode)
    : program_{compile_collection(program.collection, program.registry, supported_effects())},
      checkpoint_{std::move(checkpoint)}, trace_mode_{trace_mode} {
    validate_checkpoint(program_, checkpoint_);
}
RuntimeResult Runtime::prepare_opening(std::uint64_t expected_revision) {
    if (expected_revision == checkpoint_.world.simulation.revision &&
        (checkpoint_.runtime.prepared_day == checkpoint_.world.simulation.date || has_choice(checkpoint_.runtime) ||
         checkpoint_.world.simulation.phase == core::DayPhase::awaiting_choice))
        return {RuntimeReason::accepted, expected_revision, "This morning is already prepared or suspended at its existing choice.", {}, {}};
    return transaction(program_, checkpoint_, expected_revision, [&](auto& next, auto& result, auto& steps, bool&) {
        if (next.world.simulation.phase == core::DayPhase::finished || next.world.simulation.phase == core::DayPhase::closed)
            fail(RuntimeReason::campaign_finished, "The campaign is finished.");
        prepare(next, program_, result, steps, trace_mode_);
        (void)review_frontier_world(next);
    });
}
RuntimeResult Runtime::choose(std::uint64_t expected_revision, std::uint64_t presentation_id, std::string_view choice_id) {
    return transaction(program_, checkpoint_, expected_revision, [&](auto& next, auto& result, auto& steps, bool&) {
        if (next.world.simulation.phase == core::DayPhase::closed)
            fail(RuntimeReason::campaign_finished,"The company is closed; its final state is preserved.");
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
        prepare(next, program_, result, steps, trace_mode_);
        (void)review_frontier_world(next);
    });
}
RuntimeResult Runtime::submit(const core::Command& command) {
    if (command.expected_revision != checkpoint_.world.simulation.revision)
        return {RuntimeReason::stale_revision, checkpoint_.world.simulation.revision, "The command's observed revision is stale.", {}, {}};
    if (checkpoint_.world.simulation.phase == core::DayPhase::closed)
        return {RuntimeReason::campaign_finished,checkpoint_.world.simulation.revision,
            "The company is closed; its final accounts remain inspectable.",{}, {}};
    // Core submit increments once itself; transaction's reservation is removed
    // inside the copied state so the whole command/opening shares one revision.
    return transaction(program_, checkpoint_, command.expected_revision, [&](auto& next, auto& result, auto& steps, bool& core_already_validated) {
        const bool pause_only = std::holds_alternative<core::SetPaused>(command.action) && std::get<core::SetPaused>(command.action).paused;
        const bool core_choice = std::holds_alternative<core::ResolveOpeningChoice>(command.action);
        if (!pause_only && !core_choice &&
            (has_choice(next.runtime) || next.runtime.prepared_day != next.world.simulation.date))
            fail(RuntimeReason::morning_not_ready, "Resolve required content choices and prepare the morning before work.");
        if(std::holds_alternative<core::AdvanceDay>(command.action) && required_public_case(next.world))
            fail(RuntimeReason::morning_not_ready,"Review and acknowledge the public dispute notice before advancing.");
        if(submit_authority_action(next.world,command.action)) return;
        if(const auto* media=std::get_if<core::BuyMediaCampaign>(&command.action)) {
            buy_media_campaign(next.world,*media);return;
        }
        if(const auto* forum=std::get_if<core::EndorseForumStandard>(&command.action)) {
            endorse_forum_standard(next.world,*forum);return;
        }
        --next.world.simulation.revision;
        const auto prior_day = next.world.simulation.date;
        auto actual=command;IndustryAllocations assignments;
        if (std::holds_alternative<core::TransferCompanyOwnership>(actual.action) || std::holds_alternative<core::TransferGroupFunds>(actual.action)) fail(RuntimeReason::effect_rejected,"Company control and group transfers require the owning bilateral runtime transaction.");
        if (auto* day=std::get_if<core::AdvanceDay>(&actual.action)) {
            if (day->market_allocation || !day->industry_adoptions.empty() || !day->wholesale_work.empty() || !day->market_influences.empty())
                fail(RuntimeReason::effect_rejected,"Industry allocations and wholesale work belong to the shared world settlement.");
            competition_detail::refresh_wholesale(next.world,program_,true);
            assignments=prepare_competition_day(next.world,program_);
            day->wholesale_work=competition_detail::settle_wholesale(next.world,program_,assignments);
            day->market_influences=regional_market_conditions(next.world,next.world.simulation,prior_day);
            if (const auto it=assignments.find(next.world.company_id);it!=assignments.end()) {
                day->market_allocation=it->second;day->industry_adoptions=industry_adoption_snapshot(next.world);
            }
        }
        if(const auto* cancel=std::get_if<core::CancelProject>(&actual.action);cancel && next.world.authority) {
            for(const auto& m:next.world.authority->mandates) if(m.project==cancel->id && !m.staff_released_on)
                fail(RuntimeReason::effect_rejected,"Relinquish the institutional agreement to release its paid operating team.");
            for(const auto& c:next.world.authority->cases) if(c.remedy_project==cancel->id && !c.resolved_on && !c.disclosed_on)
                fail(RuntimeReason::effect_rejected,"The negotiated response keeps its review team until resolution or the response deadline.");
        }
        if(const auto* project=std::get_if<core::ReserveProject>(&actual.action);
            project && project->project.id.starts_with("authority."))
            fail(RuntimeReason::effect_rejected,"Institutional projects must be funded through their governing agreement or response.");
        if(const auto* recurring=std::get_if<core::AddRecurringObligation>(&actual.action);
            recurring && next.world.authority && recurring->obligation.id.starts_with("authority."))
            fail(RuntimeReason::effect_rejected,"Institutional payroll belongs to its governing agreement.");
        const auto institutional_posting=[](const auto* action){return action && action->key.starts_with("authority.");};
        if(next.world.authority && (institutional_posting(std::get_if<core::SettleInvoice>(&actual.action)) ||
            institutional_posting(std::get_if<core::RepayDebt>(&actual.action)) ||
            institutional_posting(std::get_if<core::InvestEquity>(&actual.action))))
            fail(RuntimeReason::effect_rejected,"This posting reference belongs to the institutional ledger.");
        if(std::holds_alternative<core::WithdrawOperations>(actual.action) && next.world.authority) {
            const auto& authority=*next.world.authority;
            if(std::ranges::any_of(authority.mandates,[](const auto& m){return !m.staff_released_on;}))
                fail(RuntimeReason::effect_rejected,"Relinquish institutional teams before withdrawing ordinary operations.");
            for(const auto& row:authority.cases) if(row.negotiated_on && !row.resolved_on && !row.disclosed_on) {
                const auto& projects=next.world.simulation.capacity->projects;
                const auto review=std::ranges::find(projects,row.remedy_project,&core::CapacityProject::id);
                if(review!=projects.end() && review->status==core::ProjectStatus::active)
                    fail(RuntimeReason::effect_rejected,"Resolve the negotiated public review before withdrawing operations.");
            }
        }
        if(const auto* launch=std::get_if<core::DeployModel>(&actual.action))
            if(const auto blocked=society_launch_block(next.world.society?&*next.world.society:nullptr,next.world.simulation,*launch,prior_day); !blocked.empty())
                fail(RuntimeReason::effect_rejected,blocked);
        if (const auto* insight=std::get_if<core::AcquireResearchInsight>(&actual.action)) validate_public_insight(next.world,insight->insight);
        core::Simulation core_state{std::move(next.world.simulation), core::TrustedSimulationState{}};
        const auto outcome = core_state.submit_in_transaction(actual);
        if (!outcome.accepted()) {
            if (outcome.reason == core::CommandReason::campaign_finished) fail(RuntimeReason::campaign_finished, outcome.explanation);
            if (outcome.reason == core::CommandReason::arithmetic_overflow) fail(RuntimeReason::arithmetic_overflow, outcome.explanation);
            if (outcome.reason == core::CommandReason::limit_exceeded) fail(RuntimeReason::budget_exceeded, outcome.explanation);
            fail(RuntimeReason::effect_rejected, outcome.explanation);
        }
        next.world.simulation = std::move(core_state).release_checkpoint();
        complete_approved_acquisitions(next.world,program_);
        core::prepare_governance(next.world.simulation);
        if (next.world.simulation.date!=prior_day) {
            advance_competition(next.world,program_,prior_day,assignments);
            advance_society(next.world,prior_day);
        }
        else competition_detail::refresh_wholesale(next.world,program_);
        const bool institutional_resources_changed=prepare_authority(next.world,next.world.simulation.date!=prior_day?std::optional{prior_day}:std::nullopt);
        update_competitive_offers(next.world,program_);
        update_heavyweight_responses(next.world,program_);
        const auto cash_after_submit = next.world.simulation.cash;
        const auto phase_after_submit = next.world.simulation.phase;
        if (next.world.simulation.phase == core::DayPhase::closed) next.world.facts.at("company.is_operating") = false;
        if (next.world.simulation.date != prior_day || next.world.simulation.phase == core::DayPhase::closed)
            next.runtime.prepared_day.reset();
        if (next.world.simulation.phase == core::DayPhase::work_ready &&
            next.runtime.prepared_day != next.world.simulation.date) prepare(next, program_, result, steps, trace_mode_);
        if (!has_choice(next.runtime) && next.runtime.prepared_day==next.world.simulation.date) {
            core::Cents protected_cash{0};for (const auto& reservation:next.world.simulation.reservations) protected_cash=core::checked_unit_add(protected_cash,reservation.reserved);
            core::review_delegation(next.world.simulation,protected_cash);
        }
        const bool frontier_resources_changed=review_frontier_world(next);
        core_already_validated = (!institutional_resources_changed && !frontier_resources_changed && next.world.simulation.cash == cash_after_submit &&
                                  next.world.simulation.phase == phase_after_submit &&
                                  next.world.simulation.phase != core::DayPhase::opening &&
                                  !std::holds_alternative<core::VoteBoardMotion>(command.action));
    });
}
RuntimeResult Runtime::respondToCompetitiveOffer(std::uint64_t expected_revision,std::string_view offer_id,bool accept) {
    return transaction(program_,checkpoint_,expected_revision,[&](auto& next,auto&,auto&,auto&) {
        if (has_choice(next.runtime) || next.runtime.prepared_day!=next.world.simulation.date)
            fail(RuntimeReason::morning_not_ready,"Prepare the morning and resolve required choices before answering an offer.");
        respond_to_competitive_offer(next.world,program_,offer_id,accept);
        update_heavyweight_responses(next.world,program_);
    });
}
RuntimeResult Runtime::proposeAcquisition(std::uint64_t expected_revision,std::string_view target,bool equity) {
    return transaction(program_,checkpoint_,expected_revision,[&](auto& next,auto&,auto&,auto&) {
        if (has_choice(next.runtime) || next.runtime.prepared_day!=next.world.simulation.date || next.world.simulation.phase!=core::DayPhase::work_ready)
            fail(RuntimeReason::morning_not_ready,"Prepare the morning and resolve required choices before negotiating a merger.");
        propose_acquisition(next.world,program_,target,equity);
    });
}
RuntimeResult Runtime::signWholesale(std::uint64_t expected_revision,std::string_view plan,std::string_view source,std::string_view buyer) {
    return transaction(program_,checkpoint_,expected_revision,[&](auto& next,auto&,auto&,auto&) {
        if (has_choice(next.runtime) || next.runtime.prepared_day!=next.world.simulation.date || next.world.simulation.phase!=core::DayPhase::work_ready)
            fail(RuntimeReason::morning_not_ready,"Prepare the morning and resolve required choices before signing wholesale capacity.");
        sign_wholesale(next.world,program_,plan,source,buyer);
    });
}
RuntimeResult Runtime::transferSubsidiaryFunds(std::uint64_t expected_revision,std::string_view target,core::Cents amount,bool funding) {
    return transaction(program_,checkpoint_,expected_revision,[&](auto& next,auto&,auto&,auto&) {
        if (has_choice(next.runtime) || next.runtime.prepared_day!=next.world.simulation.date || next.world.simulation.phase!=core::DayPhase::work_ready)
            fail(RuntimeReason::morning_not_ready,"Prepare the morning and resolve required choices before transferring group funds.");
        --next.world.simulation.revision;
        transfer_subsidiary_funds(next.world,target,amount,funding);
    });
}
RuntimeResult Runtime::synthetic_cleanup(std::uint64_t expected_revision, std::uint64_t activation_id) {
    return transaction(program_, checkpoint_, expected_revision, [&](auto& next, auto& result, auto&, bool&) {
        if (next.world.simulation.phase == core::DayPhase::closed)
            fail(RuntimeReason::campaign_finished,"A closed company retains its final flow state.");
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
