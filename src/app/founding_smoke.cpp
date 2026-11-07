#include "founding_smoke.hpp"
#include "campaign_controller.hpp"
#include <QCoreApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <iostream>
#include <memory>

namespace {
QQuickItem* find(QQuickItem* root,const QString& name) {
    if(root->objectName()==name) return root;
    for(auto* child:root->childItems()) if(auto* r=find(child,name)) return r;
    return nullptr;
}
bool press(QQuickWindow& window,const QString& name) {
    auto* item=find(window.contentItem(),name);
    if(!item || !item->isVisible() || !item->isEnabled()) return false;
    item->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent down{QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier},up{QEvent::KeyRelease,Qt::Key_Space,Qt::NoModifier};
    QCoreApplication::sendEvent(&window,&down);QCoreApplication::sendEvent(&window,&up);return true;
}
}
void schedule_founding_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot) {
    struct State {int ticks=0,phase=0;QString revision;};
    auto state=std::make_shared<State>();auto* timer=new QTimer(&window);timer->setInterval(75);
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,state,paths,screenshot] {
        if(++state->ticks>800) {std::cerr<<"Founding UI timed out, phase "<<state->phase<<": "<<campaign.notice().toStdString()<<'\n';QCoreApplication::exit(3);timer->stop();return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        if(auto* screen=find(window.contentItem(),"foundingScreen");screen && screen->isVisible() && screen->property("settling").toBool()) return;
        const auto fail=[&](const char* why) {std::cerr<<"Founding UI: "<<why<<" (phase "<<state->phase<<") "<<campaign.notice().toStdString()<<'\n';timer->stop();QCoreApplication::exit(3);};
        const auto click=[&](const QString& name) {return press(window,name);};
        auto* opening=campaign.founding();const auto entry=opening->chapters().size()+3;
        switch(state->phase) {
        case 0:
            opening->reset();
            if(!click("menuNewGameButton")) {fail("New Game entry unavailable");return;}
            ++state->phase;break;
        case 1: {
            auto* screen=find(window.contentItem(),"foundingScreen");auto* old=find(window.contentItem(),"statusLine");
            if(!screen || !screen->isVisible() || (old && old->isVisible()) || !click("foundingNextButton")) {fail("New Game did not open the introduction");return;}
            ++state->phase;break;
        }
        case 2:
            if(opening->page()!=1 || !find(window.contentItem(),"foundingFirstName") || !click("foundingNextButton")) {fail("birth identity screen");return;}
            ++state->phase;break;
        case 3: {
            if(!click("foundingChoice_poor") || !click("foundingChoice_charisma")) {fail("family and leg up");return;}
            auto* wealthy=find(window.contentItem(),"foundingChoice_wealthy");
            if(!wealthy || wealthy->isEnabled() || !click("foundingNextButton")) {fail("wealth eligibility");return;}
            ++state->phase;break;
        }
        case 4:
            if(opening->page()<entry) {
                if(opening->page()==4 && !screenshot.isEmpty()) window.grabWindow().save(screenshot+".childhood.png");
                const auto chapter=opening->chapters()[opening->page()-3].toMap();
                const auto choice=chapter.value("choices").toList().front().toMap().value("id").toString();
                if(!click("foundingChoice_"+choice) || !click("foundingNextButton")) {fail("childhood choice/continuation");return;}
            } else ++state->phase;
            break;
        case 5:
            if(!click("foundingChoice_nonprofit") || !click("foundingChoice_mission_director") || !click("foundingNextButton")) {fail("founding route/legal form");return;}
            ++state->phase;break;
        case 6: {
            auto* profit=find(window.contentItem(),"foundingChoice_profit");
            if(!profit || profit->isEnabled() || !click("foundingNextButton")) {fail("nonprofit mission compatibility");return;}
            ++state->phase;break;
        }
        case 7:
            if(!click("foundingChoice_safety") || !click("foundingNextButton")) {fail("donor selection");return;}
            ++state->phase;break;
        case 8:
            if(!click("foundingChoice_home") || !click("foundingChoice_first_hire") || !click("foundingNextButton")) {fail("first office/researcher");return;}
            ++state->phase;break;
        case 9:
            if(!screenshot.isEmpty() && !window.grabWindow().save(screenshot+".terms.png")) {fail("terms capture");return;}
            if(!click("foundingStartButton")) {fail("enter office action");return;}
            ++state->phase;break;
        case 10: {
            const auto& w=campaign.snapshot()->checkpoint.world;
            if(campaign.welcome() || !w.founding || w.founding->choices.size()!=8 || w.founding->legal_form!="nonprofit" ||
                w.identity->founder_name!="Alex Vale" || pgp::core::iso_date(w.simulation.date)!="2015-01-01" ||
                pgp::core::iso_date(w.simulation.end_date)!="2115-01-01" || !campaign.paused()) {fail("real campaign handoff");return;}
            if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+".office.png");
            state->revision=campaign.revision();
            campaign.saveTo(QUrl::fromLocalFile(QString::fromStdU16String((paths.saves_directory/"founder-ui.pgpsave").u16string())));
            ++state->phase;break;
        }
        case 11:campaign.step();++state->phase;break;
        case 12:
            if(campaign.date()!=QStringLiteral("2015-01-02")) {fail("actual first day");return;}
            campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdU16String((paths.saves_directory/"founder-ui.pgpsave").u16string())));
            ++state->phase;break;
        case 13:
            if(campaign.revision()!=state->revision || !campaign.snapshot()->checkpoint.world.founding || campaign.welcome()) {fail("exact saved founder continuation");return;}
            std::cout<<"Founding UI passed: New Game, biography, eight chapters, blocked choices, nonprofit/donor terms, office, first hire, first day and exact reload\n";
            timer->stop();QCoreApplication::exit(0);break;
        }
    });
    timer->start();
}
