#include <pgp/studio/flow_graph.hpp>
#include <pgp/studio/flow_probe.hpp>
#include <pgp/persistence/storage.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <iostream>

namespace {
using namespace pgp; namespace fs = std::filesystem; using Json = nlohmann::json;
int checks = 0;
void check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }
std::string read(const fs::path& file) { return persistence::read_bounded(file, 4U * 1024U * 1024U); }
void put(const fs::path& file, std::string_view bytes) { fs::create_directories(file.parent_path()); std::ofstream out{file, std::ios::binary}; out << bytes; out.close(); if (!out) throw std::runtime_error("Cannot write canvas fixture"); }
struct Scratch {
    fs::path root = fs::path{PGP_CANVAS_SCRATCH} / ("canvas-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() {
        fs::create_directories(root / ".git"); put(root / "CMakeLists.txt", "# engineering fixture\n");
        put(root / "src/content/collection.cpp", "// engineering fixture\n");
        put(root / "project-versions.json", read(PGP_CANVAS_VERSIONS));
        put(root / "data/runtime/foundation.json", read(PGP_CANVAS_FIXTURE));
    }
    ~Scratch() { std::error_code error; fs::remove_all(root, error); }
};
template<class Action> void refused(Action action, const char* why) {
    bool rejected = false; try { action(); } catch (const std::exception&) { rejected = true; } check(rejected, why);
}
constexpr auto flow_id = "flow.canvas";
void edit(studio::DraftDocument& doc, std::string_view row, std::string_view field, std::string_view value = {}) {
    (void)studio::FlowForm::open(doc, flow_id, row).apply(doc, field, value);
}
void insert(studio::DraftDocument& doc, std::string_view port, std::string_view type) {
    check(studio::FlowGraph::open(doc, flow_id).insert(doc, port, type), "Expected a canvas insertion");
}
const content::FlowDefinition& flow(const studio::DraftDocument& doc) { return std::get<content::FlowDefinition>(doc.collection().definitions.at(flow_id).data); }
void create(studio::DraftDocument& doc) {
    check(studio::create_flow(doc, flow_id, "company"), "Create the canvas flow");
    edit(doc, "trigger", "fact", "company.is_operating");
}
void projection_and_typed_edits() {
    Scratch scratch; studio::DraftDocument doc{scratch.root, "data/runtime/foundation.json"};
    const auto original = doc.program().canonical_content_bytes;
    for (const auto& [id, definition] : doc.collection().definitions) if (std::holds_alternative<content::FlowDefinition>(definition.data)) {
        const auto graph = studio::FlowGraph::open(doc, id);
        check(!graph.nodes().empty() && !graph.ports().empty() && !graph.edges().empty(), "Every existing lifecycle needs a graph projection");
        const auto again = studio::FlowGraph::open(doc, id);
        check(graph.automatic_layout() == again.automatic_layout(), "Default graph layout is deterministic");
    }
    check(doc.program().canonical_content_bytes == original && !doc.dirty(), "Opening graphs mutated runtime content");
    create(doc);
    insert(doc, "flow@entry", "insert/effect");
    edit(doc, "node/effect/effect/0", "operation", "adjust_cash"); edit(doc, "node/effect/effect/0", "argument/amount_cents", "-250");
    const auto graph = studio::FlowGraph::open(doc, flow_id); const auto stable = doc.program().canonical_content_bytes;
    refused([&] { (void)graph.connect(doc, "trigger/expression@value", "node/effect@in"); }, "A boolean value cannot become execution");
    refused([&] { (void)graph.connect(doc, "trigger/expression@value", "node/effect/effect/0@argument/amount_cents"); }, "Boolean cannot become money");
    check(doc.program().canonical_content_bytes == stable, "A rejected graph connection changed the document");
    edit(doc, "trigger", "fact", "company.cash_cents"); edit(doc, "trigger", "comparison", "gte");
    refused([&] { (void)graph.insert(doc, "flow@entry", "insert/news"); }, "A stale graph edit was accepted");
    const auto money = studio::FlowGraph::open(doc, flow_id);
    refused([&] { (void)money.connect(doc, "trigger/fact@value", "node/effect/effect/0@argument/amount_cents"); }, "A literal-only effect silently accepted a live fact expression");
    check(money.connect(doc, "trigger/expression@value", "node/effect/effect/0@argument/amount_cents"), "A compatible literal connection failed");
    check(std::get<std::int64_t>(flow(doc).nodes.at("effect").effects.at(0).arguments.at("amount_cents")) == 0, "Graph literal connection did not update the shared form value");
    check(doc.undo() && std::get<std::int64_t>(flow(doc).nodes.at("effect").effects.at(0).arguments.at("amount_cents")) == -250, "Graph edits are not in the shared undo history");
    insert(doc, "trigger/expression@value", "value/add");
    check(flow(doc).trigger.clause->expression->kind == content::ExpressionKind::add, "Canvas arithmetic did not edit the canonical expression tree");
    const auto arithmetic = studio::FlowGraph::open(doc, flow_id);
    refused([&] { (void)arithmetic.connect(doc, "trigger/expression@value", "trigger/expression@operand/0"); }, "An expression was connected into its descendant");
    insert(doc, "trigger/expression/0@value", "value/fact");
    edit(doc, "trigger", "kind", "all"); edit(doc, "trigger", "append");
    edit(doc, "trigger/0", "kind", "duration");
    const auto predicates = studio::FlowGraph::open(doc, flow_id);
    check(predicates.connect(doc, "trigger/0@result", "trigger@condition/1"), "A compatible owned condition connection failed");
    check(flow(doc).trigger.children.at(0).predicate_id != flow(doc).trigger.children.at(1).predicate_id, "Copied duration predicates reused a history ID");
    const auto conditions = studio::FlowGraph::open(doc, flow_id);
    refused([&] { (void)conditions.connect(doc, "trigger@result", "trigger@condition/0"); }, "A condition was connected into its descendant");
    edit(doc, "trigger", "append"); edit(doc, "trigger/2", "kind", "related");
    const auto related = studio::FlowGraph::open(doc, flow_id);
    const auto relation = flow(doc).trigger.children.at(2).quantifier->relation;
    const auto member_scope = doc.registry().relation(relation)->member_scope;
    refused([&] { (void)related.connect(doc, "binding/" + std::string(content::scope_name(member_scope)) + "@self", "node/effect/effect/0@target"); }, "A related member binding silently became company self");
    doc.save("canvas.json"); studio::DraftDocument reopened{scratch.root, "data/drafts/canvas.json"};
    check(reopened.program().canonical_content_bytes == doc.program().canonical_content_bytes, "Graph/forms save and reopen changed semantics");
    const auto before_layout = reopened.program().canonical_content_bytes; const auto positions = studio::FlowGraph::open(reopened, flow_id).automatic_layout();
    studio::LayoutDocument layout{scratch.root, flow_id};
    check(!layout.dirty() && !fs::exists(scratch.root / "authoring/layouts"), "Opening a graph created tracked layout data");
    check(layout.replace(positions), "Explicit auto-layout did not create a layout edit"); layout.save();
    check(studio::LayoutDocument{scratch.root, flow_id}.positions() == positions, "Saved integer node positions did not reopen exactly");
    studio::save_canvas_view(scratch.root, reopened.relative_file(), flow_id, {137, -64, 1250, "trigger/0"});
    check(studio::load_canvas_view(scratch.root, reopened.relative_file(), flow_id) == studio::CanvasView{137, -64, 1250, "trigger/0"}, "Local viewport/selection did not roundtrip");
    check(reopened.program().canonical_content_bytes == before_layout && !reopened.dirty(), "Layout/viewport changes altered content identity or revisions");
}
void execution_and_waits() {
    Scratch scratch; studio::DraftDocument doc{scratch.root, "data/runtime/foundation.json"}; create(doc);
    insert(doc, "flow@entry", "insert/sequence"); insert(doc, "node/sequence@child/0", "insert/effect");
    edit(doc, "node/effect/effect/0", "argument/value", "canvas_first");
    insert(doc, "node/effect@next", "insert/delay"); edit(doc, "node/delay", "days", "2");
    edit(doc, "node/sequence", "append"); insert(doc, "node/sequence@child/1", "insert/news");
    insert(doc, "node/sequence@next", "insert/dialogue");
    const auto stable = doc.program().canonical_content_bytes;
    const auto graph = studio::FlowGraph::open(doc, flow_id);
    refused([&] { (void)graph.connect(doc, "node/effect@next", "node/effect@in"); }, "An immediate execution cycle was accepted");
    refused([&] { (void)graph.connect(doc, "node/sequence@child/0", "node/news@in"); }, "A cross-sibling jump was accepted");
    check(doc.program().canonical_content_bytes == stable, "A rejected execution wire changed prior valid paths");
    std::string scenario; for (const auto& [id, d] : doc.collection().definitions) if (std::holds_alternative<content::ScenarioDefinition>(d.data)) { scenario = id; break; }
    const auto& setup = std::get<content::ScenarioDefinition>(doc.collection().definitions.at(scenario).data);
    const auto headquarters = setup.headquarters_choices.front();
    studio::FlowProbe probe{doc.program(), {scenario, headquarters, flow_id, 42}};
    const auto operation = probe.prepare(); check(operation.trace.accepted(), "Canvas-authored execution did not prepare");
    const auto checkpoint = probe.state();
    const auto trace = studio::graph_traces(doc.program(), flow_id, checkpoint, &operation.trace);
    check(trace.at("node/delay").state == "waiting" && trace.at("node/sequence/child/1").state == "queued", "Saved sequence wait did not identify the pending sibling");
    check(trace.at("node/dialogue").state == "queued", "The saved return boundary was not shown as queued");
    check(trace.at("node/sequence@next").state == "queued", "The sequence continuation port implied early execution");
    check(probe.state() == checkpoint, "Graph trace inspection mutated state/RNG");
    probe.reload(); check(probe.state() == checkpoint, "Canvas wait did not reload exactly");
    check(probe.submit(core::AdvanceDay{}).trace.accepted() && probe.submit(core::AdvanceDay{}).trace.accepted(), "Canvas-authored wait did not advance");
    check(probe.state().runtime.presentations.size() == 2, "The waiting sibling/news and dialogue did not execute in order");
    // Retargeting drops only the abandoned path, through the shared form edit.
    const auto before = doc.program().canonical_content_bytes;
    check(studio::FlowGraph::open(doc, flow_id).connect(doc, "node/effect@next", "node/child_end@in"), "Retargeting a route could not safely remove its abandoned delay");
    check(!flow(doc).nodes.contains("delay") && doc.undo() && doc.program().canonical_content_bytes == before, "Retarget/undo lost another route or failed to restore the graph");
    insert(doc, "flow@entry", "insert/random");
    studio::FlowProbe random{doc.program(), {scenario, headquarters, flow_id, 42}};
    const auto resolved = random.prepare(); const auto after_draw = random.state();
    check(resolved.trace.accepted() && after_draw.runtime.random_resolutions.size() == 1, "The canvas weighted branch did not make one actual saved draw");
    (void)studio::graph_traces(doc.program(), flow_id, after_draw, &resolved.trace);
    check(random.state() == after_draw, "Graph tracing drew from an authoritative random stream");
    random.reload(); check(random.state() == after_draw, "A canvas-authored random branch rerolled on reload");
    check(doc.undo(), "Restore the sequence before its nested-call check");
    constexpr auto helper = "flow.canvas.helper";
    check(studio::create_flow(doc, helper, "company"), "Create a referenced canvas helper");
    check(studio::FlowForm::open(doc, helper, "flow").apply(doc, "entry/insert", "delay"), "Give the helper a real delayed return");
    insert(doc, "node/sequence@child/1", "insert/subflow/flow.canvas.helper");
    studio::FlowProbe nested{doc.program(), {scenario, headquarters, flow_id, 42}};
    check(nested.prepare().trace.accepted() && nested.submit(core::AdvanceDay{}).trace.accepted(), "The nested canvas fixture did not reach its first wait");
    const auto nested_step = nested.submit(core::AdvanceDay{}); const auto nested_state = nested.state();
    check(nested_step.trace.accepted(), "The canvas helper did not enter through its incoming call");
    const auto parent_trace = studio::graph_traces(doc.program(), flow_id, nested_state, &nested_step.trace);
    const auto helper_trace = studio::graph_traces(doc.program(), helper, nested_state, &nested_step.trace);
    check(parent_trace.at("node/subflow").state == "waiting" && parent_trace.at("node/subflow@next").state == "queued", "A suspended subflow call looked completed before returning");
    check(helper_trace.at("node/delay").state == "waiting" && helper_trace.at("flow").detail.find("Incoming subflow") != std::string::npos, "The helper graph lost its actual incoming-call context");
    nested.reload(); check(nested.state() == nested_state, "Nested canvas call frames did not reload exactly");
}
void layout_conflicts_and_bounds() {
    Scratch scratch; const auto source = read(scratch.root / "data/runtime/foundation.json"); const auto versions = read(scratch.root / "project-versions.json");
    studio::LayoutDocument first{scratch.root, flow_id}; check(first.move("flow", {64, 64}), "A grid move failed"); first.save();
    studio::LayoutDocument stale{scratch.root, flow_id};
    check(first.move("flow", {80, 64}) && first.undo() && first.redo(), "Layout move undo/redo failed"); first.save();
    check(stale.move("flow", {96, 64}), "Stale layout fixture did not move");
    const auto file = scratch.root / "authoring/layouts/flow.canvas.layout.json"; const auto concurrent = read(file);
    refused([&] { stale.save(); }, "A stale layout overwrote a concurrent change"); check(read(file) == concurrent, "Layout conflict discarded an external edit");
    refused([&] { (void)first.move("flow", {81, 64}); }, "Fractional-grid positions were accepted");
    refused([&] { (void)first.move("flow", {-16, 64}); }, "Negative canvas coordinates were accepted");
    auto malformed = Json::parse(concurrent); malformed["nodes"]["flow"]["x"] = 1.5; put(file, malformed.dump());
    refused([&] { studio::LayoutDocument invalid{scratch.root, flow_id}; }, "Floating-point layout tokens were accepted");
    put(file, concurrent); auto future = Json::parse(concurrent); future["format"] = 99; put(file, future.dump());
    const auto future_bytes = read(file); refused([&] { studio::LayoutDocument invalid{scratch.root, flow_id}; }, "A future layout was rewritten"); check(read(file) == future_bytes, "Future layout bytes were not preserved");
    put(file, concurrent); fs::remove(file); fs::create_symlink(scratch.root / "data/runtime/foundation.json", file);
    refused([&] { studio::LayoutDocument linked{scratch.root, flow_id}; }, "A linked layout path was followed");
    check(read(scratch.root / "data/runtime/foundation.json") == source && read(scratch.root / "project-versions.json") == versions, "Layout failure modified content/versions");
    refused([&] { studio::save_canvas_view(scratch.root, "../outside.json", flow_id, {}); }, "A session view accepted an outside source");
}
}
int main() { try { projection_and_typed_edits(); execution_and_waits(); layout_conflicts_and_bounds(); std::cout << checks << " canvas checks passed\n"; return 0; } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
