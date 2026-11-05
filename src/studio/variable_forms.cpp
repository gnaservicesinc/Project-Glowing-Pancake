#include "form_builder.hpp"

#include <algorithm>
#include <limits>

namespace pgp::studio::detail {
namespace {
FactMetadata& find_variable(Collection& collection, std::string_view id) {
    const auto selected = std::find_if(collection.variables.begin(), collection.variables.end(), [id](const auto& variable) {
        return variable.id == id;
    });
    if (selected == collection.variables.end()) throw std::invalid_argument("Select a current variable.");
    return *selected;
}
void change_type(FactMetadata& variable, Unit unit) {
    variable.unit = unit;
    variable.ports = {{"scope", PortType::scope, variable.scope, Unit::none, false},
                      {"value", PortType::value, variable.scope, unit, true}};
    if (unit == Unit::boolean) {
        variable.type = ValueType::boolean;
        variable.minimum = 0;
        variable.maximum = 1;
        variable.default_value = false;
    } else if (unit == Unit::none) {
        variable.type = ValueType::text;
        variable.minimum = 0;
        variable.maximum = 0;
        variable.default_value = std::string{};
    } else {
        variable.type = ValueType::integer;
        variable.minimum = 0;
        variable.maximum = std::numeric_limits<std::int64_t>::max();
        if (unit == Unit::constant_2014_cents) variable.minimum = std::numeric_limits<std::int64_t>::min();
        if (unit == Unit::basis_point_delta) variable.minimum = -10'000;
        if (unit == Unit::basis_points || unit == Unit::basis_point_delta) variable.maximum = 10'000;
        variable.default_value = std::int64_t{0};
    }
}
std::vector<FormOption> variable_types() {
    std::vector<FormOption> options{{"boolean", "Boolean flag"}, {"none", "Text"}};
    for (unsigned index = static_cast<unsigned>(Unit::constant_2014_cents); index <= static_cast<unsigned>(Unit::count); ++index) {
        const auto unit = static_cast<Unit>(index);
        options.push_back({std::string(unit_name(unit)), "Integer · " + unit_label(unit)});
    }
    return options;
}
Unit parse_unit(std::string_view name) {
    for (unsigned index = 0; index <= static_cast<unsigned>(Unit::count); ++index) {
        const auto unit = static_cast<Unit>(index);
        if (unit_name(unit) == name) return unit;
    }
    throw std::invalid_argument("Choose a registered unit.");
}
void rename_uses(Expression& expression, std::string_view before, std::string_view after) {
    if (expression.kind == ExpressionKind::fact && expression.fact == before) expression.fact = after;
    for (auto& child : expression.children) rename_uses(child, before, after);
}
void rename_uses(Condition& condition, std::string_view before, std::string_view after) {
    if (condition.clause) {
        if (condition.clause->fact == before) condition.clause->fact = after;
        if (condition.clause->expression) rename_uses(*condition.clause->expression, before, after);
    }
    for (auto& child : condition.children) rename_uses(child, before, after);
}
void rename_uses(Collection& collection, std::string_view before, std::string_view after) {
    for (auto& [id, definition] : collection.definitions) {
        (void)id;
        auto* flow = std::get_if<FlowDefinition>(&definition.data);
        if (!flow) continue;
        rename_uses(flow->trigger, before, after);
        if (flow->lifecycle.exit_when) rename_uses(*flow->lifecycle.exit_when, before, after);
        for (auto& [node_id, node] : flow->nodes) {
            (void)node_id;
            for (auto& effect : node.effects) {
                if (effect.operation == "set_variable") {
                    auto& target = std::get<std::string>(effect.arguments.at("variable"));
                    if (target == before) target = after;
                }
                if (effect.value_expression) rename_uses(*effect.value_expression, before, after);
            }
        }
    }
}
} // namespace

void append_variable_outline(std::vector<OutlineRow>& rows, const DraftDocument& draft, Scope scope) {
    rows.push_back({"variables", "Scoped variables"});
    for (const auto& variable : draft.collection().variables)
        if (variable.scope == scope)
            rows.push_back({"variables/" + variable.id, variable.id + " · " + unit_label(variable.unit), 1});
}
void build_variable_form(FormBuilder& builder, std::string_view row) {
    const auto& registry = builder.draft.registry();
    const auto scope = builder.flow.scope;
    builder.note("Variable scope", std::string(scope_name(scope)), "A stored value belongs to this scope. Unset values read the declared default.");
    if (row == "variables") {
        builder.add_document("append", "Add scoped flag", {}, "Create a false boolean flag. Rename it and choose a type from its outline row.",
                             FormControl::button, [&registry, scope](Collection& collection, FlowDefinition&, std::string_view) {
            auto id = std::string("story.flag");
            for (unsigned suffix = 2; registry.fact(id); ++suffix) id = "story.flag." + std::to_string(suffix);
            FactMetadata variable{};
            variable.id = std::move(id);
            variable.scope = scope;
            variable.help = "Author-defined scoped value.";
            change_type(variable, Unit::boolean);
            collection.variables.push_back(std::move(variable));
        });
        return;
    }
    const auto id = std::string(row.substr(std::string_view("variables/").size()));
    const auto* selected = registry.fact(id);
    if (!selected || selected->scope != scope || !selected->default_value) throw std::invalid_argument("Choose a current scoped variable.");
    builder.add_document("id", "Variable ID", id, "Renaming updates the collection's typed fact reads, comparisons and assignment targets.",
                         FormControl::text, [id, &registry](Collection& collection, FlowDefinition&, std::string_view value) {
        if (id == value) return;
        if (!valid_id(value) || registry.fact(value)) throw std::invalid_argument("Choose an unused stable variable ID.");
        find_variable(collection, id).id = value;
        rename_uses(collection, id, value);
    });
    builder.add_document("type", "Value type and unit", std::string(unit_name(selected->unit)),
                         "Changing type initializes its range and default. Incompatible references must be removed first.",
                         FormControl::choice, [id](Collection& collection, FlowDefinition&, std::string_view value) {
        auto& variable = find_variable(collection, id);
        const auto unit = parse_unit(value);
        if (variable.unit != unit) change_type(variable, unit);
    }, variable_types());
    if (selected->type == ValueType::integer) {
        builder.add_document("minimum", "Minimum", std::to_string(selected->minimum), "Inclusive lower bound in the selected unit.",
                             FormControl::integer, [id](Collection& collection, FlowDefinition&, std::string_view value) {
            find_variable(collection, id).minimum = std::get<std::int64_t>(parse_scalar(ValueType::integer, value));
        });
        builder.add_document("maximum", "Maximum", std::to_string(selected->maximum), "Inclusive upper bound in the selected unit.",
                             FormControl::integer, [id](Collection& collection, FlowDefinition&, std::string_view value) {
            find_variable(collection, id).maximum = std::get<std::int64_t>(parse_scalar(ValueType::integer, value));
        });
    }
    const auto control = selected->type == ValueType::boolean ? FormControl::choice :
        selected->type == ValueType::integer ? FormControl::integer : FormControl::text;
    builder.add_document("default", "Default · " + unit_label(selected->unit), scalar_text(*selected->default_value),
                         "Value before any committed assignment.", control,
                         [id, type = selected->type](Collection& collection, FlowDefinition&, std::string_view value) {
        find_variable(collection, id).default_value = parse_scalar(type, value);
    }, selected->type == ValueType::boolean ? boolean_choices() : std::vector<FormOption>{});
    builder.add_document("visibility", "Player visibility", selected->visibility == Visibility::player_known ? "visible" : "hidden",
                         "Hidden values can be used by the simulation. Player explanations redact their truth.",
                         FormControl::choice, [id](Collection& collection, FlowDefinition&, std::string_view value) {
        find_variable(collection, id).visibility = value == "visible" ? Visibility::player_known : Visibility::hidden_truth;
    }, {{"visible", "Player known"}, {"hidden", "Hidden truth"}});
    builder.add_document("help", "Author help", selected->help, "Describe what the value measures and when to use it.",
                         FormControl::text, [id](Collection& collection, FlowDefinition&, std::string_view value) {
        find_variable(collection, id).help = value;
    });
    builder.add_document("remove", "Remove variable", {}, "The compiler refuses to remove a variable with remaining references.",
                         FormControl::button, [id](Collection& collection, FlowDefinition&, std::string_view) {
        std::erase_if(collection.variables, [&](const auto& variable) { return variable.id == id; });
    });
}
} // namespace pgp::studio::detail
