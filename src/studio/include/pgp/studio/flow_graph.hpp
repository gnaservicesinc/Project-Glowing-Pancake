#pragma once

#include <pgp/studio/flow_forms.hpp>
#include <pgp/studio/layout.hpp>
#include <pgp/runtime/runtime.hpp>
#include <memory>

namespace pgp::studio {
struct GraphPort {
    std::string id, node, label, row, help;
    content::PortType type = content::PortType::execution;
    content::Scope scope = content::Scope::company;
    content::Unit unit = content::Unit::none;
    content::ValueType value_type = content::ValueType::integer;
    std::string reference_kind;
    bool output = false, connectable = true;
    std::int32_t offset_y = 0;
};
struct GraphNode {
    std::string key, row, title, detail, kind;
    content::Scope scope = content::Scope::company;
    std::vector<std::string> ports;
    CanvasPoint position;
    std::int32_t width = 336, height = 112;
};
struct GraphEdge {
    std::string from, to, label, kind, row;
};
struct GraphTrace {
    std::string state, detail;
};
using GraphTraces = std::map<std::string, GraphTrace, std::less<>>;
namespace detail { struct GraphData; }

// A revision-bound projection of the one typed document, never a second saved
// logic representation. Connections/canvas creation use the same forms and
// compiler boundary as outline edits. Owned predicate/value inputs copy trees.
class FlowGraph {
public:
    [[nodiscard]] static FlowGraph open(const DraftDocument& document, std::string_view flow,
        const CanvasPositions& positions = {});
    [[nodiscard]] const std::vector<GraphNode>& nodes() const;
    [[nodiscard]] const std::vector<GraphPort>& ports() const;
    [[nodiscard]] const std::vector<GraphEdge>& edges() const;
    [[nodiscard]] CanvasPositions automatic_layout() const;
    [[nodiscard]] std::vector<FormOption> palette(std::string_view port) const;
    bool connect(DraftDocument& document, std::string_view output, std::string_view input) const;
    bool insert(DraftDocument& document, std::string_view port, std::string_view type) const;
private:
    explicit FlowGraph(std::shared_ptr<detail::GraphData> data) : data_{std::move(data)} {}
    std::shared_ptr<detail::GraphData> data_;
};

// Observations from actual copied runtime state only. Never simulates a future
// branch, mutates a checkpoint or draws RNG. Includes saved sibling/return waits.
[[nodiscard]] GraphTraces graph_traces(const runtime::CompiledProgram& program,
    std::string_view flow, const runtime::Checkpoint& checkpoint,
    const runtime::RuntimeResult* last_operation = nullptr, bool synthetic = false);
} // namespace pgp::studio
