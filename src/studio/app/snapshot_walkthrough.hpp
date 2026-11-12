#pragma once
#include "snapshot_controller.hpp"
#include "walkthrough.hpp"
#include <pgp/studio/snapshot.hpp>
#include <QJsonDocument>
#include <fstream>
#include <map>

namespace pgp::studio {
// Explicit fresh-copy engineering run. Only the recovery fixture uses an
// injected interruption; ordinary authoring and finalization use QML controls.
template<class Browser>
QJsonObject run_snapshot_walkthrough(Browser& browser, QQuickWindow& window, const QString& screenshot = {}) {
    using namespace walkthrough; namespace fs = std::filesystem;
    const fs::path root{browser.checkout().toStdU16String()};
    auto* finalizer = qobject_cast<SnapshotController*>(browser.snapshotController());
    require(finalizer, QStringLiteral("The graphical finalizer is unavailable."));
    const auto read = [&](const fs::path& path) { return persistence::read_bounded(root / path, 4U * 1024U * 1024U); };
    const auto owned = [&] {
        std::map<std::string, std::string> result;
        for (const auto& path : {fs::path{"project-versions.json"}, fs::path{"data/runtime/first-playable.json"}, fs::path{"data/runtime/content-manifest.json"}, fs::path{"authoring/content-state.json"}, fs::path{"authoring/content-history.json"}}) result[path.generic_string()] = read(path);
        return result;
    };
    const auto item = [&](const QString& name) {
        settle(); auto* result = find(window.contentItem(), name);
        require(result, QStringLiteral("Missing finalization control: ") + name); return result;
    };
    const auto click = [&](const QString& name) {
        auto* result = item(name); require(result->isVisible() && result->isEnabled(), QStringLiteral("Unavailable finalization control: ") + name);
        result->forceActiveFocus();
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier), release(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        QCoreApplication::sendEvent(result, &press); QCoreApplication::sendEvent(result, &release); settle();
    };
    const auto field = [&](const QString& name, const QString& value) {
        auto* result = item(name); result->setProperty("text", value);
        if (name != QStringLiteral("revisionSummary")) require(QMetaObject::invokeMethod(result, "textEdited"), QStringLiteral("Missing input edit handler."));
        settle();
    };
    const auto approve = [&] {
        click(QStringLiteral("acknowledgeContentRevision"));
        require(item(QStringLiteral("acknowledgeContentRevision"))->property("checked").toBool(), QStringLiteral("Keyboard acknowledgement failed."));
        click(QStringLiteral("finalizeReviewedContent"));
    };
    const auto open = [&] {
        auto* dialog = window.findChild<QObject*>(QStringLiteral("contentRevisionReview"));
        require(dialog && QMetaObject::invokeMethod(dialog, "open"), QStringLiteral("The review dialog did not open."));
        settle();
        require(window.findChild<QObject*>(QStringLiteral("contentRevisionReview"))->property("visible").toBool(), QStringLiteral("The review dialog did not open."));
    };
    const auto reveal = [&](QQuickItem* target) {
        for (auto* parent = target->parentItem(); parent; parent = parent->parentItem()) {
            if (!parent->property("contentY").isValid()) continue;
            auto* content = qobject_cast<QQuickItem*>(parent->property("contentItem").template value<QObject*>());
            const auto maximum = std::max(0.0, parent->property("contentHeight").toDouble() - parent->height());
            if (content) parent->setProperty("contentY", std::clamp(target->mapToItem(content, QPointF{}).y(), 0.0, maximum));
        }
        settle();
    };
    const auto baseline = owned(); const auto draft_file = fs::path{browser.workingFile().toStdU16String()};
    const auto draft_bytes = read(draft_file); const auto prior = check_snapshot(root);
    require(prior.generation > 0 && !prior.game_version_requirement_resolved, QStringLiteral("Choose a fresh finalized checkout copy with its pending game requirement for this walkthrough."));
    finalizer->refresh();const auto next_version=finalizer->suggestedVersion();
    require(!next_version.isEmpty(),QStringLiteral("The next development content version is unavailable."));
    int saves=0;
    for (const auto& value:QJsonDocument::fromJson(QByteArray::fromStdString(read("data/runtime/first-playable.json"))).object().value(QStringLiteral("definitions")).toArray()) {
        const auto definition=value.toObject();if (definition.value(QStringLiteral("kind"))==QStringLiteral("scenario")) saves+=definition.value(QStringLiteral("headquarters_choices")).toArray().size();
    }
    open();
    require(item(QStringLiteral("revisionSource"))->property("currentText").toString() == browser.workingFile(), QStringLiteral("Review did not select the saved working collection."));
    field(QStringLiteral("revisionContentVersion"), next_version);
    field(QStringLiteral("revisionSummary"), QStringLiteral("Review the branching flow, delayed effects and all lifecycle examples authored through Studio."));
    click(QStringLiteral("reviewSavedContent"));
    require(finalizer->ready() && finalizer->runtimeChanged() && finalizer->changes().size() >= 5 && finalizer->validationSummary().contains(QStringLiteral("%1 exact save").arg(saves)) && owned() == baseline,
        QStringLiteral("Graphical review lacks all authored definitions/validation or changed tracked content."));
    const auto change_count = finalizer->changes().size();
    require(item(QStringLiteral("revisionDefinitionDiffList"))->property("count").toLongLong() == finalizer->changes().front().toMap().value(QStringLiteral("fields")).toList().size() &&
        item(QStringLiteral("revisionFileDiffList"))->property("count").toLongLong() == finalizer->writes().size(), QStringLiteral("The rendered review omitted definition/file differences."));
    if (!screenshot.isEmpty()) {
        reveal(item(QStringLiteral("revisionDefinitionDiff")));
        QImage image; QElapsedTimer timer; timer.start();
        while (image.isNull() && timer.elapsed() < 3000) { QCoreApplication::processEvents(); image = window.grabWindow(); }
        require(!image.isNull() && image.save(screenshot, "PNG"), QStringLiteral("The rendered batch review screenshot failed."));
    }
    require(!item(QStringLiteral("finalizeReviewedContent"))->isEnabled(), QStringLiteral("Finalization was enabled before review acknowledgement."));
    field(QStringLiteral("revisionSummary"), QStringLiteral("Review the branching flow, delayed effects and five lifecycle examples as one internal batch."));
    require(!finalizer->ready(), QStringLiteral("Changing an input retained an old reviewed plan."));
    click(QStringLiteral("reviewSavedContent")); require(finalizer->ready(), finalizer->message());
    // A harmless external byte edit must still invalidate the exact reviewed plan.
    { std::ofstream file{root / draft_file, std::ios::binary | std::ios::app}; file << '\n'; require(static_cast<bool>(file), QStringLiteral("Cannot prepare the owned conflict fixture.")); }
    approve();
    require(!finalizer->ready() && finalizer->message().contains(QStringLiteral("conflict")) && !finalizer->pendingRecovery() && owned() == baseline && read(draft_file) == draft_bytes + "\n",
        QStringLiteral("External-edit conflict was not refused without overwriting either version."));
    click(QStringLiteral("reviewSavedContent")); require(finalizer->ready(), finalizer->message());
    const auto pending = finalizer->releaseSummary();
    approve();
    const auto committed = check_snapshot(root);
    require(committed.generation == prior.generation+1 && QString::fromStdString(committed.content_version) == next_version && !committed.game_version_requirement_resolved && finalizer->outcome().contains(QStringLiteral("Finalized internal batch")), QStringLiteral("The graphical batch did not finalize with an explicit pending game requirement."));
    require(browser.workingFile() == QStringLiteral("data/runtime/first-playable.json") && browser.workingVersion() == next_version && !browser.draftDirty() && read(draft_file) == draft_bytes + "\n", QStringLiteral("The committed runtime did not reopen cleanly or the source draft was overwritten."));
    const auto finalized = owned();
    click(QStringLiteral("closeContentRevision")); open();
    click(QStringLiteral("reviewSavedContent"));
    require(finalizer->ready() && !finalizer->runtimeChanged() && finalizer->writes().isEmpty(), QStringLiteral("The current runtime generated a spurious revision."));
    approve(); require(owned() == finalized && finalizer->outcome().contains(QStringLiteral("No-op")), QStringLiteral("No-op finalization changed files or versions."));
    click(QStringLiteral("closeContentRevision"));

    // Fault injection is limited to this explicit engineering mode and copy.
    require(browser.changeTag(QStringLiteral("recovery_fixture"), false) && browser.saveDraft(QStringLiteral("e1b02-recovery.json")), QStringLiteral("Could not prepare the isolated recovery draft."));
    finalizer->refresh();
    SnapshotRequest recovery{"data/drafts/e1b02-recovery.json", "data/runtime/first-playable.json", finalizer->suggestedVersion().toStdString(), "Recover the deliberately interrupted engineering fixture.", "reject-prior-content", {}};
    const auto plan = review_snapshot(root, recovery);
    bool interrupted = false;
    try { (void)finalize_snapshot(root, recovery, plan.plan_id, [](auto phase, std::size_t index) { if (phase == SnapshotWritePhase::replaced && index == 2) throw std::runtime_error("Injected engineering interruption"); }); }
    catch (const std::runtime_error&) { interrupted = true; }
    require(interrupted && fs::exists(root / ".pgp-editor/snapshot-transaction/journal.json"), QStringLiteral("The recovery fixture did not leave its durable journal."));
    open(); require(finalizer->pendingRecovery(), QStringLiteral("The review dialog hid the interrupted batch."));
    click(QStringLiteral("recoverContentRevision"));
    require(!finalizer->pendingRecovery() && finalizer->outcome().contains(QStringLiteral("Restored the prior complete generation")) && owned() == finalized, QStringLiteral("Graphical recovery did not restore every prior artifact."));
    require(browser.workingVersion() == next_version && read(draft_file) == draft_bytes + "\n", QStringLiteral("Recovery changed draft evidence or left the wrong working generation."));
    click(QStringLiteral("closeContentRevision")); open();
    click(QStringLiteral("revisionProposeGame")); field(QStringLiteral("revisionGameVersion"), QString::fromStdString(committed.required_game_version));
    click(QStringLiteral("reviewSavedContent"));
    require(finalizer->ready() && !finalizer->runtimeChanged() && finalizer->releaseSummary().contains(QStringLiteral("version requirement satisfied")), QStringLiteral("An explicit game-version proposal did not satisfy the pending requirement."));
    approve(); const auto resolved = check_snapshot(root);
    require(resolved.generation == committed.generation && resolved.collection_hash == committed.collection_hash && resolved.game_version_requirement_resolved, QStringLiteral("A game-version-only action changed runtime content or history."));
    // Leave the real completion summary visible for the optional screenshot.
    auto* outcome = item(QStringLiteral("contentRevisionOutcome"));
    reveal(outcome);
    return {{QStringLiteral("human_sessions"), 0}, {QStringLiteral("graphical_review_finalize_reopen"), true}, {QStringLiteral("reviewed_definitions"), static_cast<int>(change_count)},
        {QStringLiteral("review_is_read_only"), true}, {QStringLiteral("external_edit_preserved"), true}, {QStringLiteral("input_change_invalidates_review"), true},
        {QStringLiteral("no_op_keeps_versions"), true}, {QStringLiteral("journaled_multi_file_recovery"), true}, {QStringLiteral("pending_game_requirement"), pending},
        {QStringLiteral("explicit_game_version_resolution"), true}, {QStringLiteral("final_content_version"), QString::fromStdString(resolved.content_version)},
        {QStringLiteral("final_generation"), static_cast<int>(resolved.generation)}, {QStringLiteral("collection_hash"), QString::fromStdString(resolved.collection_hash)},
        {QStringLiteral("release_ready"), false}};
}
}
