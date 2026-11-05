#include "graph_internal.hpp"
#include <pgp/persistence/storage.hpp>
#include <algorithm>
#include <set>

namespace pgp::studio {
namespace {
using namespace content;
using namespace detail;
using ConditionAccess = std::function<Condition&(FlowDefinition&)>;
using ValueWrite = std::function<void(FlowDefinition&, Expression)>;
struct Builder {
    GraphData& data;
    const DraftDocument& document;
    const FlowDefinition& flow;
    std::map<std::string, std::size_t, std::less<>> indices;
    std::map<std::string, std::string, std::less<>> labels;

    explicit Builder(GraphData& graph) : data{graph}, document{*graph.document}, flow{find_flow(document, graph.flow)} {
        for (const auto& row : flow_outline(document, data.flow)) labels[row.key] = row.label;
    }
    void node(std::string key, std::string row, std::string title, std::string detail, std::string kind, Scope scope) {
        if (!indices.emplace(key, data.nodes.size()).second) throw std::logic_error("Duplicate graph node projection.");
        GraphNode card; card.key = std::move(key); card.row = std::move(row); card.title = std::move(title);
        card.detail = std::move(detail); card.kind = std::move(kind); card.scope = scope; data.nodes.push_back(std::move(card));
    }
    std::string port(const std::string& owner, std::string suffix, std::string label, PortType type, Scope scope,
        bool output, GraphEndpoint binding, Unit unit = Unit::none, ValueType value_type = ValueType::integer,
        std::string help = {}, std::string reference_kind = {}, bool connectable = true) {
        auto& card = data.nodes.at(indices.at(owner));
        const auto id = owner + "@" + suffix;
        const auto offset = static_cast<std::int32_t>(132 + card.ports.size() * 40);
        card.ports.push_back(id);
        card.height = ((static_cast<std::int32_t>(128 + card.ports.size() * 40) + 15) / 16) * 16;
        data.port_indices.emplace(id, data.ports.size());
        data.ports.push_back({id, owner, std::move(label), binding.row, std::move(help), type, scope, unit, value_type,
            std::move(reference_kind), output, connectable, offset});
        data.bindings.emplace(id, std::move(binding));
        return id;
    }
    void edge(std::string from, std::string to, std::string label, std::string kind, std::string row = {}) {
        data.edges.push_back({std::move(from), std::move(to), std::move(label), std::move(kind), std::move(row)});
    }
    std::string scope(Scope selected) {
        const auto id = "binding/" + std::string(scope_name(selected));
        if (!indices.contains(id)) {
            node(id, "flow", "Bound " + std::string(scope_name(selected)),
                "Flow self or the current related-entity member. The test trace resolves actual IDs.", "scope", selected);
            GraphEndpoint binding; binding.kind = GraphBinding::scope; binding.row = "flow";
            binding.value = Expression{ExpressionKind::literal, std::string{"self"}, Unit::none};
            (void)port(id, "self", "Self binding", PortType::scope, selected, true, std::move(binding));
        }
        return id + "@self";
    }
    void scoped_input(const std::string& owner, const std::string& row, Scope selected) {
        GraphEndpoint binding; binding.row = row;
        const auto input = port(owner, "binding", "Bound context", PortType::scope, selected, false, std::move(binding),
            Unit::none, ValueType::reference, "The owning flow/selector supplies this context; edit scope through its form.", {}, false);
        edge(scope(selected), input, std::string(scope_name(selected)), "scope", row);
    }
    std::string value(const Expression& expression, const FactMetadata& contract, std::string key, std::string row,
        ValueWrite write, std::string path) {
        std::string title;
        switch (expression.kind) {
        case ExpressionKind::literal: title = "Value · " + scalar_text(expression.value); break;
        case ExpressionKind::fact: title = "Read · " + expression.fact; break;
        case ExpressionKind::add: title = "Add operands"; break;
        case ExpressionKind::subtract: title = "Left minus right"; break;
        case ExpressionKind::scale: title = "Multiply by " + std::to_string(expression.numerator) + "/" + std::to_string(expression.denominator); break;
        }
        node(key, row, title, unit_label(contract.unit) + " · " + std::string(scope_name(contract.scope)), "value", contract.scope);
        if (expression.kind == ExpressionKind::fact) scoped_input(key, row, contract.scope);
        GraphEndpoint output; output.kind = GraphBinding::value; output.row = row; output.tree_path = path;
        output.value = expression; output.value_target = write; output.value_contract = contract;
        const auto out = port(key, "value", "Value", PortType::value, contract.scope, true, std::move(output), contract.unit, contract.type,
            contract.help);
        for (std::size_t i = 0; i < expression.children.size(); ++i) {
            const auto child_key = key + "/" + std::to_string(i);
            const auto child_row = row + "/" + std::to_string(i);
            const auto child_path = path + "/" + std::to_string(i);
            // Updating a child copies its parent expression, then writes the
            // complete owned value through the same document transaction.
            ValueWrite child_write = [write, parent = expression, i](FlowDefinition& target, Expression next) mutable {
                auto copy = parent; copy.children.at(i) = std::move(next); write(target, std::move(copy));
            };
            GraphEndpoint input; input.kind = GraphBinding::value; input.row = child_row; input.tree_path = child_path;
            input.value = expression.children.at(i); input.value_target = child_write; input.value_contract = contract;
            const auto into = port(key, "operand/" + std::to_string(i), "Operand " + std::to_string(i + 1), PortType::value,
                contract.scope, false, std::move(input), contract.unit, contract.type, "Owned typed operand; a connection copies the selected value tree.");
            edge(value(expression.children.at(i), contract, child_key, child_row, child_write, child_path), into, "Operand " + std::to_string(i + 1), "value", child_row);
        }
        return out;
    }
    std::string condition(const Condition& current, const std::string& key, Scope selected, ConditionAccess access) {
        node(key, key, labels.at(key), "Boolean · " + std::string(scope_name(selected)), "condition", selected);
        scoped_input(key, key, selected);
        GraphEndpoint result; result.kind = GraphBinding::condition; result.row = key; result.tree_path = key;
        result.condition = current; result.condition_target = access;
        const auto out = port(key, "result", "True / false", PortType::condition, selected, true, std::move(result), Unit::boolean, ValueType::boolean,
            "Eligibility result. False starts no activation; use a named NOT group when negation is intended.");
        if (current.clause) {
            const auto& clause = *current.clause; const auto& metadata = *document.registry().fact(clause.fact);
            GraphEndpoint fact; fact.kind = GraphBinding::primary_fact; fact.row = key; fact.field = "fact";
            const auto into = port(key, "fact", "Compared fact", PortType::value, selected, false, std::move(fact), metadata.unit, metadata.type,
                "A registered fact with this exact scope/type/unit; the shared form initializes its new comparison.");
            const auto fact_key = key + "/fact";
            node(fact_key, key, clause.fact, metadata.help, "fact", selected); scoped_input(fact_key, key, selected);
            GraphEndpoint read; read.kind = GraphBinding::value; read.row = key;
            read.value = Expression{ExpressionKind::fact, std::int64_t{0}, Unit::none, clause.fact};
            const auto fact_out = port(fact_key, "value", "Fact value", PortType::value, selected, true, std::move(read), metadata.unit, metadata.type, metadata.help);
            edge(fact_out, into, "Read fact", "value", key);
            const auto expression = clause.expression.value_or(Expression{ExpressionKind::literal, clause.value, clause.unit});
            ValueWrite write = [access](FlowDefinition& target, Expression next) {
                auto& destination = *access(target).clause;
                if (next.kind == ExpressionKind::literal) { destination.value = std::move(next.value); destination.expression.reset(); }
                else { destination.expression = std::move(next); destination.value = std::int64_t{0}; }
            };
            GraphEndpoint rhs; rhs.kind = GraphBinding::value; rhs.row = clause.expression ? key + "/expression" : key;
            rhs.tree_path = key + "/expression"; rhs.value = expression; rhs.value_target = write; rhs.value_contract = metadata;
            const auto rhs_in = port(key, "compare", "Compare with", PortType::value, selected, false, std::move(rhs), metadata.unit, metadata.type,
                "Compatible entered value, fact or checked expression. Units are never coerced.");
            edge(value(expression, metadata, key + "/expression", clause.expression ? key + "/expression" : key, write, key + "/expression"), rhs_in, "Comparison operand", "value", key);
        }
        const auto member = current.quantifier ? document.registry().relation(current.quantifier->relation)->member_scope : selected;
        for (std::size_t i = 0; i < current.children.size(); ++i) {
            const auto path = key + "/" + std::to_string(i);
            ConditionAccess child = [access, i](FlowDefinition& target) -> Condition& { return access(target).children.at(i); };
            GraphEndpoint input; input.kind = GraphBinding::condition; input.row = path; input.tree_path = path; input.condition_target = child;
            const auto into = port(key, "condition/" + std::to_string(i), "Condition " + std::to_string(i + 1), PortType::condition, member, false,
                std::move(input), Unit::boolean, ValueType::boolean, "Owned predicate branch. A connection copies the predicate, repairing duplicated timer IDs.");
            edge(condition(current.children.at(i), path, member, child), into, std::to_string(i + 1), "condition", path);
        }
        return out;
    }
    void execution_link(const std::string& owner, std::string suffix, std::string label, const std::string& row,
        const std::string& field, const std::string& target) {
        GraphEndpoint binding; binding.kind = GraphBinding::execution; binding.row = row; binding.field = field;
        const auto out = port(owner, std::move(suffix), label, PortType::execution, flow.scope, true, std::move(binding), Unit::none, ValueType::integer,
            "Finish this ordered route before its continuation. Retargeting removes only paths no longer reachable from entry/exit; Undo restores them.");
        edge(out, "node/" + target + "@in", std::move(label), "execution", row);
    }
    void effect(const std::string& node_id, const Effect& effect, std::size_t index) {
        const auto row = "node/" + node_id + "/effect/" + std::to_string(index);
        const auto& metadata = *document.registry().effect(effect.operation);
        node(row, row, "Effect " + std::to_string(index + 1) + " · " + effect.operation, metadata.help, "effect", effect.target_scope);
        GraphEndpoint member; member.row = row;
        const auto input = port(row, "ordered", "Atomic item " + std::to_string(index + 1), PortType::execution, flow.scope, false, std::move(member),
            Unit::none, ValueType::integer, "Ordered inside one effect node, not an independent execution route. Reorder with its form.", {}, false);
        GraphEndpoint owned; owned.row = row;
        const auto out = port("node/" + node_id, "effect/" + std::to_string(index), "Effect " + std::to_string(index + 1), PortType::execution, flow.scope, true,
            std::move(owned), Unit::none, ValueType::integer, "Atomic ordered effect member.", {}, false);
        edge(out, input, "Atomic " + std::to_string(index + 1), "owned", row);
        GraphEndpoint target; target.kind = GraphBinding::scope; target.row = row; target.field = "target";
        const auto into = port(row, "target", "Target · " + effect.target, PortType::scope, effect.target_scope, false, std::move(target), Unit::none, ValueType::reference,
            "Explicit typed self target; the shared scope validator remains authoritative.");
        edge(scope(effect.target_scope), into, "Target", "scope", row);
        if (effect.value_expression) {
            const auto variable = std::get<std::string>(effect.arguments.at("variable")); const auto& type = *document.registry().fact(variable);
            ValueWrite write = [node_id, index](FlowDefinition& target_flow, Expression next) { target_flow.nodes.at(node_id).effects.at(index).value_expression = std::move(next); };
            GraphEndpoint assign; assign.kind = GraphBinding::value; assign.row = row + "/expression"; assign.tree_path = assign.row;
            assign.value = *effect.value_expression; assign.value_target = write; assign.value_contract = type;
            const auto value_in = port(row, "assign", "Assign · " + variable, PortType::value, type.scope, false, std::move(assign), type.unit, type.type, type.help);
            edge(value(*effect.value_expression, type, row + "/expression", row + "/expression", write, row + "/expression"), value_in, "Assigned value", "value", row);
            return;
        }
        for (const auto& argument : metadata.arguments) {
            const auto found = effect.arguments.find(argument.name); if (found == effect.arguments.end()) continue;
            GraphEndpoint input_value; input_value.kind = GraphBinding::argument; input_value.row = row; input_value.field = "argument/" + argument.name;
            const auto value_in = port(row, "argument/" + argument.name, argument.name, PortType::value, effect.target_scope, false,
                std::move(input_value), argument.unit, argument.type, argument.help, argument.reference_kind);
            const auto constant_key = row + "/argument/" + argument.name;
            node(constant_key, row, argument.name + " · " + scalar_text(found->second), unit_label(argument.unit) + " · " + argument.help, "literal", effect.target_scope);
            GraphEndpoint literal; literal.kind = GraphBinding::value; literal.row = row; literal.field = "argument/" + argument.name;
            literal.value = Expression{ExpressionKind::literal, found->second, argument.unit};
            const auto literal_out = port(constant_key, "value", "Entered value", PortType::value, effect.target_scope, true, std::move(literal), argument.unit,
                argument.type, "This operation accepts a literal. General fact expressions are available only where the registry/runtime implements them.", argument.reference_kind);
            edge(literal_out, value_in, "Entered value", "value", row);
        }
    }
    void build() {
        node("flow", "flow", data.flow, "Automatic root: false starts none, true runs entry. Calls use incoming context. " + std::string(lifecycle_label(flow.lifecycle.mode)), "flow", flow.scope);
        scoped_input("flow", "flow", flow.scope);
        GraphEndpoint trigger; trigger.kind = GraphBinding::condition; trigger.row = "trigger"; trigger.tree_path = "trigger";
        trigger.condition_target = [](FlowDefinition& target) -> Condition& { return target.trigger; };
        const auto trigger_in = port("flow", "trigger", "Eligibility", PortType::condition, flow.scope, false, std::move(trigger), Unit::boolean, ValueType::boolean);
        edge(condition(flow.trigger, "trigger", flow.scope, [](FlowDefinition& target) -> Condition& { return target.trigger; }), trigger_in, "True / false", "condition", "trigger");
        execution_link("flow", "entry", "When true", "flow", "entry", flow.entry_node);
        node("lifecycle", "lifecycle", "Lifecycle · " + std::string(lifecycle_label(flow.lifecycle.mode)),
            "Timing and presentation are independent. An end returns to its enclosing sequence/subflow.", "lifecycle", flow.scope);
        if (flow.lifecycle.exit_when) {
            GraphEndpoint exit; exit.kind = GraphBinding::condition; exit.row = "exit"; exit.tree_path = "exit";
            exit.condition_target = [](FlowDefinition& target) -> Condition& { return *target.lifecycle.exit_when; };
            const auto exit_in = port("lifecycle", "condition", "Natural exit", PortType::condition, flow.scope, false, std::move(exit), Unit::boolean, ValueType::boolean);
            edge(condition(*flow.lifecycle.exit_when, "exit", flow.scope, [](FlowDefinition& target) -> Condition& { return *target.lifecycle.exit_when; }), exit_in, "Exit true", "condition", "exit");
            execution_link("lifecycle", "exit", "On exit", "lifecycle", "exit_node", flow.exit_node);
        }
        for (const auto& [id, current] : flow.nodes) {
            const auto key = "node/" + id;
            std::string detail = std::string(scope_name(flow.scope));
            if (current.kind == NodeKind::delay) detail += " · wait " + std::to_string(current.delay_days) + " simulated days";
            if (current.kind == NodeKind::sequence) detail += " · numbered children return in order, then continue";
            if (current.kind == NodeKind::subflow) detail += " · call " + current.subflow_id + ", then return";
            if (current.kind == NodeKind::random) detail += " · weighted " + current.random_stream + " draw on actual execution";
            if (current.kind == NodeKind::end) detail += " · return to saved frame; continuous owner persists until exit";
            if (current.kind == NodeKind::effect) detail += " · atomic ordered effects, then continue";
            node(key, key, id + " · " + std::string(node_label(current.kind)), detail, "execution", flow.scope);
            GraphEndpoint input; input.kind = GraphBinding::execution; input.row = key; input.node_id = id;
            (void)port(key, "in", "Execute", PortType::execution, flow.scope, false, std::move(input));
            if (!current.next.empty()) execution_link(key, "next", "Continue", key, "next", current.next);
            for (std::size_t i = 0; i < current.children.size(); ++i) execution_link(key, "child/" + std::to_string(i), "Step " + std::to_string(i + 1), key + "/child/" + std::to_string(i), "next", current.children.at(i));
            for (std::size_t i = 0; i < current.choices.size(); ++i) execution_link(key, "choice/" + std::to_string(i), "Choice · " + current.choices.at(i).id, key + "/choice/" + std::to_string(i), "next", current.choices.at(i).next);
            for (std::size_t i = 0; i < current.branches.size(); ++i) execution_link(key, "branch/" + std::to_string(i), "Weight " + std::to_string(current.branches.at(i).weight) + " · " + current.branches.at(i).id, key + "/branch/" + std::to_string(i), "next", current.branches.at(i).next);
            for (std::size_t i = 0; i < current.effects.size(); ++i) effect(id, current.effects.at(i), i);
        }
        if (data.nodes.size() > 4096 || data.ports.size() > 16384) throw std::invalid_argument("The current flow exceeds the bounded canvas projection; its shared outline remains available.");
    }
};
}

FlowGraph FlowGraph::open(const DraftDocument& document, std::string_view flow, const CanvasPositions& positions) {
    auto data = std::make_shared<detail::GraphData>(); data->document = &document; data->revision = document.edit_revision(); data->flow = flow;
    Builder{*data}.build(); data->automatic = detail::arrange_graph(data->nodes, data->ports, data->edges);
    for (auto& node : data->nodes) {
        const auto stored = positions.find(node.key); node.position = stored == positions.end() ? data->automatic.at(node.key) : stored->second;
        if (node.position.x < 0 || node.position.y < 0 || node.position.x > maximum_canvas_coordinate || node.position.y > maximum_canvas_coordinate ||
            node.position.x % canvas_grid != 0 || node.position.y % canvas_grid != 0) throw std::invalid_argument("Canvas positions must use the bounded integer grid.");
    }
    return FlowGraph{std::move(data)};
}
const std::vector<GraphNode>& FlowGraph::nodes() const { return data_->nodes; }
const std::vector<GraphPort>& FlowGraph::ports() const { return data_->ports; }
const std::vector<GraphEdge>& FlowGraph::edges() const { return data_->edges; }
CanvasPositions FlowGraph::automatic_layout() const { return data_->automatic; }
} // namespace pgp::studio

namespace pgp::studio::detail {
const GraphPort& GraphData::port(std::string_view id) const {
    const auto found = port_indices.find(id);
    if (found == port_indices.end()) throw std::invalid_argument("Choose a current typed canvas port.");
    return ports.at(found->second);
}
void GraphData::verify(const DraftDocument& target) const {
    if (&target != document || revision != target.edit_revision()) throw persistence::PersistenceError("conflict", "The document changed. Select the current canvas ports again.");
}
CanvasPositions arrange_graph(const std::vector<GraphNode>& nodes, const std::vector<GraphPort>& ports, const std::vector<GraphEdge>& edges) {
    std::map<std::string, std::string, std::less<>> owner; for (const auto& port : ports) owner[port.id] = port.node;
    std::map<std::string, std::vector<std::string>, std::less<>> outgoing;
    std::map<std::string, std::size_t, std::less<>> incoming, rank;
    std::set<std::string> ready, remaining;
    for (const auto& node : nodes) { incoming[node.key] = 0; rank[node.key] = 0; remaining.insert(node.key); }
    for (const auto& edge : edges) {
        const auto& from = owner.at(edge.from); const auto& to = owner.at(edge.to);
        outgoing[from].push_back(to); ++incoming[to];
    }
    for (const auto& [id, count] : incoming) if (count == 0) ready.insert(id);
    while (!remaining.empty()) {
        // Valid positive-delay bounded loops need a deterministic back-edge
        // break for layout only. This never alters their execution links.
        const auto id = ready.empty() ? *remaining.begin() : *ready.begin(); ready.erase(id); remaining.erase(id);
        for (const auto& target : outgoing[id]) {
            if (!remaining.contains(target)) continue;
            rank[target] = std::max(rank[target], std::min<std::size_t>(rank[id] + 1, 128));
            if (incoming[target] > 0 && --incoming[target] == 0) ready.insert(target);
        }
    }
    std::map<std::size_t, std::int32_t> next_y; CanvasPositions result;
    std::map<std::string, std::int32_t, std::less<>> heights; for (const auto& node : nodes) heights[node.key] = node.height;
    for (const auto& [id, level] : rank) {
        auto& y = next_y[level]; if (y == 0) y = 64;
        if (y > maximum_canvas_coordinate - heights.at(id) - 48) throw std::invalid_argument("Canvas layout exceeds its bounded coordinate space.");
        result[id] = {64 + static_cast<std::int32_t>(level) * 400, y}; y += heights.at(id) + 48;
    }
    return result;
}
} // namespace pgp::studio::detail
