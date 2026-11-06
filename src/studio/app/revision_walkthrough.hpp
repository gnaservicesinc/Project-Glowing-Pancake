#pragma once

#include "revision_controller.hpp"
#include "walkthrough.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <QImage>

namespace pgp::studio {
template<class Browser>
QJsonObject run_revision_walkthrough(Browser& browser, QQuickWindow& window, const QString& screenshot) {
    using namespace walkthrough;
    namespace fs = std::filesystem;
    const fs::path root{browser.checkout().toStdU16String()};
    const RevisionArchive archive{root};
    const auto generation = archive.revisions().back().generation;
    require(generation > 1, QStringLiteral("The scratch checkout needs two finalized generations."));
    const auto before = archive.open(generation - 1), after = archive.open(generation);
    constexpr auto old_flow = "slice.supplier_offer", new_flow = "slice.supplier_reviewed";
    require(before.program.collection.find(old_flow) && after.program.collection.find(new_flow),
        QStringLiteral("Use the prepared scratch revision with the supplier flow rename."));
    const auto input = root / ".work/e2-03-source.pgpsave";
    const auto export_path = root / ".work/revision-review/walkthrough";
    require(!fs::exists(input) && !fs::exists(export_path), QStringLiteral("Choose an unused scratch checkout."));
    auto* reviewer = qobject_cast<RevisionController*>(browser.revisionController());
    require(reviewer, QStringLiteral("The revision reviewer is unavailable."));
    const auto item = [&](const QString& name) {
        settle();
        auto* found = find(window.contentItem(), name);
        require(found, QStringLiteral("Missing revision control ") + name);
        for (auto* parent = found->parentItem(); parent; parent = parent->parentItem()) {
            if (!parent->property("contentY").isValid()) continue;
            auto* content = qobject_cast<QQuickItem*>(parent->property("contentItem").value<QObject*>());
            if (!content) continue;
            const auto maximum = std::max(0.0, parent->property("contentHeight").toDouble() - parent->height());
            parent->setProperty("contentY", std::clamp(found->mapToItem(content, QPointF{}).y() - 20.0, 0.0, maximum));
        }
        settle();
        return found;
    };
    const auto press = [&](const QString& name) {
        auto* control = item(name);
        require(control->isVisible() && control->isEnabled(), QStringLiteral("Revision control unavailable ") + name);
        control->forceActiveFocus();
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
        QKeyEvent up(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        QCoreApplication::sendEvent(control, &down);
        QCoreApplication::sendEvent(control, &up);
        settle();
    };
    const auto select = [&](const QString& name, const QString& text) {
        auto* control = item(name);
        const auto list = control->property("model").toStringList();
        const auto index = list.indexOf(text);
        require(index >= 0 && control->setProperty("currentIndex", index), QStringLiteral("Mapping option unavailable ") + text);
        settle();
    };
    press(QStringLiteral("openRevisionComparison"));
    press(QStringLiteral("compareSelectedGenerations"));
    require(!reviewer->differences().isEmpty() && reviewer->canMigrate(), reviewer->message());
    const auto fields = reviewer->differences().size();
    auto* tabs = item(QStringLiteral("revisionComparisonTabs"));
    tabs->setProperty("currentIndex", 1);
    press(QStringLiteral("startMatchedFixtures"));
    require(reviewer->fixtureReady() && reviewer->beforeTrace().contains(QStringLiteral("Continuation queue")) &&
        reviewer->afterTrace().contains(QStringLiteral("Continuation queue")), reviewer->message());
    press(QStringLiteral("advanceMatchedFixtures"));
    require(reviewer->beforeTrace().contains(QStringLiteral("2014-01-02")) &&
        reviewer->afterTrace().contains(QStringLiteral("2014-01-02")), QStringLiteral("Matched fixture day did not advance."));
    FlowProbe fixture{before.program, ProbeFixture{"scenario.first_playable", "US-CA", {}, 42}};
    require(fixture.prepare().trace.accepted(), QStringLiteral("Cannot prepare the original fixture save."));
    const auto bytes = persistence::encode_revision_save(before.program, before.info.game_version, fixture.state(), 0);
    persistence::atomic_write(input, bytes, [](auto) { throw std::invalid_argument("Choose a fresh scratch save."); },
        persistence::default_maximum_save_bytes, false);
    tabs->setProperty("currentIndex", 2);
    item(QStringLiteral("migrationSourceSave"))->setProperty("text", QString::fromStdU16String(input.u16string()));
    select(QStringLiteral("migrationFrom"), QString::fromLatin1(old_flow));
    select(QStringLiteral("migrationTo"), QString::fromLatin1(new_flow));
    press(QStringLiteral("addMigrationMapping"));
    require(reviewer->mappings().size() == 1, reviewer->message());
    item(QStringLiteral("migrationSummary"))->setProperty("text", QStringLiteral("Reviewed supplier flow identity and preserved the campaign state."));
    press(QStringLiteral("reviewSaveMigration"));
    require(!reviewer->planId().isEmpty() && reviewer->migrationReport().contains(QString::fromLatin1(new_flow)), reviewer->message());
    if (!screenshot.isEmpty()) {
        (void)item(QStringLiteral("acknowledgeSaveMigration"));
        const auto image = window.grabWindow();
        require(!image.isNull() && image.save(screenshot, "PNG"), QStringLiteral("The migration review screenshot failed."));
    }
    press(QStringLiteral("acknowledgeSaveMigration"));
    item(QStringLiteral("migrationExportName"))->setProperty("text", QStringLiteral("walkthrough"));
    press(QStringLiteral("exportMigratedCopy"));
    const auto output = export_path / "campaign.pgpsave";
    require(!reviewer->exportedPath().isEmpty() && fs::is_regular_file(output), reviewer->message());
    const auto new_bytes = persistence::read_bounded(output);
    const auto migrated = persistence::decode_revision_save(after.program, after.info.game_version, new_bytes);
    require(persistence::read_bounded(input) == bytes && migrated.checkpoint.world.simulation == fixture.state().world.simulation &&
        RevisionArchive{root}.stamp() == archive.stamp() && fs::is_regular_file(export_path / "review.json"),
        QStringLiteral("The copy changed original campaign state or finalized inputs."));
    return {{QStringLiteral("visible_diff_fields"), fields}, {QStringLiteral("original_sha256"), QString::fromStdString(persistence::sha256(bytes))},
        {QStringLiteral("copy_bytes"), static_cast<qint64>(new_bytes.size())}, {QStringLiteral("copy_reloaded"), true}};
}
} // namespace pgp::studio
