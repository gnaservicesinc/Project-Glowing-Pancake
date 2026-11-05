#include <pgp/studio/flow_graph.hpp>
#include "form_builder.hpp"
#include <algorithm>

namespace pgp::studio {
namespace {
using namespace content;
std::string truth(std::optional<bool> value) { return value ? (*value ? "true" : "false") : "unknown"; }
std::string binding(const runtime::ScopeBinding& value) { return std::string(scope_name(value.scope)) + " · " + value.id; }
std::string scalar(const std::optional<Scalar>& value) { return value ? detail::scalar_text(*value) : "unavailable"; }
std::string wildcard_members(std::string path) {
    std::size_t cursor = 0;
    while ((cursor = path.find("/where/", cursor)) != std::string::npos) {
        const auto start = cursor + 7; const auto end = path.find('/', start);
        path.replace(start, end == std::string::npos ? path.size() - start : end - start, "*"); cursor = start + 1;
    }
    return path;
}
void paths(const Condition& condition, const std::string& row, const std::string& path,
    std::map<std::string,std::string,std::less<>>& result) {
    result[path] = row;
    if (condition.kind == ConditionKind::clause) return;
    if (condition.kind == ConditionKind::holds_for_days) {
        result[path + "/holds_for_days"] = row;
        paths(condition.children.front(), row + "/0", path + "/holds_for_days/condition", result); return;
    }
    if (condition.quantifier) {
        result[path + "/related"] = row;
        paths(condition.children.front(), row + "/0", path + "/related/where/*", result); return;
    }
    if (condition.kind == ConditionKind::not_condition) {
        paths(condition.children.front(), row + "/0", path + "/not", result); return;
    }
    for (std::size_t i = 0; i < condition.children.size(); ++i)
        paths(condition.children.at(i), row + "/" + std::to_string(i), path +
            (condition.kind == ConditionKind::all_of ? "/all_of/" : "/any_of/") + std::to_string(i), result);
}
void append(GraphTraces& traces, const std::string& key, std::string state, std::string detail) {
    auto& value = traces[key];
    if (!value.state.empty() && value.state != state) state = "mixed";
    value.state = std::move(state);
    if (value.detail.size() < 3500) { if (!value.detail.empty()) value.detail += "\n"; value.detail += std::move(detail); }
}
}
GraphTraces graph_traces(const runtime::CompiledProgram& program, std::string_view flow_id,
    const runtime::Checkpoint& checkpoint, const runtime::RuntimeResult* operation, bool synthetic) {
    const auto* definition = program.collection.find(flow_id);
    const auto* flow = definition ? std::get_if<FlowDefinition>(&definition->data) : nullptr;
    if (!flow) return {};
    const auto id = std::string(flow_id); const auto prefix = "flows/" + id; GraphTraces result;
    std::map<std::string,std::string,std::less<>> rows;
    paths(flow->trigger, "trigger", prefix + "/trigger", rows);
    if (flow->lifecycle.exit_when) paths(*flow->lifecycle.exit_when, "exit", prefix + "/exit_when", rows);
    const auto row = [&](const std::string& path) {
        const auto found = rows.find(wildcard_members(path)); return found == rows.end() ? std::string{} : found->second;
    };
    const auto inspect = [&](runtime::FlowPredicate predicate, const std::string& root) {
        try {
            const auto current = runtime::inspect_flow_predicate(program, checkpoint, flow_id, predicate);
            append(result, root, truth(current.trace.result), "Current predicate · " + binding(current.binding) + " · " + truth(current.trace.result));
            for (const auto& clause : current.trace.clauses) {
                const auto key = row(clause.path); if (key.empty()) continue;
                append(result, key, truth(clause.result), binding(clause.binding) + " · " + clause.fact + " · " + scalar(clause.actual) +
                    " compared with " + scalar(clause.expected) + " " + std::string(unit_name(clause.unit)) + " · " + truth(clause.result));
                append(result, key + "/fact", "observed", "Current actual · " + binding(clause.binding) + " · " + scalar(clause.actual));
                append(result, key + "/expression", "observed", "Current expected · " + binding(clause.binding) + " · " + scalar(clause.expected));
                for (const auto& port : {key + "@fact", key + "/fact@value"})
                    append(result, port, "observed", "Current actual · " + binding(clause.binding) + " · " + scalar(clause.actual));
                for (const auto& port : {key + "@compare", key + "/expression@value"})
                    append(result, port, "observed", "Current expected · " + binding(clause.binding) + " · " + scalar(clause.expected));
                for (const auto& port : {key + "@binding", key + "/fact@binding"}) append(result, port, "observed", "Resolved context · " + binding(clause.binding));
            }
            for (const auto& duration : current.trace.durations) {
                const auto key = row(duration.path); if (!key.empty()) append(result, key, truth(duration.result), binding(duration.binding) +
                    " · elapsed " + (duration.elapsed_days ? std::to_string(*duration.elapsed_days) : "unsampled") + "/" + std::to_string(duration.required_days) + " days · " + truth(duration.result));
            }
            for (const auto& selection : current.trace.selections) {
                const auto key = row(selection.path); if (!key.empty()) append(result, key, truth(selection.result), binding(selection.owner) +
                    " · " + selection.relation + " · " + (selection.minimum_matches ? std::to_string(*selection.minimum_matches) : "unknown") +
                    " matched of " + std::to_string(selection.members.size()) + " · " + truth(selection.result));
            }
        } catch (const std::exception& error) { result[root] = {"unknown", std::string{"Inspection error · "} + error.what()}; }
    };
    if (!flow->group) inspect(runtime::FlowPredicate::trigger, "trigger");
    if (flow->lifecycle.exit_when) inspect(runtime::FlowPredicate::exit_when, "exit");
    std::map<std::string,std::size_t,std::less<>> receipt_counts;
    for (const auto& receipt : checkpoint.runtime.receipts) if (receipt.flow_id == flow_id) {
        const auto node = "node/" + receipt.node_id; const auto key = node + "/effect/" + std::to_string(receipt.effect_index);
        const auto count = ++receipt_counts[key];
        result[key] = {"committed", std::to_string(count) + " actual receipt(s); latest " + core::iso_date(receipt.day) + " · " +
            binding(receipt.target) + " · " + receipt.operation + " · " + detail::scalar_text(receipt.before) + " → " +
            detail::scalar_text(receipt.after) + " · counterparty " + std::to_string(receipt.counterparty_delta.value()) + " cents"};
        result[node] = {"committed", "Contains actual committed effect receipts. See each effect's target and before/after values."};
        result[key + "@target"] = {"observed", "Actual committed target · " + binding(receipt.target) +
            (receipt.bound_variable.empty() ? "" : " · " + receipt.bound_variable)};
    }
    for (const auto& presentation : checkpoint.runtime.presentations) if (presentation.flow_id == flow_id && presentation.resolved) {
        const auto found = flow->nodes.find(presentation.node_id); if (found == flow->nodes.end()) continue;
        for (std::size_t i = 0; i < found->second.choices.size(); ++i) if (found->second.choices.at(i).id == presentation.selected_choice)
            result["node/" + presentation.node_id + "/choice/" + std::to_string(i)] = {"committed", "Actual selected choice · " + presentation.selected_choice};
    }
    for (const auto& draw : checkpoint.runtime.random_resolutions) if (draw.flow_id == flow_id) {
        const auto found = flow->nodes.find(draw.node_id); if (found == flow->nodes.end()) continue;
        for (std::size_t i = 0; i < found->second.branches.size(); ++i) if (found->second.branches.at(i).id == draw.branch_id)
            result["node/" + draw.node_id + "/branch/" + std::to_string(i)] = {"committed", "Actual resolved branch · " + draw.branch_id + " · " + core::iso_date(draw.day)};
    }
    if (operation) for (const auto& path : operation->paths) if (path.flow_id == flow_id) {
        const bool probe = synthetic || path.synthetic_cleanup;
        result["node/" + path.node_id] = {probe ? "synthetic" : path.committed ? "committed" : "candidate",
            (probe ? "Synthetic separate-copy operation · " : path.committed ? "Last real committed path · " : "Rejected candidate; no new effects committed · ") +
            binding(path.binding) + " · " + path.detail};
    }
    for (const auto& activation : checkpoint.runtime.activations) {
        if (activation.status == runtime::ActivationStatus::completed) continue;
        const auto context = "Owner " + std::to_string(activation.id) + " · current " + activation.current_flow + "/" + activation.current_node +
            " · " + binding(activation.binding) + (activation.due ? " · due " + core::iso_date(*activation.due) : "");
        if (activation.current_flow == flow_id) {
            result["node/" + activation.current_node] = {activation.status == runtime::ActivationStatus::continuous_active ? "active" : "waiting", context};
            result["flow"] = {activation.status == runtime::ActivationStatus::continuous_active ? "active" : "waiting",
                (activation.definition_id == flow_id ? "Actual owning activation. " : "Incoming subflow call; the helper's automatic trigger is not rechecked. ") + context};
        }
        if (activation.definition_id == flow_id && activation.status == runtime::ActivationStatus::continuous_active)
            result["lifecycle"] = {"active", "Natural continuous owner is active. " + context};
        for (const auto& frame : activation.frames) {
            if (frame.flow_id != flow_id) continue;
            if (frame.kind == runtime::FrameKind::sequence) {
                const auto sequence = flow->nodes.find(frame.sequence_node); if (sequence == flow->nodes.end()) continue;
                result["node/" + frame.sequence_node] = {"waiting", context + " · saved next child " + std::to_string(frame.next_child + 1) + " · return " + frame.return_node};
                result["node/" + frame.sequence_node + "@next"] = {"queued", "Continue only after all numbered children return. " + context};
                for (std::size_t i = 0; i < sequence->second.children.size(); ++i) {
                    const auto state = i >= frame.next_child ? "queued" : frame.next_child > 0 && i + 1 == frame.next_child ? "waiting" : "committed";
                    result["node/" + frame.sequence_node + "/child/" + std::to_string(i)] = {state, std::string{"Step "} + std::to_string(i + 1) +
                        (i >= frame.next_child ? " waits for the current child to return. " : " · saved continuation frame. ") + context};
                }
            }
            else {
                const auto call = flow->nodes.find(frame.sequence_node);
                if (call != flow->nodes.end() && call->second.kind == NodeKind::subflow) {
                    result["node/" + frame.sequence_node] = {"waiting", "Nested call has not returned. " + context};
                    result["node/" + frame.sequence_node + "@next"] = {"queued", "Continue only after the called flow returns. " + context};
                }
            }
            if (!frame.return_node.empty()) result["node/" + frame.return_node] = {"queued", "Saved return location; not executed while the child is waiting. " + context};
        }
    }
    return result;
}
} // namespace pgp::studio
