#include "game_smoke.hpp"

#include "campaign_controller.hpp"
#include "music_controller.hpp"
#include <pgp/graphics/graphics_policy.hpp>
#include <pgp/runtime/authority.hpp>

#include <QAccessible>
#include <QCoreApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <algorithm>
#include <memory>

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
void reveal_top(QQuickItem* item) {
    if (!item) return;
    for (auto* parent=item->parentItem();parent;parent=parent->parentItem()) {
        if (!parent->property("contentY").isValid()) continue;
        auto* content=qobject_cast<QQuickItem*>(parent->property("contentItem").value<QObject*>());
        if (content) parent->setProperty("contentY",std::clamp(item->mapToItem(content,QPointF{}).y(),0.0,std::max(0.0,parent->property("contentHeight").toDouble()-parent->height())));
    }
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

void schedule_music_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,MusicController& music,
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot) {
    enum class Phase { settings, dialog, enabled, excerpt, muted };
    struct State {
        Phase phase = Phase::settings;
        int ticks = 0;
        QString backend;
    };
    auto smoke = std::make_shared<State>();
    auto* timer = new QTimer(&window);
    timer->setInterval(80);
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,smoke,expected,paths,screenshot] {
        const auto item = [&](const char* name) { return visual_item(window.contentItem(),QString::fromLatin1(name)); };
        const auto press = [&](const char* name) { return key(window,item(name),Qt::Key_Space); };
        const auto fail = [&](const char* reason) {
            qCritical() << "Music and credits smoke failed:" << reason << static_cast<int>(smoke->phase) << music.status();
            timer->stop();
            QCoreApplication::exit(3);
        };
        if (++smoke->ticks > 200) { fail("timed out"); return; }
        if (!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        switch (smoke->phase) {
        case Phase::settings:
            if (window.rendererInterface()->graphicsApi() != expected || music.credits().size() != 7 ||
                campaign.musicEnabled() || music.playing() || std::filesystem::exists(paths.settings_file) ||
                !press("settingsButton") || !press("reduceMotionControl")) {
                fail("fresh settings, seven credits, keyboard or renderer"); return;
            }
            smoke->phase = Phase::dialog;
            break;
        case Phase::dialog:
            if (!campaign.reducedMotion() || !press("musicCreditsButton")) { fail("reduced motion or credits button"); return; }
            smoke->phase = Phase::enabled;
            break;
        case Phase::enabled: {
            auto* control = item("musicEnabledControl");
            const auto* accessible = control ? QAccessible::queryAccessibleInterface(control) : nullptr;
            if (!accessible || accessible->text(QAccessible::Name).isEmpty() ||
                !music.licenseText(0).contains(QStringLiteral("Attribution 4.0 International")) ||
                !music.licenseText(3).contains(QStringLiteral("GNU LESSER GENERAL PUBLIC LICENSE")) ||
                !press("musicEnabledControl")) {
                fail("credits, license text, accessible focus or music toggle"); return;
            }
            smoke->phase = Phase::excerpt;
            break;
        }
        case Phase::excerpt: {
            auto* track = item("musicTrackControl");
            auto* volume = item("musicVolumeControl");
            if (!campaign.musicEnabled() || !music.playing() ||
                !music.status().contains(QStringLiteral("Atemübungen")) ||
                !key(window,track,Qt::Key_End) || !press("musicExcerptControl") ||
                !key(window,volume,Qt::Key_Left)) {
                fail("native ambient/tension playback, keyboard selector or volume"); return;
            }
            smoke->backend = music.backend();
            smoke->phase = Phase::muted;
            break;
        }
        case Phase::muted:
            if (campaign.musicEnabled()) {
                if (!music.playing() || !music.status().contains(QStringLiteral("004")) || campaign.musicVolume() != 95) {
                    fail("selected tension excerpt or persisted volume"); return;
                }
                if (!screenshot.isEmpty() && !window.grabWindow().save(screenshot)) { fail("screenshot"); return; }
                if (!press("musicEnabledControl")) { fail("turn music off"); return; }
                return;
            }
            if (music.playing() || !pgp::persistence::load_settings(paths.settings_file).reduced_motion ||
                pgp::persistence::load_settings(paths.settings_file).music_enabled || !press("closeMusicCredits")) {
                fail("silent state, saved preferences or return from the credits"); return;
            }
            qInfo().noquote() << "Seven music credits and offline notices, ambient and tension output via"
                              << smoke->backend << ", keyboard and reduced motion at"
                              << campaign.textScalePercent() << "% text.";
            timer->stop();
            QCoreApplication::exit(0);
            break;
        }
    });
    timer->start();
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
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot,bool competition) {
    using namespace pgp;
    struct PlayableSmoke {
        int phase=0,attempts=0;
        int progress_phase=-1;
        std::int64_t progress_day=-1;
        QString baseline,targeted,evidence;
        bridge::CampaignSnapshotPtr saved;
        bool requested=false;
        bool published=false,adopted=false,priced=false,final_saved=false;
        bool responses=false,paper_declined=false,pilot_accepted=false,pilot_paid=false;
        bool corporate=false,corporate_completed=false,dividend_paid=false;
        QString competitive_pilot,competitive_paper;
        std::int64_t before_agreement_cash=0;
        core::Cents before_customer_cash{0};
    };
    auto smoke=std::make_shared<PlayableSmoke>();
    auto* timer=new QTimer(&window);timer->setInterval(20);
    const auto save_path=paths.saves_directory/"playable-smoke.pgpsave";
    const auto save_url=QUrl::fromLocalFile(QString::fromStdU16String(save_path.u16string()));
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,smoke,expected,save_path,save_url,paths,screenshot,competition] {
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
        // Bound stalls, not the entire advancing campaign. Larger rosters keep
        // making real daily progress while monthly saves validate more history.
        if (smoke->phase!=smoke->progress_phase || state.date.value()!=smoke->progress_day) {
            smoke->progress_phase=smoke->phase;smoke->progress_day=state.date.value();smoke->attempts=0;
        }
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
            auto* setup=item("competitionChoice");if (!setup) {fail("competition selector");return;}
            if (competition) {
                if (setup->property("currentValue").toString()!=QStringLiteral("scenario.world_competition")) {fail("standard roster is not the default setup");return;}
            } else {
                const auto choices=campaign.scenarios();int intro=-1;
                for (qsizetype i=0;i<choices.size();++i) if (choices.at(i).toMap().value(QStringLiteral("id"))==QStringLiteral("scenario.first_playable")) intro=static_cast<int>(i);
                if (intro<0 || !setup->setProperty("currentIndex",intro)) {fail("introductory roster selection");return;}
            }
            campaign.setDaysPerSecond(60);
            if (press("newCampaignButton")) smoke->phase=1;
            return;
        }
        case 1:
            if (campaign.welcome() || checkpoint->checkpoint.world.state_id!="state.us_tx" || !campaign.paused()) {fail("headquarters choice was not committed");return;}
            if (competition) {
                if (!manager->plannedCompetition() || checkpoint->checkpoint.world.scenario_id!="scenario.world_competition" || checkpoint->checkpoint.world.competition->companies.size()!=15) {fail("standard fifteen-company setup was not committed");return;}
                smoke->responses=std::any_of(checkpoint->checkpoint.world.competition->companies.begin(),checkpoint->checkpoint.world.competition->companies.end(),[](const auto& row){return row.response.has_value();});
                press("office_rivals");if (!capture(QStringLiteral("-competition"))) {fail("public rival capture");return;}
            }
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
            if (competition && !smoke->published) {
                press("lab_research");manager->researchTopics()->setSelectedId(QStringLiteral("research.held_out_evaluation"));
                if (press("publishResearchButton")) smoke->phase=36;return;
            }
            if (!manager->evidence()->selectedRow().value(QStringLiteral("detail")).toString().contains(QStringLiteral("scanned tables"))) {fail("concrete subgroup finding missing");return;}
            if (!capture(QStringLiteral("-evidence"))) {fail("evidence capture");return;}
            press("lab_deploy");if (press("deployButton")) smoke->phase=9;return;
        case 9:
            if (state.deployments->records.size()!=1) {fail("qualified document-only deployment");return;}
            if (competition && !smoke->priced) {
                press("lab_deploy");manager->products()->setSelectedId(QStringLiteral("product.documents"));
                if (!set("productPriceField",QStringLiteral("149"))) {fail("service price input");return;}
                if (press("setProductPriceButton")) smoke->phase=38;return;
            }
            press("officeTab");press("office_offers");manager->offers()->setSelectedId(QStringLiteral("offer.enterprise_service"));
            smoke->before_customer_cash=state.cash;
            if (press("acceptOfferButton"))
                smoke->phase=manager->offers()->selectedRow().value(QStringLiteral("fields")).toMap()
                    .value(QStringLiteral("customerAdvance")).toBool()?44:10;
            return;
        case 10:
            if (state.market->contracts.empty()) {fail("customer offer did not create contract");return;}
            for (const auto& contract:state.market->contracts) if (contract.id=="pilot/offer.enterprise_service" &&
                contract.customer_advance.value()>0 &&
                (state.cash!=core::checked_unit_add(smoke->before_customer_cash,contract.customer_advance) ||
                 core::accounts_report(*state.accounts).customer_advances!=contract.customer_advance)) {
                fail("customer advance cash or liability");return;
            }
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
            if (smoke->corporate_completed && !smoke->dividend_paid && state.phase==core::DayPhase::work_ready) {
                const auto& acquisition=checkpoint->checkpoint.world.competition->acquisitions.front();
                const auto found=std::find_if(checkpoint->checkpoint.world.competition->companies.begin(),checkpoint->checkpoint.world.competition->companies.end(),[&](const auto& rival){return rival.definition_id==acquisition.target;});
                if (found==checkpoint->checkpoint.world.competition->companies.end()) {fail("owned dividend counterparty");return;}
                const auto& subsidiary=found->simulation;
                if (core::accounts_report(*subsidiary.accounts).profit>core::checked_unit_add(acquisition.acquired_profit,core::Cents{100000}) && subsidiary.cash.value()>50000000) {
                    if (!pause()) return;press("officeTab");press("office_governance");
                    if (!set("corporatePageChoice",QStringLiteral("mergers")) || !set("groupTransferField",QStringLiteral("1000.00"))) {fail("earned dividend form");return;}
                    manager->mergerTargets()->setSelectedId(QString::fromStdString(acquisition.target));
                    if (press("dividendSubsidiaryButton")) smoke->phase=67;return;
                }
            }
            if (smoke->responses && state.phase!=core::DayPhase::finished) {
                const auto& agreements=checkpoint->checkpoint.world.competition->offers;
                if (!smoke->paper_declined) for (const auto& offer:agreements)
                    if (offer.kind==runtime::CompetitiveOfferKind::publication && offer.status==runtime::CompetitiveOfferStatus::pending) {
                        if (!pause()) return;press("officeTab");press("office_rivals");smoke->competitive_paper=QString::fromStdString(offer.id);manager->competitiveOffers()->setSelectedId(smoke->competitive_paper);
                        if (press("declineCompetitiveOfferButton")) smoke->phase=43;return;
                    }
                if (!smoke->pilot_accepted) for (const auto& offer:agreements)
                    if (offer.kind==runtime::CompetitiveOfferKind::pilot && offer.status==runtime::CompetitiveOfferStatus::pending) {
                        if (!pause()) return;press("officeTab");press("office_rivals");smoke->competitive_pilot=QString::fromStdString(offer.id);manager->competitiveOffers()->setSelectedId(smoke->competitive_pilot);smoke->before_agreement_cash=state.cash.value();
                        if (press("acceptCompetitiveOfferButton")) smoke->phase=41;return;
                    }
                if (smoke->pilot_accepted && !smoke->pilot_paid) for (const auto& offer:agreements)
                    if (offer.id==smoke->competitive_pilot.toStdString() && offer.status==runtime::CompetitiveOfferStatus::paid) {
                        if (!pause()) return;press("officeTab");press("office_rivals");manager->competitiveOffers()->setSelectedId(smoke->competitive_pilot);reveal_top(item("competitiveOfferReviewDetail"));
                        if (!capture(QStringLiteral("-pilot-paid"))) {fail("funded pilot completion capture");return;}
                        smoke->pilot_paid=true;play();return;
                    }
            }
            if (competition && !smoke->adopted && state.phase!=core::DayPhase::finished) {
                QString adopt;
                for (int i=0;i<manager->researchTopics()->rowCount();++i) {
                    const auto id=manager->researchTopics()->data(manager->researchTopics()->index(i),bridge::StableListModel::IdRole).toString();
                    const auto row=manager->researchTopics()->row(id);
                    if (row.value(QStringLiteral("fields")).toMap().value(QStringLiteral("canAdopt")).toBool()) {adopt=id;break;}
                }
                if (!adopt.isEmpty()) {
                    if (!pause()) return;press("labTab");press("lab_research");manager->researchTopics()->setSelectedId(adopt);
                    if (press("adoptPublicationButton")) smoke->phase=37;return;
                }
            }
            if (state.phase!=core::DayPhase::finished) {
                if (state.phase==core::DayPhase::closed) {fail("company closed before ending");return;}
                play();return;
            }
            if (state.cash.value()<=0 || state.date!=core::to_ordinal({2017,1,1}) || manager->ending().isEmpty() || manager->rivals()->rowCount()<3) {fail("ending or public rival presentation");return;}
            if (state.business_offers->renewals.size()!=1 || state.management->policies.front().runs!=1 || state.management->policies.back().runs!=1 ||
                core::required_management_notice(state) || !core::slice_score(state).final || !manager->ending().contains(QStringLiteral("slice score"))) {fail("policy/recovery/scoring ending");return;}
            if (competition) {
                const auto& rivals=*checkpoint->checkpoint.world.competition;
                if (!smoke->published || !smoke->adopted || !smoke->priced || rivals.releases.size()<2 || rivals.adoptions.empty() ||
                    std::any_of(rivals.companies.begin(),rivals.companies.end(),[](const auto& row){return !row.planning->entered_on;})) {fail("staged entry, public research, pricing or shared adoption was not exercised");return;}
                if (smoke->responses && (!smoke->paper_declined || !smoke->pilot_accepted || !smoke->pilot_paid || manager->heavyweights()->rowCount()!=3)) {fail("public response briefing or real funded agreement path was not exercised");return;}
                if (smoke->corporate && (!smoke->corporate_completed || !smoke->dividend_paid || state.management->governance->grants.front().unvested.value()!=0)) {fail("corporate route or full cumulative vesting was not exercised");return;}
                if (!smoke->final_saved) {smoke->saved=checkpoint;campaign.saveTo(save_url);smoke->phase=39;return;}
            }
            if (!capture({})) {fail("ending capture");return;}
            if (competition) qInfo()<<"Competition UI additions passed: standard fifteen-company default, staged entry, explicit research publication/adoption, current prices and exact complete-roster save; bytes"<<std::filesystem::file_size(save_path);
            if (smoke->corporate) qInfo()<<"Corporate UI additions passed: cash-plus-equity executive, alternative financing counteroffer, strategic board seat, approved pool dilution, reviewed cash acquisition, retained subsidiary obligations, actual capital funding/earned dividend with group elimination, full cumulative vesting and exact reload";
            if (smoke->responses) qInfo()<<"Heavyweight UI additions passed: public pressure/finite-budget briefings, declined publication terms, reviewed funded pilot, real milestone payment and exact response/deal reload";
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
            if (competition && manager->governanceAvailable()) {
                smoke->corporate=true;press("office_governance");
                if (!set("corporatePageChoice",QStringLiteral("people"))) {fail("corporate people selector");return;}
                manager->executives()->setSelectedId(QStringLiteral("executive.finance"));
                if (press("appointEquityButton")) smoke->phase=50;return;
            }
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
        case 36:
            if (state.lab->publications.empty()) {fail("explicit research publication did not commit");return;}
            smoke->published=true;if (!capture(QStringLiteral("-publication"))) {fail("publication capture");return;}
            press("lab_evaluate");smoke->phase=8;return;
        case 37:
            if (state.lab->insights.empty()) {fail("public research adoption did not commit");return;}
            smoke->adopted=true;press("officeTab");play();smoke->phase=21;return;
        case 38:
            if (state.market->products.front().price_per_task!=core::Cents{149} || state.market->prices.empty()) {fail("explicit price change did not preserve history");return;}
            smoke->priced=true;if (!capture(QStringLiteral("-price"))) {fail("price capture");return;}
            smoke->phase=9;return;
        case 39:
            if (!std::filesystem::exists(save_path)) {fail("complete roster manual save");return;}
            campaign.loadFrom(save_url);smoke->phase=40;return;
        case 40:
            if (checkpoint->checkpoint!=smoke->saved->checkpoint || !campaign.paused()) {fail("complete roster final save did not reload exactly");return;}
            smoke->final_saved=true;smoke->phase=21;return;
        case 41:
            if (!capture(QStringLiteral("-agreement"))) {fail("funded agreement terms capture");return;}
            if (!key(window,visual_button(window.contentItem(),QStringLiteral("OK")),Qt::Key_Space)) {fail("funded agreement keyboard confirmation");return;}
            smoke->phase=42;return;
        case 42:
            if (manager->competitiveOffers()->row(smoke->competitive_pilot).value(QStringLiteral("fields")).toMap().value(QStringLiteral("status"))!=QStringLiteral("active") || state.cash.value()!=smoke->before_agreement_cash) {fail("acceptance failed or paid an unearned pilot fee");return;}
            smoke->pilot_accepted=true;play();smoke->phase=21;return;
        case 50:
            if (!capture(QStringLiteral("-executive-terms"))) {fail("executive terms capture");return;}
            if (!key(window,visual_button(window.contentItem(),QStringLiteral("OK")),Qt::Key_Space)) {fail("executive terms keyboard");return;}
            smoke->phase=51;return;
        case 51:
            if (!state.management->governance || state.management->governance->appointments.size()!=1 || state.capacity->available_staff!=8 || state.management->governance->grants.size()!=1) {fail("agreed executive cash/equity staff");return;}
            if (!set("corporatePageChoice",QStringLiteral("finance"))) {fail("corporate financing selector");return;}
            manager->corporateFinance()->setSelectedId(QStringLiteral("corporate.round_small"));
            if (press("negotiateFinanceButton")) smoke->phase=52;return;
        case 52: case 55: case 58: case 62:
            if (state.management->governance->motions.empty() || state.management->governance->motions.back().status!=core::BoardMotionStatus::pending) {fail("reviewed board proposal");return;}
            if (!set("corporatePageChoice",QStringLiteral("board"))) {fail("corporate board selector");return;}
            manager->boardMotions()->setSelectedId(QString::fromStdString(state.management->governance->motions.back().id));
            if (press("voteBoardYesButton")) ++smoke->phase;return;
        case 53: case 56: case 59: case 63:
            if (smoke->phase==53 && !capture(QStringLiteral("-board-vote"))) {fail("board terms capture");return;}
            if (!key(window,visual_button(window.contentItem(),QStringLiteral("OK")),Qt::Key_Space)) {fail("board vote keyboard confirmation");return;}
            ++smoke->phase;return;
        case 54:
            if (!state.management->governance->motions.back().executed) {fail("negotiated finance vote did not commit");return;}
            if (!set("corporatePageChoice",QStringLiteral("finance"))) {fail("strategic finance selector");return;}
            manager->corporateFinance()->setSelectedId(QStringLiteral("corporate.strategic_equity"));
            if (press("negotiateFinanceButton")) smoke->phase=55;return;
        case 57:
            if (!state.management->governance->motions.back().executed) {fail("strategic board seat funding");return;}
            if (!set("corporatePageChoice",QStringLiteral("people"))) {fail("pool expansion selector");return;}
            if (press("proposePoolButton")) smoke->phase=58;return;
        case 60:
            if (!state.management->governance->motions.back().executed || !set("corporatePageChoice",QStringLiteral("mergers"))) {fail("approved pool expansion");return;}
            manager->mergerTargets()->setSelectedId(QStringLiteral("company.roster.domestic.3"));
            if (press("reviewMergerButton")) smoke->phase=61;return;
        case 61:
            if (!capture(QStringLiteral("-merger-terms"))) {fail("merger terms capture");return;}
            if (!key(window,visual_button(window.contentItem(),QStringLiteral("OK")),Qt::Key_Space)) {fail("merger counteroffer keyboard");return;}
            smoke->phase=62;return;
        case 64:
            if (!state.management->governance->motions.back().executed || checkpoint->checkpoint.world.competition->acquisitions.size()!=1 || !checkpoint->checkpoint.world.competition->acquisitions.front().completed_on) {fail("actual bilateral acquisition");return;}
            if (!set("corporatePageChoice",QStringLiteral("mergers"))) {fail("subsidiary view selector");return;}
            manager->mergerTargets()->setSelectedId(QStringLiteral("company.roster.domestic.3"));reveal_top(item("mergerTargetSelectionDetail"));
            if (!capture(QStringLiteral("-subsidiary"))) {fail("subsidiary retained obligations capture");return;}
            smoke->corporate_completed=true;if (!set("groupTransferField",QStringLiteral("1000.00"))) {fail("subsidiary funding field");return;}
            if (press("fundSubsidiaryButton")) smoke->phase=65;return;
        case 65: case 67:
            if (smoke->phase==67 && !capture(QStringLiteral("-dividend-terms"))) {fail("dividend terms capture");return;}
            if (!key(window,visual_button(window.contentItem(),QStringLiteral("OK")),Qt::Key_Space)) {fail("group transfer confirmation keyboard");return;}
            ++smoke->phase;return;
        case 66:
            if (checkpoint->checkpoint.world.competition->acquisitions.front().transfers.size()!=1 || !checkpoint->checkpoint.world.competition->acquisitions.front().transfers.front().funding) {fail("real bilateral subsidiary capital");return;}
            press("labTab");press("lab_data");manager->dataOffers()->setSelectedId(QStringLiteral("data.licensed_documents"));
            if (press("buyDataButton")) smoke->phase=2;return;
        case 68:
            if (checkpoint->checkpoint.world.competition->acquisitions.front().transfers.size()!=2 || checkpoint->checkpoint.world.competition->acquisitions.front().transfers.back().funding) {fail("real earned subsidiary dividend");return;}
            smoke->dividend_paid=true;reveal_top(item("mergerTargetSelectionDetail"));
            if (!capture(QStringLiteral("-dividend-paid"))) {fail("earned dividend result capture");return;}
            play();smoke->phase=21;return;
        case 43:
            if (manager->competitiveOffers()->row(smoke->competitive_paper).value(QStringLiteral("fields")).toMap().value(QStringLiteral("status"))!=QStringLiteral("declined")) {fail("explicit publication offer decline");return;}
            reveal_top(item("heavyweightBriefingDetail"));if (!capture(QStringLiteral("-response"))) {fail("public pressure briefing capture");return;}
            smoke->paper_declined=true;play();smoke->phase=21;return;
        case 44:
            if (!capture(QStringLiteral("-advance"))) {fail("customer advance terms capture");return;}
            if (!key(window,visual_button(window.contentItem(),QStringLiteral("OK")),Qt::Key_Space)) {fail("customer advance confirmation keyboard");return;}
            smoke->phase=10;return;
        default:fail("unknown smoke phase");return;
        }
    });
    timer->start();
}

void schedule_infrastructure_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct State {int phase=0,attempts=0;std::uint64_t full_wh=0;bridge::CampaignSnapshotPtr saved;};
    auto smoke=std::make_shared<State>();auto* timer=new QTimer(&window);timer->setInterval(24);
    const auto save_path=paths.saves_directory/"infrastructure-smoke.pgpsave";
    const auto save_url=QUrl::fromLocalFile(QString::fromStdU16String(save_path.u16string()));
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,smoke,expected,save_path,save_url,screenshot] {
        const auto item=[&](const char* name){return visual_item(window.contentItem(),QString::fromLatin1(name));};
        const auto fail=[&](const char* why){qCritical()<<"Infrastructure smoke failed:"<<why<<"phase"<<smoke->phase<<campaign.notice();timer->stop();QCoreApplication::exit(3);};
        const auto press=[&](const char* name){auto* c=item(name);reveal_top(c);if (key(window,c,Qt::Key_Space)) return true;fail(name);return false;};
        const auto set=[&](const char* name,const char* property,const QString& value){auto* c=item(name);return c && c->setProperty(property,value);};
        const auto capture=[&](QString suffix){if (screenshot.isEmpty()) return true;auto file=screenshot;if (!suffix.isEmpty()) file.insert(file.lastIndexOf(QLatin1Char('.')),suffix);return window.grabWindow().save(file);};
        const auto next=[&](int phase){smoke->phase=phase;smoke->attempts=0;};
        if (++smoke->attempts>1200) {fail("timed out");return;}
        if (!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        const auto snap=campaign.snapshot();const auto& s=snap->checkpoint.world.simulation;
        auto* infra=campaign.infrastructure();
        if (!campaign.welcome() && campaign.requiredKind()!=QStringLiteral("none")) {
            const auto kind=campaign.requiredKind();
            if (kind==QStringLiteral("management")) {press("acknowledgeManagement");return;}
            if (kind==QStringLiteral("site")) {press("acknowledgeSiteAlert");return;}
            if (kind!=QStringLiteral("content")) {fail("unexpected blocking decision");return;}
            QString choice;
            for (const auto& entry:campaign.requiredChoices()) {const auto id=entry.toMap().value(QStringLiteral("choiceId")).toString();if (choice.isEmpty() || id==QStringLiteral("decline")) choice=id;}
            if (choice.isEmpty() || !key(window,visual_item(window.contentItem(),QStringLiteral("flowChoice_")+choice),Qt::Key_Space)) fail("required choice unavailable");return;
        }
        const auto confirm=[&](int phase){if (!infra->canConfirm()) {qCritical().noquote()<<infra->reviewText();fail("infrastructure review did not approve valid terms");return;}if (press("confirmInfrastructure")) next(phase);};
        switch (smoke->phase) {
        case 0: {
            if (!campaign.welcome() || std::filesystem::exists(save_path) || window.rendererInterface()->graphicsApi()!=expected) {fail("fresh headquarters chooser and requested renderer required");return;}
            auto* hq=item("headquartersChoice");auto* setup=item("competitionChoice");int index=-1;const auto choices=campaign.scenarios();
            const auto scenario=campaign.management()->launchRegions().isEmpty()?QStringLiteral("scenario.first_playable"):QStringLiteral("scenario.world_competition");
            for (qsizetype i=0;i<choices.size();++i) if (choices.at(i).toMap().value(QStringLiteral("id"))==scenario) index=static_cast<int>(i);
            if (!hq || !setup || index<0 || !hq->setProperty("currentIndex",1) || !setup->setProperty("currentIndex",index)) {fail("headquarters/scenario selection");return;}
            campaign.setDaysPerSecond(60);if (press("newCampaignButton")) next(1);return;
        }
        case 1:
            if (campaign.welcome() || !infra->available() || infra->states()->rowCount()!=51 || infra->mapCredit().isEmpty() || snap->checkpoint.world.state_id!="state.us_tx") {fail("51 state profiles, offline geometry or chosen HQ missing");return;}
            if (press("operationsTab")) next(2);return;
        case 2: {
            auto* map=item("domesticMap");if (!map || !map->isVisible()) {fail("offline map did not render");return;}reveal_top(map);
            if (!capture(QStringLiteral("-map"))) {fail("map capture");return;}
            QVariantList point;
            for (const auto& feature:infra->mapGeometry().value(QStringLiteral("features")).toList()) {
                const auto row=feature.toMap();if (row.value(QStringLiteral("id"))==QStringLiteral("US-DC")) point=row.value(QStringLiteral("label")).toList();
            }
            if (point.size()!=2) {fail("DC callout has no prepared display coordinate");return;}
            const auto ratio=map->property("ratio").toDouble();
            const auto scene=map->mapToScene(QPointF{map->property("insetX").toDouble()+point[0].toDouble()*ratio,map->property("insetY").toDouble()+point[1].toDouble()*ratio});
            const auto global=window.mapToGlobal(scene.toPoint());
            QMouseEvent down{QEvent::MouseButtonPress,scene,scene,QPointF{global},Qt::LeftButton,Qt::LeftButton,Qt::NoModifier};
            QMouseEvent up{QEvent::MouseButtonRelease,scene,scene,QPointF{global},Qt::LeftButton,Qt::NoButton,Qt::NoModifier};
            QCoreApplication::sendEvent(&window,&down);QCoreApplication::sendEvent(&window,&up);
            if (infra->states()->selectedId()!=QStringLiteral("US-DC")) {fail("DC callout pointer selection");return;}
            if (press("toggleStateMap")) next(3);return;
        }
        case 3: {
            auto* list=item("domesticStateList");auto* search=item("stateSearch");auto* accessible=search?QAccessible::queryAccessibleInterface(search):nullptr;
            if (!search || !list || !accessible || accessible->text(QAccessible::Name).isEmpty() || !key(window,list,Qt::Key_Down) || infra->states()->selectedId()==QStringLiteral("US-DC")) {fail("keyboard state list, focus or label missing");return;}
            search->forceActiveFocus(Qt::TabFocusReason);QKeyEvent filter{QEvent::KeyPress,Qt::Key_W,Qt::NoModifier,QStringLiteral("Washington")};QCoreApplication::sendEvent(&window,&filter);
            if (infra->states()->rowCount()!=1 || infra->states()->selectedId()!=QStringLiteral("US-WA") || item("domesticMap")->isVisible()) {fail("keyboard list filter did not select Washington independently of geometry");return;}
            if (press("reviewBuildOwnedSite")) next(4);return;
        }
        case 4:
            if (!capture(QStringLiteral("-construction"))) {fail("construction review capture");return;}confirm(5);return;
        case 5:
            if (s.infrastructure->sites.size()!=1 || s.infrastructure->sites.front().state_id!="US-WA") {fail("reviewed site was not committed");return;}
            if (infra->resilienceAvailable()) {
                if (!press("operationsResilience") || !set("protectionOfferPicker","value",QStringLiteral("protection.generator"))) {fail("resilience selection");return;}
                if (press("reviewSiteProtection")) next(31);return;
            }
            if (!press("operationsSites")) return;
            if (press("reviewOrderOwnedRack")) next(6);return;
        case 6:confirm(7);return;
        case 7:
            if (s.infrastructure->hardware.size()!=1 || s.capacity->sources.size()!=1) {fail("paid order gained capacity before delivery");return;}
            if (press("reviewHireOwnedOperators")) next(8);return;
        case 8:confirm(9);return;
        case 9:
            if (s.infrastructure->hires.size()!=1 || !press("operationsCalendar") || !set("inspectionMethodPicker","value",QStringLiteral("onsite"))) {fail("staffing or inspection method control");return;}
            if (press("reviewSiteInspection")) next(10);return;
        case 10:confirm(11);return;
        case 11:
            if (s.infrastructure->appointments.size()!=1 || s.infrastructure->appointments.front().mode!=core::AppointmentMode::onsite || !set("inspectionMethodPicker","value",QStringLiteral("remote"))) {fail("on-site appointment did not book paid travel");return;}
            if (press("reviewSiteInspection")) next(12);return;
        case 12:
            if (infra->canConfirm() || !infra->reviewText().contains(QStringLiteral("conflicting"))) {fail("overlapping remote appointment did not refuse");return;}
            if (press("dismissInfrastructure")) next(13);return;
        case 13:
            if (s.date<s.infrastructure->appointments.front().starts_on || !campaign.paused()) {if (campaign.paused()) press("nextSiteMilestone");return;}
            if (!core::executive_location(s).traveling || !infra->executiveLocation().contains(QStringLiteral("traveling"))) {fail("milestone did not stop at outbound trip");return;}
            reveal_top(item("executiveLocation"));if (!capture(QStringLiteral("-travel")) || !press("labTab") || !press("lab_data")) {fail("Lab inaccessible during trip");return;}
            campaign.management()->dataOffers()->setSelectedId(QStringLiteral("data.open_documents"));
            if (press("buyDataButton")) next(14);return;
        case 14:
            if (s.lab->licenses.size()!=1 || !core::executive_location(s).traveling) {fail("ordinary management did not commit while traveling");return;}
            if (press("operationsTab") && press("nextSiteMilestone")) next(15);return;
        case 15:
            if (!campaign.paused() || s.date<s.infrastructure->appointments.front().visit_on) {if (campaign.paused()) press("nextSiteMilestone");return;}
            if (!s.infrastructure->hardware.front().commissioned_on || !s.infrastructure->appointments.front().inspection || !press("labTab") || !press("lab_research")) {fail("inspection did not commission owned hardware");return;}
            campaign.management()->researchTopics()->setSelectedId(QStringLiteral("research.held_out_evaluation"));
            if (!set("researchSourceChoice","value",QStringLiteral("owned/rack.1")) || !set("researchRateField","value",QStringLiteral("50")) || !set("researchStaffField","value",QStringLiteral("1"))) {fail("owned source missing from Lab allocation");return;}
            if (press("startResearchButton")) next(16);return;
        case 16:
            if (s.lab->research.empty() || s.capacity->projects.back().source_id!="owned/rack.1") {fail("research did not reserve the physical owned source");return;}
            if (press("stepButton")) next(17);return;
        case 17:
            smoke->full_wh=s.infrastructure->sites.front().last_energy_wh;
            if (smoke->full_wh==0 || !press("operationsTab") || !press("operationsSites") || !set("ownedPowerBudget","text",QStringLiteral("5"))) {fail("actual site meter or power control missing");return;}
            if (press("reviewOwnedPower")) next(18);return;
        case 18:confirm(19);return;
        case 19:
            if (s.infrastructure->sites.front().power.back().watts!=5000) {fail("reviewed power budget did not apply");return;}
            if (press("stepButton")) next(20);return;
        case 20:
            if (s.infrastructure->sites.front().last_energy_wh>=smoke->full_wh) {fail("curtailment did not reduce billed energy");return;}
            if (press("reviewEndOpeningLease")) next(21);return;
        case 21:confirm(22);return;
        case 22:
            if (!s.infrastructure->initial_lease_ends_before) {fail("lease notice did not commit");return;}
            if (s.date<*s.infrastructure->initial_lease_ends_before || !campaign.paused()) {if (campaign.paused()) press("nextSiteMilestone");return;}
            if (s.capacity->sources.front().effective.value()!=0 || s.infrastructure->sites.front().meters.empty() || !infra->leaseStatus().contains(QStringLiteral("ended"))) {fail("initial lease continued or metered site vanished");return;}
            reveal_top(item("ownedSiteList"));if (!capture(QStringLiteral("-meter"))) {fail("meter capture");return;}
            if (!infra->wholesalePlans().isEmpty()) {
                if (!set("ownedPowerBudget","text",QString::number(s.infrastructure->sites.front().grid_watts/1000))) {fail("restore power for wholesale");return;}
                if (press("reviewOwnedPower")) next(26);return;
            }
            smoke->saved=snap;campaign.saveTo(save_url);next(23);return;
        case 23:
            if (!std::filesystem::is_regular_file(save_path)) {fail("owned site save missing");return;}
            if (press("stepButton")) next(24);return;
        case 24:
            if (s.date==smoke->saved->checkpoint.world.simulation.date) {fail("clock did not advance before reload");return;}
            campaign.loadFrom(save_url);next(25);return;
        case 25:
            if (snap->generation!=smoke->saved->generation+1 || snap->checkpoint!=smoke->saved->checkpoint || !campaign.paused() || !capture({})) {fail("exact owned site, travel, lease or ledger reload failed");return;}
            qInfo().noquote()<<"Infrastructure smoke passed"<<graphics::name(expected)<<"text="<<campaign.textScalePercent()<<"51 states, map pointer and keyboard list, paid construction, calendar conflict, management during travel, owned Lab work, curtailment, lease notice, wholesale agreements"<<infra->wholesaleContracts()->rowCount()<<"exact save"<<std::filesystem::file_size(save_path)<<"bytes";
            timer->stop();QCoreApplication::exit(0);return;
        case 26:confirm(27);return;
        case 27:
            if (press("operationsWholesale") && press("reviewWholesale")) next(28);return;
        case 28:
            if (!infra->reviewText().contains(QStringLiteral("proportional fee credit")) || !capture(QStringLiteral("-wholesale"))) {fail("wholesale terms and credit review");return;}
            confirm(29);return;
        case 29:
            if (infra->wholesaleContracts()->rowCount()!=1) {fail("wholesale agreement did not commit");return;}
            if (press("stepButton")) next(30);return;
        case 30: {
            const auto& contract=snap->checkpoint.world.competition->wholesale.front();
            if (contract.days.size()!=1 || contract.days.front().seller_posting==0 || contract.days.front().buyer_posting==0) {fail("wholesale payment did not settle bilaterally");return;}
            if (!press("officeTab") || !press("office_markets")) return;
            if (campaign.management()->regionalMarkets()->rowCount()!=8) {fail("regional work report is incomplete");return;}
            reveal_top(item("regionalMarketSelectionDetail"));if (!capture(QStringLiteral("-regions"))) {fail("regional report capture");return;}
            smoke->saved=snap;campaign.saveTo(save_url);next(23);return;
        }
        case 31:
            if (!infra->reviewText().contains(QStringLiteral("fuel")) || !capture(QStringLiteral("-generator"))) {fail("generator terms and readable fuel limit");return;}
            confirm(32);return;
        case 32:
            if (!s.resilience || s.resilience->protections.size()!=1 || !set("insuranceOfferPicker","value",QStringLiteral("insurance.repair"))) {fail("paid resilience installation");return;}
            if (press("reviewSiteInsurance")) next(33);return;
        case 33:
            if (!infra->reviewText().contains(QStringLiteral("deductible")) || !capture(QStringLiteral("-insurance"))) {fail("insurance terms and waiting period");return;}
            confirm(34);return;
        case 34:
            if (s.resilience->policies.size()!=1 || s.resilience->policies.front().covers_from<=s.date) {fail("insurance paid waiting period");return;}
            if (press("operationsSites") && press("reviewOrderOwnedRack")) next(6);return;
        default:fail("unexpected smoke phase");return;
        }
    });
    timer->start();
}

void schedule_civic_smoke(QQuickWindow& window, pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected, pgp::persistence::LocalPaths paths, QString screenshot) {
    using namespace pgp;
    struct State {
        int phase = 0, attempts = 0;
        core::OrdinalDay hearing{0};
        core::Cents cash{0};
        bridge::CampaignSnapshotPtr saved;
    };
    const auto run = std::make_shared<State>();
    auto* timer = new QTimer(&window);
    timer->setInterval(24);
    const auto save = paths.saves_directory / "civic-smoke.pgpsave";
    const auto url = QUrl::fromLocalFile(QString::fromStdU16String(save.u16string()));
    QObject::connect(timer, &QTimer::timeout, &window, [&, timer, run, expected, save, url, screenshot] {
        const auto fail = [&](const char* why) {
            qCritical() << "Civic smoke failed" << run->phase << why << campaign.notice();
            timer->stop();
            QCoreApplication::exit(3);
        };
        const auto item = [&](const char* name) { return visual_item(window.contentItem(), QString::fromLatin1(name)); };
        const auto press = [&](const char* name) {
            auto* control = item(name);
            reveal_top(control);
            if (key(window, control, Qt::Key_Space)) return true;
            fail(name);
            return false;
        };
        if (++run->attempts > 1200) { fail("timed out"); return; }
        if (!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        const auto snapshot = campaign.snapshot();
        const auto& state = snapshot->checkpoint.world.simulation;
        auto* civic = campaign.civic();
        if (!campaign.welcome() && campaign.requiredKind() == QStringLiteral("content")) {
            const auto choices = campaign.requiredChoices();
            if (choices.isEmpty()) { fail("missing required choice"); return; }
            campaign.choose(choices.back().toMap().value("choiceId").toString());
            return;
        }
        switch (run->phase) {
        case 0: {
            if (!campaign.welcome() || !civic->available() || std::filesystem::exists(save) ||
                window.rendererInterface()->graphicsApi() != expected) { fail("scratch campaign or renderer"); return; }
            const auto choices = campaign.scenarios();
            const auto selected = std::find_if(choices.begin(), choices.end(), [](const auto& row) {
                return row.toMap().value("id") == QStringLiteral("scenario.first_playable");
            });
            auto* picker = item("competitionChoice");
            if (!picker || selected == choices.end() ||
                !picker->setProperty("currentIndex", static_cast<int>(selected - choices.begin()))) { fail("intro scenario selection"); return; }
            campaign.setDaysPerSecond(60);
            if (press("newCampaignButton")) ++run->phase;
            return;
        }
        case 1:
            if (press("civicTab")) ++run->phase;
            return;
        case 2:
            if (civic->dockets()->rowCount() != 2 || !civic->calendar().contains(QStringLiteral("2016-11-08")) ||
                !civic->institutions().contains(QStringLiteral("House"))) { fail("opening docket, officeholders or dates"); return; }
            civic->dockets()->setSelectedId(QStringLiteral("2"));
            run->cash = state.cash;
            run->hearing = *state.civic->dockets.at(1).next_on;
            if (press("reviewCivicAdvocacy")) ++run->phase;
            return;
        case 3:
            if (civic->reviewText().isEmpty() || state.cash != run->cash) { fail("confirmation changed cash"); return; }
            if (press("confirmCivicAdvocacy")) ++run->phase;
            return;
        case 4: {
            const auto& docket = state.civic->dockets.at(1);
            if (docket.submissions.size() != 1 || run->cash.value() - state.cash.value() != docket.submissions.front().fee.value()) {
                fail("disclosure lacks exact paid expense"); return;
            }
            if (press("advanceCivic")) ++run->phase;
            return;
        }
        case 5:
            if (!campaign.paused()) return;
            if (state.date < run->hearing) { (void)press("advanceCivic"); return; }
            if (state.date != run->hearing || state.civic->dockets.at(1).decisions.size() != 1) { fail("committee stop"); return; }
            if (!screenshot.isEmpty()) {
                reveal_top(item("civicPolicyDetail"));
                if (!window.grabWindow().save(screenshot)) { fail("civic view screenshot"); return; }
            }
            run->saved = snapshot;
            campaign.saveTo(url);
            ++run->phase;
            return;
        case 6:
            if (!std::filesystem::is_regular_file(save)) { fail("civic save missing"); return; }
            campaign.loadFrom(url);
            ++run->phase;
            return;
        default:
            if (snapshot->generation == run->saved->generation || snapshot->checkpoint != run->saved->checkpoint) {
                fail("civic save did not reopen exactly"); return;
            }
            qInfo().noquote() << "Civic smoke passed" << graphics::name(expected) << "text=" << campaign.textScalePercent()
                << "docket preview, paid disclosure, committee boundary and exact reload" << std::filesystem::file_size(save) << "bytes";
            timer->stop();
            QCoreApplication::exit(0);
        }
    });
    timer->start();
}

void schedule_society_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot,bool institutions) {
    using namespace pgp;
    struct State {
        int phase=0,attempts=0;
        QString model,report;
        core::Cents cash{0};
        core::OrdinalDay boundary{0};
        bridge::CampaignSnapshotPtr saved;
    };
    const auto run=std::make_shared<State>();auto* timer=new QTimer(&window);timer->setInterval(24);
    const auto save=paths.saves_directory/(institutions?"institutions-smoke.pgpsave":"society-smoke.pgpsave");
    const auto url=QUrl::fromLocalFile(QString::fromStdU16String(save.u16string()));
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,run,expected,save,url,screenshot,institutions] {
        const auto fail=[&](const char* why){qCritical()<<(institutions?"Institutions smoke failed":"Society smoke failed")<<run->phase<<why<<campaign.notice();timer->stop();QCoreApplication::exit(3);};
        const auto item=[&](const char* name){return visual_item(window.contentItem(),QString::fromLatin1(name));};
        const auto press=[&](const char* name){auto* control=item(name);reveal_top(control);if(key(window,control,Qt::Key_Space)) return true;fail(name);return false;};
        if(++run->attempts>2400) {fail("timed out");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        const auto snapshot=campaign.snapshot();const auto& state=snapshot->checkpoint.world.simulation;
        auto* society=campaign.society();auto* management=campaign.management();
        if(!campaign.welcome() && campaign.requiredKind()==QStringLiteral("content")) {
            const auto choices=campaign.requiredChoices();if(choices.isEmpty()) {fail("required choice missing");return;}
            campaign.choose(choices.back().toMap().value("choiceId").toString());return;
        }
        if(institutions && run->phase>=6) {
            const auto& world=snapshot->checkpoint.world;
            auto* desk=campaign.institutions();
            if(!world.authority || !desk->available()) {fail("authority actor is unavailable");return;}
            const auto& a=*world.authority;
            const auto section=[&](int index) {
                auto* picker=item("institutionsSection");
                if(picker && picker->setProperty("currentIndex",index)) return true;
                fail("institutions section");
                return false;
            };
            const auto confirm=[&] {
                if(desk->reviewText().isEmpty()) {fail("institutional terms were not displayed");return false;}
                return press("confirmInstitutionAction");
            };
            const auto capture=[&](QString suffix,const char* reveal) {
                if(screenshot.isEmpty()) return true;
                auto target=screenshot;
                if(!suffix.isEmpty()) {
                    const auto dot=target.lastIndexOf(u'.');if(dot>=0) target.truncate(dot);
                    target+=suffix+QStringLiteral(".png");
                }
                reveal_top(item(reveal));
                if(window.grabWindow().save(target)) return true;
                fail("institutional screenshot");return false;
            };
            if(campaign.requiredKind()==QStringLiteral("management")) {campaign.acknowledgeManagement();return;}
            if(campaign.requiredKind()==QStringLiteral("site")) {campaign.acknowledgeSiteAlert();return;}
            switch(run->phase) {
            case 6:
                if(state.deployments->records.empty() || desk->regions()->rowCount()!=8) {fail("deployed service or eight regions");return;}
                if(press("institutionsTab")) ++run->phase;
                return;
            case 7:
                desk->regions()->setSelectedId(QStringLiteral("region.europe"));
                if(section(1)) ++run->phase;
                return;
            case 8: if(press("reviewInstitutionTestimony")) ++run->phase;return;
            case 9:
                if(!a.testimonies.empty()) {fail("preview disclosed the report");return;}
                if(confirm()) ++run->phase;
                return;
            case 10:
                if(a.testimonies.size()!=1 || QString::fromStdString(a.testimonies.front().report)!=run->report) {fail("actual testimony");return;}
                if(press("reviewInstitutionServicePromise")) ++run->phase;
                return;
            case 11: if(confirm()) ++run->phase;return;
            case 12: if(press("reviewInstitutionPublicationPromise")) ++run->phase;return;
            case 13: if(confirm()) ++run->phase;return;
            case 14:
                if(!campaign.paused()) return;
                if(a.promises.size()!=2) {fail("public commitments");return;}
                if(a.cases.empty()) {(void)press("advanceInstitutions");return;}
                if(campaign.requiredKind()!=QStringLiteral("institution") || state.date!=a.promises.back().due_on ||
                    !campaign.requiredText().contains(QStringLiteral("public commitment"))) {fail("prepared morning halted for actual concern");return;}
                if(press("openInstitutionNotice")) ++run->phase;
                return;
            case 15: if(press("reviewInstitutionCitation")) ++run->phase;return;
            case 16: if(confirm()) ++run->phase;return;
            case 17: if(press("acknowledgeInstitutionalNotice")) ++run->phase;return;
            case 18: {
                if(!campaign.paused()) return;
                const auto formal=core::advance_days(a.cases.front().opened_on,a.catalog.concern_period);
                if(state.date<formal) {(void)press("advanceInstitutions");return;}
                if(state.date!=formal || campaign.requiredKind()!=QStringLiteral("institution") || !a.cases.front().testimony) {fail("formal response boundary");return;}
                run->cash=state.cash;
                if(press("reviewInstitutionNegotiation")) ++run->phase;
                return;
            }
            case 19:
                if(state.cash!=run->cash || a.cases.front().negotiated_on) {fail("review altered the dispute");return;}
                if(confirm()) ++run->phase;
                return;
            case 20:
                if(!a.cases.front().negotiated_on || state.cash.value()!=run->cash.value()-a.catalog.remedy_fee.value()) {fail("negotiated response or fee");return;}
                if(press("labTab")) ++run->phase;
                return;
            case 21:
                if(press("lab_research")) {
                    management->researchTopics()->setSelectedId(QStringLiteral("research.held_out_evaluation"));
                    ++run->phase;
                }
                return;
            case 22: if(press("publishResearchButton")) ++run->phase;return;
            case 23:
                if(!a.cases.front().resolved_on || !a.promises.front().met_on) {fail("publication or service did not satisfy its obligation");return;}
                if(press("institutionsTab") && section(0)) ++run->phase;
                return;
            case 24: {
                const auto roles=desk->roles();
                const auto role=std::ranges::find_if(roles,[](const auto& row){return row.toMap().value("id")==QStringLiteral("institution.delegation");});
                auto* picker=item("institutionRole");
                if(!picker || role==roles.end() || !picker->setProperty("currentIndex",static_cast<int>(role-roles.begin()))) {fail("select delegation");return;}
                run->cash=state.cash;
                if(press("reviewInstitutionMandate")) ++run->phase;
                return;
            }
            case 25:
                if(state.cash!=run->cash || !a.mandates.empty() || !desk->reviewText().contains(QStringLiteral("Europe Public Services Council"))) {
                    fail("named terms or preview mutation");return;
                }
                if(capture(QStringLiteral(".terms"),"institutionsReviewText") && confirm()) ++run->phase;
                return;
            case 26:
                if(!campaign.paused()) return;
                if(a.mandates.size()!=1 || a.mandates.front().region!="region.europe") {fail("funded delegation");return;}
                if(a.mandates.front().reviews.empty()) {(void)press("advanceInstitutions");return;}
                if(runtime::mandate_status(world,a.mandates.front(),state.date)!="operating") {fail("actual operating grant");return;}
                if(!capture({},"institutionCoverage")) return;
                run->saved=snapshot;campaign.saveTo(url);++run->phase;return;
            case 27:
                if(!std::filesystem::is_regular_file(save)) {fail("save missing");return;}
                campaign.loadFrom(url);++run->phase;return;
            default:
                if(snapshot->generation==run->saved->generation || snapshot->checkpoint!=run->saved->checkpoint) {fail("exact institutional reload");return;}
                qInfo().noquote()<<"Institutions smoke passed"<<graphics::name(expected)<<"text="<<campaign.textScalePercent()
                    <<"testimony, commitments, formal response, paid delegation and exact reload"<<std::filesystem::file_size(save)<<"bytes";
                timer->stop();QCoreApplication::exit(0);return;
            }
        }
        switch(run->phase) {
        case 0: {
            if(!campaign.welcome() || !society->available() || std::filesystem::exists(save) || window.rendererInterface()->graphicsApi()!=expected) {fail("scratch campaign or renderer");return;}
            const auto choices=campaign.scenarios();
            const auto selected=std::find_if(choices.begin(),choices.end(),[](const auto& row){return row.toMap().value("id")==QStringLiteral("scenario.first_playable");});
            auto* picker=item("competitionChoice");
            if(!picker || selected==choices.end() || !picker->setProperty("currentIndex",static_cast<int>(selected-choices.begin()))) {fail("intro scenario selection");return;}
            campaign.setDaysPerSecond(60);if(press("newCampaignButton")) ++run->phase;return;
        }
        case 1: management->buyData(QStringLiteral("data.open_documents"));++run->phase;return;
        case 2: management->research(QStringLiteral("research.held_out_evaluation"),QStringLiteral("lease.initial"),QStringLiteral("20"),QStringLiteral("2"));++run->phase;return;
        case 3: management->train(QStringLiteral("recipe.document_baseline"),QStringLiteral("lease.initial"),{QStringLiteral("data.open_documents")},QStringLiteral("60"),QStringLiteral("3"),QStringLiteral("3600"));++run->phase;return;
        case 4: {
            if(state.lab->models.empty()) {fail("training command");return;}
            const auto& model=state.lab->models.back();
            const auto work=std::find_if(state.capacity->projects.begin(),state.capacity->projects.end(),[&](const auto& row){return row.id==model.project_id;});
            if(work==state.capacity->projects.end()) {fail("training project");return;}
            if(work->status==core::ProjectStatus::active) {if(campaign.paused()) campaign.advanceTo(QStringLiteral("project"));return;}
            run->model=QString::fromStdString(model.id);
            management->evaluate(run->model,QStringLiteral("evaluation.held_out"),QStringLiteral("lease.initial"),QStringLiteral("20"),QStringLiteral("1"));++run->phase;return;
        }
        case 5:
            if(state.lab->evaluations.empty()) {fail("evaluation command");return;}
            if(!state.lab->evaluations.back().completed) {if(campaign.paused()) campaign.advanceTo(QStringLiteral("evaluation"));return;}
            run->report=QString::fromStdString(state.lab->evaluations.back().id);
            management->deploy(QStringLiteral("product.education_model"),run->model,run->report,{},false,QStringLiteral("region.europe"));++run->phase;return;
        case 6:
            if(state.deployments->records.empty()) {fail("deployment command");return;}
            if(press("societyTab")) ++run->phase;return;
        case 7:
            if(society->regions()->rowCount()!=8 || !society->global().contains(QStringLiteral("lowest region")) ||
                !society->forum().contains(QStringLiteral("ballot"))) {fail("regional or forum readouts");return;}
            society->regions()->setSelectedId(QStringLiteral("region.europe"));++run->phase;return;
        case 8:
            run->cash=state.cash;if(press("reviewMediaCampaign")) ++run->phase;return;
        case 9:
            if(society->reviewText().isEmpty() || state.cash!=run->cash) {fail("media review mutated cash");return;}
            if(press("confirmSocietyAction")) ++run->phase;return;
        case 10:
            if(snapshot->checkpoint.world.society->campaigns.size()!=1 || run->cash.value()-state.cash.value()!=snapshot->checkpoint.world.society->catalog.media.fee.value()) {fail("media fee or receipt");return;}
            run->cash=state.cash;if(press("reviewForumEndorsement")) ++run->phase;return;
        case 11:
            if(society->reviewText().isEmpty() || state.cash!=run->cash) {fail("forum review mutated cash");return;}
            if(!screenshot.isEmpty()) {
                auto actions=screenshot;const auto dot=actions.lastIndexOf(u'.');if(dot>=0) actions.truncate(dot);actions+=QStringLiteral(".actions.png");
                reveal_top(item("societyReview"));if(!window.grabWindow().save(actions)) {fail("disclosure screenshot");return;}
            }
            if(press("confirmSocietyAction")) ++run->phase;return;
        case 12: {
            const auto& shared=*snapshot->checkpoint.world.society;
            if(shared.rounds.front().endorsements.size()!=1 || shared.rounds.front().endorsements.front().region!="region.europe" ||
                run->cash.value()-state.cash.value()!=shared.catalog.forum.endorsement_fee.value()) {fail("forum disclosure or exact fee");return;}
            run->boundary=*core::next_society_boundary(shared,state.date);
            if(press("advanceSociety")) ++run->phase;return;
        }
        case 13:
            if(!campaign.paused()) return;
            if(state.date<run->boundary) {(void)press("advanceSociety");return;}
            if(state.date!=run->boundary || snapshot->checkpoint.world.society->months.empty()) {fail("regional boundary stop");return;}
            if(!screenshot.isEmpty()) {reveal_top(item("societyGlobal"));if(!window.grabWindow().save(screenshot)) {fail("world screenshot");return;}}
            run->saved=snapshot;campaign.saveTo(url);++run->phase;return;
        case 14:
            if(!std::filesystem::is_regular_file(save)) {fail("save missing");return;}
            campaign.loadFrom(url);++run->phase;return;
        default:
            if(snapshot->generation==run->saved->generation || snapshot->checkpoint!=run->saved->checkpoint) {fail("exact reload");return;}
            qInfo().noquote()<<"Society smoke passed"<<graphics::name(expected)<<"text="<<campaign.textScalePercent()
                <<"8 regional profiles, delivered welfare, media preview/payment, member disclosure, month boundary and exact reload"<<std::filesystem::file_size(save)<<"bytes";
            timer->stop();QCoreApplication::exit(0);
        }
    });
    timer->start();
}

void schedule_frontier_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct State {
        int phase=0,attempts=0;
        core::Cents cash{0},fee{0};
        core::OrdinalDay result_on{0};
        bridge::CampaignSnapshotPtr saved;
    };
    const auto run=std::make_shared<State>();
    auto* timer=new QTimer(&window);timer->setInterval(24);
    const auto starting=paths.saves_directory/"frontier-start.pgpsave";
    const auto save=paths.saves_directory/"frontier-smoke.pgpsave";
    const auto url=QUrl::fromLocalFile(QString::fromStdU16String(save.u16string()));
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,run,expected,starting,save,url,screenshot] {
        const auto fail=[&](const char* why){qCritical()<<"Frontier smoke failed"<<run->phase<<why<<campaign.notice();timer->stop();QCoreApplication::exit(3);};
        const auto item=[&](const char* name){return visual_item(window.contentItem(),QString::fromLatin1(name));};
        const auto press=[&](const char* name){auto* control=item(name);reveal_top(control);if(key(window,control,Qt::Key_Space)) return true;fail(name);return false;};
        const auto set=[&](const char* name,const char* value){auto* field=item(name);if(field && field->setProperty("text",QString::fromLatin1(value))) return true;fail(name);return false;};
        const auto choose=[&](const char* name,const char* id) {
            auto* picker=item(name);
            if (!picker) {fail(name);return false;}
            const auto rows=picker->property("model").toList();
            for (qsizetype i=0;i<rows.size();++i) if (rows[i].toMap().value("id").toString()==QString::fromLatin1(id)) {
                if (!key(window,picker,Qt::Key_Home)) break;
                for (qsizetype step=0;step<i;++step) if (!key(window,picker,Qt::Key_Down)) {fail(name);return false;}
                return true;
            }
            fail(name);return false;
        };
        const auto section=[&](int index) {
            auto* picker=item("frontierSection");
            if(picker && picker->setProperty("currentIndex",index)) return true;
            fail("Frontier section");return false;
        };
        const auto capture=[&](QString suffix,const char* reveal) {
            if(screenshot.isEmpty()) return true;
            auto target=screenshot;
            if(!suffix.isEmpty()) {const auto dot=target.lastIndexOf(u'.');if(dot>=0) target.truncate(dot);target+=suffix+QStringLiteral(".png");}
            reveal_top(item(reveal));
            if(window.grabWindow().save(target)) return true;
            fail("Frontier screenshot");return false;
        };
        if(++run->attempts>1200) {fail("timed out");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        if(window.rendererInterface()->graphicsApi()!=expected) {fail("graphics backend");return;}
        if(campaign.requiredKind()==QStringLiteral("content")) {
            const auto choices=campaign.requiredChoices();if(choices.isEmpty()) {fail("required choice missing");return;}
            campaign.choose(choices.back().toMap().value("choiceId").toString());return;
        }
        if(campaign.requiredKind()==QStringLiteral("management")) {campaign.acknowledgeManagement();return;}
        if(campaign.requiredKind()==QStringLiteral("site")) {campaign.acknowledgeSiteAlert();return;}
        const auto snapshot=campaign.snapshot();const auto& state=snapshot->checkpoint.world.simulation;
        auto* desk=campaign.frontier();
        switch(run->phase) {
        case 0:
            campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdU16String(starting.u16string())));++run->phase;return;
        case 1:
            if(!desk->available() || !desk->editable() || !state.frontier->systems.empty() || state.deployments->records.size()!=1) {fail("prepared late-era fixture");return;}
            if(press("frontierTab")) ++run->phase;return;
        case 2:
            if(set("frontierSystemName","Native research service") && press((QStringLiteral("frontierMember/")+desk->services().front().toMap().value("id").toString()).toUtf8().constData()) && press("reviewFrontierSystem")) {
                run->cash=state.cash;++run->phase;
            }
            return;
        case 3:
            if(desk->reviewText().isEmpty() || state.cash!=run->cash || !state.frontier->systems.empty()) {fail("registration preview changed state");return;}
            if(capture(QStringLiteral("-registration"),"frontierReview") && press("confirmFrontierAction")) ++run->phase;return;
        case 4:
            if(state.frontier->systems.size()!=1 || desk->systems()->rowCount()!=1) {fail("deployed system registration");return;}
            if(choose("frontierTrialSource","lease.initial") && set("frontierTrialRate","100") && set("frontierTrialStaff","2") && press("reviewFrontierTrial")) ++run->phase;
            return;
        case 5: {
            const auto& panel=state.frontier->catalog.panels.front();
            const auto quote=core::quote_frontier_trial(state,{1,core::FrontierTrialKind::assessment,panel.id,{"lease.initial",core::MilliCUPerDay{100000},2}},core::Cents{0});
            if(desk->reviewText().isEmpty() || state.cash!=run->cash || !state.frontier->trials.empty()) {fail("paid assessment preview changed state");return;}
            run->fee=quote.fee;run->result_on=quote.earliest_result;
            if(capture(QStringLiteral("-terms"),"frontierReview") && press("confirmFrontierAction")) ++run->phase;return;
        }
        case 6:
            if(state.frontier->trials.size()!=1 || core::checked_unit_subtract(run->cash,state.cash)!=run->fee) {fail("actual assessment funding");return;}
            campaign.advanceTo(QStringLiteral("evaluation"));++run->phase;return;
        case 7:
            if(!campaign.paused()) return;
            if(state.date<run->result_on) {campaign.advanceTo(QStringLiteral("evaluation"));return;}
            if(state.date!=run->result_on || !state.frontier->trials.front().result || !state.frontier->trials.front().result->valid ||
                core::frontier_qualification(*state.frontier,1).agi_on) {fail("independent result or premature qualification");return;}
            if(capture({},"frontierSystemsDetail") && section(2)) ++run->phase;return;
        case 8:
            if(choose("frontierQueuedTopic","research.refresh_measurement") && press("appendFrontierQueue") &&
                set("frontierResearchLimit","50000") && set("frontierResearchReserve","20000") && set("frontierResearchInterval","14") &&
                set("frontierResearchRate","40") && set("frontierResearchStaff","2") && press("reviewFrontierResearch")) ++run->phase;
            return;
        case 9:
            if(desk->reviewText().isEmpty() || !state.frontier->research_programs.empty()) {fail("research preview");return;}
            if(press("confirmFrontierAction")) ++run->phase;return;
        case 10:
            if(state.frontier->research_programs.size()!=1 || !state.frontier->research_programs.front().decisions.empty()) {fail("bounded ordered queue");return;}
            if(press("cancelFrontierResearch")) ++run->phase;return;
        case 11: if(press("confirmFrontierAction")) ++run->phase;return;
        case 12:
            if(!state.frontier->research_programs.front().cancelled_on) {fail("research cancellation");return;}
            if(capture(QStringLiteral("-queue"),"frontierResearchProgramsDetail") && section(3)) ++run->phase;return;
        case 13:
            if(choose("frontierRefreshRecipe","recipe.frontier_mesh") && choose("frontierRefreshEvaluation","evaluation.held_out") && press((QStringLiteral("frontierRefreshData/")+desk->licensedData().front().toMap().value("id").toString()).toUtf8().constData()) &&
                set("frontierRefreshWork","30000") && set("frontierRefreshGain","1") && set("frontierRefreshRate","100") && set("frontierRefreshStaff","2") &&
                set("frontierRefreshLimit","50000") && set("frontierRefreshReserve","20000") && set("frontierRefreshInterval","90") &&
                set("frontierRefreshRuns","1") && press("reviewFrontierRefresh")) ++run->phase;
            return;
        case 14:
            if(desk->reviewText().isEmpty() || !state.frontier->refresh_programs.empty()) {fail("refresh preview");return;}
            if(capture(QStringLiteral("-refresh"),"frontierReview") && press("confirmFrontierAction")) ++run->phase;return;
        case 15:
            if(state.frontier->refresh_programs.size()!=1 || !state.frontier->refresh_programs.front().runs.empty()) {fail("bounded refresh authorization");return;}
            if(press("cancelFrontierRefresh")) ++run->phase;return;
        case 16: if(press("confirmFrontierAction")) ++run->phase;return;
        case 17:
            if(!state.frontier->refresh_programs.front().cancelled_on || state.deployments->records.size()!=1) {fail("refresh cancellation or serving protection");return;}
            run->saved=snapshot;campaign.saveTo(url);++run->phase;return;
        case 18:
            if(!std::filesystem::is_regular_file(save)) {fail("save missing");return;}
            campaign.loadFrom(url);++run->phase;return;
        default:
            if(snapshot->generation==run->saved->generation || snapshot->checkpoint!=run->saved->checkpoint) {fail("exact Frontier reload");return;}
            qInfo().noquote()<<"Frontier smoke passed"<<graphics::name(expected)<<"text="<<campaign.textScalePercent()
                <<"deployed system, paid independent assessment, bounded queue, refresh authorization, cancellations and exact reload"
                <<std::filesystem::file_size(save)<<"bytes";
            timer->stop();QCoreApplication::exit(0);
        }
    });
    timer->start();
}

void schedule_succession_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct State {
        int phase=0,attempts=0;
        core::Cents cash{0};core::OrdinalDay handover{0};
        std::vector<core::EquityHolding> holdings;
        bridge::CampaignSnapshotPtr saved;
    };
    const auto run=std::make_shared<State>();auto* timer=new QTimer(&window);timer->setInterval(24);
    const auto starting=paths.saves_directory/"leadership-start.pgpsave";
    const auto save=paths.saves_directory/"leadership-smoke.pgpsave";
    const auto url=QUrl::fromLocalFile(QString::fromStdU16String(save.u16string()));
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,run,expected,starting,save,url,screenshot] {
        const auto fail=[&](const char* why){qCritical()<<"Leadership smoke failed"<<run->phase<<why<<campaign.notice();timer->stop();QCoreApplication::exit(3);};
        const auto item=[&](const char* name){return visual_item(window.contentItem(),QString::fromLatin1(name));};
        const auto press=[&](const char* name){auto* control=item(name);reveal_top(control);if(key(window,control,Qt::Key_Space)) return true;fail(name);return false;};
        const auto set=[&](const char* name,const char* value){auto* field=item(name);if(field && field->setProperty("text",QString::fromLatin1(value))) return true;fail(name);return false;};
        const auto capture=[&](QString suffix,const char* reveal) {
            if(screenshot.isEmpty()) return true;
            auto target=screenshot;
            if(!suffix.isEmpty()) {const auto dot=target.lastIndexOf(u'.');if(dot>=0) target.truncate(dot);target+=suffix+QStringLiteral(".png");}
            reveal_top(item(reveal));if(window.grabWindow().save(target)) return true;fail("leadership screenshot");return false;
        };
        if(++run->attempts>1800) {fail("timed out");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        if(window.rendererInterface()->graphicsApi()!=expected) {fail("graphics backend");return;}
        if(campaign.requiredKind()==QStringLiteral("content")) {
            const auto choices=campaign.requiredChoices();if(choices.isEmpty()) {fail("required choice missing");return;}
            campaign.choose(choices.back().toMap().value("choiceId").toString());return;
        }
        if(campaign.requiredKind()==QStringLiteral("management")) {campaign.acknowledgeManagement();return;}
        if(campaign.requiredKind()==QStringLiteral("site")) {campaign.acknowledgeSiteAlert();return;}
        const auto snapshot=campaign.snapshot();const auto& company=snapshot->checkpoint.world.simulation;
        auto* desk=campaign.leadership();
        switch(run->phase) {
        case 0: campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdU16String(starting.u16string())));++run->phase;return;
        case 1:
            if(!desk->available() || !desk->editable() || company.succession->terms.size()!=1) {fail("prepared leadership fixture");return;}
            run->holdings=company.accounts->holdings;run->cash=company.cash;campaign.setDaysPerSecond(60);
            if(press("leadershipTab")) ++run->phase;return;
        case 2:
            if(set("leadershipRate","10") && set("leadershipStaff","1") && press("leadershipRetainChair") && press("reviewLeadershipNomination")) ++run->phase;return;
        case 3:
            if(desk->reviewText().isEmpty() || !company.succession->plans.empty() || company.cash!=run->cash) {fail("nomination preview changed state");return;}
            if(capture(QStringLiteral("-nomination"),"leadershipReview") && press("confirmLeadershipAction")) ++run->phase;return;
        case 4:
            if(company.succession->plans.size()!=1 || company.cash!=run->cash) {fail("nomination needs a real board vote");return;}
            run->handover=company.succession->plans.front().handover_on;
            if(press("reviewLeadershipSupport")) ++run->phase;return;
        case 5:
            if(desk->reviewText().isEmpty() || company.cash!=run->cash || !company.succession->plans.front().project.empty()) {fail("board preview changed cash or funded work");return;}
            if(capture(QStringLiteral("-board"),"leadershipReview") && press("confirmLeadershipAction")) ++run->phase;return;
        case 6: {
            const auto& p=company.succession->plans.front();
            const auto person=core::leadership_candidate(core::succession_catalog(company),p.candidate);
            if(p.status!=core::SuccessionStatus::approved || core::checked_unit_subtract(run->cash,company.cash)!=person.signing_cost || p.ballots.empty()) {fail("paid board-approved transition");return;}
            if(press("advanceLeadership")) ++run->phase;return;
        }
        case 7:
            if(!campaign.paused()) return;
            if(company.date<run->handover) {(void)press("advanceLeadership");return;}
            if(company.date!=run->handover || company.succession->terms.size()!=2 || !desk->founderChair() ||
                company.accounts->holdings!=run->holdings || campaign.requiredKind()!=QStringLiteral("leadership")) {fail("actual handover, retained trust or required review");return;}
            if(capture({},"leadershipOverview") && press("acknowledgeLeadership")) ++run->phase;return;
        case 8: if(press("reviewFounderChairRetirement")) ++run->phase;return;
        case 9:
            if(desk->reviewText().isEmpty() || !desk->founderChair()) {fail("chair retirement preview");return;}
            if(capture(QStringLiteral("-chair"),"leadershipReview") && press("confirmLeadershipAction")) ++run->phase;return;
        case 10:
            if(core::founder_role(*company.succession)!=core::FounderRole::trust || company.accounts->holdings!=run->holdings || company.succession->terms.size()!=2) {fail("retained trust and operating successor");return;}
            if(press("acknowledgeLeadership")) ++run->phase;return;
        case 11: {
            auto* picker=item("leadershipCandidate");const auto choices=picker?picker->property("model").toList():QVariantList{};
            const auto current=QString::fromStdString(company.succession->terms.back().person);
            for(qsizetype i=0;i<choices.size();++i) if(choices[i].toMap().value("id").toString()!=current) {
                if(!key(window,picker,Qt::Key_Home)) {fail("successor picker");return;}
                for(qsizetype step=0;step<i;++step) (void)key(window,picker,Qt::Key_Down);
                if(press("reviewLeadershipNomination")) ++run->phase;return;
            }
            fail("next eligible successor");return;
        }
        case 12: if(press("confirmLeadershipAction")) ++run->phase;return;
        case 13:
            if(company.succession->plans.size()!=2) {fail("next leadership proposal");return;}
            desk->plans()->setSelectedId(QStringLiteral("2"));++run->phase;return;
        case 14: if(press("reviewLeadershipCancellation")) ++run->phase;return;
        case 15: if(press("confirmLeadershipAction")) ++run->phase;return;
        case 16:
            if(company.succession->plans.back().status!=core::SuccessionStatus::cancelled || company.succession->terms.size()!=2) {fail("cancelled next plan displaced current executive");return;}
            if(!capture(QStringLiteral("-trust"),"leadershipOverview")) return;
            run->saved=snapshot;campaign.saveTo(url);++run->phase;return;
        case 17:
            if(!std::filesystem::is_regular_file(save)) {fail("leadership save missing");return;}
            campaign.loadFrom(url);++run->phase;return;
        default:
            if(snapshot->generation==run->saved->generation || snapshot->checkpoint!=run->saved->checkpoint) {fail("exact leadership reload");return;}
            qInfo().noquote()<<"Leadership smoke passed"<<graphics::name(expected)<<"text="<<campaign.textScalePercent()
                <<"nomination, actual board vote/payment, prepared handover, founder chair retirement, cancelled next plan and exact reload"
                <<std::filesystem::file_size(save)<<"bytes";
            timer->stop();QCoreApplication::exit(0);
        }
    });
    timer->start();
}

void schedule_objectives_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct State { int phase=0,attempts=0;bridge::CampaignSnapshotPtr saved; };
    const auto run=std::make_shared<State>();
    auto* timer=new QTimer(&window);timer->setInterval(25);
    const auto start=paths.saves_directory/"objective-start.pgpsave";
    const auto save=paths.saves_directory/"objective-breach.pgpsave";
    const auto start_url=QUrl::fromLocalFile(QString::fromStdU16String(start.u16string()));
    const auto save_url=QUrl::fromLocalFile(QString::fromStdU16String(save.u16string()));
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,run,expected,start_url,save_url,save,screenshot] {
        const auto fail=[&](const char* why){qCritical()<<"Objectives smoke failed"<<run->phase<<why<<campaign.notice();timer->stop();QCoreApplication::exit(3);};
        const auto item=[&](const char* name){return visual_item(window.contentItem(),QString::fromLatin1(name));};
        const auto press=[&](const char* name){auto* control=item(name);reveal_top(control);if(key(window,control,Qt::Key_Space)) return true;fail(name);return false;};
        if(++run->attempts>650) {fail("timed out");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        if(window.rendererInterface()->graphicsApi()!=expected) {fail("graphics backend");return;}
        const auto snapshot=campaign.snapshot();const auto& world=snapshot->checkpoint.world;
        auto* desk=campaign.objectives();
        const auto breach_day=core::to_ordinal({2104,2,1});
        switch(run->phase) {
        case 0: campaign.loadFrom(start_url);++run->phase;return;
        case 1:
            if(!desk->available() || desk->primary()!=QStringLiteral("utopia") || !world.objectives->months.empty()) {fail("unsettled fixture");return;}
            if(press("objectivesTab")) ++run->phase;return;
        case 2: if(press("stepButton")) ++run->phase;return;
        case 3:
            if(runtime::campaign_day(world)!=breach_day || campaign.requiredKind()!=QStringLiteral("objective") || !campaign.paused() || campaign.canAdvance()) {
                fail("actual month-end loss and automatic pause");return;
            }
            if(press("openObjectivePostmortem")) ++run->phase;return;
        case 4: {
            auto* postmortem=item("objectivesPostmortem");
            if(!postmortem || !postmortem->isVisible() || !postmortem->property("text").toString().contains(QStringLiteral("2104-01-31"))) {
                fail("causal postmortem date");return;
            }
            if(!screenshot.isEmpty()) {reveal_top(postmortem);if(!window.grabWindow().save(screenshot)) {fail("objective screenshot");return;}}
            run->saved=snapshot;campaign.saveTo(save_url);++run->phase;return;
        }
        case 5:
            if(!std::filesystem::is_regular_file(save)) {fail("breach save");return;}
            campaign.loadFrom(save_url);++run->phase;return;
        case 6:
            if(snapshot->generation==run->saved->generation || snapshot->checkpoint!=run->saved->checkpoint || campaign.requiredKind()!=QStringLiteral("objective")) {
                fail("exact breach reload");return;
            }
            if(press("continueScoredObjectives")) ++run->phase;return;
        case 7:
            if(campaign.requiredKind()!=QStringLiteral("none") || runtime::objective_result(world,core::ObjectiveKind::utopia)!=runtime::ObjectiveResult::lost) {
                fail("continuation erased the lost check");return;
            }
            if(press("stepButton")) ++run->phase;return;
        case 8:
            if(world.simulation.date!=core::advance_days(breach_day,core::Days{1}) || !desk->canEndEarly()) {fail("one more scored day");return;}
            if(press("endScoredObjectives")) ++run->phase;return;
        case 9: if(press("confirmObjectiveAction")) ++run->phase;return;
        case 10:
            if(!runtime::objective_time_stopped(world) || campaign.canAdvance()) {fail("recorded early ending");return;}
            if(press("continueAfterObjectiveEnding")) ++run->phase;return;
        case 11: if(press("confirmObjectiveAction")) ++run->phase;return;
        case 12: if(press("stepButton")) ++run->phase;return;
        default:
            if(world.objectives->mode!=runtime::ObjectiveMode::sandbox ||
                world.objectives->end->day!=core::advance_days(breach_day,core::Days{1}) ||
                world.simulation.date!=core::advance_days(breach_day,core::Days{2})) {fail("sandbox work and preserved score");return;}
            qInfo().noquote()<<"Objectives smoke passed"<<graphics::name(expected)<<"text="<<campaign.textScalePercent()
                <<"settled breach, exact reload, scored continuation, early postmortem and sandbox"<<std::filesystem::file_size(save)<<"bytes";
            timer->stop();QCoreApplication::exit(0);
        }
    });
    timer->start();
}
