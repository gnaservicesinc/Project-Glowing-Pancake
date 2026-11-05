#include "graph_forms.hpp"

#include <algorithm>

namespace pgp::studio::detail {
namespace {
std::vector<FormOption> variables(const Registry& registry, Scope scope) {
    std::vector<FormOption> options;
    for (const auto& [id, fact] : registry.facts())
        if (fact.scope == scope && fact.default_value) options.push_back({id, id, fact.help});
    return options;
}
std::vector<FormOption> argument_choices(const Collection& collection, const ArgumentMetadata& argument) {
    if (argument.type == ValueType::boolean) return boolean_choices();
    std::vector<FormOption> options;
    if (argument.type == ValueType::reference) {
        for (const auto& [id, definition] : collection.definitions)
            if (definition_kind(definition) == argument.reference_kind) options.push_back({id, id});
    } else {
        for (const auto& choice : argument.choices) options.push_back({choice, choice});
    }
    return options;
}
Scalar argument_default(const ArgumentMetadata& argument, const std::vector<FormOption>& options) {
    if (!options.empty()) return parse_scalar(argument.type, options.front().value);
    if (argument.type == ValueType::integer) return std::clamp<std::int64_t>(0, argument.minimum, argument.maximum);
    if (argument.type == ValueType::reference) throw std::invalid_argument("No compatible definition is available for this effect.");
    return argument.name;
}
void initialize_assignment(Effect& effect, const FactMetadata& variable) {
    effect.arguments["variable"] = variable.id;
    effect.value_expression = Expression{ExpressionKind::literal, *variable.default_value, variable.unit};
}
} // namespace

std::vector<FormOption> effect_choices(const DraftDocument& draft, Scope scope) {
    std::vector<FormOption> result;
    for (const auto& [id, effect] : draft.registry().effects()) {
        if (std::find(effect.target_scopes.begin(), effect.target_scopes.end(), scope) == effect.target_scopes.end())
            continue;
        if (id == "set_variable" && variables(draft.registry(), scope).empty()) continue;
        const auto missing = std::any_of(effect.arguments.begin(), effect.arguments.end(), [&](const auto& argument) {
            return argument.required && argument.type == ValueType::reference && argument_choices(draft.collection(), argument).empty();
        });
        if (!missing) result.push_back({id, id, effect.help});
    }
    return result;
}
Effect new_effect(const DraftDocument& draft, Scope scope, std::string_view operation) {
    const auto& metadata = *draft.registry().effect(operation);
    Effect result{metadata.id, "self", scope, {}};
    if (operation == "set_variable") {
        const auto choices = variables(draft.registry(), scope);
        if (choices.empty()) throw std::invalid_argument("Declare a writable variable for this scope first.");
        initialize_assignment(result, *draft.registry().fact(choices.front().value));
        return result;
    }
    for (const auto& argument : metadata.arguments)
        if (argument.required)
            result.arguments.emplace(argument.name, argument_default(argument, argument_choices(draft.collection(), argument)));
    return result;
}
void build_effect_fields(FormBuilder& builder, const Effect& effect, EffectAccess access) {
    const auto& draft = builder.draft;
    const auto& registry = draft.registry();
    const auto& metadata = *registry.effect(effect.operation);
    builder.add("operation", "Effect operation", effect.operation, metadata.help +
                "\nChanging operation initializes the new arguments.", FormControl::choice,
                [access, &draft](FlowDefinition& flow, std::string_view value) {
        auto& target = access(flow);
        if (target.operation != value) target = new_effect(draft, flow.scope, value);
    }, effect_choices(draft, builder.flow.scope));
    builder.add("target", "Explicit target", effect.target,
                "This foundation operation binds to the flow's " + std::string(scope_name(builder.flow.scope)) + " scope.",
                FormControl::choice, [access](FlowDefinition& flow, std::string_view value) {
        auto& target = access(flow);
        target.target = value;
        target.target_scope = flow.scope;
    }, {{"self", "Self · " + std::string(scope_name(builder.flow.scope))}});
    if (effect.operation == "set_variable") {
        const auto& id = std::get<std::string>(effect.arguments.at("variable"));
        builder.add("variable", "Writable variable", id, registry.fact(id)->help +
                    "\nChanging variables resets the assigned expression to the new variable's default.", FormControl::choice,
                    [access, &registry](FlowDefinition& flow, std::string_view value) {
            auto& target = access(flow);
            if (std::get<std::string>(target.arguments.at("variable")) != value)
                initialize_assignment(target, *registry.fact(value));
        }, variables(registry, builder.flow.scope));
        return;
    }
    for (const auto& argument : metadata.arguments) {
        const auto found = effect.arguments.find(argument.name);
        auto options = argument_choices(draft.collection(), argument);
        const auto value = found == effect.arguments.end() ? std::string{} : scalar_text(found->second);
        auto label = argument.name;
        std::replace(label.begin(), label.end(), '_', ' ');
        auto help = argument.help;
        if (argument.type == ValueType::integer) {
            label += " · " + unit_label(argument.unit);
            help += "\nRange " + std::to_string(argument.minimum) + " through " + std::to_string(argument.maximum) + ".";
        }
        const auto control = !options.empty() ? FormControl::choice :
            argument.type == ValueType::integer ? FormControl::integer : FormControl::text;
        builder.add("argument/" + argument.name, std::move(label), value, std::move(help), control,
                    [access, key = argument.name, type = argument.type](FlowDefinition& flow, std::string_view input) {
            access(flow).arguments[key] = parse_scalar(type, input);
        }, std::move(options));
    }
}
} // namespace pgp::studio::detail
