#include "form_builder.hpp"

#include <algorithm>
#include <array>

namespace pgp::studio::detail {
namespace {
struct ConditionPath {
    bool exit = false;
    std::vector<std::size_t> children;

    [[nodiscard]] std::string key() const {
        std::string result = exit ? "exit" : "trigger";
        for (const auto index : children) result += "/" + std::to_string(index);
        return result;
    }
    [[nodiscard]] Condition& edit(FlowDefinition& flow) const {
        auto* condition = exit ? &flow.lifecycle.exit_when.value() : &flow.trigger;
        for (const auto index : children) condition = &condition->children.at(index);
        return *condition;
    }
};
struct ConditionRow {
    const Condition& condition;
    ConditionPath path;
    Scope scope;
    bool inside_relation;
    const Condition* parent;
};
template<class Visit>
bool walk(const Condition& condition, const ConditionPath& path, Scope scope, bool inside_relation,
          const Condition* parent, const Registry& registry, const Visit& visit) {
    if (visit(ConditionRow{condition, path, scope, inside_relation, parent})) return true;
    const auto child_scope = condition.quantifier ? registry.relation(condition.quantifier->relation)->member_scope : scope;
    auto child = path;
    child.children.push_back(0);
    for (std::size_t index = 0; index < condition.children.size(); ++index) {
        child.children.back() = index;
        if (walk(condition.children[index], child, child_scope, inside_relation || condition.quantifier.has_value(),
                 &condition, registry, visit)) return true;
    }
    return false;
}
template<class Visit>
bool walk_flow(const FlowDefinition& flow, const Registry& registry, const Visit& visit) {
    if (walk(flow.trigger, {}, flow.scope, false, nullptr, registry, visit)) return true;
    return flow.lifecycle.exit_when && walk(*flow.lifecycle.exit_when, {true, {}}, flow.scope, false, nullptr, registry, visit);
}

struct ComparisonOption { Comparison comparison; const char* key; const char* label; };
constexpr std::array comparisons{
    ComparisonOption{Comparison::equal, "eq", "Equals"},
    ComparisonOption{Comparison::not_equal, "ne", "Does not equal"},
    ComparisonOption{Comparison::less, "lt", "Less than"},
    ComparisonOption{Comparison::less_equal, "lte", "At most"},
    ComparisonOption{Comparison::greater, "gt", "Greater than"},
    ComparisonOption{Comparison::greater_equal, "gte", "At least"}
};
std::string comparison_key(Comparison comparison) {
    return std::find_if(comparisons.begin(), comparisons.end(), [comparison](const auto& option) {
        return option.comparison == comparison;
    })->key;
}
Comparison parse_comparison(std::string_view key) {
    const auto option = std::find_if(comparisons.begin(), comparisons.end(), [key](const auto& entry) { return entry.key == key; });
    if (option == comparisons.end()) throw std::invalid_argument("Choose an available comparison.");
    return option->comparison;
}
std::vector<FormOption> comparison_choices(ValueType type) {
    std::vector<FormOption> options;
    for (const auto& option : comparisons) {
        options.push_back({option.key, option.label});
        if (type != ValueType::integer && option.comparison == Comparison::not_equal) break;
    }
    return options;
}
std::string boundary_label(Boundary boundary) {
    switch (boundary) {
    case Boundary::opening: return "Opening";
    case Boundary::prepared_morning: return "Prepared morning";
    case Boundary::work: return "Work";
    case Boundary::settlement: return "Settlement";
    case Boundary::committed_result: return "Committed result";
    }
    throw std::invalid_argument("Unknown fact timing.");
}
std::string fact_help(const FactMetadata& fact) {
    auto result = fact.help + "\nScope " + std::string(scope_name(fact.scope)) + " · " + boundary_label(fact.boundary);
    if (fact.type == ValueType::integer)
        result += "\nRange " + std::to_string(fact.minimum) + " through " + std::to_string(fact.maximum) + " " + unit_label(fact.unit);
    if (fact.visibility == Visibility::hidden_truth) result += "\nHidden from the player.";
    return result;
}
FormControl scalar_control(ValueType type) {
    if (type == ValueType::boolean) return FormControl::choice;
    return type == ValueType::integer ? FormControl::integer : FormControl::text;
}
std::vector<FormOption> relations(const Registry& registry, Scope scope) {
    std::vector<FormOption> options;
    for (const auto& [id, relation] : registry.relations())
        if (relation.owner_scope == scope) options.push_back({id, id, relation.help});
    return options;
}
std::string condition_kind(ConditionKind kind) {
    switch (kind) {
    case ConditionKind::clause: return "fact";
    case ConditionKind::all_of: return "all";
    case ConditionKind::any_of: return "any";
    case ConditionKind::not_condition: return "not";
    case ConditionKind::holds_for_days: return "duration";
    case ConditionKind::quantified: return "related";
    }
    throw std::invalid_argument("Unknown condition type.");
}
bool aggregate(ConditionKind kind) { return kind == ConditionKind::all_of || kind == ConditionKind::any_of; }
std::string condition_label(const Condition& condition) {
    switch (condition.kind) {
    case ConditionKind::clause: {
        const auto& clause = *condition.clause;
        const auto op = std::find_if(comparisons.begin(), comparisons.end(), [&](const auto& entry) {
            return entry.comparison == clause.comparison;
        });
        return clause.fact + " · " + op->label + " · " + (clause.expression ? "expression" : scalar_text(clause.value));
    }
    case ConditionKind::all_of: return "All of these conditions";
    case ConditionKind::any_of: return "Any of these conditions";
    case ConditionKind::not_condition: return "Negate condition";
    case ConditionKind::holds_for_days: return "Holds for " + std::to_string(condition.duration_days) + " days · " + condition.predicate_id;
    case ConditionKind::quantified: return "Related entities · " + condition.quantifier->relation;
    }
    throw std::invalid_argument("Unknown condition type.");
}
bool has_timer(const Condition& condition, std::string_view id) {
    return condition.predicate_id == id || std::any_of(condition.children.begin(), condition.children.end(), [&](const auto& child) {
        return has_timer(child, id);
    });
}
std::string next_timer_id(const FlowDefinition& flow) {
    auto id = std::string("duration");
    for (unsigned index = 2; has_timer(flow.trigger, id) || (flow.lifecycle.exit_when && has_timer(*flow.lifecycle.exit_when, id)); ++index)
        id = "duration." + std::to_string(index);
    return id;
}
void change_shape(FlowDefinition& flow, const ConditionPath& path, Scope scope, const Registry& registry,
                  std::string_view kind) {
    auto& condition = path.edit(flow);
    if (kind == condition_kind(condition.kind)) return;
    if (kind == "fact") {
        condition = default_condition(registry, scope);
    } else if (kind == "related") {
        const auto choices = relations(registry, scope);
        const auto& relation = *registry.relation(choices.at(0).value);
        Condition selected{ConditionKind::quantified, {}, {default_condition(registry, relation.member_scope)}};
        selected.quantifier = Quantifier{QuantifierKind::any, relation.id};
        condition = std::move(selected);
    } else if (kind == "duration") {
        const auto id = next_timer_id(flow);
        condition = Condition{ConditionKind::holds_for_days, {}, {condition}, id, 1};
    } else {
        const auto selected = kind == "all" ? ConditionKind::all_of : kind == "any" ? ConditionKind::any_of : ConditionKind::not_condition;
        if (aggregate(selected) && aggregate(condition.kind)) condition.kind = selected;
        else condition = Condition{selected, {}, {condition}};
    }
}
void clause_form(FormBuilder& builder, const ConditionRow& row) {
    const auto& clause = *row.condition.clause;
    const auto& registry = builder.draft.registry();
    const auto* fact = registry.fact(clause.fact);
    const auto path = row.path;
    builder.add("fact", "Fact", clause.fact, fact_help(*fact) + "\nChanging facts starts a new comparison using the selected fact's unit.",
                FormControl::choice, [path, &registry](FlowDefinition& flow, std::string_view id) {
        auto& target = *path.edit(flow).clause;
        if (target.fact == id) return;
        const auto& next = *registry.fact(id);
        target = {next.id, Comparison::equal, default_scalar(next), next.unit};
    }, fact_choices(registry, row.scope));
    builder.add("comparison", "Comparison", comparison_key(clause.comparison),
                "Boolean and text values support equality checks. Numeric facts also support ordering.",
                FormControl::choice, [path](FlowDefinition& flow, std::string_view value) {
        path.edit(flow).clause->comparison = parse_comparison(value);
    }, comparison_choices(fact->type));
    builder.add("operand", "Compare against", clause.expression ? "expression" : "literal",
                "A typed expression can use compatible facts or checked arithmetic. Its units must match the fact.",
                FormControl::choice, [path, fact](FlowDefinition& flow, std::string_view value) {
        auto& target = *path.edit(flow).clause;
        if (value == "expression" && !target.expression) {
            target.expression = Expression{ExpressionKind::literal, target.value, target.unit};
            target.value = std::int64_t{0};
        } else if (value == "literal" && target.expression) {
            target.value = default_scalar(*fact);
            target.expression.reset();
        }
    }, {{"literal", "Entered value"}, {"expression", "Typed expression"}});
    if (clause.expression) return;
    builder.add("value", "Value · " + unit_label(fact->unit), scalar_text(clause.value), fact_help(*fact),
                scalar_control(fact->type), [path, type = fact->type](FlowDefinition& flow, std::string_view value) {
        path.edit(flow).clause->value = parse_scalar(type, value);
    }, fact->type == ValueType::boolean ? boolean_choices() : std::vector<FormOption>{});
}
void relation_form(FormBuilder& builder, const ConditionRow& row) {
    const auto& registry = builder.draft.registry();
    const auto& condition = row.condition;
    const auto& selected = *condition.quantifier;
    const auto& metadata = *registry.relation(selected.relation);
    const auto path = row.path;
    builder.add("relation", "Related entities", selected.relation, metadata.help +
                "\nA different member scope starts a new member condition.", FormControl::choice,
                [path, &registry](FlowDefinition& flow, std::string_view value) {
        auto& target = path.edit(flow);
        const auto& before = *registry.relation(target.quantifier->relation);
        const auto& after = *registry.relation(value);
        if (before.member_scope != after.member_scope) target.children.front() = default_condition(registry, after.member_scope);
        if (target.quantifier->count && target.quantifier->count->value > after.maximum_members)
            throw std::invalid_argument("Lower the count threshold before changing to this smaller collection.");
        target.quantifier->relation = value;
    }, relations(registry, row.scope));
    const auto kind = selected.kind == QuantifierKind::any ? "any" : selected.kind == QuantifierKind::all ? "all" : "count";
    builder.add("matching", "Match", kind, "Members are evaluated in stable ID order. Any is false for an empty set. All is true for an empty set.",
                FormControl::choice, [path](FlowDefinition& flow, std::string_view value) {
        auto& target = *path.edit(flow).quantifier;
        if (value == "count") {
            target.kind = QuantifierKind::count;
            if (!target.count) target.count = CountComparison{Comparison::greater_equal, 1};
        } else {
            target.kind = value == "any" ? QuantifierKind::any : QuantifierKind::all;
            target.count.reset();
        }
    }, {{"any", "Any member"}, {"all", "All members"}, {"count", "Count matching members"}});
    if (!selected.count) return;
    builder.add("comparison", "Compare count", comparison_key(selected.count->comparison), {}, FormControl::choice,
                [path](FlowDefinition& flow, std::string_view value) {
        path.edit(flow).quantifier->count->comparison = parse_comparison(value);
    }, comparison_choices(ValueType::integer));
    builder.add("count", "Member count", std::to_string(selected.count->value),
                "Maximum " + std::to_string(metadata.maximum_members) + " members.", FormControl::integer,
                [path, maximum = static_cast<std::uint32_t>(metadata.maximum_members)](FlowDefinition& flow, std::string_view value) {
        path.edit(flow).quantifier->count->value = parse_count(value, 0, maximum);
    });
}
void edit_condition(FormBuilder& builder, const ConditionRow& row) {
    const auto& condition = row.condition;
    const auto& registry = builder.draft.registry();
    const auto path = row.path;
    builder.note("Scope", std::string(scope_name(row.scope)));
    std::vector<FormOption> kinds{{"fact", "Fact comparison"}, {"all", "All conditions"},
                                  {"any", "Any condition"}, {"not", "Negate condition"}};
    if (!row.inside_relation) kinds.push_back({"duration", "Holds for days"});
    if (!relations(registry, row.scope).empty()) kinds.push_back({"related", "Related entities"});
    builder.add("kind", "Condition type", condition_kind(condition.kind),
                "Groups, negation and duration keep the current condition as a child. Fact or related-entity selection replaces it.",
                FormControl::choice, [path, scope = row.scope, &registry](FlowDefinition& flow, std::string_view kind) {
        change_shape(flow, path, scope, registry, kind);
    }, std::move(kinds));
    if (condition.clause) clause_form(builder, row);
    if (condition.quantifier) relation_form(builder, row);
    if (condition.kind == ConditionKind::holds_for_days) {
        builder.add("timer", "Timer ID", condition.predicate_id, "Unique within this flow. Changing an existing ID may require migration when finalized.",
                    FormControl::text, [path](FlowDefinition& flow, std::string_view value) { path.edit(flow).predicate_id = value; });
        builder.add("days", "Elapsed days", std::to_string(condition.duration_days),
                    "First true on day D satisfies N days on D+N. A false or missing daily observation resets the duration.",
                    FormControl::integer, [path](FlowDefinition& flow, std::string_view value) {
            path.edit(flow).duration_days = parse_count(value, 1, 1'000'000);
        });
    }
    if (aggregate(condition.kind))
        builder.action("append", "Add condition", "Append another fact comparison to this group.", [path, scope = row.scope, &registry](FlowDefinition& flow, std::string_view) {
            path.edit(flow).children.push_back(default_condition(registry, scope));
        });
    if (!row.parent || row.parent->quantifier) return;
    auto parent = path;
    const auto index = parent.children.back();
    parent.children.pop_back();
    if (row.parent->children.size() == 1)
        builder.action("unwrap", "Remove parent wrapper", "Keep this condition and remove its enclosing wrapper.", [parent, index](FlowDefinition& flow, std::string_view) {
            auto& wrapper = parent.edit(flow);
            auto preserved = std::move(wrapper.children.at(index));
            wrapper = std::move(preserved);
        });
    else
        builder.action("remove", "Remove this condition", "Remove this branch from its group.", [parent, index](FlowDefinition& flow, std::string_view) {
            auto& children = parent.edit(flow).children;
            children.erase(children.begin() + static_cast<std::ptrdiff_t>(index));
        });
}

std::string expression_kind(ExpressionKind kind) {
    switch (kind) {
    case ExpressionKind::literal: return "literal";
    case ExpressionKind::fact: return "fact";
    case ExpressionKind::add: return "add";
    case ExpressionKind::subtract: return "subtract";
    case ExpressionKind::scale: return "scale";
    }
    throw std::invalid_argument("Unknown expression type.");
}
std::string expression_label(const Expression& expression) {
    switch (expression.kind) {
    case ExpressionKind::literal: return "Value · " + scalar_text(expression.value);
    case ExpressionKind::fact: return "Read fact · " + expression.fact;
    case ExpressionKind::add: return "Add operands";
    case ExpressionKind::subtract: return "Subtract right from left";
    case ExpressionKind::scale: return "Multiply by " + std::to_string(expression.numerator) + "/" + std::to_string(expression.denominator);
    }
    throw std::invalid_argument("Unknown expression type.");
}
std::vector<FormOption> expression_facts(const Registry& registry, const FactMetadata& type) {
    std::vector<FormOption> result;
    for (const auto& [id, fact] : registry.facts())
        if (fact.scope == type.scope && fact.type == type.type && fact.unit == type.unit) result.push_back({id, id, fact_help(fact)});
    return result;
}
void expression_fields(FormBuilder& builder, const Expression& expression, const FactMetadata& type, const ExpressionAccess& access) {
    const auto& registry = builder.draft.registry();
    auto facts = expression_facts(registry, type);
    std::vector<FormOption> kinds{{"literal", "Entered value"}};
    if (!facts.empty()) kinds.push_back({"fact", "Read a fact"});
    if (type.type == ValueType::integer) {
        kinds.push_back({"add", "Add"});
        kinds.push_back({"subtract", "Subtract"});
        kinds.push_back({"scale", "Multiply by a ratio"});
    }
    // A scoped variable read starts from that variable. New built-in facts must
    // not redirect an authored counter assignment to an unrelated game count.
    const auto current=type.default_value?std::find_if(facts.begin(),facts.end(),[&](const auto& option){return option.value==type.id;}):facts.end();
    const auto default_fact=current!=facts.end()?current->value:(facts.empty()?std::string{}:facts.front().value);
    const Expression literal{ExpressionKind::literal, default_scalar(type), type.unit};
    builder.add("kind", "Expression type", expression_kind(expression.kind),
                "Arithmetic keeps the current expression as its first operand. Entered values and fact reads replace it.",
                FormControl::choice, [access, literal, default_fact](FlowDefinition& flow, std::string_view value) {
        auto& target = access(flow);
        if (value == expression_kind(target.kind)) return;
        if (value == "literal") target = literal;
        else if (value == "fact") target = Expression{ExpressionKind::fact, std::int64_t{0}, Unit::none, default_fact};
        else if (value == "scale") {
            Expression wrapper{ExpressionKind::scale};
            wrapper.children.push_back(std::move(target));
            wrapper.numerator = 1;
            target = std::move(wrapper);
        } else {
            const auto kind = value == "add" ? ExpressionKind::add : ExpressionKind::subtract;
            if (target.kind == ExpressionKind::add || target.kind == ExpressionKind::subtract) target.kind = kind;
            else {
                Expression right{ExpressionKind::literal, std::int64_t{0}, literal.unit};
                target = Expression{kind, std::int64_t{0}, Unit::none, {}, {target, std::move(right)}};
            }
        }
    }, std::move(kinds));
    if (expression.kind == ExpressionKind::literal)
        builder.add("value", "Value · " + unit_label(type.unit), scalar_text(expression.value),
                    "This operand uses the same value type and units as its destination.", scalar_control(type.type),
                    [access, scalar = type.type](FlowDefinition& flow, std::string_view value) { access(flow).value = parse_scalar(scalar, value); },
                    type.type == ValueType::boolean ? boolean_choices() : std::vector<FormOption>{});
    if (expression.kind == ExpressionKind::fact)
        builder.add("fact", "Compatible fact", expression.fact, fact_help(*registry.fact(expression.fact)), FormControl::choice,
                    [access](FlowDefinition& flow, std::string_view value) { access(flow).fact = value; }, std::move(facts));
    if (expression.kind == ExpressionKind::scale) {
        builder.add("numerator", "Numerator", std::to_string(expression.numerator),
                    "Scale uses nonnegative ratios and rounds the exact product once.", FormControl::integer,
                    [access](FlowDefinition& flow, std::string_view value) { access(flow).numerator = parse_count(value, 0, 1'000'000); });
        builder.add("denominator", "Denominator", std::to_string(expression.denominator),
                    "Denominator must be positive.", FormControl::integer,
                    [access](FlowDefinition& flow, std::string_view value) { access(flow).denominator = parse_count(value, 1, 1'000'000); });
    }
    for (std::size_t index = 0; index < expression.children.size(); ++index) {
        const auto key = std::to_string(index);
        builder.action("unwrap/" + key, "Keep only operand " + std::to_string(index + 1),
                       "Replace this expression with the selected operand.", [access, index](FlowDefinition& flow, std::string_view) {
            auto& target = access(flow);
            auto operand = std::move(target.children.at(index));
            target = std::move(operand);
        });
    }
}
} // namespace

void append_expression_outline(std::vector<OutlineRow>& rows, const Expression& expression,
                               std::string key, std::size_t depth) {
    rows.push_back({key, expression_label(expression), depth});
    for (std::size_t index = 0; index < expression.children.size(); ++index)
        append_expression_outline(rows, expression.children[index], key + "/" + std::to_string(index), depth + 1);
}
bool build_expression_form(FormBuilder& builder, std::string_view row, const Expression& expression,
                           std::string_view key, const FactMetadata& type, ExpressionAccess access) {
    if (row == key) {
        expression_fields(builder, expression, type, access);
        return true;
    }
    if (!row.starts_with(std::string(key) + "/")) return false;
    for (std::size_t index = 0; index < expression.children.size(); ++index)
        if (build_expression_form(builder, row, expression.children[index], std::string(key) + "/" + std::to_string(index), type,
                                  [access, index](FlowDefinition& flow) -> Expression& { return access(flow).children.at(index); })) return true;
    return false;
}
void append_trigger_outline(std::vector<OutlineRow>& rows, const DraftDocument& draft, const FlowDefinition& flow) {
    (void)walk_flow(flow, draft.registry(), [&](const ConditionRow& row) {
        const auto& path = row.path;
        const auto key = path.key();
        const auto title = path.children.empty() ? (path.exit ? "Exit · " : "Trigger · ") : "";
        rows.push_back({key, title + condition_label(row.condition), path.children.size()});
        if (row.condition.clause && row.condition.clause->expression)
            append_expression_outline(rows, *row.condition.clause->expression, key + "/expression", path.children.size() + 1);
        return false;
    });
}
bool build_trigger_form(FormBuilder& builder, std::string_view key) {
    return walk_flow(builder.flow, builder.draft.registry(), [&](const ConditionRow& row) {
        const auto row_key = row.path.key();
        if (row_key == key) {
            edit_condition(builder, row);
            return true;
        }
        const auto& clause = row.condition.clause;
        if (!clause || !clause->expression) return false;
        return build_expression_form(builder, key, *clause->expression, row_key + "/expression", *builder.draft.registry().fact(clause->fact),
                                     [path = row.path](FlowDefinition& flow) -> Expression& { return path.edit(flow).clause->expression.value(); });
    });
}
} // namespace pgp::studio::detail
