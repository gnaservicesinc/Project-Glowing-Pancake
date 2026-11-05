#include "snapshot_controller.hpp"
#include <pgp/studio/draft.hpp>
#include <pgp/studio/flow_forms.hpp>
#include <pgp/studio/layout.hpp>
#include <pgp/persistence/storage.hpp>
#include <nlohmann/json.hpp>
#include <QCoreApplication>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>

namespace {
using namespace pgp; using Json = nlohmann::json; namespace fs = std::filesystem;
int checks = 0;
void check(bool yes, const char* why) { ++checks; if (!yes) throw std::runtime_error(why); }
std::string read(const fs::path& path) { return persistence::read_bounded(path, 4U * 1024U * 1024U); }
void put(const fs::path& path, std::string_view bytes) { fs::create_directories(path.parent_path()); std::ofstream file{path, std::ios::binary}; file << bytes; file.close(); if (!file) throw std::runtime_error("Cannot write controller fixture"); }
struct Scratch {
    fs::path root = fs::path{PGP_SNAPSHOT_UI_SCRATCH} / ("snapshot-ui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() {
        fs::create_directories(root / ".git"); put(root / "CMakeLists.txt", "# fixture\n"); put(root / "src/content/collection.cpp", "// fixture\n");
        auto versions = Json::parse(read(PGP_SNAPSHOT_UI_VERSIONS)); versions["components"]["content"] = "0.0.0-dev";
        auto content = Json::parse(read(PGP_SNAPSHOT_UI_SOURCE)); content["content_version"] = "0.0.0-dev";
        put(root / "project-versions.json", versions.dump(2)); put(root / "data/runtime/first-playable.json", content.dump(2));
        studio::SnapshotRequest first{"data/runtime/first-playable.json", "data/runtime/first-playable.json", "0.1.0-dev.1", "Initial controller fixture.", "reject-prior-content", {}};
        const auto review = studio::review_snapshot(root, first); (void)studio::finalize_snapshot(root, first, review.plan_id);
    }
    ~Scratch() { std::error_code error; fs::remove_all(root, error); }
    QString name() const { return QString::fromStdU16String(root.u16string()); }
};
std::map<std::string, std::string> tracked(const fs::path& root) {
    std::map<std::string, std::string> result;
    for (const auto& entry : fs::recursive_directory_iterator(root)) if (fs::is_regular_file(entry.symlink_status())) {
        const auto path = entry.path().lexically_relative(root).generic_string();
        if (!path.starts_with(".pgp-editor/") && !path.starts_with(".git/")) result[path] = read(entry.path());
    }
    return result;
}
constexpr auto runtime_path = "data/runtime/first-playable.json";
constexpr auto draft_path = "data/drafts/batch.json";
void draft_batch(Scratch& scratch) {
    studio::DraftDocument doc{scratch.root, runtime_path}; auto data = doc.collection();
    std::get<content::FlowDefinition>(data.definitions.at("slice.evaluation_finding").data).tags.push_back("review_one"); doc.replace(data);
    data = doc.collection(); std::get<content::FlowDefinition>(data.definitions.at("slice.evaluation_finding").data).tags.push_back("review_two");
    const auto& flow = std::get<content::FlowDefinition>(data.definitions.at("slice.evaluation_finding").data);
    data.messages.at(flow.nodes.at(flow.entry_node).text_key) += " Reviewed wording.";
    doc.replace(data); doc.save("batch.json");
}
bool review(studio::SnapshotController& ui) { return ui.review(QStringLiteral("data/drafts/batch.json"), QStringLiteral("0.2.0-dev.1"), QStringLiteral("Review one combined definition and wording batch."), {}); }
void batches() {
    Scratch scratch; draft_batch(scratch); studio::SnapshotController ui;
    ui.set_context(scratch.name(), QStringLiteral("data/drafts/batch.json"), 0, false, false); ui.refresh();
    check(ui.sources().size() == 2 && ui.preferredSource() == QStringLiteral("data/drafts/batch.json"), "Graphical source selection lost the working saved collection");
    check(ui.currentStatus().contains(QStringLiteral("unresolved")) && ui.suggestedVersion() == QStringLiteral("0.2.0-dev.1"), "Current identity or suggested development version is missing");
    const auto before = tracked(scratch.root);
    check(review(ui) && ui.ready() && ui.canFinalize() && tracked(scratch.root) == before, "Graphical review wrote files or lacked a valid plan");
    check(ui.changes().size() == 1 && ui.changes().front().toMap().value(QStringLiteral("revision")).toString() == QStringLiteral("2"), "Multiple edits incremented a definition more than once in review");
    check(ui.textChanges().size() == 1 && ui.textChanges().front().toMap().value(QStringLiteral("after")).toString().endsWith(QStringLiteral("Reviewed wording.")), "The actual before/after visible text was omitted");
    const auto fixture=Json::parse(read(scratch.root/runtime_path));std::size_t saves=0;
    for (const auto& definition:fixture.at("definitions")) if (definition.at("kind")=="scenario") saves+=definition.at("headquarters_choices").size();
    check(ui.writes().size() == 6 && ui.validationSummary().contains(QStringLiteral("%1 exact save").arg(saves)) && ui.releaseSummary().contains(QStringLiteral("unresolved")), "Graphical review hid files, exact-save validation or pending requirements");
    ui.set_context(scratch.name(), QStringLiteral("data/drafts/batch.json"), 1, true, false);
    check(!ui.ready() && !review(ui) && !ui.finalizeReview() && tracked(scratch.root) == before, "An unsaved content edit could use an old reviewed plan");
    ui.set_context(scratch.name(), QStringLiteral("data/drafts/batch.json"), 2, false, false); check(review(ui), "A clean saved context could not review");
    ui.clearReview(); check(!ui.finalizeReview() && tracked(scratch.root) == before, "Changing review inputs retained an actionable plan");
    check(review(ui), "Review failed before external edit"); const auto source = read(scratch.root / draft_path); put(scratch.root / draft_path, source + "\n");
    check(!ui.finalizeReview() && ui.message().contains(QStringLiteral("conflict")) && !ui.ready(), "An external draft edit did not invalidate finalization");
    check(read(scratch.root / draft_path) == source + "\n" && studio::check_snapshot(scratch.root).generation == 1 && !ui.pendingRecovery(), "A stale review overwrote source or started an unnecessary journal");
    check(review(ui), "A freshly reviewed external edit was refused");
    ui.set_context(scratch.name(), QStringLiteral("data/drafts/batch.json"), 2, false, true);
    check(ui.ready() && !ui.canFinalize() && !ui.finalizeReview(), "A writing batch discarded unsaved shared layout work");
    ui.set_context(scratch.name(), QStringLiteral("data/drafts/batch.json"), 2, false, false);
    int finished = 0; QObject::connect(&ui, &studio::SnapshotController::finalized, [&finished](const QString&) { ++finished; });
    check(ui.finalizeReview() && finished == 1 && !ui.ready() && ui.outcome().contains(QStringLiteral("0.2.0-dev.1")), "Finalization did not complete once and report the committed identity");
    const auto finalized = tracked(scratch.root);
    check(!ui.finalizeReview() && finished == 1 && tracked(scratch.root) == finalized, "Repeated finalization reused a consumed review");
    check(studio::check_snapshot(scratch.root).generation == 2 && read(scratch.root / draft_path) == source + "\n", "Graphical promotion changed its draft or created the wrong generation");
    const auto data = Json::parse(read(scratch.root / runtime_path));
    for (const auto& definition : data.at("definitions")) check(definition.at("revision") == (definition.at("id") == "slice.evaluation_finding" ? 2 : 1), "A changed definition did not increment exactly once");
    check(!review(ui) && ui.message().contains(QStringLiteral("version")), "The obsolete draft was allowed to overwrite newer finalized revisions");
    ui.set_context(scratch.name(), QStringLiteral("data/runtime/first-playable.json"), 0, false, false); ui.refresh();
    check(ui.review(QStringLiteral("data/runtime/first-playable.json"), QStringLiteral("99.0.0"), QStringLiteral("No-op"), {}), "Current runtime failed no-op review");
    const auto id = ui.planId();
    studio::LayoutDocument layout{scratch.root, "slice.evaluation_finding"}; check(layout.move("flow", {32, 48}), "Layout fixture did not move"); layout.save();
    ui.set_context(scratch.name(), QStringLiteral("data/runtime/first-playable.json"), 0, false, true);
    check(ui.review(QStringLiteral("data/runtime/first-playable.json"), QStringLiteral("99.0.0"), QStringLiteral("No-op"), {}) && ui.planId() == id && !ui.runtimeChanged() && ui.writes().isEmpty() && ui.canFinalize(), "Layout-only changes altered review identity or forced a runtime write");
    const auto stable = tracked(scratch.root);
    check(ui.finalizeReview() && tracked(scratch.root) == stable && finished == 1 && ui.outcome().contains(QStringLiteral("No-op")), "No-op wrote versions or requested a layout-discarding document reload");
    ui.set_context(scratch.name(), {}, 0, false, false); ui.refresh();
    check(!ui.review(QStringLiteral("../outside.json"), {}, {}, {}) && !ui.ready(), "The adapter accepted a source outside its selected library");
    check(!ui.review(QStringLiteral("data/runtime/first-playable.json"), {}, {}, QStringLiteral("0.0.0")), "The adapter bypassed the minimum game-version rule");
    check(ui.review(QStringLiteral("data/runtime/first-playable.json"), {}, {}, QStringLiteral("0.1.0-dev.1")) && ui.finalizeReview(), "Explicit game-version resolution failed");
    check(studio::check_snapshot(scratch.root).game_version_requirement_resolved && studio::check_snapshot(scratch.root).generation == 2, "Resolving a game requirement created another content generation");
}
void recovery() {
    Scratch scratch; draft_batch(scratch); const auto before = tracked(scratch.root);
    studio::SnapshotRequest request{draft_path, runtime_path, "0.2.0-dev.1", "Interrupt this isolated batch.", "reject-prior-content", {}};
    const auto plan = studio::review_snapshot(scratch.root, request);
    try { (void)studio::finalize_snapshot(scratch.root, request, plan.plan_id, [](auto phase, std::size_t index) { if (phase == studio::SnapshotWritePhase::replaced && index == 1) throw std::runtime_error("Injected interruption"); }); } catch (const std::runtime_error&) {}
    studio::SnapshotController ui; ui.set_context(scratch.name(), QStringLiteral("data/drafts/batch.json"), 1, true, true); ui.refresh();
    check(ui.pendingRecovery() && !ui.ready(), "Opening an interrupted batch hid graphical recovery");
    const auto versions = read(scratch.root / "project-versions.json"); put(scratch.root / "project-versions.json", versions + "\n");
    int recovered = 0; QObject::connect(&ui, &studio::SnapshotController::recovered, [&recovered] { ++recovered; });
    check(!ui.recover() && ui.pendingRecovery() && recovered == 0 && read(scratch.root / "project-versions.json") == versions + "\n", "Recovery overwrote a concurrent third version or reported success");
    put(scratch.root / "project-versions.json", versions);
    check(ui.recover() && !ui.pendingRecovery() && recovered == 1 && tracked(scratch.root) == before, "Graphical recovery failed to restore the entire prior generation");
    check(!ui.blocker().isEmpty() && !ui.ready(), "Recovery silently cleared the unsaved working-content context");
    const auto original = read(scratch.root / "project-versions.json"); auto future = Json::parse(original); future["compatibility"]["content_schema"] = 99; put(scratch.root / "project-versions.json", future.dump(2));
    ui.set_context(scratch.name(), {}, 0, false, false); ui.refresh();
    check(ui.sources().isEmpty() && !review(ui) && !ui.finalizeReview() && read(scratch.root / "project-versions.json") == future.dump(2), "Future schema was writable from graphical review");
}
void exact_values() {
    Scratch scratch; studio::DraftDocument document{scratch.root, runtime_path};
    check(studio::create_flow(document, "review.exact", "company"), "Cannot create the exact-integer review fixture");
    auto form = studio::FlowForm::open(document, "review.exact", "trigger"); (void)form.apply(document, "fact", "company.cash_cents");
    check(std::get<content::FlowDefinition>(document.collection().definitions.at("review.exact").data).trigger.clause->fact == "company.cash_cents", "Cannot select a money fact");
    form = studio::FlowForm::open(document, "review.exact", "trigger"); check(form.apply(document, "value", "9007199254740993"), "Cannot enter an exact 64-bit boundary value"); document.save("exact.json");
    studio::SnapshotController ui; ui.set_context(scratch.name(), QStringLiteral("data/drafts/exact.json"), 0, false, false); ui.refresh();
    check(ui.review(QStringLiteral("data/drafts/exact.json"), QStringLiteral("0.2.0-dev.1"), QStringLiteral("Inspect exact integer text."), {}), "The exact-integer review did not validate");
    bool found = false;
    for (const auto& change : ui.changes()) for (const auto& field : change.toMap().value(QStringLiteral("fields")).toList())
        if (field.toMap().value(QStringLiteral("after")).toString() == QStringLiteral("9007199254740993")) found = true;
    check(found, "The graphical difference display rounded a 64-bit integer through floating point");
}
}
int main(int argc, char** argv) {
    QCoreApplication app{argc, argv};
    try { batches(); recovery(); exact_values(); std::cout << checks << " graphical snapshot controller checks passed\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
