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
QQuickItem* visual_item(QQuickItem* root,const QString& name) {
    if (root->objectName()==name) return root;
    for (auto* child:root->childItems()) if (auto* found=visual_item(child,name)) return found;
    return nullptr;
}
QQuickItem* visual_button(QQuickItem* root,const QString& label) {
    if (root->isVisible() && root->property("text").toString()==label && root->metaObject()->indexOfSignal("clicked()")>=0) return root;
    for (auto* child:root->childItems()) if (auto* found=visual_button(child,label)) return found;
    return nullptr;
}
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
    enum class Phase { opening, stepping, editing, playing, pausing, saving, advancing, loading, rejecting, settings };
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
        const auto item = [&](const char* name) { return visual_item(window.contentItem(),QString::fromLatin1(name)); };
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
            smoke->phase = Phase::editing;
            break;
        }
        case Phase::editing: {
            item("projectSearch")->forceActiveFocus(Qt::TabFocusReason);
            QKeyEvent edit{QEvent::KeyPress,Qt::Key_D,Qt::NoModifier,QStringLiteral("D")};
            QCoreApplication::sendEvent(&window,&edit);
            smoke->phase = Phase::playing;
            break;
        }
        case Phase::playing:
            if (campaign.paused()) { fail("play did not resume the clock"); return; }
            if (campaign.snapshot()->checkpoint.world.simulation.date < pgp::core::advance_days(smoke->date,pgp::core::Days{3})) return;
            // Terminal-launched windows can lose native activation to the
            // desktop while retaining their Qt focus scope. Snapshot delivery
            // must preserve logical edit focus in either case; active focus is
            // additionally required whenever this window owns native focus.
            if (!item("projectSearch")->hasFocus() || (window.isActive() && window.activeFocusItem() != item("projectSearch")) || campaign.selectedProjectId() != smoke->project ||
                item("projectSearch")->property("text").toString() != QStringLiteral("D") ||
                campaign.projects()->filterText() != QStringLiteral("D")) {
                qCritical()<<"Window active"<<window.isActive()<<"logical focus"<<item("projectSearch")->hasFocus()<<"Focus"<<window.activeFocusItem()<<"expected"<<item("projectSearch")<<"selection"<<campaign.selectedProjectId()<<"expected"<<smoke->project<<"edit"<<item("projectSearch")->property("text")<<"filter"<<campaign.projects()->filterText();
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

// Exercise the same visible forms and queued commands used by a player. The
// smoke inspects checkpoints only for assertions; it never installs a prepared
// save, alters hidden capability, skips a day, or injects a business command.
void schedule_playable_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct PlayableSmoke {
        int phase=0,attempts=0;
        QString baseline,targeted,evidence;
        bridge::CampaignSnapshotPtr saved;
        bool requested=false;
    };
    auto smoke=std::make_shared<PlayableSmoke>();
    auto* timer=new QTimer(&window);timer->setInterval(20);
    const auto save_path=paths.saves_directory/"playable-smoke.pgpsave";
    const auto save_url=QUrl::fromLocalFile(QString::fromStdU16String(save_path.u16string()));
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,smoke,expected,save_path,save_url,paths,screenshot] {
        const auto item=[&](const char* name){return visual_item(window.contentItem(),QString::fromLatin1(name));};
        const auto fail=[&](const char* why) { qCritical()<<"Playable smoke failed:"<<why<<"phase"<<smoke->phase<<campaign.notice();timer->stop();QCoreApplication::exit(3); };
        const auto press=[&](const char* name){if (!key(window,item(name),Qt::Key_Space)) {auto* c=item(name);qCritical()<<"Control"<<name<<c<<(c?c->isVisible():false)<<(c?c->isEnabled():false);fail(name);return false;}return true;};
        const auto set=[&](const char* name,const QString& value){auto* control=item(name);return control && control->setProperty("value",value);};
        const auto capture=[&](const QString& suffix) {
            if (screenshot.isEmpty()) return true;
            QString file=screenshot;
            if (!suffix.isEmpty()) file.insert(file.lastIndexOf(QLatin1Char('.')),suffix);
            return window.grabWindow().save(file);
        };
        if (++smoke->attempts>5000) {fail("timed out");return;}
        if (!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        auto* manager=campaign.management();const auto checkpoint=campaign.snapshot();const auto& state=checkpoint->checkpoint.world.simulation;
        // Every fast tick still stops at an actual required presentation.
        if (!campaign.welcome() && campaign.requiredKind()!=QStringLiteral("none")) {
            if (campaign.requiredKind()==QStringLiteral("content")) {
                const auto choices=campaign.requiredChoices();
                QString id;
                for (const auto& choice:choices) if (choice.toMap().value(QStringLiteral("choiceId")).toString()==QStringLiteral("decline")) id=QStringLiteral("decline");
                if (id.isEmpty() && !choices.isEmpty()) id=choices.front().toMap().value(QStringLiteral("choiceId")).toString();
                const auto name=QStringLiteral("flowChoice_")+id;
                if (!key(window,visual_item(window.contentItem(),name),Qt::Key_Space)) fail("required choice was not keyboard reachable");
            } else if (campaign.requiredKind()==QStringLiteral("management")) {press("acknowledgeManagement");}
            else if (campaign.requiredKind()==QStringLiteral("liquidity")) fail("baseline UI path unexpectedly exhausted cash");
            else fail("unexpected core decision");
            return;
        }
        const auto play=[&]{if (campaign.paused()) press("pauseButton");};
        const auto pause=[&]{if (!campaign.paused()) {press("pauseButton");return false;}return true;};
        switch(smoke->phase) {
        case 0: {
            if (!campaign.welcome() || campaign.headquarters().size()!=3 || window.rendererInterface()->graphicsApi()!=expected ||
                std::filesystem::exists(save_path) || std::filesystem::exists(paths.settings_file)) {fail("fresh bundled welcome, headquarters or renderer");return;}
            auto* hq=item("headquartersChoice");if (!hq || !hq->setProperty("currentIndex",1)) {fail("headquarters selector");return;}
            campaign.setDaysPerSecond(60);
            if (press("newCampaignButton")) smoke->phase=1;
            return;
        }
        case 1:
            if (campaign.welcome() || checkpoint->checkpoint.world.state_id!="state.us_tx" || !campaign.paused()) {fail("headquarters choice was not committed");return;}
            if (!capture(QStringLiteral("-office"))) {fail("office capture");return;}
            press("office_funding");manager->funding()->setSelectedId(QStringLiteral("finance.bridge_equity"));
            if (press("takeFinancingButton")) smoke->phase=30;return;
        case 2:
            if (state.lab->licenses.size()!=1) {fail("license purchase");return;}
            press("lab_research");manager->researchTopics()->setSelectedId(QStringLiteral("research.held_out_evaluation"));
            if (press("startResearchButton")) smoke->phase=3;return;
        case 3:
            if (state.lab->research.empty()) {fail("research reservation");return;}
            press("lab_train");
            if (press("trainingData_data.licensed_documents")) smoke->phase=4;return;
        case 4:
            if (!item("startTrainingButton") || !item("startTrainingButton")->isEnabled()) { if (item("trainingPreview")) qCritical()<<item("trainingPreview")->property("text");capture(QStringLiteral("-failed"));fail("deterministic training preview did not enable valid plan");return;}
            if (!capture(QStringLiteral("-training"))) {fail("training capture");return;}
            if (press("startTrainingButton")) smoke->phase=5;return;
        case 5:
            if (state.lab->models.empty()) {fail("training start");return;}
            smoke->baseline=QString::fromStdString(state.lab->models.front().id);
            press("officeTab");press("office_policies");
            if (!set("policyRunsField",QStringLiteral("1"))) {fail("finite evaluation policy runs");return;}
            if (press("authorizePolicyButton")) smoke->phase=33;return;
        case 6:
            if (core::lab_report(*state.lab,*state.capacity).models.front().training) {play();return;}
            if (!pause()) return;
            press("labTab");press("lab_evaluate");
            if (state.lab->evaluations.empty() || state.management->policies.front().runs!=1) {fail("delegation did not start the independent paid evaluation");return;}
            smoke->phase=7;return;
        case 7:
            if (state.lab->evaluations.empty()) {fail("held-out evaluation start");return;}
            play();smoke->phase=8;return;
        case 8:
            if (!state.lab->evaluations.front().completed) {play();return;}
            if (!pause()) return;
            if (!manager->evidence()->selectedRow().value(QStringLiteral("detail")).toString().contains(QStringLiteral("scanned tables"))) {fail("concrete subgroup finding missing");return;}
            if (!capture(QStringLiteral("-evidence"))) {fail("evidence capture");return;}
            press("lab_deploy");if (press("deployButton")) smoke->phase=9;return;
        case 9:
            if (state.deployments->records.size()!=1) {fail("qualified document-only deployment");return;}
            press("officeTab");press("office_offers");manager->offers()->setSelectedId(QStringLiteral("offer.enterprise_service"));
            if (press("acceptOfferButton")) smoke->phase=10;return;
        case 10:
            if (state.market->contracts.empty()) {fail("customer offer did not create contract");return;}
            manager->offers()->setSelectedId(QStringLiteral("offer.supplier_small"));
            if (press("acceptOfferButton")) smoke->phase=34;return;
        case 11:
            press("lab_train");if (press("trainingData_data.consented_scans")) smoke->phase=12;return;
        case 12:
            if (press("startTrainingButton")) smoke->phase=13;return;
        case 13:
            if (state.lab->models.size()!=2) {fail("targeted retraining did not start");return;}
            smoke->targeted=QString::fromStdString(state.lab->models.back().id);play();smoke->phase=14;return;
        case 14:
            if (core::lab_report(*state.lab,*state.capacity).models.back().training) {play();return;}
            if (!pause()) return;
            press("lab_evaluate");if (!set("evaluationModelChoice",smoke->targeted)) {fail("targeted checkpoint selector");return;}
            if (press("evaluateButton")) smoke->phase=15;return;
        case 15:
            if (state.lab->evaluations.size()!=2) {fail("targeted evaluation start");return;}
            play();smoke->phase=16;return;
        case 16:
            if (!state.lab->evaluations.back().completed) {play();return;}
            if (!pause()) return;
            if (state.capacity->sources.front().effective.value()<80000) {play();return;} // Wait through the disclosed temporary source reduction.
            smoke->evidence=QString::fromStdString(state.lab->evaluations.back().id);
            press("lab_deploy");
            if (!set("deploymentModelChoice",smoke->targeted) || !set("deploymentEvidenceChoice",smoke->evidence) || !set("deploymentServiceChoice",QStringLiteral("product.documents_tables"))) {fail("broader service selectors");return;}
            if (press("deployButton")) smoke->phase=17;return;
        case 17:
            if (state.deployments->records.size()!=2 || state.market->products.size()!=2) {fail("qualified broader deployment");return;}
            smoke->saved=checkpoint;campaign.saveTo(save_url);smoke->phase=18;return;
        case 18:
            if (!std::filesystem::exists(save_path)) {fail("manual save");return;}
            if (press("stepButton")) smoke->phase=19;return;
        case 19:
            campaign.loadFrom(save_url);smoke->phase=20;return;
        case 20:
            if (checkpoint->checkpoint!=smoke->saved->checkpoint || !campaign.paused()) {fail("exact playable campaign reload");return;}
            press("officeTab");play();smoke->phase=21;return;
        case 21:
            if (state.phase!=core::DayPhase::finished) {
                if (state.phase==core::DayPhase::closed) {fail("company closed before ending");return;}
                play();return;
            }
            if (state.cash.value()<=0 || state.date!=core::to_ordinal({2017,1,1}) || manager->ending().isEmpty() || manager->rivals()->rowCount()<3) {fail("ending or public rival presentation");return;}
            if (state.business_offers->renewals.size()!=1 || state.management->policies.front().runs!=1 || state.management->policies.back().runs!=1 ||
                core::required_management_notice(state) || !core::slice_score(state).final || !manager->ending().contains(QStringLiteral("slice score"))) {fail("policy/recovery/scoring ending");return;}
            if (!capture({})) {fail("ending capture");return;}
            qInfo()<<"Playable UI smoke passed: Texas start, fixed equity financing, hired specialist, bounded evaluation/renewal policies, milestone, paid data/research, subgroup finding, deployments, contract, exact reload, mandatory setback/restoration, rivals and scored solvent 2017 ending; cash cents"<<state.cash.value()<<"text scale"<<campaign.textScalePercent()<<"renderer"<<graphics::name(expected);
            timer->stop();QCoreApplication::exit(0);return;
        case 30:
            if (!key(window,visual_button(window.contentItem(),QStringLiteral("OK")),Qt::Key_Space)) {fail("financing confirmation keyboard");return;}
            smoke->phase=31;return;
        case 31:
            if (state.management->financing.size()!=1 || core::slice_score(state).founder_share.value()!=4510) {fail("fixed equity proceeds/dilution");return;}
            if (!capture(QStringLiteral("-funding"))) {fail("funding capture");return;}
            if (press("hireSpecialistButton")) smoke->phase=32;return;
        case 32:
            if (state.capacity->available_staff!=7 || state.management->specialists.size()!=1) {fail("paid specialist hire");return;}
            press("labTab");press("lab_data");manager->dataOffers()->setSelectedId(QStringLiteral("data.licensed_documents"));
            if (press("buyDataButton")) smoke->phase=2;return;
        case 33:
            if (state.management->policies.size()!=1 || state.management->policies.front().maximum_runs!=1) {fail("bounded evaluation authorization");return;}
            if (!capture(QStringLiteral("-policies"))) {fail("policy capture");return;}
            press("labTab");press("lab_projects");
            if (press("advanceToButton")) smoke->phase=6;return;
        case 34:
            if (state.business_offers->accepted.size()!=2) {fail("paid renewable compute lease");return;}
            press("office_policies");
            if (!set("policyKindChoice",QStringLiteral("lease_renewal")) || !set("policyRunsField",QStringLiteral("1"))) {fail("renewal policy form");return;}
            if (press("authorizePolicyButton")) smoke->phase=35;return;
        case 35:
            if (state.management->policies.size()!=2 || state.management->policies.back().kind!=core::DelegationKind::lease_renewal) {fail("bounded renewal authorization");return;}
            press("labTab");press("lab_data");manager->dataOffers()->setSelectedId(QStringLiteral("data.consented_scans"));
            if (press("buyDataButton")) smoke->phase=11;return;
        default:fail("unknown smoke phase");return;
        }
    });
    timer->start();
}
