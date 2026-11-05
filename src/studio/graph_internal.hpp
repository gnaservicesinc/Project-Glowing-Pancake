#pragma once
#include <pgp/studio/flow_graph.hpp>
#include "form_builder.hpp"

namespace pgp::studio::detail {
enum class GraphBinding { structural, execution, condition, value, primary_fact, argument, scope };
struct GraphEndpoint {
    GraphBinding kind = GraphBinding::structural;
    std::string row, field, node_id, tree_path;
    std::optional<content::Condition> condition;
    std::optional<content::Expression> value;
    std::function<content::Condition&(content::FlowDefinition&)> condition_target;
    std::function<void(content::FlowDefinition&,content::Expression)> value_target;
    std::optional<content::FactMetadata> value_contract;
};
struct GraphData {
    const DraftDocument* document;
    std::uint64_t revision;
    std::string flow;
    std::vector<GraphNode> nodes;
    std::vector<GraphPort> ports;
    std::vector<GraphEdge> edges;
    std::map<std::string, std::size_t, std::less<>> port_indices;
    std::map<std::string, GraphEndpoint, std::less<>> bindings;
    CanvasPositions automatic;
    void verify(const DraftDocument& target) const;
    [[nodiscard]] const GraphPort& port(std::string_view id) const;
};
[[nodiscard]] CanvasPositions arrange_graph(const std::vector<GraphNode>& nodes,
    const std::vector<GraphPort>& ports, const std::vector<GraphEdge>& edges);
} // namespace pgp::studio::detail
