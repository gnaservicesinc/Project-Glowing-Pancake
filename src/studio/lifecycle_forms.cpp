#include "graph_forms.hpp"
#include "references.hpp"

#include <algorithm>
#include <array>
#include <set>

namespace pgp::studio {
namespace detail {
namespace {
constexpr std::array root_scopes{Scope::company, Scope::world, Scope::state};
constexpr std::array modes{LifecycleMode::once, LifecycleMode::recurring, LifecycleMode::interval,
                           LifecycleMode::scheduled, LifecycleMode::continuous};
constexpr std::array anchors{AnchorKind::campaign_start, AnchorKind::scope_created,
                             AnchorKind::month_start, AnchorKind::fixed_date};
std::string_view mode_key(LifecycleMode mode) {
    switch (mode) {
    case LifecycleMode::once: return "once";
    case LifecycleMode::recurring: return "recurring";
    case LifecycleMode::interval: return "interval";
    case LifecycleMode::scheduled: return "scheduled";
    case LifecycleMode::continuous: return "continuous";
    }
    throw std::invalid_argument("Unknown lifecycle.");
}
std::string_view anchor_key(AnchorKind kind) {
    switch (kind) {
    case AnchorKind::campaign_start: return "campaign_start";
    case AnchorKind::scope_created: return "scope_created";
    case AnchorKind::month_start: return "month_start";
    case AnchorKind::fixed_date: return "fixed_date";
    }
    throw std::invalid_argument("Unknown anchor.");
}
std::string_view anchor_label(AnchorKind kind) {
    switch (kind) {
    case AnchorKind::campaign_start: return "Campaign start";
    case AnchorKind::scope_created: return "Scope created";
    case AnchorKind::month_start: return "First month boundary";
    case AnchorKind::fixed_date: return "Fixed date";
    }
    throw std::invalid_argument("Unknown anchor.");
}
std::vector<FormOption> mode_choices(const FlowDefinition& flow) {
    std::vector<FormOption> result;
    for (const auto mode : modes) {
        if (!flow.on_committed.empty() && mode != LifecycleMode::once && mode != LifecycleMode::recurring) continue;
        if (seeded_root(flow) && mode != LifecycleMode::scheduled && mode != LifecycleMode::interval) continue;
        result.push_back({std::string(mode_key(mode)), std::string(lifecycle_label(mode))});
    }
    return result;
}
void rename_definition(Collection& collection, const Registry& registry, const std::string& from, std::string_view to) {
    if (from == to) return;
    if (!valid_id(to) || collection.definitions.contains(to))
        throw std::invalid_argument("Choose an unused stable definition ID.");
    auto renamed = collection.definitions.extract(from);
    if (renamed.empty()) throw std::invalid_argument("The definition no longer exists.");
    renamed.key() = to;
    renamed.mapped().id = to;
    collection.definitions.insert(std::move(renamed));
    for (auto& [id, definition] : collection.definitions) {
        (void)id;
        visit_definition_references(definition, registry, [&](std::string& target) {
            if (target == from) target = to;
        });
    }
}
void collect_reachable(const FlowDefinition& flow, const std::string& node, std::set<std::string>& reached) {
    if (!reached.insert(node).second) return;
    visit_node_links(flow.nodes.at(node), [&](const auto& next) { collect_reachable(flow, next, reached); });
}
void remove_exit_only_terminal(FlowDefinition& flow) {
    std::set<std::string> reached;
    collect_reachable(flow, flow.entry_node, reached);
    for (const auto& [id, node] : flow.nodes) {
        if (!reached.contains(id) && node.kind != NodeKind::end)
            throw std::invalid_argument("Remove the exit-only actions before changing this lifecycle.");
        if (std::any_of(node.effects.begin(), node.effects.end(), [](const auto& effect) {
            return effect.operation == "register_owned_modifier";
        })) throw std::invalid_argument("Remove owned modifier registration before changing this lifecycle.");
    }
    std::erase_if(flow.nodes, [&](const auto& node) { return !reached.contains(node.first); });
    flow.exit_node.clear();
}
template<class ConditionType, class Visit>
void visit_conditions(ConditionType& condition, const Visit& visit) {
    visit(condition);
    for (auto& child : condition.children) visit_conditions(child, visit);
}
Condition default_exit(const Condition& entry) {
    std::set<std::string> used;
    visit_conditions(entry, [&](const auto& condition) {
        if (condition.kind == ConditionKind::holds_for_days) used.insert(condition.predicate_id);
    });
    auto copy = entry;
    unsigned index = 1;
    visit_conditions(copy, [&](auto& condition) {
        if (condition.kind != ConditionKind::holds_for_days) return;
        do { condition.predicate_id = "exit.duration." + std::to_string(index++); }
        while (!used.insert(condition.predicate_id).second);
    });
    return {ConditionKind::not_condition, {}, {std::move(copy)}};
}
void change_mode(FlowDefinition& flow, LifecycleMode mode, core::OrdinalDay initial_date) {
    if (mode == flow.lifecycle.mode) return;
    if (flow.lifecycle.mode == LifecycleMode::continuous) remove_exit_only_terminal(flow);
    Lifecycle next{};
    next.mode = mode;
    next.occurrence_scope = flow.scope;
    switch (mode) {
    case LifecycleMode::once: break;
    case LifecycleMode::recurring:
        next.max_occurrences = 2;
        next.cooldown_days = 1;
        break;
    case LifecycleMode::interval:
        next.max_occurrences = 2;
        next.interval = Interval{IntervalUnit::days, 7};
        next.anchor = Anchor{AnchorKind::campaign_start, {}};
        break;
    case LifecycleMode::scheduled:
        next.due_date = initial_date;
        break;
    case LifecycleMode::continuous: {
        next.exit_when = default_exit(flow.trigger);
        flow.exit_node = available_node_id(flow, "exit");
        Node terminal{};
        terminal.kind = NodeKind::end;
        flow.nodes.emplace(flow.exit_node, std::move(terminal));
        break;
    }
    }
    flow.lifecycle = std::move(next);
}
void interval_units(FormBuilder& builder, std::string key, std::string label, Interval current, bool offset) {
    builder.add(std::move(key), std::move(label), current.unit == IntervalUnit::days ? "days" : "months",
                "Calendar months keep the preferred day and clamp to the end of short months.", FormControl::choice,
                [offset](FlowDefinition& flow, std::string_view unit) {
        auto& value = offset ? flow.lifecycle.anchor->offset : flow.lifecycle.interval.value();
        value.unit = unit == "days" ? IntervalUnit::days : IntervalUnit::calendar_months;
    }, {{"days", "Days"}, {"months", "Calendar months"}});
}
const ScenarioDefinition* example_scenario(const FormBuilder& builder) {
    for (const auto& [id, definition] : builder.draft.collection().definitions) {
        (void)id;
        const auto* scenario = std::get_if<ScenarioDefinition>(&definition.data);
        if (scenario && std::find(scenario->flows.begin(), scenario->flows.end(), builder.id) != scenario->flows.end())
            return scenario;
    }
    return nullptr;
}
void interval_form(FormBuilder& builder, const Lifecycle& lifecycle, core::OrdinalDay initial_date, bool has_scenario) {
    const auto& anchor = *lifecycle.anchor;
    builder.add("interval", "Repeat every", std::to_string(lifecycle.interval->value), {}, FormControl::integer,
                [](FlowDefinition& flow, std::string_view value) { flow.lifecycle.interval->value = parse_count(value, 1, 1'000'000); });
    interval_units(builder, "interval_unit", "Repeat unit", *lifecycle.interval, false);
    std::vector<FormOption> options;
    for (const auto kind : anchors) options.push_back({std::string(anchor_key(kind)), std::string(anchor_label(kind))});
    builder.add("anchor", "Anchor", std::string(anchor_key(anchor.kind)),
                "The first month boundary is the first month start on or after scope creation.",
                FormControl::choice, [initial_date](FlowDefinition& flow, std::string_view value) {
        auto& selected = *flow.lifecycle.anchor;
        const auto kind = *std::find_if(anchors.begin(), anchors.end(), [value](auto item) { return anchor_key(item) == value; });
        if (kind == selected.kind) return;
        selected.kind = kind;
        selected.date = kind == AnchorKind::fixed_date ? std::optional{initial_date} : std::nullopt;
    }, std::move(options));
    if (anchor.date)
        builder.add("anchor_date", "Fixed anchor date", core::iso_date(*anchor.date), "Enter YYYY-MM-DD.", FormControl::text,
                    [](FlowDefinition& flow, std::string_view value) { flow.lifecycle.anchor->date = core::parse_iso_date(value); });
    builder.add("offset", "First due offset", std::to_string(anchor.offset.value), "Applied once to the anchor.", FormControl::integer,
                [](FlowDefinition& flow, std::string_view value) { flow.lifecycle.anchor->offset.value = parse_count(value, 0, 1'000'000); });
    interval_units(builder, "offset_unit", "Offset unit", anchor.offset, true);
    if (anchor.date || has_scenario) {
        try {
            const auto due = runtime::resolve_anchor(anchor, initial_date, initial_date);
            builder.note("First due at scenario opening", core::iso_date(due.first_due),
                         "Calculated by the shared scheduler. Scope-created anchors follow the actual scope date in play.");
        } catch (const std::exception& error) {
            builder.note("First due cannot be resolved", error.what());
        }
    }
    builder.note("Missed checks", "Skip");
}
} // namespace

void build_flow_settings(FormBuilder& builder) {
    const auto& registry = builder.draft.registry();
    builder.add_document("id", "Definition ID", builder.id,
                         "Renaming updates typed references in this collection. Existing save compatibility is checked during finalization.",
                         FormControl::text, [id = builder.id, &registry](Collection& collection, FlowDefinition&, std::string_view value) {
        rename_definition(collection, registry, id, value);
    });
    builder.note("Flow scope", std::string(scope_name(builder.flow.scope)), "Choose the scope when creating a flow.");
    add_link_fields(builder, "entry", "Entry node", builder.flow.entry_node,
                    [](FlowDefinition& flow) -> std::string& { return flow.entry_node; });
    builder.add("visit_limit", "Node visit limit", std::to_string(builder.flow.max_node_visits),
                "A positive limit is required for a cycle that includes a delay. Zero uses the default for acyclic paths.", FormControl::integer,
                [](FlowDefinition& flow, std::string_view value) { flow.max_node_visits = parse_count(value, 0, 100'000); });
    if (builder.flow.lifecycle.mode == LifecycleMode::once || builder.flow.lifecycle.mode == LifecycleMode::recurring) {
        std::vector<FormOption> sources{{"", "Normal lifecycle check"}};
        for (const auto& [id, source] : registry.committed_facts())
            if (source.owner_scope == builder.flow.scope) sources.push_back({id, id, source.help});
        if (sources.size() > 1) {
            const auto* selected = registry.committed_fact(builder.flow.on_committed);
            builder.add("source", "Activation source", builder.flow.on_committed,
                        selected ? selected->help : "Choose a committed fact subscription or ordinary lifecycle evaluation.",
                        FormControl::choice, [](FlowDefinition& flow, std::string_view value) { flow.on_committed = value; }, std::move(sources));
        }
    }
    for (const auto& [id, definition] : builder.draft.collection().definitions) {
        const auto* scenario = std::get_if<ScenarioDefinition>(&definition.data);
        if (!scenario) continue;
        const bool enabled = std::find(scenario->flows.begin(), scenario->flows.end(), builder.id) != scenario->flows.end();
        builder.add_document("scenario/" + id, id, enabled ? "true" : "false", "Enable this flow as a scenario root.",
                             FormControl::choice, [id, flow_id = builder.id](Collection& collection, FlowDefinition&, std::string_view value) {
            auto& list = std::get<ScenarioDefinition>(collection.definitions.at(id).data).flows;
            const auto found = std::find(list.begin(), list.end(), flow_id);
            if (value == "true" && found == list.end()) list.push_back(flow_id);
            if (value == "false" && found != list.end()) list.erase(found);
        }, boolean_choices());
    }
}
void build_lifecycle_form(FormBuilder& builder) {
    const auto& lifecycle = builder.flow.lifecycle;
    const auto* scenario = example_scenario(builder);
    const auto initial_date = scenario ? scenario->starts_on : core::OrdinalDay{0};
    builder.note("Occurrence scope", std::string(scope_name(lifecycle.occurrence_scope)));
    builder.add("mode", "Lifecycle", std::string(mode_key(lifecycle.mode)),
                "Changing mode starts its default timing. A continuous flow gets a separate exit and an editable negated entry condition.",
                FormControl::choice, [initial_date](FlowDefinition& flow, std::string_view value) {
        const auto mode = *std::find_if(modes.begin(), modes.end(), [value](auto item) { return mode_key(item) == value; });
        change_mode(flow, mode, initial_date);
    }, mode_choices(builder.flow));
    if (lifecycle.mode == LifecycleMode::recurring || lifecycle.mode == LifecycleMode::interval)
        builder.add("maximum", "Maximum occurrences", std::to_string(lifecycle.max_occurrences),
                    "A finite bound for the occurrence scope.", FormControl::integer, [](FlowDefinition& flow, std::string_view value) {
            flow.lifecycle.max_occurrences = parse_count(value, 1, 1'000'000);
        });
    if (lifecycle.mode == LifecycleMode::recurring) {
        builder.add("cooldown", "Cooldown days", std::to_string(lifecycle.cooldown_days), "Recheck after cooldown while still eligible.",
                    FormControl::integer, [](FlowDefinition& flow, std::string_view value) {
            flow.lifecycle.cooldown_days = parse_count(value, 1, 1'000'000);
        });
        builder.note("Overlap", "Forbid");
    }
    if (lifecycle.mode == LifecycleMode::interval) interval_form(builder, lifecycle, initial_date, scenario != nullptr);
    if (lifecycle.mode == LifecycleMode::scheduled) {
        builder.add("due", "Due date", core::iso_date(*lifecycle.due_date), "Enter YYYY-MM-DD. A missed check is skipped.",
                    FormControl::text, [](FlowDefinition& flow, std::string_view value) { flow.lifecycle.due_date = core::parse_iso_date(value); });
        builder.note("Maximum occurrences", "One");
    }
    if (lifecycle.mode == LifecycleMode::continuous) {
        add_link_fields(builder, "exit_node", "Exit node", builder.flow.exit_node,
                        [](FlowDefinition& flow) -> std::string& { return flow.exit_node; });
        builder.add("reentry", "Allow reentry", lifecycle.allow_reentry ? "true" : "false",
                    "Allow a new activation after the current owner has exited.", FormControl::choice,
                    [](FlowDefinition& flow, std::string_view value) { flow.lifecycle.allow_reentry = value == "true"; }, boolean_choices());
        builder.note("Stacking", "One active owner per scope");
    }
}
} // namespace detail

std::vector<FormOption> flow_scope_options(const DraftDocument& draft) {
    std::vector<FormOption> result;
    for (const auto scope : detail::root_scopes) {
        if (detail::fact_choices(draft.registry(), scope).empty()) continue;
        const auto label = std::string(content::scope_name(scope));
        result.push_back({label, label});
    }
    return result;
}
bool create_flow(DraftDocument& draft, std::string_view id, std::string_view scope) {
    const auto typed_scope = std::find_if(detail::root_scopes.begin(), detail::root_scopes.end(), [scope](auto value) {
        return content::scope_name(value) == scope;
    });
    if (typed_scope == detail::root_scopes.end()) throw std::invalid_argument("Choose an available flow scope.");
    if (!content::valid_id(id) || draft.collection().definitions.contains(id))
        throw std::invalid_argument("Choose an unused stable flow ID.");
    auto candidate = draft.collection();
    content::FlowDefinition flow{};
    flow.scope = *typed_scope;
    flow.trigger = detail::default_condition(draft.registry(), flow.scope);
    flow.lifecycle.mode = content::LifecycleMode::once;
    flow.lifecycle.occurrence_scope = flow.scope;
    flow.entry_node = "end";
    content::Node terminal{};
    terminal.kind = content::NodeKind::end;
    flow.nodes.emplace(flow.entry_node, std::move(terminal));
    candidate.definitions.emplace(std::string(id), content::Definition{std::string(id), 1, std::move(flow)});
    return draft.replace(candidate);
}
} // namespace pgp::studio
