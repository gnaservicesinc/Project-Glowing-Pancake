#include <pgp/studio/reusable_flow.hpp>
#include "form_builder.hpp"
#include "references.hpp"
#include <algorithm>

namespace pgp::studio {
std::vector<FormOption> group_variable_options(const DraftDocument& draft, content::Scope scope,
    const content::GroupPort* port, const content::FlowDefinition* caller) {
    const auto* contract = port ? draft.registry().fact(port->variable) : nullptr;
    std::vector<FormOption> result;
    for (const auto& variable : draft.collection().variables) {
        if (variable.scope != scope) continue;
        if (contract && (variable.type != contract->type || variable.unit != contract->unit ||
            variable.minimum != contract->minimum || variable.maximum != contract->maximum ||
            variable.visibility != contract->visibility)) continue;
        if (port && port->output && caller && caller->group &&
            std::any_of(caller->group->begin(),caller->group->end(),[&](const auto& parent) {
                return parent.variable == variable.id && !parent.output;
            })) continue;
        result.push_back({variable.id, variable.id + " · " + detail::unit_label(variable.unit), variable.help});
    }
    return result;
}
bool create_flow_group(DraftDocument& draft, std::string_view source, std::string_view id) {
    if (!content::valid_id(id) || draft.collection().find(id)) throw std::invalid_argument("Choose a new stable lowercase group ID.");
    auto group = detail::find_flow(draft, source);
    if (!group.on_committed.empty() || detail::seeded_root(group) || group.lifecycle.mode == content::LifecycleMode::continuous)
        throw std::invalid_argument("Choose a flow body without a committed source, system incident or continuous exit.");
    content::Lifecycle lifecycle{};
    lifecycle.mode = content::LifecycleMode::once;
    lifecycle.occurrence_scope = group.scope;
    group.lifecycle = std::move(lifecycle);
    if (!group.group) group.group.emplace();
    std::erase(group.tags,"base_event");
    auto candidate = draft.collection();
    candidate.definitions.emplace(id, content::Definition{std::string(id),1,std::move(group)});
    return draft.replace(candidate);
}
bool insert_group_call(DraftDocument& draft, std::string_view caller, std::string_view before,
    std::string_view group_id, std::map<std::string,std::string,std::less<>> bindings) {
    const auto& group = detail::find_flow(draft, group_id);
    if (!group.group) throw std::invalid_argument("Select a reusable group.");
    auto candidate = draft.collection();
    auto& flow = std::get<content::FlowDefinition>(candidate.definitions.at(std::string(caller)).data);
    if (!flow.nodes.contains(before)) throw std::invalid_argument("Select an existing continuation node.");
    const auto id = detail::available_node_id(flow, "group_call");
    const auto replace = [&](std::string& link) { if (link == before) link = id; };
    replace(flow.entry_node);
    replace(flow.exit_node);
    for (auto& [node_id, node] : flow.nodes) {
        (void)node_id;
        detail::visit_node_links(node, replace);
    }
    content::Node call{};
    call.kind = content::NodeKind::subflow;
    call.next = before;
    call.subflow_id = group_id;
    call.group_bindings = std::move(bindings);
    flow.nodes.emplace(id,std::move(call));
    return draft.replace(candidate);
}
namespace detail {
void build_group_form(FormBuilder& builder) {
    if (!builder.flow.group) throw std::invalid_argument("Select a reusable group.");
    builder.note("Call scope", std::string(content::scope_name(builder.flow.scope)),
        "The group receives the caller's existing entity. Its end node returns to the caller's saved continuation.");
    const auto choices = group_variable_options(builder.draft, builder.flow.scope);
    for (std::size_t index = 0; index < builder.flow.group->size(); ++index) {
        const auto prefix = "port/" + std::to_string(index) + "/";
        const auto& port = builder.flow.group->at(index);
        builder.add(prefix + "name", "Port name", port.name, "Stable name used by call sites.",
            FormControl::text, [index](FlowDefinition& flow, std::string_view value) { flow.group->at(index).name = value; });
        builder.add(prefix + "variable", "Body variable", port.variable, "Read or assign this variable in the group body.",
            FormControl::choice, [index](FlowDefinition& flow, std::string_view value) { flow.group->at(index).variable = value; }, choices);
        builder.add(prefix + "direction", "Direction", port.output ? "output" : "input",
            "An input is read only. An output writes the caller's bound variable through normal effects.",
            FormControl::choice, [index](FlowDefinition& flow, std::string_view value) { flow.group->at(index).output = value == "output"; },
            {{"input", "Read-only input"}, {"output", "Writable output"}});
        builder.action(prefix + "remove", "Remove " + port.name, "Existing callers must still satisfy the complete contract.",
            [index](FlowDefinition& flow, std::string_view) {
                flow.group->erase(flow.group->begin() + static_cast<std::ptrdiff_t>(index));
            });
    }
    std::vector<FormOption> available{{"", "Choose a scoped variable"}};
    for (const auto& choice : choices) {
        if (std::none_of(builder.flow.group->begin(), builder.flow.group->end(), [&](const auto& port) {
            return port.variable == choice.value;
        })) available.push_back(choice);
    }
    if (available.size() == 1) builder.note("Add a port", "Declare a new variable in Scoped variables first.");
    if (builder.flow.group->size() < 16 && available.size() > 1) {
        for (const bool output : {false,true}) builder.add(output ? "add_output" : "add_input", output ? "Add output" : "Add input",
            "", "Select a body variable to expose under a named port. Its type, unit, range and visibility define the binding.",
            FormControl::choice, [output](FlowDefinition& flow, std::string_view value) {
                if (value.empty()) return;
                auto name = std::string(value);
                for (unsigned suffix = 2; std::any_of(flow.group->begin(),flow.group->end(),[&](const auto& port) {
                    return port.name == name;
                }); ++suffix) name = "port." + std::to_string(suffix);
                flow.group->push_back({std::move(name),std::string(value),output});
            }, available);
    }
}
} // namespace detail
} // namespace pgp::studio
