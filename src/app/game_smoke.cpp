#include "game_smoke.hpp"

#include "campaign_controller.hpp"
#include <pgp/graphics/graphics_policy.hpp>

#include <QAccessible>
#include <QCoreApplication>
#include <QImage>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

namespace {
bool key(QQuickWindow& window,QQuickItem* item,Qt::Key code) {
    if (!item || !item->isEnabled() || !item->isVisible()) return false;
    item->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent press{QEvent::KeyPress,code,Qt::NoModifier};
    QKeyEvent release{QEvent::KeyRelease,code,Qt::NoModifier};
    QCoreApplication::sendEvent(&window,&press);
    QCoreApplication::sendEvent(&window,&release);
    return true;
}

struct SmokeState {
    enum class Phase { opening, stepping, playing, pausing, saving, advancing, loading, rejecting, settings };
    Phase phase = Phase::opening;
    int attempts = 0;
    pgp::bridge::CampaignSnapshotPtr saved;
    pgp::core::OrdinalDay date{0};
    QString project;
};
}

void schedule_game_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot) {
    using Phase = SmokeState::Phase;
    const auto save_path = paths.saves_directory / "shell-smoke.pgpsave";
    const auto save_url = QUrl::fromLocalFile(QString::fromStdU16String(save_path.u16string()));
    auto* timer = new QTimer(&window);
    timer->setInterval(40);
    auto smoke = std::make_shared<SmokeState>();
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,smoke,expected,save_path,save_url,paths,screenshot] {
        const auto item = [&](const char* name) { return window.findChild<QQuickItem*>(QString::fromLatin1(name)); };
        const auto fail = [&](const char* reason) {
            qCritical() << "Campaign smoke failed:" << reason << "phase" << static_cast<int>(smoke->phase) << campaign.notice();
            timer->stop();
            QCoreApplication::exit(3);
        };
        if (++smoke->attempts > 200) { fail("timed out"); return; }
        if (!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        switch (smoke->phase) {
        case Phase::opening: {
            if (window.rendererInterface()->graphicsApi() != expected || !campaign.paused() ||
                std::filesystem::exists(save_path) || std::filesystem::exists(paths.settings_file)) {
                fail("expected renderer, paused opening and fresh scratch storage"); return;
            }
            if (campaign.requiredKind() != QStringLiteral("none")) {
                fail("use the business engineering fixture for this smoke"); return;
            }
            auto* step = item("stepButton");
            auto* accessible = step ? QAccessible::queryAccessibleInterface(step) : nullptr;
            if (!accessible || accessible->text(QAccessible::Name).isEmpty() ||
                step->mapToScene(QPointF{}).y() + step->height() > window.height()) {
                if (step) qCritical() << step->mapToScene(QPointF{}) << step->size()
                                      << (accessible ? accessible->text(QAccessible::Name) : QStringLiteral("unavailable"));
                fail("step control is outside the window or has no accessible name"); return;
            }
            if (!key(window,item("labTab"),Qt::Key_Space) || window.property("room").toString() != QStringLiteral("lab") ||
                !key(window,item("inboxTab"),Qt::Key_Space) || window.property("room").toString() != QStringLiteral("inbox") ||
                !key(window,item("labTab"),Qt::Key_Space) || campaign.selectedProjectId().isEmpty()) {
                fail("keyboard room navigation or initial project selection"); return;
            }
            campaign.projects()->setSortByTitle(true);
            smoke->project = campaign.selectedProjectId();
            smoke->date = campaign.snapshot()->checkpoint.world.simulation.date;
            if (!key(window,step,Qt::Key_Space)) { fail("step control unavailable"); return; }
            smoke->phase = Phase::stepping;
            break;
        }
        case Phase::stepping: {
            if (campaign.snapshot()->checkpoint.world.simulation.date != pgp::core::advance_days(smoke->date,pgp::core::Days{1}) ||
                !campaign.paused()) { fail("step did not advance one paused day"); return; }
            smoke->date = campaign.snapshot()->checkpoint.world.simulation.date;
            if (!key(window,item("pauseButton"),Qt::Key_Space) || !item("projectSearch")) {
                fail("play control or project search unavailable"); return;
            }
            item("projectSearch")->forceActiveFocus(Qt::TabFocusReason);
            QKeyEvent edit{QEvent::KeyPress,Qt::Key_D,Qt::NoModifier,QStringLiteral("D")};
            QCoreApplication::sendEvent(&window,&edit);
            smoke->phase = Phase::playing;
            break;
        }
        case Phase::playing:
            if (campaign.paused()) { fail("play did not resume the clock"); return; }
            if (campaign.snapshot()->checkpoint.world.simulation.date < pgp::core::advance_days(smoke->date,pgp::core::Days{3})) return;
            if (window.activeFocusItem() != item("projectSearch") || campaign.selectedProjectId() != smoke->project ||
                item("projectSearch")->property("text").toString() != QStringLiteral("D") ||
                campaign.projects()->filterText() != QStringLiteral("D")) {
                fail("running snapshot changed edit focus or project selection"); return;
            }
            if (!key(window,item("pauseButton"),Qt::Key_Space)) { fail("pause control unavailable"); return; }
            smoke->phase = Phase::pausing;
            break;
        case Phase::pausing:
            if (!campaign.paused()) { fail("clock did not pause"); return; }
            smoke->saved = campaign.snapshot();
            campaign.saveTo(save_url);
            smoke->phase = Phase::saving;
            break;
        case Phase::saving:
            if (!std::filesystem::is_regular_file(save_path)) { fail("manual save was not written"); return; }
            campaign.step();
            smoke->phase = Phase::advancing;
            break;
        case Phase::advancing:
            if (campaign.snapshot()->checkpoint.world.simulation.date == smoke->saved->checkpoint.world.simulation.date) {
                fail("advance after saving failed"); return;
            }
            campaign.loadFrom(save_url);
            smoke->phase = Phase::loading;
            break;
        case Phase::loading:
            if (campaign.snapshot()->generation != smoke->saved->generation + 1 ||
                campaign.snapshot()->checkpoint != smoke->saved->checkpoint || !campaign.paused()) {
                fail("load did not restore the exact saved checkpoint"); return;
            }
            smoke->saved = campaign.snapshot();
            campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdU16String((save_path.parent_path() / "absent.pgpsave").u16string())));
            smoke->phase = Phase::rejecting;
            break;
        case Phase::rejecting:
            if (campaign.notice().isEmpty() || campaign.snapshot()->generation != smoke->saved->generation ||
                campaign.snapshot()->checkpoint != smoke->saved->checkpoint) {
                fail("failed load changed the campaign or lacked a notice"); return;
            }
            campaign.clearNotice();
            if (!key(window,item("settingsButton"),Qt::Key_Space) || !key(window,item("compactControl"),Qt::Key_Space)) {
                fail("settings and compact layout keyboard controls unavailable"); return;
            }
            smoke->phase = Phase::settings;
            break;
        case Phase::settings:
            if (!campaign.compactManagement() || pgp::persistence::load_settings(paths.settings_file) != campaign.snapshot()->settings) {
                fail("compact layout preference was not persisted"); return;
            }
            campaign.clearNotice();
            if (!key(window,item("settingsButton"),Qt::Key_Space)) { fail("could not close settings"); return; }
            if (!screenshot.isEmpty() && !window.grabWindow().save(screenshot)) { fail("screenshot could not be saved"); return; }
            qInfo().noquote() << "Campaign smoke passed" << pgp::graphics::name(expected)
                << "text=" << campaign.textScalePercent()
                << "keyboard rooms/clock, running focus, exact save/load, failed load, compact preferences";
            timer->stop();
            QCoreApplication::exit(0);
            break;
        }
    });
    timer->start();
}
