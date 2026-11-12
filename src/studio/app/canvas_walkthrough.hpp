#pragma once
#include "canvas_controller.hpp"
#include "walkthrough.hpp"
#include <pgp/persistence/sha256.hpp>
#include <QMouseEvent>

namespace pgp::studio {
// Continue the E1A engineering example through the actual canvas controls.
// Run only on the explicitly selected disposable checkout used by that example.
template<class Browser>
QJsonObject run_canvas_walkthrough(Browser& browser, QQuickWindow& window) {
    using namespace walkthrough; namespace fs = std::filesystem;
    auto* canvas = qobject_cast<CanvasController*>(browser.canvasController());
    auto* inspector = qobject_cast<ProbeController*>(browser.probeController());
    require(canvas && inspector, QStringLiteral("Canvas/inspection controllers are missing."));
    const fs::path root{browser.checkout().toStdU16String()};
    const fs::path draft{"data/drafts/e1a05-walkthrough.json"};
    const auto runtime_bytes = persistence::read_bounded(root / "data/runtime/first-playable.json", 4U * 1024U * 1024U);
    const auto version_bytes = persistence::read_bounded(root / "project-versions.json", 65536);
    window.setProperty("page", 2); settle();
    const auto item = [&](const QString& name) {
        settle(); auto* value = find(window.contentItem(), name);
        require(value != nullptr, QStringLiteral("Missing canvas control: ") + name); return value;
    };
    quint64 pointer_tick = 1;
    const auto pointer = [&](QEvent::Type type, const QPointF& point, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event{type, point, QPointF{window.mapToGlobal(point.toPoint())}, button, buttons, Qt::NoModifier};
        event.setTimestamp(pointer_tick++ * 20); QCoreApplication::sendEvent(&window, &event); settle();
    };
    const auto click = [&](const QString& name) {
        auto* value = item(name); require(value->isEnabled() && value->isVisible(), QStringLiteral("Unavailable canvas control: ") + name);
        if (name.startsWith(QStringLiteral("canvasPort_"))) {
            const auto point = value->mapToScene(QPointF{value->width() / 2, value->height() / 2});
            pointer(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
            pointer(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        } else require(QMetaObject::invokeMethod(value, "clicked"), QStringLiteral("Missing canvas click handler: ") + name);
        settle();
    };
    const auto focus = [&](const QString& key) {
        item(QStringLiteral("canvasSearch"))->setProperty("text", QString{});
        int index = -1;
        for (qsizetype i = 0; i < canvas->nodes().size(); ++i)
            if (canvas->nodes().at(i).toMap().value(QStringLiteral("key")).toString() == key) index = static_cast<int>(i);
        require(index >= 0, QStringLiteral("Missing canvas box: ") + key);
        item(QStringLiteral("canvasNavigator"))->setProperty("currentIndex", index); click(QStringLiteral("focusCanvasNode"));
        require(canvas->selectedKey() == key, QStringLiteral("Canvas navigator and form selection diverged."));
    };
    const auto port = [&](const QString& id) {
        focus(id.left(id.indexOf(QChar{'@'}))); click(QStringLiteral("canvasPort_") + id);
    };
    const auto create = [&](const QString& value) {
        int index = -1;
        for (qsizetype i = 0; i < canvas->palette().size(); ++i)
            if (canvas->palette().at(i).toMap().value(QStringLiteral("value")).toString() == value) index = static_cast<int>(i);
        require(index >= 0, QStringLiteral("Missing typed canvas palette item: ") + value);
        item(QStringLiteral("canvasPalette"))->setProperty("currentIndex", index); click(QStringLiteral("insertCanvasNode"));
        require(browser.draftMessage().isEmpty(), browser.draftMessage());
    };
    const auto before_failed = inspector->report();
    port(QStringLiteral("trigger@result")); port(QStringLiteral("node/effect/effect/0@argument/amount_cents"));
    require(canvas->message().contains(QStringLiteral("mismatch")) && !browser.draftDirty() && inspector->report() == before_failed,
        QStringLiteral("An incompatible visual connection changed content or the actual fixture."));
    click(QStringLiteral("cancelCanvasLink"));
    port(QStringLiteral("trigger/expression@value")); port(QStringLiteral("node/effect/effect/0@argument/amount_cents"));
    require(browser.draftDirty() && !inspector->ready(), QStringLiteral("A compatible canvas connection did not update content and invalidate the old fixture."));
    click(QStringLiteral("undoStudioContent"));
    require(!browser.draftDirty(), QStringLiteral("Canvas edits did not share the form/outline undo history."));
    require(item(QStringLiteral("formInput/argument/amount_cents"))->property("text").toString() == QStringLiteral("-250"),
        QStringLiteral("Undo did not restore the shared effect form value."));
    port(QStringLiteral("node/effect@next")); create(QStringLiteral("insert/news"));
    require(canvas->nodeLookup().contains(QStringLiteral("node/news")), QStringLiteral("Canvas palette failed to insert the selected execution node."));
    click(QStringLiteral("undoStudioContent")); require(!browser.draftDirty(), QStringLiteral("Undo did not restore the saved graph after palette creation."));
    focus(QStringLiteral("node/sequence")); click(QStringLiteral("formAction/append"));
    port(QStringLiteral("node/sequence@child/1")); create(QStringLiteral("insert/news"));
    click(QStringLiteral("saveExistingDraft")); require(!browser.draftDirty(), QStringLiteral("Canvas changes did not save through the existing draft control."));
    require(!canvas->warnings().isEmpty(), QStringLiteral("The shared waiting-sibling compiler notice was hidden."));
    click(QStringLiteral("showCanvasNotices"));
    auto* notices = window.findChild<QObject*>(QStringLiteral("canvasNotices"));
    auto* notice_text = window.findChild<QObject*>(QStringLiteral("canvasNoticesText"));
    require(notices && notices->property("visible").toBool() && notice_text && notice_text->property("text").toString().contains(QStringLiteral("waiting_sibling")),
        QStringLiteral("The compiler notice did not open in its readable keyboard-accessible view."));
    require(QMetaObject::invokeMethod(notices, "close"), QStringLiteral("The compiler notice did not dismiss.")); settle();
    const auto saved_bytes = persistence::read_bounded(root / draft, 4U * 1024U * 1024U);
    DraftDocument saved{root, draft}; const auto semantic_hash = persistence::sha256(saved.program().canonical_content_bytes);
    window.setProperty("page", 3);
    item(QStringLiteral("flowTestSeed"))->setProperty("text", QStringLiteral("42")); click(QStringLiteral("startFlowTest"));
    window.setProperty("page", 2);
    require(inspector->ready() && inspector->graphTrace().value(QStringLiteral("node/sequence/child/1")).toMap().value(QStringLiteral("state")).toString() == QStringLiteral("queued"),
        QStringLiteral("The actual saved sibling wait was not projected on the canvas."));
    focus(QStringLiteral("node/sequence"));
    require(item(QStringLiteral("canvasPortState_node/sequence@child/1"))->property("text").toString() == QStringLiteral("QUEUED"),
        QStringLiteral("The visible numbered port does not show its queued continuation."));
    const auto before_drag = canvas->nodeLookup().value(QStringLiteral("node/sequence")).toMap();
    auto* header = item(QStringLiteral("canvasNode_node/sequence"));
    const auto start = header->mapToScene(QPointF{header->width() / 3, header->height() / 2});
    pointer(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    pointer(QEvent::MouseMove, start + QPointF{20, 0}, Qt::NoButton, Qt::LeftButton);
    pointer(QEvent::MouseMove, start + QPointF{52, 16}, Qt::NoButton, Qt::LeftButton);
    require(!canvas->layoutDirty() && canvas->nodeLookup().value(QStringLiteral("node/sequence")).toMap().value(QStringLiteral("x")) == before_drag.value(QStringLiteral("x")),
        QStringLiteral("A drag wrote layout positions before its release."));
    pointer(QEvent::MouseButtonRelease, start + QPointF{52, 16}, Qt::LeftButton, Qt::NoButton);
    require(canvas->layoutDirty() && canvas->nodeLookup().value(QStringLiteral("node/sequence")).toMap().value(QStringLiteral("x")) != before_drag.value(QStringLiteral("x")),
        QStringLiteral("Pointer drag failed to commit a grid-aligned box position on release."));
    const auto before_move = canvas->nodeLookup().value(QStringLiteral("node/sequence")).toMap();
    auto* workspace = item(QStringLiteral("flowCanvas")); workspace->forceActiveFocus();
    QKeyEvent move(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier); QCoreApplication::sendEvent(workspace, &move); settle();
    const auto moved = canvas->nodeLookup().value(QStringLiteral("node/sequence")).toMap();
    require(moved.value(QStringLiteral("x")).toInt() == before_move.value(QStringLiteral("x")).toInt() + canvas_grid && canvas->layoutDirty() && !browser.draftDirty(),
        QStringLiteral("Keyboard box movement changed runtime data or failed to use the integer layout grid."));
    click(QStringLiteral("saveCanvasLayout")); require(!canvas->layoutDirty(), QStringLiteral("Explicit layout save failed."));
    click(QStringLiteral("canvasZoomIn"));
    workspace->forceActiveFocus(); QKeyEvent pan(QEvent::KeyPress, Qt::Key_Right, Qt::ControlModifier); QCoreApplication::sendEvent(workspace, &pan); settle();
    const auto local_view = canvas->view();
    const auto layout = LayoutDocument{root, "walkthrough.branching"}.positions();
    require(layout.at("node/sequence").x == moved.value(QStringLiteral("x")).toInt(), QStringLiteral("Integer layout positions did not persist."));
    require(persistence::read_bounded(root / draft, 4U * 1024U * 1024U) == saved_bytes && !browser.draftDirty(), QStringLiteral("Layout or viewport gestures rewrote authored runtime semantics."));
    click(QStringLiteral("closeWorkingDraft")); require(!browser.editing(), QStringLiteral("The saved canvas document did not close."));
    require(browser.beginDraft(QStringLiteral("data/drafts/e1a05-walkthrough.json"), QStringLiteral("walkthrough.branching")), browser.draftMessage());
    require(canvas->nodeLookup().value(QStringLiteral("node/sequence")).toMap().value(QStringLiteral("x")).toInt() == moved.value(QStringLiteral("x")).toInt(),
        QStringLiteral("Reopening lost shared layout positions."));
    require(canvas->view().value(QStringLiteral("x")) == local_view.value(QStringLiteral("x")) && canvas->view().value(QStringLiteral("zoom")) == local_view.value(QStringLiteral("zoom")),
        QStringLiteral("Reopening lost ignored viewport state."));
    require(DraftDocument{root, draft}.program().canonical_content_bytes == saved.program().canonical_content_bytes,
        QStringLiteral("Reopening graph/forms changed the saved logic."));
    window.setProperty("page", 3);
    item(QStringLiteral("flowTestSeed"))->setProperty("text", QStringLiteral("42")); click(QStringLiteral("startFlowTest"));
    click(QStringLiteral("reloadFlowTest"));
    window.setProperty("page", 2); focus(QStringLiteral("node/sequence"));
    require(inspector->graphTrace().value(QStringLiteral("node/sequence/child/1")).toMap().value(QStringLiteral("state")).toString() == QStringLiteral("queued"),
        QStringLiteral("Exact checkpoint reload lost the waiting sibling overlay."));
    window.resize(900, 720); settle();
    auto* viewport = item(QStringLiteral("canvasViewport"));
    QElapsedTimer resize_timer; resize_timer.start();
    while (viewport->width() <= 450 && resize_timer.elapsed() < 3000) QCoreApplication::processEvents();
    require(viewport->width() > 450, QStringLiteral("The narrow-window canvas did not use its pane: viewport %1, window %2, inspector visible %3.")
        .arg(viewport->width()).arg(window.width()).arg(item(QStringLiteral("studioInspector"))->isVisible()));
    click(QStringLiteral("showStudioSettings"));
    require(item(QStringLiteral("studioPreview"))->isVisible() && !item(QStringLiteral("flowCanvas"))->isVisible(), QStringLiteral("Narrow-window settings navigation failed."));
    click(QStringLiteral("showStudioCanvas"));
    require(item(QStringLiteral("flowCanvas"))->isVisible(), QStringLiteral("Narrow-window navigation lost the canvas."));
    for (int i = 0; i < 4; ++i) click(QStringLiteral("canvasZoomIn"));
    require(canvas->view().value(QStringLiteral("zoom")).toInt() == 2000 && !browser.draftDirty() && !canvas->layoutDirty(),
        QStringLiteral("Canvas zoom did not reach 200% without changing authored content/layout."));
    window.resize(1280, 720); settle(); focus(QStringLiteral("node/sequence"));
    require(persistence::read_bounded(root / "data/runtime/first-playable.json", 4U * 1024U * 1024U) == runtime_bytes &&
        persistence::read_bounded(root / "project-versions.json", 65536) == version_bytes, QStringLiteral("Canvas editing changed reviewed runtime or component versions."));
    return {{QStringLiteral("visual_type_refusal"), true}, {QStringLiteral("graph_form_undo_parity"), true}, {QStringLiteral("palette_and_numbered_routes"), true},
        {QStringLiteral("waiting_sibling_after_exact_reload"), true}, {QStringLiteral("pointer_ports_and_drag"), true}, {QStringLiteral("position_committed_on_release"), true},
        {QStringLiteral("keyboard_move_grid"), canvas_grid}, {QStringLiteral("layout_save_reopen"), true},
        {QStringLiteral("compiler_notices"), static_cast<int>(canvas->warnings().size())},
        {QStringLiteral("narrow_window_navigation"), true}, {QStringLiteral("canvas_zoom_percent_checked"), 200},
        {QStringLiteral("viewport_local_reopen"), true}, {QStringLiteral("layout_excluded_from_runtime_identity"), true}, {QStringLiteral("draft_collection_hash"), QString::fromStdString(semantic_hash)},
        {QStringLiteral("human_sessions"), 0}};
}
} // namespace pgp::studio
