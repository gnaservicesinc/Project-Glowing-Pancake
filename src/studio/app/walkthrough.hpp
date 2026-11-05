#pragma once

#include "probe_controller.hpp"
#include <pgp/persistence/storage.hpp>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <algorithm>
#include <stdexcept>

namespace pgp::studio {
namespace walkthrough {
inline void require(bool value, const QString& explanation) {
    if (!value) throw std::runtime_error((QStringLiteral("Authoring walkthrough: ") + explanation).toStdString());
}
inline void settle() {
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}
inline QQuickItem* find(QQuickItem* root, const QString& name) {
    if (root->objectName() == name) return root;
    for (auto* child : root->childItems()) if (auto* item = find(child, name)) return item;
    return nullptr;
}
inline std::int64_t cash_delta(const QString& report) {
    const auto match = QRegularExpression(QStringLiteral("cash (-?[0-9]+) → (-?[0-9]+) constant")).match(report);
    require(match.hasMatch(), QStringLiteral("The actual before/after cash report is missing."));
    return match.captured(2).toLongLong() - match.captured(1).toLongLong();
}
inline std::int64_t reviewed_effect_delta(const QString& report) {
    const auto match = QRegularExpression(QStringLiteral("walkthrough.branching / effect · effect 1 · adjust_cash · company · company.player · (-?[0-9]+) → (-?[0-9]+)")).match(report);
    require(match.hasMatch(), QStringLiteral("The reviewed effect's actual target/before/after receipt is missing."));
    return match.captured(2).toLongLong() - match.captured(1).toLongLong();
}
inline void reveal_report(QQuickWindow& window) {
    auto* report = find(window.contentItem(), QStringLiteral("flowTestReport"));
    require(report != nullptr, QStringLiteral("The rendered inspection report is missing."));
    for (auto* parent = report->parentItem(); parent; parent = parent->parentItem()) {
        if (!parent->property("contentY").isValid()) continue;
        auto* content = qobject_cast<QQuickItem*>(parent->property("contentItem").value<QObject*>());
        if (!content) continue;
        const auto maximum = std::max(0.0, parent->property("contentHeight").toDouble() - parent->height());
        parent->setProperty("contentY", std::clamp(report->mapToItem(content, QPointF{}).y(), 0.0, maximum));
    }
    report->forceActiveFocus();
    settle();
}
}

// An explicit engineering path through the same QML controls and typed forms
// used by authors. It writes one new draft in the explicitly selected checkout;
// it never finalizes content, fabricates human evidence or edits runtime JSON.
template<class Browser>
QJsonObject run_authoring_walkthrough(Browser& browser, QQuickWindow& window) {
    using namespace walkthrough;
    namespace fs = std::filesystem;
    const fs::path root{browser.checkout().toStdU16String()};
    constexpr auto source = "data/runtime/first-playable.json";
    constexpr auto draft = "data/drafts/e1a05-walkthrough.json";
    require(!fs::exists(root / draft), QStringLiteral("Choose a fresh checkout copy; the walkthrough draft already exists."));
    const auto before_source = persistence::read_bounded(root / source, 4U * 1024U * 1024U);
    const auto before_versions = persistence::read_bounded(root / "project-versions.json", 65536);
    const auto item = [&](const QString& name) {
        settle();
        auto* found = find(window.contentItem(), name);
        require(found != nullptr, QStringLiteral("Missing visible control: ") + name);
        return found;
    };
    const auto click = [&](const QString& name) {
        auto* control = item(name);
        require(control->isEnabled(), QStringLiteral("Disabled control: ") + name);
        require(QMetaObject::invokeMethod(control, "clicked"), QStringLiteral("No click handler: ") + name);
        settle();
    };
    const auto activate = [&](QQuickItem* control, int index) {
        require(index >= 0, QStringLiteral("The requested typed choice is unavailable."));
        control->setProperty("currentIndex", index);
        require(QMetaObject::invokeMethod(control, "activated", Q_ARG(int, index)), QStringLiteral("No typed-choice handler."));
        settle();
    };
    const auto field = [&](const QString& key) {
        for (const auto& row : browser.formFields()) {
            const auto value = row.toMap();
            if (value.value(QStringLiteral("key")).toString() == key) return value;
        }
        throw std::runtime_error((QStringLiteral("Missing form field: ") + key).toStdString());
    };
    const auto select = [&](const QString& key) {
        qsizetype index = -1;
        for (qsizetype i = 0; i < browser.flowOutline().size(); ++i)
            if (browser.flowOutline().at(i).toMap().value(QStringLiteral("key")).toString() == key) { index = i; break; }
        require(index >= 0, QStringLiteral("Missing outline row: ") + key);
        auto* outline = item(QStringLiteral("flowOutline"));
        outline->forceActiveFocus();
        QKeyEvent home(QEvent::KeyPress, Qt::Key_Home, Qt::NoModifier);
        QCoreApplication::sendEvent(outline, &home);
        for (qsizetype i = 0; i < index; ++i) {
            QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
            QCoreApplication::sendEvent(outline, &down);
        }
        settle();
        require(browser.outlineIndex() == index, QStringLiteral("Keyboard outline selection failed: ") + key);
    };
    int submissions = 0;
    const auto change = [&](const QString& row, const QString& key, const QString& value = {}) {
        select(row);
        const auto projection = field(key);
        const auto type = projection.value(QStringLiteral("control")).toString();
        if (type == QStringLiteral("choice")) {
            const auto options = projection.value(QStringLiteral("options")).toList();
            int index = -1;
            for (qsizetype i = 0; i < options.size(); ++i)
                if (options.at(i).toMap().value(QStringLiteral("value")).toString() == value) index = static_cast<int>(i);
            activate(item(QStringLiteral("formChoice/") + key), index);
        } else if (type == QStringLiteral("button")) click(QStringLiteral("formAction/") + key);
        else {
            auto* control = item(QStringLiteral("formInput/") + key);
            control->setProperty("text", value);
            require(QMetaObject::invokeMethod(control, "accepted"), QStringLiteral("No form input handler: ") + key);
            settle();
        }
        ++submissions;
        require(browser.draftMessage().isEmpty(), browser.draftMessage());
    };
    const auto choose_flow = [&](const QString& id) {
        activate(item(QStringLiteral("workingFlow")), static_cast<int>(browser.workingFlows().indexOf(id)));
        require(browser.workingFlow() == id, QStringLiteral("Working-flow selection failed."));
    };
    const auto create = [&](const QString& id) {
        item(QStringLiteral("newFlowId"))->setProperty("text", id);
        int scope = -1;
        for (qsizetype i = 0; i < browser.flowScopes().size(); ++i)
            if (browser.flowScopes().at(i).toMap().value(QStringLiteral("value")).toString() == QStringLiteral("company")) scope = static_cast<int>(i);
        require(scope >= 0, QStringLiteral("The company scope is unavailable."));
        item(QStringLiteral("newFlowScope"))->setProperty("currentIndex", scope);
        click(QStringLiteral("createFlow"));
        require(browser.workingFlow() == id && browser.draftMessage().isEmpty(), browser.draftMessage());
        change(QStringLiteral("trigger"), QStringLiteral("fact"), QStringLiteral("company.is_operating"));
    };
    const auto open = [&](const QString& file, const QString& id) {
        const auto index = browser.locate(file, id);
        require(index >= 0, QStringLiteral("The browsed definition is missing: ") + id);
        item(QStringLiteral("studioLibrary"))->setProperty("currentIndex", index);
        click(QStringLiteral("editSelectedDefinition"));
        require(browser.editing() && browser.workingFile() == file, QStringLiteral("Opening the working collection failed."));
    };
    auto* query = item(QStringLiteral("librarySearch"));
    query->setProperty("text", QStringLiteral("slice."));
    require(QMetaObject::invokeMethod(query, "textEdited"), QStringLiteral("Library search handler is missing."));
    require(!browser.rows().isEmpty(), QStringLiteral("Library search lost the slice flows."));
    query->setProperty("text", QString{});
    (void)QMetaObject::invokeMethod(query, "textEdited");
    open(QString::fromLatin1(source), QStringLiteral("slice.liquidity_warning"));

    constexpr auto branch = "walkthrough.branching";
    create(QString::fromLatin1(branch));
    change(QStringLiteral("flow"), QStringLiteral("scenario/scenario.first_playable"), QStringLiteral("true"));
    change(QStringLiteral("flow"), QStringLiteral("entry/insert"), QStringLiteral("sequence"));
    change(QStringLiteral("node/sequence/child/0"), QStringLiteral("next/insert"), QStringLiteral("effect"));
    change(QStringLiteral("node/effect/effect/0"), QStringLiteral("operation"), QStringLiteral("adjust_cash"));
    change(QStringLiteral("node/effect/effect/0"), QStringLiteral("argument/amount_cents"), QStringLiteral("250"));
    select(QStringLiteral("node/effect/effect/0"));
    const auto targets = field(QStringLiteral("target")).value(QStringLiteral("options")).toList();
    for (const auto& option : targets)
        require(option.toMap().value(QStringLiteral("value")).toString() != QStringLiteral("world"), QStringLiteral("Cash form offered the wrong target scope."));
    require(!browser.changeForm(browser.formTicket(), QStringLiteral("target"), QStringLiteral("world")), QStringLiteral("The typed form accepted a wrong-scope target."));
    const auto scope_error = browser.draftMessage();
    require(!scope_error.isEmpty(), QStringLiteral("A scope refusal needs an explanation."));
    change(QStringLiteral("node/effect"), QStringLiteral("append"));
    change(QStringLiteral("node/effect/effect/1"), QStringLiteral("argument/value"), QStringLiteral("walkthrough_opened"));
    change(QStringLiteral("node/effect"), QStringLiteral("next/insert"), QStringLiteral("delay"));
    change(QStringLiteral("node/delay"), QStringLiteral("days"), QStringLiteral("2"));
    change(QStringLiteral("node/delay"), QStringLiteral("id"), QStringLiteral("review_wait"));
    change(QStringLiteral("node/sequence"), QStringLiteral("next/insert"), QStringLiteral("dialogue"));
    change(QStringLiteral("node/dialogue"), QStringLiteral("append"));
    change(QStringLiteral("node/dialogue/choice/0"), QStringLiteral("id"), QStringLiteral("decline"));
    change(QStringLiteral("node/dialogue/choice/1"), QStringLiteral("id"), QStringLiteral("accept"));
    change(QStringLiteral("node/dialogue/choice/1"), QStringLiteral("next/insert"), QStringLiteral("result"));
    change(QStringLiteral("node/result"), QStringLiteral("next/insert"), QStringLiteral("delay"));
    change(QStringLiteral("node/delay"), QStringLiteral("next/insert"), QStringLiteral("effect"));
    change(QStringLiteral("node/effect.2/effect/0"), QStringLiteral("argument/value"), QStringLiteral("walkthrough_accepted"));
    change(QStringLiteral("node/dialogue/choice/0"), QStringLiteral("next/insert"), QStringLiteral("result"));
    // Fund the outside pool through an actual prior debit. The planted credit
    // must remain a conserving transfer rather than an unfunded money source.
    change(QStringLiteral("flow"), QStringLiteral("entry/insert"), QStringLiteral("effect"));
    change(QStringLiteral("node/effect.3/effect/0"), QStringLiteral("operation"), QStringLiteral("adjust_cash"));
    change(QStringLiteral("node/effect.3/effect/0"), QStringLiteral("argument/amount_cents"), QStringLiteral("-1000"));
    change(QStringLiteral("node/effect.3"), QStringLiteral("id"), QStringLiteral("funded_pool"));

    auto* probe = qobject_cast<ProbeController*>(browser.probeController());
    require(probe != nullptr, QStringLiteral("The real inspection controller is missing."));
    const auto start = [&] {
        item(QStringLiteral("flowTestSeed"))->setProperty("text", QStringLiteral("42"));
        click(QStringLiteral("startFlowTest"));
        require(probe->ready() && !probe->report().isEmpty() && probe->status().startsWith(QStringLiteral("accepted")), probe->status());
    };
    start();
    const auto inverted = probe->report();
    require(cash_delta(inverted) == -750 && reviewed_effect_delta(inverted) == 250 && inverted.contains(QStringLiteral("review_wait")), QStringLiteral("The planted positive sign was not exposed in actual cash/queue inspection.\n") + inverted);
    change(QStringLiteral("node/effect/effect/0"), QStringLiteral("argument/amount_cents"), QStringLiteral("-250"));
    require(!probe->ready(), QStringLiteral("An edit retained stale inspection state."));
    start();
    const auto corrected = probe->report();
    require(cash_delta(corrected) == -1250 && reviewed_effect_delta(corrected) == -250, QStringLiteral("The corrected sign/scope was not reflected in actual inspection."));
    change(QStringLiteral("trigger"), QStringLiteral("fact"), QStringLiteral("company.cash_cents"));
    change(QStringLiteral("trigger"), QStringLiteral("comparison"), QStringLiteral("gte"));
    start();
    click(QStringLiteral("inspectFlowBoundaries"));
    require(probe->report().contains(QStringLiteral("boundary overrides")), QStringLiteral("Threshold boundaries did not render."));
    const auto boundaries = probe->report();

    const QStringList modes{QStringLiteral("once"), QStringLiteral("recurring"), QStringLiteral("interval"), QStringLiteral("scheduled"), QStringLiteral("continuous")};
    for (qsizetype i = 1; i < modes.size(); ++i) {
        create(QStringLiteral("walkthrough.") + modes.at(i));
        change(QStringLiteral("lifecycle"), QStringLiteral("mode"), modes.at(i));
        if (modes.at(i) == QStringLiteral("scheduled")) change(QStringLiteral("lifecycle"), QStringLiteral("due"), QStringLiteral("2014-01-02"));
    }
    choose_flow(QString::fromLatin1(branch));
    item(QStringLiteral("draftFilename"))->setProperty("text", QStringLiteral("e1a05-walkthrough.json"));
    click(QStringLiteral("saveDraftAs"));
    require(browser.existingDraft() && !browser.draftDirty(), QStringLiteral("Saving the new separate draft failed."));
    click(QStringLiteral("closeWorkingDraft"));
    require(!browser.editing(), QStringLiteral("The saved draft did not close."));
    open(QString::fromLatin1(draft), QString::fromLatin1(branch));
    QJsonArray reopened;
    for (const auto& mode : modes) {
        choose_flow(mode == QStringLiteral("once") ? QString::fromLatin1(branch) : QStringLiteral("walkthrough.") + mode);
        select(QStringLiteral("lifecycle"));
        require(field(QStringLiteral("mode")).value(QStringLiteral("value")).toString() == mode, QStringLiteral("Reopening changed a lifecycle."));
        start();
        click(QStringLiteral("reloadFlowTest"));
        require(probe->ready(), QStringLiteral("A reopened lifecycle failed exact checkpoint reload."));
        if (mode == QStringLiteral("continuous")) require(probe->cleanupOwners().size() == 1, QStringLiteral("The reopened continuous flow did not enter naturally."));
        reopened.push_back(mode);
    }
    choose_flow(QString::fromLatin1(branch));
    start();
    click(QStringLiteral("reloadFlowTest"));
    const auto advance = [&](const QString& date) {
        item(QStringLiteral("flowAdvanceDate"))->setProperty("text", date);
        click(QStringLiteral("advanceFlowTest"));
        QElapsedTimer timer; timer.start();
        while (probe->busy() && timer.elapsed() < 3000) QCoreApplication::processEvents();
        require(!probe->busy() && probe->date() == date && probe->status().startsWith(QStringLiteral("accepted")), QStringLiteral("The real delayed continuation did not reach ") + date + QStringLiteral(": ") + probe->status());
    };
    advance(QStringLiteral("2014-01-03"));
    require(probe->presentations().size() == 1 && find(window.contentItem(), QStringLiteral("flowChoice_accept")), QStringLiteral("The shared branching dialogue did not render."));
    click(QStringLiteral("flowChoice_accept"));
    require(probe->presentations().size() == 2 && probe->presentations().at(1).toMap().value(QStringLiteral("source")).toString() == QStringLiteral("walkthrough.branching / result"), QStringLiteral("The accepted branch did not show only its own result."));
    const auto accepted = probe->report();
    click(QStringLiteral("reloadFlowTest"));
    advance(QStringLiteral("2014-01-04"));
    const auto hidden = probe->report();
    require(hidden.count(QStringLiteral("walkthrough.branching / effect.2 · effect 1 · record_decision")) == 1 && hidden.contains(QStringLiteral("walkthrough_accepted")), QStringLiteral("The delayed hidden consequence did not resume exactly once."));
    require(persistence::read_bounded(root / source, 4U * 1024U * 1024U) == before_source &&
            persistence::read_bounded(root / "project-versions.json", 65536) == before_versions,
            QStringLiteral("An ordinary form/draft operation changed reviewed runtime content or versions."));
    require(!browser.draftDirty(), QStringLiteral("Inspection changed the saved authored document."));
    return {{QStringLiteral("format"), 1}, {QStringLiteral("kind"), QStringLiteral("engineering-walkthrough")},
        {QStringLiteral("human_sessions"), 0}, {QStringLiteral("checkout"), browser.checkout()},
        {QStringLiteral("draft"), QString::fromLatin1(draft)}, {QStringLiteral("form_submissions"), submissions},
        {QStringLiteral("reopened_lifecycles"), reopened}, {QStringLiteral("wrong_scope_refused"), scope_error},
        {QStringLiteral("funded_pool_debit_cents"), -1000},
        {QStringLiteral("planted_cash_delta_cents"), 250}, {QStringLiteral("corrected_cash_delta_cents"), -250},
        {QStringLiteral("runtime_and_versions_unchanged"), true},
        {QStringLiteral("reports"), QJsonObject{{QStringLiteral("inverted_effect"), inverted}, {QStringLiteral("corrected_effect"), corrected},
            {QStringLiteral("threshold_boundaries"), boundaries}, {QStringLiteral("accepted_branch"), accepted}, {QStringLiteral("resumed_hidden_consequence"), hidden}}}};
}
} // namespace pgp::studio
