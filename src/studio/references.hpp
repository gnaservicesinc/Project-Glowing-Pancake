#pragma once

#include <pgp/content/collection.hpp>

namespace pgp::studio::detail {

template<class Node, class Visit>
void visit_node_links(Node& node, const Visit& visit) {
    for (auto& child : node.children) visit(child);
    if (!node.next.empty()) visit(node.next);
    for (auto& choice : node.choices) visit(choice.next);
    for (auto& branch : node.branches) visit(branch.next);
}

} // namespace pgp::studio::detail
