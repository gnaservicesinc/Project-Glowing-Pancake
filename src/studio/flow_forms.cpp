#include "form_builder.hpp"
#include "graph_forms.hpp"
#include "references.hpp"

#include <pgp/core/integer_codec.hpp>

#include <algorithm>
#include <cctype>
#include <set>
#include <type_traits>

namespace pgp::studio {
std::string readable_name(std::string_view name) {
    std::string result;
    bool initial = true;
    for (char character : name) {
        if (character == '.' || character == '_' || character == '/') {
            if (!result.empty() && result.back() != ' ') result.push_back(' ');
        } else {
            result.push_back(initial ? static_cast<char>(std::toupper(static_cast<unsigned char>(character))) : static_cast<char>(character));
            initial = false;
        }
    }
    return result;
}
std::string_view event_presentation(const content::FlowDefinition& flow) {
    std::vector<std::string_view> pending{flow.entry_node};
    std::set<std::string_view> seen;
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        if (!seen.insert(id).second) continue;
        const auto node = flow.nodes.find(id);
        if (node == flow.nodes.end()) continue;
        if (node->second.kind == content::NodeKind::dialogue || node->second.kind == content::NodeKind::news)
            return node->first;
        const auto start = pending.size();
        detail::visit_node_links(node->second, [&](const auto& next) { pending.push_back(next); });
        std::reverse(pending.begin() + static_cast<std::ptrdiff_t>(start), pending.end());
    }
    return {};
}
std::string event_name(const content::Collection& collection, const content::Definition& definition) {
    if (const auto* flow = std::get_if<content::FlowDefinition>(&definition.data)) {
        if (const auto* title = collection.message(definition.id + ".editor_title"); title && !title->empty())
            return *title;
        const auto entry = flow->nodes.find(event_presentation(*flow));
        if (entry != flow->nodes.end())
            if (const auto* title = collection.message(entry->second.title_key); title && !title->empty())
                return title->substr(0, title->find('\n'));
    }
    const auto start = definition.id.find('.');
    return readable_name(start == std::string::npos ? definition.id : definition.id.substr(start + 1));
}
namespace detail {
const FlowDefinition& find_flow(const DraftDocument& draft, std::string_view id) {
    const auto* definition = draft.collection().find(id);
    const auto* flow = definition ? std::get_if<FlowDefinition>(&definition->data) : nullptr;
    if (!flow) throw std::invalid_argument("Select an existing flow.");
    return *flow;
}
std::string scalar_text(const Scalar& value) {
    return std::visit([](const auto& scalar) -> std::string {
        using T = std::decay_t<decltype(scalar)>;
        if constexpr (std::is_same_v<T, std::string>) return scalar;
        else if constexpr (std::is_same_v<T, bool>) return scalar ? "true" : "false";
        else return std::to_string(scalar);
    }, value);
}
std::string unit_label(Unit unit) {
    switch (unit) {
    case Unit::constant_2014_cents: return "constant-2014 cents";
    case Unit::basis_points: return "basis points";
    case Unit::basis_point_delta: return "signed basis-point change";
    case Unit::milli_CU: return "milli-CU";
    case Unit::milli_CU_per_day: return "milli-CU per day";
    case Unit::calendar_months: return "calendar months";
    default: return std::string(unit_name(unit));
    }
}
Scalar parse_scalar(ValueType type, std::string_view value) {
    switch (type) {
    case ValueType::integer: return core::parse_json_i64(value);
    case ValueType::boolean:
        if (value == "true") return true;
        if (value == "false") return false;
        throw std::invalid_argument("Choose true or false.");
    case ValueType::reference:
    case ValueType::text: return std::string(value);
    }
    throw std::invalid_argument("Unsupported value type.");
}
std::uint32_t parse_count(std::string_view value, std::uint32_t minimum, std::uint32_t maximum) {
    const auto number = core::parse_json_u64(value);
    if (number < minimum || number > maximum)
        throw std::invalid_argument("Enter a whole number from " + std::to_string(minimum) + " to " + std::to_string(maximum) + ".");
    return static_cast<std::uint32_t>(number);
}
std::vector<FormOption> boolean_choices() { return {{"false", "False"}, {"true", "True"}}; }
std::vector<FormOption> fact_choices(const Registry& registry, Scope scope) {
    std::vector<FormOption> result;
    for (const auto& [id, fact] : registry.facts())
        if (fact.scope == scope) result.push_back({id, fact.default_value && !fact.help.empty() ? fact.help : readable_name(id), fact.help});
    return result;
}
Scalar default_scalar(const FactMetadata& fact) {
    if (fact.default_value && *fact.default_value != Scalar{std::string{}}) return *fact.default_value;
    switch (fact.type) {
    case ValueType::integer: return std::clamp<std::int64_t>(0, fact.minimum, fact.maximum);
    case ValueType::boolean: return true;
    case ValueType::text: return std::string("value");
    case ValueType::reference: break;
    }
    throw std::invalid_argument("Choose a fact with an editable value type.");
}
Condition default_condition(const Registry& registry, Scope scope) {
    const auto options = fact_choices(registry, scope);
    if (options.empty()) throw std::invalid_argument("This scope has no registered facts.");
    const auto& fact = *registry.fact(options.front().value);
    return {ConditionKind::clause, FactClause{fact.id, Comparison::equal, default_scalar(fact), fact.unit}, {}};
}
std::string_view node_label(NodeKind kind) {
    switch (kind) {
    case NodeKind::end: return "End";
    case NodeKind::effect: return "Effects";
    case NodeKind::news: return "News";
    case NodeKind::dialogue: return "Dialogue";
    case NodeKind::result: return "Result";
    case NodeKind::delay: return "Delay";
    case NodeKind::sequence: return "Sequence";
    case NodeKind::subflow: return "Subflow";
    case NodeKind::random: return "Weighted branches";
    }
    throw std::invalid_argument("Unknown node kind.");
}
std::string_view lifecycle_label(LifecycleMode mode) {
    switch (mode) {
    case LifecycleMode::once: return "Once";
    case LifecycleMode::recurring: return "Recurring";
    case LifecycleMode::interval: return "Interval";
    case LifecycleMode::scheduled: return "Scheduled";
    case LifecycleMode::continuous: return "Continuous";
    }
    throw std::invalid_argument("Unknown lifecycle.");
}
std::vector<FormOption> node_choices(const FlowDefinition& flow) {
    std::vector<FormOption> result;
    for (const auto& [id, node] : flow.nodes)
        result.push_back({id, std::string(node_label(node.kind)) + " · " + readable_name(id)});
    return result;
}
std::string available_node_id(const FlowDefinition& flow, std::string_view base) {
    auto id = std::string(base);
    for (unsigned suffix = 2; flow.nodes.contains(id); ++suffix)
        id = std::string(base) + "." + std::to_string(suffix);
    return id;
}
bool seeded_root(const FlowDefinition& flow) {
    const auto& entry = flow.nodes.at(flow.entry_node);
    return entry.kind == NodeKind::random && entry.random_stream != "narrative";
}
void FormBuilder::add_document(std::string key, std::string label, std::string value, std::string help,
                               FormControl control, DocumentChange change, std::vector<FormOption> options) {
    FormField field;
    field.key = std::move(key);
    field.label = std::move(label);
    field.value = std::move(value);
    field.help = std::move(help);
    field.control = control;
    field.options = std::move(options);
    field.change_ = std::move(change);
    form_.fields_.push_back(std::move(field));
}
void FormBuilder::add(std::string key, std::string label, std::string value, std::string help,
                      FormControl control, Change change, std::vector<FormOption> options) {
    DocumentChange document_change;
    if (change) document_change = [change = std::move(change)](Collection&, FlowDefinition& working, std::string_view value) {
        change(working, value);
    };
    add_document(std::move(key), std::move(label), std::move(value), std::move(help), control,
                 std::move(document_change), std::move(options));
}
void FormBuilder::note(std::string label, std::string value, std::string help) {
    add({}, std::move(label), std::move(value), std::move(help), FormControl::note);
}
void FormBuilder::action(std::string key, std::string label, std::string help, Change change) {
    add(std::move(key), std::move(label), {}, std::move(help), FormControl::button, std::move(change));
}
} // namespace detail

std::vector<OutlineRow> flow_outline(const DraftDocument& draft, std::string_view id) {
    const auto& flow = detail::find_flow(draft, id);
    std::vector<OutlineRow> rows{{"flow", "Event settings"}};
    if (flow.group) rows.push_back({"group", "What the group needs and returns"});
    else {
        rows.push_back({"lifecycle", "When it runs"});
        detail::append_trigger_outline(rows, draft, flow);
    }
    detail::append_variable_outline(rows, draft, flow.scope);
    detail::append_execution_outline(rows, draft, flow);
    return rows;
}
FlowForm FlowForm::open(const DraftDocument& draft, std::string_view flow, std::string_view row) {
    detail::FormBuilder builder{draft, flow};
    if (row == "flow") detail::build_flow_settings(builder);
    else if (row == "group") detail::build_group_form(builder);
    else if (row == "lifecycle") detail::build_lifecycle_form(builder);
    else if (row == "variables" || row.starts_with("variables/")) detail::build_variable_form(builder, row);
    else if (row == "execution" || row.starts_with("node/")) detail::build_execution_form(builder, row);
    else if (!detail::build_trigger_form(builder, row)) {
        throw std::invalid_argument("Choose a current outline row.");
    }
    return std::move(builder).finish();
}
bool FlowForm::apply(DraftDocument& draft, std::string_view key, std::string_view value) const {
    if (&draft != document_ || revision_ != draft.edit_revision())
        throw std::invalid_argument("The working document changed. Reopen the current form.");
    const auto field = std::find_if(fields_.begin(), fields_.end(), [&](const auto& item) { return item.key == key && item.change_; });
    if (field == fields_.end()) throw std::invalid_argument("This form field cannot be changed.");
    if (field->control == FormControl::choice &&
        std::none_of(field->options.begin(), field->options.end(), [&](const auto& option) { return option.value == value; }))
        throw std::invalid_argument("Choose an available option.");
    auto candidate = draft.collection();
    auto& flow = std::get<content::FlowDefinition>(candidate.definitions.at(flow_).data);
    field->change_(candidate, flow, value);
    return draft.replace(candidate);
}
} // namespace pgp::studio
