#include "menu_smoke.hpp"
#include "campaign_controller.hpp"
#include "development_tools.hpp"
#include "founding_fixture.hpp"
#include "menu_media.hpp"
#include "music_controller.hpp"
#include <pgp/persistence/settings.hpp>
#include <QAccessible>
#include <QCoreApplication>
#include <QFile>
#include <QKeyEvent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <memory>
#include <iostream>

namespace {
QQuickItem* find(QQuickItem* root,const char* name) {
    if (root->objectName()==QLatin1String{name}) return root;
    for (auto* child:root->childItems()) if (auto* result=find(child,name)) return result;
    return nullptr;
}
bool key(QQuickWindow& window,QQuickItem* item,Qt::Key code) {
    if (!item || !item->isVisible() || !item->isEnabled()) return false;
    item->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent press{QEvent::KeyPress,code,Qt::NoModifier},release{QEvent::KeyRelease,code,Qt::NoModifier};
    QCoreApplication::sendEvent(&window,&press); QCoreApplication::sendEvent(&window,&release);
    return true;
}
}
void schedule_menu_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,MusicController& music,
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot,bool settingsOnly,bool developmentOnly) {
    struct State {
        int phase=0,ticks=0,loop_ticks=0,frames_before=0,display_ticks=0,layout_ticks=0,reported=-1;
        bool studio_opened=false,studio_checked=false,reset_checked=false;
        QString revision;
        pgp::persistence::Settings windowed_settings;
    };
    auto state=std::make_shared<State>();
    auto* timer=new QTimer(&window); timer->setInterval(60);
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,state,paths,screenshot,expected,settingsOnly,developmentOnly] {
        const auto item=[&](const char* name) { return find(window.contentItem(),name); };
        const auto press=[&](const char* name) { return key(window,item(name),Qt::Key_Space); };
        const auto fail=[&](const char* reason) {
            qCritical() << "Main menu smoke failed:" << reason << "phase" << state->phase << campaign.notice();
            timer->stop(); QCoreApplication::exit(3);
        };
        if (++state->ticks>650) { fail("timed out"); return; }
        if (!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        auto* menu=item("mainMenu");
        auto* video=qobject_cast<MenuVideo*>(item("menuVideo"));
        if (!menu || !video) { fail("menu/video layer unavailable"); return; }
        if (state->reported!=state->phase) { state->reported=state->phase; std::cout << "Menu check phase " << state->phase << std::endl; }
        switch (state->phase) {
        case 0: {
            if(campaign.metaObject()->indexOfMethod("resetLocalData()")<0 || !item("contentStudioButton")) {fail("development menu availability");return;}
            if(!state->studio_checked) {
                auto* tools=qobject_cast<pgp::app::DevelopmentTools*>(qmlContext(menu)->contextProperty("developmentTools").value<QObject*>());
                if(!tools) {fail("Content Studio launcher context");return;}
                const auto checkout=tools->checkout();
                if(!checkout.isEmpty()) {
                    if(!state->studio_opened) {
                        if(!tools->selectCheckout(QUrl::fromLocalFile(checkout))) {fail("Remember Content Studio checkout");return;}
                        const auto settings=QString::fromStdU16String((paths.settings_file.parent_path()/"development-tools.ini").u16string());
                        pgp::app::DevelopmentTools restored(settings);
                        if(restored.checkout()!=checkout || restored.selectCheckout(QUrl::fromLocalFile(settings)) || restored.checkout()!=checkout) {
                            fail("Reloaded checkout preference or invalid folder rejection");return;
                        }
                        if(!press("contentStudioSettingsButton")) {fail("Content Studio repository menu");return;}
                        state->studio_opened=true;
                        return;
                    }
                    if(!item("openContentStudioButton") || !item("openContentStudioButton")->isEnabled() || !item("contentStudioFolderButton")) {
                        fail("Content Studio open and folder actions");return;
                    }
                    if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+"-content-studio.png");
                    if(!key(window,item("openContentStudioButton"),Qt::Key_Escape)) {fail("Close Content Studio menu by keyboard");return;}
                    state->studio_checked=true;
                    return;
                }
                if(developmentOnly) {fail("A checkout is required for the development menu walkthrough");return;}
                state->studio_checked=true;
            }
            if(!state->reset_checked) {
                if(!press("playtestEnvironmentButton")) {fail("Playtest Environment launcher");return;}
                if(!press("developmentResetButton")) {fail("one-click development reset");return;}
                state->reset_checked=true;return;
            }
            if(developmentOnly) {
                campaign.applyPresentationSettings(false,1024,640,false,40,60,200,false,false);
                state->phase=29;
                return;
            }
            const auto* accessible=QAccessible::queryAccessibleInterface(item("menuNewGameButton"));
            if (!menu->isVisible() || !campaign.paused() || (item("statusLine") && item("statusLine")->isVisible()) ||
                window.rendererInterface()->graphicsApi()!=expected || !accessible || accessible->text(QAccessible::Name).isEmpty() ||
                item("menuStillImage")->property("status").toInt()!=1) { fail("clean menu, accessible entry or still image"); return; }
            if (!screenshot.isEmpty()) window.grabWindow().save(screenshot+QStringLiteral(".launch.png"));
            if (!press("menuCreditsButton")) { fail("keyboard credits entry"); return; }
            ++state->phase; break;
        }
        case 1:
            if (menu->property("page").toString()!=QStringLiteral("credits") || music.credits().size()!=7 ||
                !music.licenseText(0).contains(QStringLiteral("Attribution 4.0 International")) || !press("menuBackButton") ||
                !press("menuSettingsButton")) { fail("credits, notices or settings navigation"); return; }
            ++state->phase; break;
        case 2: {
            auto* settings=item("menuSettingsPage");
            if(settingsOnly) {
                auto* apply=item("menuApplySettings");auto* bar=item("menuDetailScrollbar");
                const auto point=apply->mapToScene(QPointF{});
                if(!bar || !bar->isVisible() || bar->width()<14 || bar->height()<100 || bar->x()<menu->width()/2 || point.y()<0 || point.y()+apply->height()>window.height()) {
                    fail("full-screen settings need a visible scrollbar and pinned apply");return;
                }
                if(!screenshot.isEmpty())window.grabWindow().save(screenshot+QStringLiteral(".fullscreen-settings.png"));
            }
            if (!settings || menu->property("page").toString()!=QStringLiteral("settings") ||
                !key(window,item("menuDisplayMode"),Qt::Key_End) || !key(window,item("menuResolution"),Qt::Key_Home) ||
                !key(window,item("menuTextScale"),Qt::Key_End)) { fail("keyboard display, resolution or text choices"); return; }
            settings->setProperty("draftMusic",true);
            settings->setProperty("draftMusicVolume",35);
            settings->setProperty("draftEffectsVolume",45);
            settings->setProperty("draftReducedMotion",true);
            settings->setProperty("draftMotionBackground",true);
            if (!press("menuApplySettings")) { fail("apply settings keyboard action"); return; }
            ++state->phase; break;
        }
        case 3: {
            if (!window.isExposed()) return;
            if(settingsOnly && window.visibility()!=QWindow::Windowed && ++state->display_ticks<50)return;
            const auto saved=pgp::persistence::load_settings(paths.settings_file);
            const bool audio_ok=music.loaded() && (expected==QSGRendererInterface::Software
                ? music.backend()==QStringLiteral("silent") : music.playing());
            if (campaign.fullScreen() || campaign.windowWidth()!=1024 || campaign.windowHeight()!=640 ||
                campaign.textScalePercent()!=200 || !campaign.reducedMotion() || campaign.musicVolume()!=35 ||
                campaign.effectsVolume()!=45 || (!settingsOnly && !audio_ok) || video->active() ||
                window.visibility()!=QWindow::Windowed || saved!=campaign.snapshot()->settings) {
                qCritical()<<"Settings values"<<campaign.fullScreen()<<campaign.windowWidth()<<campaign.windowHeight()<<campaign.textScalePercent()<<campaign.reducedMotion()<<campaign.musicVolume()<<campaign.effectsVolume()<<video->active()<<window.visibility()<<(saved==campaign.snapshot()->settings);
                fail("applied display/audio/accessibility or exact persisted settings"); return;
            }
            auto* apply=item("menuApplySettings");
            const auto position=apply->mapToScene(QPointF{});
            if (position.y()<0 || position.y()+apply->height()>window.height()) {
                if (++state->layout_ticks<30) return;
                fail("focused control scroll at 200 percent"); return;
            }
            if(settingsOnly) {
                if(window.size()!=QSize{650,480}) {window.resize(650,480);return;}
                auto* scroll=item("menuDetailScroll");auto* bar=item("menuDetailScrollbar");
                auto* content=qvariant_cast<QObject*>(scroll->property("contentItem"));
                const auto maximum=content->property("contentHeight").toReal()-scroll->property("availableHeight").toReal();
                if(maximum<=0 || !bar || !bar->isVisible() || bar->property("size").toReal()>=1) {fail("compact settings must scroll");return;}
                const auto before=apply->mapToScene(QPointF{});
                content->setProperty("contentY",0.);
                content->setProperty("contentY",maximum);
                if(apply->mapToScene(QPointF{})!=before || before.y()+apply->height()>window.height()) {fail("apply must remain pinned while scrolling");return;}
                if(!screenshot.isEmpty())window.grabWindow().save(screenshot+QStringLiteral(".compact-settings.png"));
                window.setProperty("menuSmokePassed",true);timer->stop();std::cout<<"Settings scrollbar, pinned apply, 200% compact layout and exact saved preferences passed\n";QCoreApplication::exit(0);return;
            }
            if (!screenshot.isEmpty() && !window.grabWindow().save(screenshot+QStringLiteral(".settings.png"))) { fail("settings screenshot"); return; }
            if (!press("menuBackButton") || !press("menuNewGameButton")) { fail("new game entry"); return; }
            ++state->phase; break;
        }
        case 29:
            if(window.visibility()!=QWindow::Windowed || window.size()!=QSize{650,480}) {window.showNormal();window.resize(650,480);return;}
            if(campaign.textScalePercent()!=200 || !press("contentStudioSettingsButton")) {fail("Open Content Studio repository settings at 200 percent");return;}
            state->phase=30;return;
        case 30: {
            if(window.size()!=QSize{650,480}) {window.resize(650,480);return;}
            const auto within=[&](const char* name) {
                const auto* control=item(name);
                if(!control || !control->isVisible()) return false;
                const auto point=control->mapToScene(QPointF{});
                return point.x()>=0 && point.y()>=0 && point.x()+control->width()<=window.width() && point.y()+control->height()<=window.height();
            };
            if(!within("openContentStudioButton") || !within("contentStudioFolderButton")) {fail("Compact Content Studio actions must fit");return;}
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".compact-studio.png");
            if(!key(window,item("openContentStudioButton"),Qt::Key_Escape) || !press("playtestEnvironmentButton")) {fail("Switch to Playtest Environment at 200 percent");return;}
            state->phase=31;return;
        }
        case 31:
            if(!item("openGodModeButton") || !item("openGodModeButton")->isVisible() ||
                !item("developmentResetButton") || !item("developmentResetButton")->isVisible() ||
                item("developmentResetButton")->mapToScene(QPointF{}).y()+item("developmentResetButton")->height()>window.height()) {
                fail("Compact Playtest Environment action must fit");return;
            }
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".compact-playtest.png");
            window.setProperty("menuSmokePassed",true);timer->stop();
            std::cout<<"Development menus passed at 650x480 with 200% text, including checkout preference reload and playtest reset.\n";
            QCoreApplication::exit(0);return;
        case 4: {
            const bool has_prologue = campaign.founding()->available();
            if (menu->isVisible() || !campaign.welcome() || item("foundingScreen")->isVisible() != has_prologue ||
                (has_prologue && item("workspaceScene")->isVisible())) { fail("New Game entry"); return; }
            campaign.saveTo(QUrl::fromLocalFile(QString::fromStdU16String((paths.saves_directory/"menu-check.pgpsave").u16string())));
            state->revision=campaign.revision(); ++state->phase; break;
        }
        case 5:
            if (item("foundingMainMenuButton") || item("mainMenuButton") ||
                !key(window,window.contentItem(),Qt::Key_Escape)) { fail("Escape opens the menu from setup"); return; }
            ++state->phase; break;
        case 6:
            if (!menu->isVisible() || !campaign.paused() || !press("menuResumeButton")) {
                const auto* resume=item("menuResumeButton");
                qWarning() << "Menu visible" << menu->isVisible() << "paused" << campaign.paused()
                           << "Resume visible" << (resume && resume->isVisible());
                fail("resume entry"); return;
            }
            ++state->phase; break;
        case 7:
            if (menu->isVisible()) { fail("resume route"); return; }
            QMetaObject::invokeMethod(&window,"openMainMenu");
            if (!press("menuLoadGameButton")) { fail("load menu action"); return; }
            ++state->phase; break;
        case 8: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("campaignLoadDialog"));
            if (!dialog || !dialog->property("visible").toBool()) { fail("local save picker"); return; }
            // Exercise picker open/cancel and the actual QML load handler.
            // Selecting a filesystem row still has a human acceptance check.
            QMetaObject::invokeMethod(dialog,"reject");
            const QVariant file=QUrl::fromLocalFile(QString::fromStdU16String((paths.saves_directory/"menu-check.pgpsave").u16string()));
            if (!QMetaObject::invokeMethod(&window,"loadCampaign",Q_ARG(QVariant,file))) { fail("load handler"); return; }
            ++state->phase; break;
        }
        case 9:
            if (menu->isVisible() || campaign.revision()!=state->revision) { fail("successful load route"); return; }
            QMetaObject::invokeMethod(&window,"openMainMenu");
            window.setProperty("pendingMenuLoad",true);
            campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdU16String((paths.saves_directory/"missing.pgpsave").u16string())));
            ++state->phase; break;
        case 10:
            if (!menu->isVisible() || campaign.notice().isEmpty() || campaign.revision()!=state->revision ||
                window.property("pendingMenuLoad").toBool()) { fail("failed load preserves menu and session"); return; }
            campaign.clearNotice(); menu->setProperty("message",QString{});
            campaign.applyPresentationSettings(true,1280,720,true,40,60,100,false,true);
            ++state->phase; break;
        case 11:
            if (!campaign.fullScreen()) { fail("restored full screen preference"); return; }
            if (window.visibility()!=QWindow::FullScreen || !window.isExposed()) {
                if (++state->display_ticks<50) return; // Native Space transitions are asynchronous.
                fail("restored full screen window"); return;
            }
            if (video->supported()) {
                // Explicit offscreen checks exercise the system decoder without
                // taking over the desktop. Normal playback follows app focus.
                if (expected==QSGRendererInterface::Software) {
                    if (!video->active()) video->setActive(true);
                    pump_menu_media_events_for_check();
                }
                if (!video->error().isEmpty()) { fail("video decoder unavailable"); return; }
                if (!video->frameReady()) return;
                if (++state->loop_ticks==1) state->frames_before=video->decodedFrames();
                if (state->loop_ticks<185) return; // Cross the ten-second loop boundary.
                if (video->decodedFrames() < state->frames_before+150 || !video->error().isEmpty()) { fail("native video loop"); return; }
            }
            if (!screenshot.isEmpty() && !window.grabWindow().save(screenshot)) { fail("menu screenshot"); return; }
            if (!key(window,window.contentItem(),Qt::Key_Escape)) { fail("Escape in the full-screen menu"); return; }
            ++state->phase; break;
        case 12: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("exitConfirmDialog"));
            if (!dialog || !dialog->property("visible").toBool() || !press("exitCancelButton")) { fail("Escape confirmation or cancel"); return; }
            ++state->phase; break;
        }
        case 13:
            if (!menu->isVisible() || !key(window,window.contentItem(),Qt::Key_Escape)) { fail("reopen full-screen confirmation"); return; }
            ++state->phase; break;
        case 14:
            state->windowed_settings=campaign.snapshot()->settings;
            state->windowed_settings.full_screen=false;
            if (!press("exitFullScreenButton")) { fail("Exit Full Screen action"); return; }
            state->display_ticks=0; ++state->phase; break;
        case 15: {
            if (window.visibility()!=QWindow::Windowed && ++state->display_ticks<50) return;
            const auto* dialog=window.findChild<QObject*>(QStringLiteral("exitConfirmDialog"));
            if (window.visibility()!=QWindow::Windowed || dialog->property("visible").toBool() ||
                !menu->isVisible() || !campaign.paused() || campaign.snapshot()->settings!=state->windowed_settings ||
                pgp::persistence::load_settings(paths.settings_file)!=state->windowed_settings) {
                fail("Exit Full Screen keeps the menu and saves and applies windowed mode"); return;
            }
            ++state->phase; break;
        }
        case 16: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("exitConfirmDialog"));
            if (dialog->property("visible").toBool() || !menu->isVisible() || !press("menuResumeButton")) { fail("cancel preserves menu/session"); return; }
            if (!key(window,window.contentItem(),Qt::Key_Escape)) { fail("escape in game"); return; }
            ++state->phase; break;
        }
        case 17: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("exitConfirmDialog"));
            if (dialog->property("visible").toBool() || !menu->isVisible() || !campaign.paused()) { fail("in-game Escape must pause into the menu"); return; }
            if (!key(window,window.contentItem(),Qt::Key_Escape)) { fail("Escape from paused menu"); return; }
            ++state->phase; break;
        }
        case 18: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("exitConfirmDialog"));
            if (dialog->property("visible").toBool() || menu->isVisible()) {fail("Escape returns to the paused game");return;}
            if (!key(window,window.contentItem(),Qt::Key_Escape) || !press("menuQuitButton")) {fail("Quit from the paused menu");return;}
            if (!screenshot.isEmpty()) window.grabWindow().save(screenshot+QStringLiteral(".exit.png"));
            if (!key(window,item("exitCancelButton"),Qt::Key_Escape)) { fail("escape cancels confirmation"); return; }
            ++state->phase; break;
        }
        case 19: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("exitConfirmDialog"));
            if (dialog->property("visible").toBool() || !menu->isVisible() || !press("deleteCurrentGameButton")) {
                fail("cancel keeps the current game and its delete action"); return;
            }
            ++state->phase; break;
        }
        case 20: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("deleteCampaignDialog"));
            auto* remove=item("confirmDeleteCurrentGame");
            if (!dialog || !dialog->property("visible").toBool() || !remove ||
                !key(window,remove,Qt::Key_Escape)) { fail("Escape cancels the delete confirmation"); return; }
            ++state->phase; break;
        }
        case 21:
            if (!campaign.currentCampaign() || !press("deleteCurrentGameButton") ||
                !press("confirmDeleteCurrentGame")) { fail("confirm deletion from the menu"); return; }
            ++state->phase; break;
        case 22:
            if (campaign.currentCampaign() || campaign.founding()->hasDraft() ||
                std::filesystem::exists(paths.saves_directory/"continuing/current.pgpsave") ||
                !std::filesystem::exists(paths.saves_directory/"menu-check.pgpsave") ||
                campaign.fullScreen() || !press("menuQuitButton")) {
                fail("delete clears the continuing game and keeps manual saves and settings"); return;
            }
            ++state->phase; break;
        case 23:
            if (auto* button=item("exitFullScreenButton");button && button->isVisible()) { fail("windowed exit confirmation"); return; }
            if (!press("exitConfirmButton") || !window.property("exitConfirmed").toBool()) { fail("confirmed exit"); return; }
            window.setProperty("menuSmokePassed",true);
            std::cout << "Main menu check passed, including the confirmed exit." << std::endl;
            qInfo() << "Main menu smoke passed: keyboard, settings, load/error recovery, Escape/Resume, persisted windowed mode,"
                       " confirmed deletion with manual saves retained, and quit confirmation.";
            if (video->supported()) qInfo() << "Native menu video loop verified.";
            timer->stop(); break;
        }
    });
    timer->start();
}

void schedule_playtest_founder_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,QString screenshot) {
    struct State { int phase=0, ticks=0; QString revision; };
    auto state=std::make_shared<State>();
    auto* timer=new QTimer(&window);
    timer->setInterval(80);
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,state,screenshot] {
        const auto item=[&](const char* name) { return find(window.contentItem(),name); };
        const auto press=[&](const char* name) { return key(window,item(name),Qt::Key_Space); };
        const auto fail=[&](const char* reason) {
            qCritical()<<"God mode walkthrough"<<state->phase<<reason<<campaign.notice();
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".failed.png");
            timer->stop();
            QCoreApplication::exit(3);
        };
        const auto pass=[&](const char* message) {
            std::cout<<message<<'\n';
            window.setProperty("menuSmokePassed",true);
            timer->stop();
            QCoreApplication::exit(0);
        };
        if(++state->ticks>400) {fail("timed out");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        const auto& world=campaign.snapshot()->checkpoint.world;
        if(!world.developer_playtest || campaign.storageBlocked()) {fail("isolated playtest save");return;}
        const auto founder=campaign.playtestFounder();
        const QStringList expected{"Interest: Skateboarding","Social: Outgoing"};
        const auto regular_save=std::filesystem::path{campaign.saveFolder().toLocalFile().toStdU16String()}/"ordinary-mode-fixture.pgpsave";
        if(state->phase==0) {
            if(campaign.currentCampaign()) {
                if(founder.value("traits").toStringList()!=expected || !founder.value("genius").toBool()) {
                    fail("saved traits did not resume");return;
                }
                pass("God mode founder traits and genius resumed from the dedicated save.");
                return;
            }
            auto regular=campaign.snapshot()->checkpoint;
            regular.world.developer_playtest=false;
            try {
                pgp::persistence::save_file(*campaign.snapshot()->content,regular,regular_save);
            } catch(const std::exception&) {
                fail("could not write the ordinary save fixture");return;
            }
            campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdU16String(regular_save.u16string())));
            state->phase=10;
            return;
        }
        if(state->phase==10) {
            if(campaign.currentCampaign() || !campaign.notice().contains("regular game")) {
                fail("God mode accepted an ordinary campaign");return;
            }
            std::filesystem::remove(regular_save);
            auto* founding=campaign.founding();
            founding->reset();
            founding->setField("family","rich");
            founding->setAdvantage("political",true);
            founding->setField("route","hired_ceo");
            pgp::bridge::complete_formative_fixture(*founding);
            campaign.startFounderCampaign();
            state->phase=1;
            return;
        }
        if(campaign.requiredKind()=="content") {
            campaign.choose(campaign.requiredChoices().back().toMap().value("choiceId").toString());
            return;
        }
        switch(state->phase) {
        case 1:
            if(!campaign.currentCampaign() || founder.isEmpty() || founder.value("genius").toBool()) {
                fail("non-genius founder campaign did not start");return;
            }
            campaign.applyPresentationSettings(false,1024,640,false,40,60,200,true,false);
            ++state->phase;
            return;
        case 2:
            if(window.size()!=QSize{650,480}) {window.showNormal();window.resize(650,480);return;}
            QMetaObject::invokeMethod(&window,"openMainMenu");
            if(!press("playtestEnvironmentButton") || !press("playtestFounderButton")) {fail("founder editor launcher");return;}
            state->revision=campaign.revision();
            ++state->phase;
            return;
        case 3: {
            auto* entry=item("playtestFounderTraits");
            auto* apply=item("savePlaytestFounder");
            if(!entry || !apply || !window.title().contains("God mode") ||
               apply->mapToScene(QPointF{}).y()+apply->height()>window.height()) {fail("compact editor layout");return;}
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".compact.png");
            entry->setProperty("text","Duplicate\nDuplicate");
            if(!press("savePlaytestFounder")) {fail("apply edited traits");return;}
            ++state->phase;
            return;
        }
        case 4: {
            auto* apply=item("savePlaytestFounder");
            auto* entry=item("playtestFounderTraits");
            if(!apply || !entry || !entry->isVisible() || campaign.revision()!=state->revision ||
               !campaign.notice().contains("Use each trait once.") ||
               apply->mapToScene(QPointF{}).y()+apply->height()>window.height()) {
                fail("duplicate traits must be rejected without changing the campaign");return;
            }
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".validation.png");
            entry->setProperty("text",expected.join("\n"));
            item("playtestFounderGenius")->setProperty("checked",true);
            if(!press("savePlaytestFounder")) {fail("apply corrected traits");return;}
            ++state->phase;
            return;
        }
        default:
            if(campaign.revision()==state->revision || founder.value("traits").toStringList()!=expected ||
               !founder.value("genius").toBool() || campaign.storageBlocked()) {fail("playtest edits did not commit");return;}
            pass("God mode founder traits passed compact editing, rejection and durable commit.");
        }
    });
    timer->start();
}
