#include "menu_smoke.hpp"
#include "campaign_controller.hpp"
#include "menu_media.hpp"
#include "music_controller.hpp"
#include <pgp/persistence/settings.hpp>
#include <QAccessible>
#include <QCoreApplication>
#include <QFile>
#include <QKeyEvent>
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
    QSGRendererInterface::GraphicsApi expected,pgp::persistence::LocalPaths paths,QString screenshot,bool settingsOnly) {
    struct State { int phase=0,ticks=0,loop_ticks=0,frames_before=0,display_ticks=0,layout_ticks=0,reported=-1; QString revision; };
    auto state=std::make_shared<State>();
    auto* timer=new QTimer(&window); timer->setInterval(60);
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,state,paths,screenshot,expected,settingsOnly] {
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
        case 4: {
            const bool has_prologue = campaign.founding()->available();
            if (menu->isVisible() || !campaign.welcome() || item("foundingScreen")->isVisible() != has_prologue ||
                item("statusLine")->isVisible() == has_prologue) { fail("New Game entry"); return; }
            campaign.saveTo(QUrl::fromLocalFile(QString::fromStdU16String((paths.saves_directory/"menu-check.pgpsave").u16string())));
            state->revision=campaign.revision(); ++state->phase; break;
        }
        case 5:
            if (!press(campaign.founding()->available() ? "foundingMainMenuButton" : "mainMenuButton")) { fail("main menu from setup"); return; }
            ++state->phase; break;
        case 6:
            if (!menu->isVisible() || !campaign.paused() || !press("menuResumeButton")) { fail("resume entry"); return; }
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
            if (!key(window,window.contentItem(),Qt::Key_Escape)) { fail("escape on main menu"); return; }
            ++state->phase; break;
        case 12: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("exitConfirmDialog"));
            if (!dialog || !dialog->property("visible").toBool() || !press("exitCancelButton")) { fail("escape confirmation or cancel"); return; }
            ++state->phase; break;
        }
        case 13: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("exitConfirmDialog"));
            if (dialog->property("visible").toBool() || !menu->isVisible() || !press("menuResumeButton")) { fail("cancel preserves menu/session"); return; }
            if (!key(window,window.contentItem(),Qt::Key_Escape)) { fail("escape in game"); return; }
            ++state->phase; break;
        }
        case 14: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("exitConfirmDialog"));
            if (!dialog->property("visible").toBool() || menu->isVisible() || !campaign.paused()) { fail("in-game exit confirmation"); return; }
            if (!screenshot.isEmpty()) window.grabWindow().save(screenshot+QStringLiteral(".exit.png"));
            if (!key(window,item("exitCancelButton"),Qt::Key_Escape)) { fail("escape cancels confirmation"); return; }
            ++state->phase; break;
        }
        case 15: {
            auto* dialog=window.findChild<QObject*>(QStringLiteral("exitConfirmDialog"));
            if (dialog->property("visible").toBool() || menu->isVisible() || !press("mainMenuButton") || !press("menuQuitButton")) {
                fail("cancel keeps game and quit uses confirmation"); return;
            }
            ++state->phase; break;
        }
        case 16:
            if (!press("exitConfirmButton") || !window.property("exitConfirmed").toBool()) { fail("confirmed exit"); return; }
            window.setProperty("menuSmokePassed",true);
            std::cout << "Main menu check passed, including the confirmed exit." << std::endl;
            qInfo() << "Main menu smoke passed: launch, keyboard, credits, 200% settings, persistence, music, video loop,"
                       " setup, resume, load/error recovery and Escape/quit confirmation with cancel and actual exit.";
            timer->stop(); break;
        }
    });
    timer->start();
}
