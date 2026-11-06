#include "../runtime/integration_fixture.hpp"

#include <pgp/core/build_identity.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/studio/revision_review.hpp>
#include <pgp/studio/snapshot.hpp>

#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace pgp;
using Json = nlohmann::json;
namespace fs = std::filesystem;
constexpr auto target = "data/runtime/first-playable.json";
constexpr auto chain = "flow.hidden.sequence_resume";
constexpr auto new_chain = "flow.saved.sequence";
constexpr auto group = "group.saved.choice";
constexpr auto new_group = "group.saved.decision";
constexpr auto continuous = "flow.hidden.sustained_workload";
int checks = 0;

void check(bool value, const char* detail) {
    ++checks;
    if (!value) throw std::runtime_error(detail);
}
template<class Action> void refused(Action action, std::string_view code) {
    try { action(); }
    catch (const persistence::PersistenceError& error) { check(error.code() == code, error.what()); return; }
    throw std::runtime_error("Expected refusal " + std::string(code));
}
void put(const fs::path& path, std::string_view bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file{path, std::ios::binary};
    file << bytes;
    file.close();
    if (!file) throw std::runtime_error("Failed to write isolated revision fixture");
}
std::string read(const fs::path& path) { return persistence::read_bounded(path, 4U * 1024U * 1024U); }
struct Scratch {
    fs::path root = fs::path{PGP_REVISION_SCRATCH} / ("revisions-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() {
        if (!fs::create_directory(root)) throw std::runtime_error("The revision fixture directory already exists.");
        fs::create_directory(root / ".git");
        put(root / "CMakeLists.txt", "# Fixture\n");
        put(root / "src/content/collection.cpp", "// Fixture\n");
        auto versions = Json::parse(read(fs::path{PGP_REVISION_CHECKOUT} / "project-versions.json"));
        versions["components"]["content"] = "0.0.0-dev";
        put(root / "project-versions.json", versions.dump());
    }
    ~Scratch() { std::error_code error; fs::remove_all(root, error); }
};

Json& definition(Json& doc, std::string_view id) {
    for (auto& row : doc.at("definitions")) if (row.at("id") == id) return row;
    throw std::runtime_error("Missing fixture definition " + std::string(id));
}
Json effect(const char* decision, const std::string& next) {
    return {{"kind", "effect"}, {"next", next},
        {"effects", Json::array({{{"op", "record_decision"}, {"target", "self"}, {"value", decision}}})}};
}
Json flow(const char* id, const char* entry, Json nodes) {
    return {{"id", id}, {"kind", "flow"}, {"revision", 1}, {"scope", "company"},
        {"lifecycle", {{"mode", "once"}, {"max_occurrences_per_scope", 1}}},
        {"trigger", {{"fact", "company.is_operating"}, {"op", "eq"}, {"value", true}}},
        {"entry_node", entry}, {"nodes", std::move(nodes)}};
}

Json source_document() {
    auto doc = Json::parse(read(PGP_REVISION_FOUNDATION));
    for (const auto* id : {"story.output", "story.result"}) doc["variables"].push_back(
        {{"id", id}, {"scope", "company"}, {"type", "integer"}, {"unit", "count"},
            {"minimum", 0}, {"maximum", 100}, {"default", 0}, {"help", "Migration fixture result"}});
    const Json end{{"kind", "end"}};
    auto nested = flow(group, "sample", {
        {"sample", {{"kind", "random"}, {"stream", "narrative"}, {"branches", Json::array({
            {{"id", "left"}, {"weight", 1}, {"next", "assign"}}, {{"id", "right"}, {"weight", 2}, {"next", "assign"}}})}}},
        {"assign", {{"kind", "effect"}, {"next", "wait"}, {"effects", Json::array({
            {{"op", "set_variable"}, {"target", "self"}, {"variable", "story.output"}, {"value", {{"literal", 43}, {"unit", "count"}}}}})}}},
        {"wait", {{"kind", "delay"}, {"duration_days", 14}, {"next", "choice"}}},
        {"choice", {{"kind", "dialogue"}, {"title_key", "fixture.group.title"}, {"text_key", "fixture.group.body"},
            {"choices", Json::array({{{"id", "continue"}, {"label_key", "fixture.group.continue"}, {"next", "end"}}})},
            {"default_choice_if_dismissed", "continue"}}},
        {"end", end}});
    nested["group_ports"] = Json::array({{{"name", "result"}, {"variable", "story.output"}, {"direction", "output"}}});
    doc["messages"] = {{"fixture.group.title", "Group title"}, {"fixture.group.body", "Saved group decision"},
        {"fixture.group.continue", "Continue the recorded chain"}};
    doc["definitions"].push_back(std::move(nested));
    definition(doc, chain)["nodes"]["wait"] = {{"kind", "subflow"}, {"flow_id", group}, {"next", "wait_return"},
        {"bindings", {{"result", "story.result"}}}};
    const auto trigger = definition(doc, chain).at("trigger");
    definition(doc, chain)["trigger"] = {{"holds_for_days", {{"id", "operating"}, {"days", 1}, {"condition", trigger}}}};
    // A separate saved flow is visible only on new campaign fixtures.
    doc["definitions"].push_back(flow("flow.new_preview", "mark", {{"mark", effect("new_preview", "end")}, {"end", end}}));
    Json large = Json::object();
    for (int i = 0; i != 511; ++i) large["node" + std::to_string(i)] = effect("large_graph", "node" + std::to_string(i + 1));
    large["node511"] = end;
    doc["definitions"].push_back(flow("flow.large", "node0", std::move(large)));
    return doc;
}

void rename_node(Json& flow, const std::string& from, const std::string& to) {
    auto& nodes = flow.at("nodes");
    nodes[to] = nodes.at(from);
    nodes.erase(from);
    const auto fix = [&](Json& value) { if (value == from) value = to; };
    fix(flow.at("entry_node"));
    if (flow.contains("exit_node")) fix(flow.at("exit_node"));
    for (auto& node : nodes) {
        if (node.contains("next")) fix(node.at("next"));
        if (node.contains("children")) for (auto& child : node.at("children")) fix(child);
        for (const auto* key : {"choices", "branches"})
            if (node.contains(key)) for (auto& row : node.at(key)) fix(row.at("next"));
    }
}

std::vector<studio::IdentityMapping> changes(Json& doc) {
    using Kind = studio::MigrationKind;
    auto& scenario = definition(doc, "scenario.foundation");
    scenario["id"] = "scenario.review";
    scenario["revision"] = 1;
    for (auto& root : scenario.at("flows")) if (root == chain) root = new_chain;
    scenario["flows"].push_back("flow.new_preview");
    auto& root = definition(doc, chain);
    root["id"] = new_chain;
    root["revision"] = 1;
    root["nodes"]["wait"]["flow_id"] = new_group;
    root["nodes"]["wait"]["bindings"]["result"] = "story.reviewed_result";
    rename_node(root, "wait", "call");
    rename_node(root, "inner_sequence", "inner");
    rename_node(root, "wait_return", "group_return");
    auto& subflow = definition(doc, group);
    subflow["id"] = new_group;
    subflow["revision"] = 1;
    rename_node(subflow, "wait", "pending");
    rename_node(subflow, "choice", "decision");
    rename_node(subflow, "sample", "draw");
    subflow["nodes"]["decision"]["text_key"] = "fixture.group.review";
    doc["messages"]["fixture.group.review"] = "Reviewed group decision";
    doc["messages"].erase("fixture.group.body");
    for (auto& row : doc.at("variables")) if (row.at("id") == "story.result") row["id"] = "story.reviewed_result";
    definition(doc, "deal.compute_exclusivity")["id"] = "deal.reviewed_exclusivity";
    definition(doc, "flow.supplier.exclusivity_offer")["nodes"]["review_effect"]["effects"][0]["template_id"] = "deal.reviewed_exclusivity";
    rename_node(definition(doc, continuous), "end", "retained_end");
    definition(doc, "flow.large")["nodes"]["node510"]["effects"][0]["value"] = "large_reviewed";
    return {{Kind::flow, {}, chain, new_chain}, {Kind::flow, {}, group, new_group}, {Kind::scenario, {}, "scenario.foundation", "scenario.review"},
        {Kind::node, chain, "wait", "call"}, {Kind::node, chain, "inner_sequence", "inner"}, {Kind::node, chain, "wait_return", "group_return"},
        {Kind::node, group, "wait", "pending"}, {Kind::node, group, "choice", "decision"}, {Kind::node, group, "sample", "draw"},
        {Kind::node, continuous, "end", "retained_end"}, {Kind::message, {}, "fixture.group.body", "fixture.group.review"},
        {Kind::variable, {}, "story.result", "story.reviewed_result"}, {Kind::deal, {}, "deal.compute_exclusivity", "deal.reviewed_exclusivity"}};
}

studio::SnapshotResult finalize(const fs::path& root, const fs::path& source, std::string version) {
    const studio::SnapshotRequest request{source, target, std::move(version), "Review fixture generation.", "reject-prior-content", {}};
    const auto plan = studio::review_snapshot(root, request);
    return studio::finalize_snapshot(root, request, plan.plan_id);
}

void bundled_history() {
    const studio::RevisionArchive archive{PGP_REVISION_CHECKOUT};
    check(archive.revisions().size() >= 7, "The checked-in finalized history is incomplete");
    for (const auto& info : archive.revisions())
        check(archive.open(info.generation).info.collection_hash == info.collection_hash, "Archived canonical snapshot cannot be recompiled");
    const auto first = archive.open(1), last = archive.open(archive.revisions().back().generation);
    const auto diff = studio::compare_finalized(first, last);
    check(!diff.empty(), "The actual archived revisions compare identically");
    studio::MatchedFixture fixture{first, last, "scenario.first_playable", "scenario.first_playable", "US-CA", 42};
    const auto pair = fixture.prepare();
    check(pair.before.trace.accepted() && pair.after.trace.accepted(), "Real historical comparison could not prepare both openings");
}

void migration_and_review() {
    Scratch scratch;
    put(scratch.root / target, source_document().dump());
    check(finalize(scratch.root, target, "0.1.0-dev.1").generation == 1, "Fixture did not finalize through the shared writer");
    const auto before = studio::RevisionArchive{scratch.root}.open(1);
    runtime::Runtime old_game{before.program, integration_fixture::checkpoint(before.program)};
    integration_fixture::prepare(old_game);
    integration_fixture::choose_all(old_game);
    integration_fixture::step(old_game);
    const auto delayed = old_game.checkpoint();
    const auto waiting = std::find_if(delayed.runtime.activations.begin(), delayed.runtime.activations.end(),
        [](const auto& active) { return active.definition_id == chain; });
    check(waiting != delayed.runtime.activations.end() && waiting->frames.size() == 3 && waiting->due &&
        waiting->current_flow == group, "Fixture did not save two sequences and a typed call waiting on a delay");
    check(delayed.runtime.modifiers.size() == 1 && delayed.runtime.random_resolutions.size() == 1 &&
        delayed.world.variables.size() == 1 && delayed.world.negotiations == std::vector<std::string>{"deal.compute_exclusivity"} &&
        !delayed.runtime.duration_states.empty(), "Fixture lacks actual owned modifier, RNG, variable, negotiation or duration history");
    const auto old_path = scratch.root / ".work/delayed.pgpsave";
    const auto bytes = persistence::encode_save(before.program, delayed, 23);
    put(old_path, bytes);
    while (old_game.current_state().world.simulation.date < *waiting->due) integration_fixture::step(old_game);
    const auto pending = old_game.checkpoint();
    const auto old_dialogue = scratch.root / ".work/dialogue.pgpsave";
    const auto pending_bytes = persistence::encode_save(before.program, pending);
    put(old_dialogue, pending_bytes);

    auto doc = Json::parse(read(scratch.root / target));
    const auto mappings = changes(doc);
    put(scratch.root / "data/drafts/next.json", doc.dump());
    check(finalize(scratch.root, "data/drafts/next.json", "0.2.0-dev.1").generation == 2, "Destination generation did not finalize");
    const studio::RevisionArchive archive{scratch.root};
    const auto after = archive.open(2);
    const auto diff = studio::compare_finalized(before, after);
    check(std::any_of(diff.begin(), diff.end(), [](const auto& row) {
        return row.path == "/definitions/flow.large/nodes/node510/effects/0/value" && row.after == "large_reviewed";
    }), "Maximum-size graph diff missed a changed effect argument");
    studio::MatchedFixture fixture{before, after, "scenario.foundation", "scenario.review", "US-CA", 42};
    const auto opening = fixture.prepare();
    check(opening.before.trace.accepted() && opening.after.trace.accepted() &&
        fixture.before().world.root_seed == fixture.after().world.root_seed &&
        std::none_of(opening.before.effects.begin(), opening.before.effects.end(), [](const auto& r) { return r.flow_id == "flow.new_preview"; }) &&
        std::any_of(opening.after.effects.begin(), opening.after.effects.end(), [](const auto& r) { return r.flow_id == "flow.new_preview"; }),
        "Matched openings failed to reveal changed scenario reach and its actual effect");
    const auto fixture_before = fixture.before(), fixture_after = fixture.after();
    const auto rejected = fixture.choose({999, "continue"}, {999, "continue"});
    check(!rejected.before.trace.accepted() && !rejected.after.trace.accepted() &&
        fixture.before() == fixture_before && fixture.after() == fixture_after, "Rejected paired action changed a fixture");

    studio::SaveMigrationRequest request{1, 2, old_path, mappings, "Reviewed stable execution and renamed author identities."};
    refused([&] { (void)persistence::decode_save(after.program, bytes); }, "content_identity");
    auto without_maps = request;
    without_maps.mappings.clear();
    refused([&] { (void)studio::review_save_migration(scratch.root, without_maps); }, "variable_contract");
    without_maps.mappings = mappings;
    without_maps.mappings.pop_back();
    refused([&] { (void)studio::review_save_migration(scratch.root, without_maps); }, "flow_contract");
    auto duplicate = request;
    duplicate.mappings.push_back(mappings.front());
    refused([&] { (void)studio::review_save_migration(scratch.root, duplicate); }, "mapping");
    auto broken = request;
    broken.mappings.front().to = "flow.does_not_exist";
    refused([&] { (void)studio::review_save_migration(scratch.root, broken); }, "mapping");

    const auto reviewed = studio::review_save_migration(scratch.root, request);
    const auto evidence = Json::parse(reviewed.report_json);
    check(evidence.at("mappings").size() == mappings.size() && evidence.at("owned_modifiers") == 1 &&
        evidence.at("resolved_random_draws") == 1 && evidence.at("source_save_sha256") == persistence::sha256(bytes),
        "Review evidence omitted explicit mappings, original bytes or saved history");
    check(!fs::exists(scratch.root / ".work/revision-review"), "Review wrote files before approval");
    const auto output = studio::export_save_migration(scratch.root, request, reviewed.plan_id, "delayed");
    const auto loaded = persistence::load_file(after.program, output);
    check(read(old_path) == bytes && read(output.parent_path() / "review.json") == reviewed.report_json &&
        read(output) != bytes && loaded.autosave_sequence == 23, "Export changed the original or lost reviewed identity/sequence");
    refused([&] { (void)studio::export_save_migration(scratch.root, request, reviewed.plan_id, "delayed"); }, "conflict");
    check(loaded.checkpoint.world.simulation == delayed.world.simulation &&
        loaded.checkpoint.runtime.modifiers == delayed.runtime.modifiers &&
        loaded.checkpoint.world.scenario_id == "scenario.review" &&
        loaded.checkpoint.world.variables.front().variable_id == "story.reviewed_result" &&
        loaded.checkpoint.world.variables.front().value == content::Scalar{std::int64_t{43}} &&
        loaded.checkpoint.runtime.duration_states.front().true_since == delayed.runtime.duration_states.front().true_since,
        "Migration changed core state, RNG, owned modifiers, scoped values or duration observations");
    const auto& resolution = loaded.checkpoint.runtime.random_resolutions.front();
    check(resolution.flow_id == new_group && resolution.node_id == "draw" &&
        resolution.stream_after == delayed.runtime.random_resolutions.front().stream_after,
        "Migration lost the actual random branch record");
    check(std::any_of(loaded.checkpoint.runtime.receipts.begin(), loaded.checkpoint.runtime.receipts.end(), [](const auto& receipt) {
        return receipt.bound_variable == "story.reviewed_result" && receipt.group_calls == std::vector<std::string>{"call"};
    }), "Migration lost typed group output caller evidence");

    request.source_save = old_dialogue;
    const auto dialogue_review = studio::review_save_migration(scratch.root, request);
    const auto migrated_dialogue = studio::export_save_migration(scratch.root, request, dialogue_review.plan_id, "dialogue");
    runtime::Runtime resumed{after.program, loaded.checkpoint};
    while (resumed.current_state().world.simulation.date < *waiting->due) integration_fixture::step(resumed);
    const auto pending_reloaded = persistence::load_file(after.program, migrated_dialogue).checkpoint;
    const auto card = std::find_if(pending_reloaded.runtime.presentations.begin(), pending_reloaded.runtime.presentations.end(),
        [](const auto& row) { return !row.resolved; });
    check(resumed.checkpoint() == pending_reloaded && card != pending_reloaded.runtime.presentations.end() &&
        card->flow_id == new_group && card->node_id == "decision" && card->text_key == "fixture.group.review" &&
        read(old_dialogue) == pending_bytes, "Delayed resume and independently migrated pending dialogue diverged");
    integration_fixture::choose_all(resumed);
    integration_fixture::choose_all(old_game);
    const auto done = resumed.checkpoint();
    check(done.world.decisions == old_game.current_state().world.decisions &&
        std::count(done.world.decisions.begin(), done.world.decisions.end(), "inner_done") == 1 &&
        std::count(done.world.decisions.begin(), done.world.decisions.end(), "outer_done") == 1 &&
        done.world.simulation.random == old_game.current_state().world.simulation.random &&
        done.world.negotiations == std::vector<std::string>{"deal.reviewed_exclusivity"},
        "Pending choice repeated or reordered effects, rerolled the branch or lost its recorded negotiation");

    auto exiting = done;
    exiting.world.facts.at("company.workload_stress_basis_points") = std::int64_t{5000};
    runtime::Runtime natural_exit{after.program, std::move(exiting)};
    integration_fixture::step(natural_exit);
    const auto cleaned = natural_exit.checkpoint();
    check(cleaned.runtime.modifiers.empty() && std::count_if(cleaned.runtime.receipts.begin(), cleaned.runtime.receipts.end(),
        [](const auto& r) { return r.operation == "remove_owned_modifier"; }) == 1 &&
        persistence::decode_save(after.program, persistence::encode_save(after.program, cleaned)).checkpoint == cleaned,
        "Migrated continuous activation failed to clean up once through its natural condition");

    request.source_save = old_path;
    auto unknown = Json::parse(bytes);
    unknown["checkpoint"]["runtime"]["new_state_field"] = 1;
    unknown.erase("sha256");
    unknown["sha256"] = persistence::sha256(unknown.dump());
    put(old_path, unknown.dump());
    refused([&] { (void)studio::review_save_migration(scratch.root, request); }, "invalid_state");
    put(old_path, bytes + "\n");
    refused([&] { (void)studio::export_save_migration(scratch.root, request, reviewed.plan_id, "changed-save"); }, "conflict");
    put(old_path, bytes);

    const auto versions_path = scratch.root / "project-versions.json";
    const auto versions = read(versions_path);
    put(versions_path, versions + "\n");
    refused([&] { (void)studio::export_save_migration(scratch.root, request, reviewed.plan_id, "stale-marker"); }, "conflict");
    auto unsupported = Json::parse(versions);
    unsupported["compatibility"]["content_schema"] = 2;
    put(versions_path, unsupported.dump());
    refused([&] { (void)studio::export_save_migration(scratch.root, request, reviewed.plan_id, "stale-schema"); }, "unsupported_schema");
    put(versions_path, versions);
    const auto runtime_path = scratch.root / target;
    const auto runtime_bytes = read(runtime_path);
    auto newer_source = Json::parse(runtime_bytes);
    newer_source["future_runtime_field"] = "preserve this";
    const auto unknown_bytes = newer_source.dump();
    put(runtime_path, unknown_bytes);
    bool refused_unknown_content = false;
    try { (void)studio::export_save_migration(scratch.root, request, reviewed.plan_id, "future-source"); }
    catch (const content::ValidationError&) { refused_unknown_content = true; }
    check(refused_unknown_content && read(runtime_path) == unknown_bytes &&
        !fs::exists(scratch.root / ".work/revision-review/future-source"), "Unknown authored fields were discarded or exported over");
    put(runtime_path, runtime_bytes);
    const auto source_snapshot = scratch.root / "authoring/snapshots/000001.collection.json";
    const auto original = read(source_snapshot);
    auto altered = Json::parse(original);
    definition(altered, "flow.large")["nodes"]["node510"]["effects"][0]["value"] = "tampered";
    put(source_snapshot, altered.dump());
    refused([&] { (void)studio::review_save_migration(scratch.root, request); }, "snapshot");
    put(source_snapshot, original);

    const studio::SnapshotRequest game_change{target, target, {}, {}, "reject-prior-content", "1.0.0-dev.1"};
    const auto game_review = studio::review_snapshot(scratch.root, game_change);
    check(!game_review.runtime_changed &&
        studio::finalize_snapshot(scratch.root, game_change, game_review.plan_id).generation == 2,
        "A game identity change incorrectly created another content generation");
    refused([&] { (void)studio::export_save_migration(scratch.root, request, reviewed.plan_id, "stale-game"); }, "conflict");
    const auto game_plan = studio::review_save_migration(scratch.root, request);
    const auto game_copy = studio::export_save_migration(scratch.root, request, game_plan.plan_id, "next-game");
    const auto game_bytes = read(game_copy);
    refused([&] { (void)persistence::decode_save(after.program, game_bytes); }, "game_version");
    check(persistence::decode_revision_save(after.program, "1.0.0-dev.1", game_bytes).checkpoint == loaded.checkpoint,
        "The game-version transition changed checkpoint state or guessed the target game identity");

    auto incompatible = Json::parse(read(scratch.root / target));
    definition(incompatible, new_group)["nodes"]["pending"]["duration_days"] = 15;
    put(scratch.root / "data/drafts/incompatible.json", incompatible.dump());
    check(finalize(scratch.root, "data/drafts/incompatible.json", "0.3.0-dev.1").generation == 3, "Changed destination was not finalized");
    refused([&] { (void)studio::export_save_migration(scratch.root, request, reviewed.plan_id, "stale-generation"); }, "generation");
    request.target_generation = 3;
    refused([&] { (void)studio::review_save_migration(scratch.root, request); }, "flow_contract");
    check(read(old_path) == bytes && !fs::exists(scratch.root / ".work/revision-review/stale-schema") &&
        !fs::exists(scratch.root / ".work/revision-review/stale-generation"), "A refused review touched input or published an output");
}
} // namespace

int main() {
    try {
        bundled_history();
        migration_and_review();
        std::cout << checks << " cross-revision and save migration checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
