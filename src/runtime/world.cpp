#include <pgp/runtime/runtime.hpp>

#include "duration.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace pgp::runtime {
namespace {
[[noreturn]] void invalid(std::string detail) { throw std::invalid_argument(std::move(detail)); }
bool binding_exists(const WorldState& world, const ScopeBinding& binding) {
    switch (binding.scope) {
    case content::Scope::company: return binding.id == world.company_id;
    case content::Scope::world: return binding.id == world.world_id;
    case content::Scope::state: return binding.id == world.state_id;
    }
    return false;
}
void scalar_valid(const content::Scalar& value, const content::FactMetadata& metadata) {
    switch (metadata.type) {
    case content::ValueType::integer: {
        const auto* number = std::get_if<std::int64_t>(&value);
        if (!number || *number < metadata.minimum || *number > metadata.maximum)
            invalid("Fact type or unit range mismatch: " + metadata.id);
        return;
    }
    case content::ValueType::boolean:
        if (!std::holds_alternative<bool>(value)) invalid("Fact requires boolean: " + metadata.id);
        return;
    case content::ValueType::text: {
        const auto* text = std::get_if<std::string>(&value);
        if (!text || (!metadata.default_value && text->empty()) || text->size() > 4096 || text->find('\0') != std::string::npos)
            invalid("Fact requires bounded text: " + metadata.id);
        return;
    }
    case content::ValueType::reference: invalid("Reference facts have no runtime provider");
    }
    invalid("Unknown fact type");
}
const content::FlowDefinition& flow(const CompiledProgram& program, std::string_view id) {
    const auto* definition = program.collection.find(id);
    if (!definition || !std::holds_alternative<content::FlowDefinition>(definition->data))
        invalid("Missing runtime flow definition: " + std::string(id));
    return std::get<content::FlowDefinition>(definition->data);
}
const content::Node& node(const CompiledProgram& program, std::string_view id, std::string_view node_id) {
    const auto& definition = flow(program, id);
    const auto found = definition.nodes.find(node_id);
    if (found == definition.nodes.end()) invalid("Missing runtime flow node");
    return found->second;
}
void date_valid(core::OrdinalDay date) { (void)core::from_ordinal(date); }
// A saved stack explicitly identifies each currently open numbered child or
// subflow. Do not traverse child ports here: entering one would need another
// frame, while its normal return may legitimately have completed already.
using BranchKey = std::tuple<std::uint64_t, std::string, std::string>;
using BranchSelections = std::map<BranchKey, const RandomResolution*>;
bool region_reaches(const content::FlowDefinition& definition, const std::string& start,
                     const std::string& target, std::string_view flow_id,
                     std::uint64_t activation_id, const BranchSelections& selections) {
    std::vector<std::string> pending{start};
    std::set<std::string> visited;
    while (!pending.empty()) {
        const auto id = std::move(pending.back()); pending.pop_back();
        if (id == target) return true;
        if (!visited.insert(id).second) continue;
        const auto found = definition.nodes.find(id);
        if (found == definition.nodes.end()) return false;
        const auto& entry = found->second;
        if (!entry.next.empty()) pending.push_back(entry.next);
        for (const auto& choice : entry.choices) pending.push_back(choice.next);
        if (entry.kind == content::NodeKind::random) {
            const auto selected = selections.find({activation_id, std::string(flow_id), id});
            // Crossing a random node requires its committed outcome. An
            // unvisited node cannot explain a saved delay/choice after it.
            if (selected != selections.end())
                for (const auto& branch : entry.branches)
                    if (branch.id == selected->second->branch_id) pending.push_back(branch.next);
        }
    }
    return false;
}
template<class Row> const Row& by_id(const std::vector<Row>& rows, std::uint64_t id) {
    const auto found = std::find_if(rows.begin(), rows.end(), [id](const auto& row) { return row.id == id; });
    if (found == rows.end()) invalid("Missing runtime record identity");
    return *found;
}
template<class Row> void identities(const std::vector<Row>& rows, std::uint64_t next) {
    if (next == 0 || rows.size() > maximum_records) invalid("Runtime identifier/count limit");
    std::uint64_t previous = 0;
    for (const auto& row : rows) {
        if (row.id <= previous || row.id >= next) invalid("Runtime IDs must be unique and canonical");
        previous = row.id;
    }
}
} // namespace

WorldState foundation_world(core::Cents cash) {
    WorldState world;
    world.simulation = core::Simulation{cash}.checkpoint();
    world.facts = {{"company.monthly_compute_bill_cents", std::int64_t{0}},
                   {"company.workload_stress_basis_points", std::int64_t{0}},
                   {"world.public_breakthrough_count", std::int64_t{0}},
                   {"company.has_exclusive_compute_supplier", false},
                   {"company.has_compute_lease", false},
                   {"company.has_research_partner", false},
                   {"company.is_operating", true}};
    return world;
}
std::vector<std::string> supported_effects() {
    return {"adjust_cash", "record_decision", "open_negotiation", "register_owned_modifier", "remove_owned_modifier", "set_variable"};
}
content::Scalar read_fact(const WorldState& world, const content::FactMetadata& metadata,
                          const ScopeBinding& binding) {
    if (!binding_exists(world, binding) || binding.scope != metadata.scope)
        invalid("Fact scope binding does not exist or has the wrong type");
    if (metadata.default_value) {
        const auto found = std::find_if(world.variables.begin(), world.variables.end(), [&](const auto& value) {
            return value.binding == binding && value.variable_id == metadata.id;
        });
        const auto& value = found == world.variables.end() ? *metadata.default_value : found->value;
        scalar_valid(value, metadata);
        return value;
    }
    if (metadata.id == "company.cash_cents") return world.simulation.cash.value();
    const auto found = world.facts.find(metadata.id);
    if (found == world.facts.end()) invalid("Missing owning fact provider value: " + metadata.id);
    scalar_valid(found->second, metadata);
    return found->second;
}
std::int64_t modifier_total(const RuntimeState& state, const ScopeBinding& binding, std::string_view field) {
    std::int64_t total = 0;
    for (const auto& modifier : state.modifiers)
        if (modifier.binding == binding && modifier.field == field)
            total = core::checked_add(total, modifier.add_basis_points);
    return total;
}

void validate_checkpoint(const CompiledProgram& program, const Checkpoint& checkpoint) {
    const auto& world = checkpoint.world;
    const auto& state = checkpoint.runtime;
    // Core validation runs on a copy only; a saved unfinished opening can be
    // resumed later without this inspection modifying its cursor or cash.
    (void)core::Simulation{world.simulation};
    date_valid(world.campaign_start); date_valid(world.scope_created);
    if (!content::valid_id(world.company_id) || !content::valid_id(world.world_id) ||
        !content::valid_id(world.state_id) || world.campaign_start > world.simulation.date ||
        world.scope_created < world.campaign_start || world.scope_created > world.simulation.date ||
        world.counterparty_cash.value() < 0 || world.facts.size() > 512 ||
        world.decisions.size() > maximum_records || world.negotiations.size() > maximum_records)
        invalid("Invalid world identity, date, counterparty or count");
    for (const auto& [id, value] : world.facts) {
        const auto* metadata = program.registry.fact(id);
        if (!metadata || metadata->default_value || id == "company.cash_cents") invalid("Unknown or duplicated owning world fact");
        scalar_valid(value, *metadata);
    }
    if (world.variables.size() > maximum_records) invalid("Variable value count budget exceeded");
    std::optional<std::pair<ScopeBinding, std::string>> previous_variable;
    for (const auto& variable : world.variables) {
        const auto identity = std::pair{variable.binding, variable.variable_id};
        const auto* metadata = program.registry.fact(variable.variable_id);
        if (!metadata || !metadata->default_value || !binding_exists(world, variable.binding) ||
            metadata->scope != variable.binding.scope || (previous_variable && identity <= *previous_variable))
            invalid("Variable value has an unknown owner, wrong scope or duplicate/noncanonical identity");
        scalar_valid(variable.value, *metadata);
        previous_variable = identity;
    }
    if (world.enabled_flows) {
        if (world.enabled_flows->size() > 512) invalid("Enabled flow count budget exceeded");
        std::set<std::string> enabled;
        for (const auto& id : *world.enabled_flows) {
            (void)flow(program, id);
            if (!enabled.insert(id).second) invalid("Duplicate enabled root flow");
        }
    }
    for (const auto& decision : world.decisions)
        if (!content::valid_id(decision)) invalid("Invalid decision marker");
    for (const auto& negotiation : world.negotiations) {
        const auto* definition = program.collection.find(negotiation);
        if (!definition || !std::holds_alternative<content::DealDefinition>(definition->data))
            invalid("Negotiation references missing deal template");
    }
    const auto operations = supported_effects();
    for (const auto& [id, definition] : program.collection.definitions) {
        (void)id;
        if (const auto* value = std::get_if<content::FlowDefinition>(&definition.data)) {
            const auto binding = ScopeBinding{value->scope, value->scope == content::Scope::company ? world.company_id :
                value->scope == content::Scope::world ? world.world_id : world.state_id};
            const auto reader = [&](const auto& metadata, const auto& bound) { return read_fact(world, metadata, bound); };
            (void)evaluate_condition(value->trigger, binding, program.registry, reader);
            if (value->lifecycle.exit_when) (void)evaluate_condition(*value->lifecycle.exit_when, binding, program.registry, reader);
            for (const auto& [node_id, entry] : value->nodes) {
                (void)node_id;
                for (const auto& effect : entry.effects)
                    if (std::find(operations.begin(), operations.end(), effect.operation) == operations.end())
                        invalid("Registered effect has no available runtime provider: " + effect.operation);
            }
        }
    }
    if (state.activations.size() > maximum_activations || state.lifecycles.size() > 512 ||
        state.modifiers.size() > maximum_activations || state.receipts.size() > maximum_records ||
        state.presentations.size() > maximum_records || state.random_resolutions.size() > maximum_records)
        invalid("Runtime collection count budget exceeded");
    identities(state.activations, state.next_activation_id);
    identities(state.receipts, state.next_receipt_id);
    identities(state.presentations, state.next_presentation_id);
    if (state.prepared_day && (*state.prepared_day != world.simulation.date || world.simulation.phase != core::DayPhase::work_ready))
        invalid("Prepared runtime day differs from core date");
    detail::validate_duration_states(checkpoint, program);
    BranchSelections selections;
    for (const auto& resolution : state.random_resolutions)
        selections.insert_or_assign({resolution.activation_id, resolution.flow_id, resolution.node_id}, &resolution);
    std::map<std::pair<std::string, ScopeBinding>, std::uint32_t> counts;
    for (const auto& activation : state.activations) {
        const auto& root = flow(program, activation.definition_id);
        const auto* definition = program.collection.find(activation.definition_id);
        if (!binding_exists(world, activation.binding) || activation.binding.scope != root.scope ||
            activation.definition_revision != definition->revision || activation.frames.size() > maximum_continuation_frames ||
            flow(program, activation.current_flow).scope != activation.binding.scope)
            invalid("Activation definition revision, scope or frame budget mismatch");
        const auto& current = node(program, activation.current_flow, activation.current_node);
        date_valid(activation.started_on);
        if (activation.started_on < world.campaign_start || activation.started_on > world.simulation.date)
            invalid("Invalid activation start date");
        ++counts[{activation.definition_id, activation.binding}];
        const auto visits_limit = root.max_node_visits ? root.max_node_visits : maximum_steps_per_operation;
        if (activation.node_visits > visits_limit) invalid("Activation exceeded authored node budget");
        std::string expected_flow = activation.definition_id;
        std::string region_entry = activation.exiting ? root.exit_node : root.entry_node;
        for (const auto& frame : activation.frames) {
            const auto& owner = node(program, frame.flow_id, frame.sequence_node);
            const auto& frame_flow = flow(program, frame.flow_id);
            if (frame.flow_id != expected_flow || frame_flow.scope != activation.binding.scope ||
                !region_reaches(frame_flow, region_entry, frame.sequence_node, frame.flow_id, activation.id, selections) ||
                frame.binding != activation.binding || frame.return_node != owner.next ||
                (!frame.return_node.empty() && !flow(program, frame.flow_id).nodes.contains(frame.return_node)))
                invalid("Invalid continuation return/context");
            if (frame.kind == FrameKind::sequence) {
                if (owner.kind != content::NodeKind::sequence || frame.next_child > owner.children.size() || frame.next_child == 0)
                    invalid("Invalid saved sequence continuation");
                region_entry = owner.children[frame.next_child - 1];
            } else if (frame.kind == FrameKind::subflow) {
                if (owner.kind != content::NodeKind::subflow || frame.next_child != 0)
                    invalid("Invalid saved subflow continuation");
                expected_flow = owner.subflow_id;
                region_entry = flow(program, expected_flow).entry_node;
            } else invalid("Unknown continuation frame kind");
        }
        if (activation.current_flow != expected_flow ||
            !region_reaches(flow(program, expected_flow), region_entry, activation.current_node, expected_flow, activation.id, selections))
            invalid("Current node escaped its saved continuation region");
        switch (activation.status) {
        case ActivationStatus::ready:
            if (activation.due || activation.wait_started || activation.presentation_id) invalid("Ready activation retains wait state");
            break;
        case ActivationStatus::delayed:
            if (!activation.due || !activation.wait_started || activation.presentation_id || current.kind != content::NodeKind::delay)
                invalid("Delayed activation lacks its authored delay boundary");
            date_valid(*activation.due);
            date_valid(*activation.wait_started);
            if (*activation.wait_started < activation.started_on || *activation.wait_started > world.simulation.date ||
                *activation.due != core::advance_days(*activation.wait_started, core::Days{current.delay_days}))
                invalid("Saved delay timing differs from its authored duration");
            break;
        case ActivationStatus::awaiting_choice: {
            if (activation.due || activation.wait_started || !activation.presentation_id || current.kind != content::NodeKind::dialogue)
                invalid("Waiting activation is not on a dialogue");
            const auto& presentation = by_id(state.presentations, *activation.presentation_id);
            if (presentation.resolved || presentation.activation_id != activation.id ||
                presentation.node_id != activation.current_node || presentation.flow_id != activation.current_flow)
                invalid("Waiting dialogue identity mismatch");
            break;
        }
        case ActivationStatus::continuous_active:
            if (root.lifecycle.mode != content::LifecycleMode::continuous || current.kind != content::NodeKind::end || activation.exiting || activation.due || activation.wait_started ||
                activation.presentation_id || !activation.frames.empty()) invalid("Invalid active continuous lifecycle");
            break;
        case ActivationStatus::completed:
            if (activation.due || activation.wait_started || activation.presentation_id || !activation.frames.empty() || current.kind != content::NodeKind::end)
                invalid("Completed activation retains work");
            break;
        default: invalid("Unknown activation status");
        }
    }
    std::set<std::pair<std::string, ScopeBinding>> lifecycle_keys;
    for (const auto& entry : state.lifecycles) {
        const auto& definition = flow(program, entry.definition_id);
        if (!binding_exists(world, entry.binding) || entry.binding.scope != definition.scope ||
            (world.enabled_flows && std::find(world.enabled_flows->begin(), world.enabled_flows->end(), entry.definition_id) == world.enabled_flows->end()) ||
            !lifecycle_keys.emplace(entry.definition_id, entry.binding).second ||
            counts[{entry.definition_id, entry.binding}] != entry.occurrences ||
            (entry.last_checked && *entry.last_checked > world.simulation.date) ||
            (entry.last_started && *entry.last_started > world.simulation.date) ||
            (entry.occurrences != 0 && !entry.last_started)) invalid("Invalid occurrence scope/counter/date");
        if (entry.last_checked) date_valid(*entry.last_checked);
        if (entry.last_started) date_valid(*entry.last_started);
        if (entry.last_started && *entry.last_started < world.campaign_start) invalid("Occurrence predates the campaign");
        if (definition.lifecycle.mode != content::LifecycleMode::continuous && entry.occurrences > definition.lifecycle.max_occurrences)
            invalid("Occurrence count exceeds authored bound");
        if (entry.active_activation) {
            const auto& active = by_id(state.activations, *entry.active_activation);
            if (active.definition_id != entry.definition_id || active.binding != entry.binding || active.status == ActivationStatus::completed)
                invalid("Active lifecycle points to completed/unrelated activation");
        }
        if (definition.lifecycle.mode == content::LifecycleMode::interval) {
            if (!entry.first_due || entry.calendar_day < 1 || entry.calendar_day > 31) invalid("Interval lacks saved anchor");
            const auto created = definition.scope == content::Scope::world ? world.campaign_start : world.scope_created;
            const auto resolved = resolve_anchor(*definition.lifecycle.anchor, world.campaign_start, created);
            if (resolved.first_due != *entry.first_due || resolved.calendar_day != entry.calendar_day)
                invalid("Saved interval anchor does not match content/context");
            (void)interval_due(resolved, *definition.lifecycle.interval, entry.interval_index);
        } else if (entry.first_due || entry.interval_index != 0) invalid("Non-interval lifecycle carries an anchor");
    }
    for (const auto& activation : state.activations)
        if (!lifecycle_keys.contains({activation.definition_id, activation.binding})) invalid("Activation has no occurrence state");
        else if (activation.status != ActivationStatus::completed) {
            const auto owner = std::find_if(state.lifecycles.begin(), state.lifecycles.end(), [&](const auto& entry) {
                return entry.definition_id == activation.definition_id && entry.binding == activation.binding;
            });
            if (owner->active_activation != activation.id) invalid("Live activation is not its occurrence scope's sole active instance");
        }
    std::map<std::uint64_t, std::uint32_t> last_random_visit;
    std::optional<core::OrdinalDay> last_random_day;
    if (state.narrative_origin.has_value() != !state.random_resolutions.empty())
        invalid("Random resolution history and its starting stream must be retained together");
    auto last_random_state = state.narrative_origin;
    std::map<std::string, std::set<std::string>, std::less<>> called_flows;
    for (const auto& resolution : state.random_resolutions) {
        const auto& activation = by_id(state.activations, resolution.activation_id);
        const auto& owner = node(program, resolution.flow_id, resolution.node_id);
        auto [allowed, inserted] = called_flows.try_emplace(activation.definition_id);
        if (inserted) {
            std::vector<std::string> pending{activation.definition_id};
            while (!pending.empty()) {
                const auto id = std::move(pending.back()); pending.pop_back();
                if (!allowed->second.insert(id).second) continue;
                for (const auto& [node_id, entry] : flow(program, id).nodes) {
                    (void)node_id;
                    if (entry.kind == content::NodeKind::subflow) pending.push_back(entry.subflow_id);
                }
            }
        }
        date_valid(resolution.day);
        if (owner.kind != content::NodeKind::random || owner.random_stream != "narrative" ||
            !allowed->second.contains(resolution.flow_id) ||
            flow(program, resolution.flow_id).scope != activation.binding.scope ||
            resolution.day < activation.started_on || resolution.day > world.simulation.date ||
            (last_random_day && resolution.day < *last_random_day) ||
            resolution.node_visit == 0 || resolution.node_visit > activation.node_visits ||
            resolution.node_visit <= last_random_visit[activation.id] ||
            (last_random_state && resolution.stream_before != *last_random_state))
            invalid("Random resolution has an invalid node, scope, date, visit or stream transition");
        std::uint64_t total = 0;
        for (const auto& branch : owner.branches) total += branch.weight;
        core::SplitMix64 copied{resolution.stream_before};
        const auto sample = copied.bounded(total);
        if (sample != resolution.sample || copied.state() != resolution.stream_after)
            invalid("Random resolution differs from its saved stream draw");
        auto remaining = sample;
        const content::WeightedBranch* selected = nullptr;
        for (const auto& branch : owner.branches) {
            if (remaining < branch.weight) { selected = &branch; break; }
            remaining -= branch.weight;
        }
        if (!selected || selected->id != resolution.branch_id)
            invalid("Random resolution differs from its authored weighted outcome");
        last_random_visit[activation.id] = resolution.node_visit;
        last_random_day = resolution.day;
        last_random_state = resolution.stream_after;
    }
    if (last_random_state && *last_random_state !=
        world.simulation.random[static_cast<std::size_t>(core::RandomStream::narrative)])
        invalid("Saved narrative stream does not continue its resolved branch history");
    std::set<std::tuple<std::uint64_t, ScopeBinding, std::string>> modifier_keys;
    for (const auto& modifier : state.modifiers) {
        const auto& activation = by_id(state.activations, modifier.activation_id);
        if (flow(program, activation.definition_id).lifecycle.mode != content::LifecycleMode::continuous ||
            activation.status == ActivationStatus::completed || activation.binding != modifier.binding ||
            !content::valid_id(modifier.key) || modifier.field != "research_coordination_basis_points" ||
            modifier.add_basis_points < -10000 || modifier.add_basis_points > 10000 ||
            !modifier_keys.emplace(modifier.activation_id, modifier.binding, modifier.key).second)
            invalid("Invalid/duplicate owned modifier");
    }
    using ModifierKey = std::tuple<std::uint64_t, ScopeBinding, std::string>;
    std::map<ModifierKey, OwnedModifier> recorded_modifiers;
    for (const auto& receipt : state.receipts) {
        const auto& activation = by_id(state.activations, receipt.activation_id);
        const auto& owner = node(program, receipt.flow_id, receipt.node_id);
        if (receipt.revision == 0 || receipt.revision > world.simulation.revision || receipt.day < activation.started_on || receipt.day > world.simulation.date ||
            receipt.target != activation.binding) invalid("Invalid effect receipt scope/date/revision");
        date_valid(receipt.day);
        if (receipt.operation != "lifecycle_cleanup" &&
            (owner.kind != content::NodeKind::effect || receipt.effect_index >= owner.effects.size() ||
             owner.effects[receipt.effect_index].operation != receipt.operation)) invalid("Effect receipt differs from definition");
        if (receipt.operation == "adjust_cash") {
            const auto* before = std::get_if<std::int64_t>(&receipt.before);
            const auto* after = std::get_if<std::int64_t>(&receipt.after);
            const auto amount = std::get<std::int64_t>(owner.effects[receipt.effect_index].arguments.at("amount_cents"));
            if (!before || !after || *before < 0 || *after < 0 || core::checked_subtract(*after, *before) != amount ||
                core::checked_subtract(*before, *after) != receipt.counterparty_delta.value())
                invalid("Cash transfer receipt is unbalanced");
        } else if (receipt.operation == "set_variable") {
            if (receipt.counterparty_delta != core::Cents{0}) invalid("Variable receipt changes counterparty cash");
            const auto& variable_id = std::get<std::string>(owner.effects[receipt.effect_index].arguments.at("variable"));
            const auto* metadata = program.registry.fact(variable_id);
            if (!metadata || !metadata->default_value || metadata->scope != receipt.target.scope)
                invalid("Variable receipt has no registered scoped variable");
            scalar_valid(receipt.before, *metadata);
            scalar_valid(receipt.after, *metadata);
        } else {
            if (receipt.counterparty_delta != core::Cents{0}) invalid("Non-cash receipt changes counterparty");
            const auto* before = std::get_if<std::int64_t>(&receipt.before);
            if (!before) invalid("Effect receipt has the wrong before-value type");
            if (receipt.operation == "record_decision" || receipt.operation == "open_negotiation") {
                const auto* after = std::get_if<std::string>(&receipt.after);
                const auto argument = receipt.operation == "record_decision" ? "value" : "template_id";
                const auto& authored = std::get<std::string>(owner.effects[receipt.effect_index].arguments.at(argument));
                const auto& records = receipt.operation == "record_decision" ? world.decisions : world.negotiations;
                if (!after || *after != authored || *before < 0 || static_cast<std::uint64_t>(*before) >= records.size() ||
                    records[static_cast<std::size_t>(*before)] != *after)
                    invalid("Recorded decision/negotiation receipt differs from its operation and history");
            } else if (receipt.operation == "register_owned_modifier" || receipt.operation == "remove_owned_modifier" ||
                       receipt.operation == "lifecycle_cleanup") {
                const auto* after = std::get_if<std::int64_t>(&receipt.after);
                if (!after || *before < -10000 || *before > 10000 || *after < -10000 || *after > 10000)
                    invalid("Modifier receipt requires basis-point deltas");
                if (receipt.operation == "register_owned_modifier") {
                    if (*after != std::get<std::int64_t>(owner.effects[receipt.effect_index].arguments.at("add_basis_points")))
                        invalid("Modifier receipt delta differs from its definition");
                } else if (*after != 0) invalid("Modifier removal receipt must remove its owned contribution");
                if (receipt.operation == "lifecycle_cleanup" &&
                    (flow(program, activation.definition_id).lifecycle.mode != content::LifecycleMode::continuous ||
                     activation.status != ActivationStatus::completed || owner.kind != content::NodeKind::end || receipt.effect_index != 0))
                    invalid("Lifecycle cleanup receipt has no continuous end boundary");
                if (receipt.operation == "lifecycle_cleanup") {
                    const auto found = std::find_if(recorded_modifiers.begin(), recorded_modifiers.end(), [&](const auto& pair) {
                        return pair.second.activation_id == activation.id && pair.second.add_basis_points == *before;
                    });
                    if (found == recorded_modifiers.end()) invalid("Lifecycle cleanup has no recorded owned contribution");
                    recorded_modifiers.erase(found);
                } else {
                    if (flow(program, activation.definition_id).lifecycle.mode != content::LifecycleMode::continuous &&
                        receipt.operation == "register_owned_modifier") invalid("Noncontinuous activation registered a persistent modifier");
                    const auto& arguments = owner.effects[receipt.effect_index].arguments;
                    const auto& key = std::get<std::string>(arguments.at("key"));
                    const ModifierKey identity{activation.id, activation.binding, key};
                    const auto found = recorded_modifiers.find(identity);
                    const auto expected_before = found == recorded_modifiers.end() ? 0 : found->second.add_basis_points;
                    if (*before != expected_before) invalid("Modifier receipt does not continue its prior owned contribution");
                    if (receipt.operation == "remove_owned_modifier") recorded_modifiers.erase(identity);
                    else recorded_modifiers.insert_or_assign(identity, OwnedModifier{activation.id, activation.binding, key,
                        std::get<std::string>(arguments.at("field")), *after});
                }
            } else invalid("Effect receipt has an unavailable operation");
        }
    }
    if (recorded_modifiers.size() != state.modifiers.size()) invalid("Saved modifiers differ from committed effect ownership");
    for (const auto& modifier : state.modifiers) {
        const auto found = recorded_modifiers.find({modifier.activation_id, modifier.binding, modifier.key});
        if (found == recorded_modifiers.end() || found->second != modifier)
            invalid("Saved modifier contribution differs from its committed effects");
    }
    for (const auto& presentation : state.presentations) {
        const auto& activation = by_id(state.activations, presentation.activation_id);
        const auto& owner = node(program, presentation.flow_id, presentation.node_id);
        if (presentation.day < activation.started_on || presentation.day > world.simulation.date || presentation.title_key != owner.title_key ||
            presentation.text_key != owner.text_key) invalid("Presentation date/localization differs from definition");
        date_valid(presentation.day);
        if (presentation.kind == PresentationKind::dialogue) {
            if (owner.kind != content::NodeKind::dialogue || presentation.choices.size() != owner.choices.size())
                invalid("Saved dialogue choices differ from definition");
            for (std::size_t i = 0; i < owner.choices.size(); ++i)
                if (presentation.choices[i].id != owner.choices[i].id || presentation.choices[i].label_key != owner.choices[i].label_key)
                    invalid("Saved choice identity/text mismatch");
            if (presentation.resolved) {
                if (std::none_of(owner.choices.begin(), owner.choices.end(), [&](const auto& choice) { return choice.id == presentation.selected_choice; }))
                    invalid("Resolved dialogue selected an unknown choice");
            } else if (activation.presentation_id != presentation.id || activation.status != ActivationStatus::awaiting_choice ||
                       !presentation.selected_choice.empty()) invalid("Unresolved dialogue has no suspended activation");
        } else if ((presentation.kind != PresentationKind::news && presentation.kind != PresentationKind::result) ||
                   owner.kind != (presentation.kind == PresentationKind::news ? content::NodeKind::news : content::NodeKind::result) ||
                   !presentation.resolved || !presentation.choices.empty() || !presentation.selected_choice.empty())
            invalid("Invalid saved news/result presentation");
    }
    if (state.prepared_day && std::any_of(state.activations.begin(), state.activations.end(), [](const auto& activation) {
        return activation.status == ActivationStatus::awaiting_choice || activation.status == ActivationStatus::ready;
    })) invalid("Prepared runtime morning still contains required work");
}
} // namespace pgp::runtime
