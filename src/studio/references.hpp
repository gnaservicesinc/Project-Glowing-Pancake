#pragma once

#include <pgp/content/collection.hpp>

namespace pgp::studio::detail {

template<class Definition, class Visit>
void visit_definition_references(Definition& definition, const content::Registry& registry, const Visit& visit) {
    if (auto* scenario = std::get_if<content::ScenarioDefinition>(&definition.data)) {
        for (auto& target : scenario->research) visit(target);
        for (auto& target : scenario->flows) visit(target);
        if (scenario->lab) {
            for (auto& recipe:scenario->lab->recipes) for (auto& target:recipe.requires_research) visit(target);
            for (auto& evaluation:scenario->lab->evaluations) for (auto& target:evaluation.requires_research) visit(target);
        }
    } else if (auto* research = std::get_if<content::ResearchDefinition>(&definition.data)) {
        for (auto& target : research->all_of) visit(target);
        for (auto& target : research->any_of) visit(target);
    } else if (auto* flow = std::get_if<content::FlowDefinition>(&definition.data)) {
        for (auto& [id, node] : flow->nodes) {
            (void)id;
            if (node.kind == content::NodeKind::subflow) visit(node.subflow_id);
            for (auto& effect : node.effects)
                for (const auto& argument : registry.effect(effect.operation)->arguments) {
                    if (argument.type != content::ValueType::reference) continue;
                    const auto found = effect.arguments.find(argument.name);
                    if (found != effect.arguments.end()) visit(std::get<std::string>(found->second));
                }
        }
    }
}

template<class Node, class Visit>
void visit_node_links(Node& node, const Visit& visit) {
    for (auto& child : node.children) visit(child);
    if (!node.next.empty()) visit(node.next);
    for (auto& choice : node.choices) visit(choice.next);
    for (auto& branch : node.branches) visit(branch.next);
}

} // namespace pgp::studio::detail
