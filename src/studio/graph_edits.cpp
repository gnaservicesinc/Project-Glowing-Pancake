#include "graph_internal.hpp"
#include <algorithm>
#include <set>

namespace pgp::studio {
namespace {
using namespace content;
using namespace detail;
void collect_timers(const Condition& condition, const Condition* skip, std::set<std::string>& used) {
    if (&condition == skip) return;
    if (condition.kind == ConditionKind::holds_for_days) used.insert(condition.predicate_id);
    for (const auto& child : condition.children) collect_timers(child, skip, used);
}
void repair_timers(Condition& condition, std::set<std::string>& used, unsigned& next) {
    if (condition.kind == ConditionKind::holds_for_days) {
        if (used.contains(condition.predicate_id)) {
            do { condition.predicate_id = "canvas.duration." + std::to_string(next++); } while (used.contains(condition.predicate_id));
        }
        used.insert(condition.predicate_id);
    }
    for (auto& child : condition.children) repair_timers(child, used, next);
}
const FormField* field(const FlowForm& form, std::string_view key) {
    const auto found = std::find_if(form.fields().begin(), form.fields().end(), [&](const auto& item) { return item.key == key; });
    return found == form.fields().end() ? nullptr : &*found;
}
std::vector<FormOption> value_kinds(const DraftDocument& document, const FactMetadata& type) {
    std::vector<FormOption> result{{"value/literal", "Entered value"}};
    for (const auto& [id, fact] : document.registry().facts()) {
        (void)id;
        if (fact.scope == type.scope && fact.type == type.type && fact.unit == type.unit) { result.push_back({"value/fact", "Read a compatible fact"}); break; }
    }
    if (type.type == ValueType::integer) {
        result.push_back({"value/add", "Add operands"}); result.push_back({"value/subtract", "Subtract right from left"});
        result.push_back({"value/scale", "Multiply by a checked ratio"});
    }
    return result;
}
}

std::vector<FormOption> FlowGraph::palette(std::string_view selected) const {
    data_->verify(*data_->document);
    const auto& port = data_->port(selected); const auto& binding = data_->bindings.at(port.id);
    if (!port.connectable) return {};
    if (binding.kind == GraphBinding::execution && port.output) {
        const auto form = FlowForm::open(*data_->document, data_->flow, binding.row);
        const auto* insert = field(form, binding.field + "/insert"); std::vector<FormOption> result;
        if (insert) for (const auto& option : insert->options) if (!option.value.empty()) result.push_back({"insert/" + option.value, option.label, option.help});
        return result;
    }
    if (binding.kind == GraphBinding::condition && binding.condition_target) {
        const auto form = FlowForm::open(*data_->document, data_->flow, binding.row); std::vector<FormOption> result;
        if (const auto* shape = field(form, "kind")) for (const auto& option : shape->options)
            result.push_back({"condition/" + option.value, option.label, option.help});
        if (field(form, "append")) result.push_back({"append_condition", "Add another condition"});
        return result;
    }
    if (binding.kind == GraphBinding::value && binding.value_target && binding.value_contract) return value_kinds(*data_->document, *binding.value_contract);
    return {};
}
bool FlowGraph::connect(DraftDocument& document, std::string_view output_id, std::string_view input_id) const {
    data_->verify(document);
    const auto& output = data_->port(output_id); const auto& input = data_->port(input_id);
    if (!output.output || input.output) throw std::invalid_argument("Connect an output to an input; directions are explicit.");
    if (!output.connectable || !input.connectable) throw std::invalid_argument("This owned binding/order is structural. Select its box and use the shared form to edit it.");
    if (output.type != input.type || output.scope != input.scope) throw std::invalid_argument("Port kind/scope mismatch. Execution, conditions, values and entity bindings cannot be coerced.");
    if (output.type == PortType::value && (output.unit != input.unit || output.value_type != input.value_type || output.reference_kind != input.reference_kind))
        throw std::invalid_argument("Port value type, unit or reference family does not match. No conversion was made.");
    const auto& from = data_->bindings.at(output.id); const auto& to = data_->bindings.at(input.id);
    if (output.type == PortType::execution) {
        if (from.kind != GraphBinding::execution || to.kind != GraphBinding::execution || to.node_id.empty()) throw std::invalid_argument("Choose an actual execution route and node input.");
        return FlowForm::open(document, data_->flow, from.row).apply(document, from.field, to.node_id);
    }
    if (!from.tree_path.empty() && !to.tree_path.empty()) {
        if (from.tree_path == to.tree_path) return false;
        if (to.tree_path.starts_with(from.tree_path + "/")) throw std::invalid_argument("An owned predicate/value cannot be connected into one of its own descendants.");
    }
    if (output.type == PortType::condition) {
        if (!from.condition || !to.condition_target) throw std::invalid_argument("Choose an owned predicate input.");
        auto candidate = document.collection(); auto& flow = std::get<FlowDefinition>(candidate.definitions.at(data_->flow).data);
        auto& target = to.condition_target(flow); auto copy = *from.condition; std::set<std::string> used;
        collect_timers(flow.trigger, &target, used);
        if (flow.lifecycle.exit_when) collect_timers(*flow.lifecycle.exit_when, &target, used);
        unsigned next = 1; repair_timers(copy, used, next); target = std::move(copy);
        return document.replace(candidate);
    }
    if (output.type == PortType::scope) {
        if (!from.value || to.kind != GraphBinding::scope || to.field != "target") throw std::invalid_argument("Choose a supported explicit effect target.");
        return FlowForm::open(document, data_->flow, to.row).apply(document, to.field, std::get<std::string>(from.value->value));
    }
    if (!from.value) throw std::invalid_argument("This output has no supported typed value.");
    if (to.kind == GraphBinding::primary_fact) {
        if (from.value->kind != ExpressionKind::fact) throw std::invalid_argument("A comparison's left operand must be a registered fact; entered values belong on Compare with.");
        return FlowForm::open(document, data_->flow, to.row).apply(document, "fact", from.value->fact);
    }
    if (to.kind == GraphBinding::argument) {
        if (from.value->kind != ExpressionKind::literal) throw std::invalid_argument("This operation accepts an entered value. The registry does not support a live fact/expression for this argument.");
        return FlowForm::open(document, data_->flow, to.row).apply(document, to.field, scalar_text(from.value->value));
    }
    if (!to.value_target) throw std::invalid_argument("This input cannot own a value expression.");
    auto candidate = document.collection(); auto& flow = std::get<FlowDefinition>(candidate.definitions.at(data_->flow).data);
    to.value_target(flow, *from.value); return document.replace(candidate);
}
bool FlowGraph::insert(DraftDocument& document, std::string_view port_id, std::string_view type) const {
    data_->verify(document);
    const auto choices = palette(port_id);
    if (std::none_of(choices.begin(), choices.end(), [&](const auto& option) { return option.value == type; })) throw std::invalid_argument("Choose a currently supported palette item for this typed port.");
    const auto& port = data_->port(port_id); const auto& binding = data_->bindings.at(port.id);
    if (type.starts_with("insert/")) return FlowForm::open(document, data_->flow, binding.row).apply(document, binding.field + "/insert", type.substr(7));
    if (type.starts_with("condition/")) return FlowForm::open(document, data_->flow, binding.row).apply(document, "kind", type.substr(10));
    if (type == "append_condition") return FlowForm::open(document, data_->flow, binding.row).apply(document, "append", {});
    const auto& contract = binding.value_contract.value(); auto expression = binding.value.value();
    const auto shape = type.substr(6);
    const auto kind = shape == "literal" ? ExpressionKind::literal : shape == "fact" ? ExpressionKind::fact : shape == "add" ? ExpressionKind::add : shape == "subtract" ? ExpressionKind::subtract : ExpressionKind::scale;
    if (kind == expression.kind && (kind == ExpressionKind::literal || kind == ExpressionKind::fact)) return false;
    const Expression literal{ExpressionKind::literal, default_scalar(contract), contract.unit};
    if (kind == ExpressionKind::literal) expression = literal;
    else if (kind == ExpressionKind::fact) {
        std::string fact;
        for (const auto& [id, metadata] : document.registry().facts())
            if (metadata.scope == contract.scope && metadata.type == contract.type && metadata.unit == contract.unit) {
                if (fact.empty() || id == contract.id) fact = id;
                if (id == contract.id) break;
            }
        expression = Expression{ExpressionKind::fact, std::int64_t{0}, Unit::none, std::move(fact)};
    } else {
        Expression wrapper{kind}; wrapper.children.push_back(std::move(expression));
        if (kind == ExpressionKind::scale) { wrapper.numerator = 1; wrapper.denominator = 1; }
        else wrapper.children.push_back(literal);
        expression = std::move(wrapper);
    }
    auto candidate = document.collection(); auto& flow = std::get<FlowDefinition>(candidate.definitions.at(data_->flow).data);
    binding.value_target(flow, std::move(expression)); return document.replace(candidate);
}
} // namespace pgp::studio
