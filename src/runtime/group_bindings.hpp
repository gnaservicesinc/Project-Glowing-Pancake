#pragma once

#include <pgp/runtime/runtime.hpp>
#include <algorithm>
#include <array>

namespace pgp::runtime::detail {
inline const content::FlowDefinition& group_flow(const CompiledProgram& program, std::string_view id) {
    const auto* definition = program.collection.find(id);
    const auto* flow = definition ? std::get_if<content::FlowDefinition>(&definition->data) : nullptr;
    if (!flow) throw std::invalid_argument("Group call refers to an unavailable flow.");
    return *flow;
}

inline const content::GroupPort* group_port(const content::FlowDefinition& flow, std::string_view variable) {
    if (!flow.group) return nullptr;
    const auto port = std::find_if(flow.group->begin(), flow.group->end(),
        [&](const auto& entry) { return entry.variable == variable; });
    return port == flow.group->end() ? nullptr : &*port;
}

// The existing saved frames already describe the actual call sites. Each
// explicit alias can forward into one parent port or end at a real variable.
template<class CallChain>
std::string_view resolve_group_variable(const CompiledProgram& program, std::string_view root,
    std::string_view current, const CallChain& calls, std::string_view variable, bool writing) {
    if (calls.size() > maximum_continuation_frames) throw std::invalid_argument("Group call chain exceeds the frame limit.");
    std::array<const content::Node*, maximum_continuation_frames> sites{};
    std::array<const content::FlowDefinition*, maximum_continuation_frames + 1> flows{};
    flows[0] = &group_flow(program, root);
    auto flow_id = root;
    for (std::size_t i = 0; i < calls.size(); ++i) {
        const auto found = flows[i]->nodes.find(calls[i]);
        if (found == flows[i]->nodes.end() || found->second.kind != content::NodeKind::subflow)
            throw std::invalid_argument("Group call chain has an unavailable call site.");
        sites[i] = &found->second;
        flow_id = sites[i]->subflow_id;
        flows[i + 1] = &group_flow(program, flow_id);
    }
    if (flow_id != current) throw std::invalid_argument("Group call chain does not reach the recorded flow.");
    for (std::size_t i = calls.size(); i != 0; --i) {
        const auto* port = group_port(*flows[i], variable);
        if (!port) return variable;
        if (writing && !port->output) throw std::invalid_argument("A group input is read only.");
        variable = sites[i - 1]->group_bindings.at(port->name);
    }
    return variable;
}
} // namespace pgp::runtime::detail
