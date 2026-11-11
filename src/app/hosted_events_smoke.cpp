#include "founding_fixture.hpp"
#include "hosted_events_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <iostream>

namespace {
QQuickItem* find(QQuickItem* root,const QString& name) {
    if(!root->isVisible())return nullptr;
    if(root->objectName()==name)return root;
    for(auto* child:root->childItems())if(auto* found=find(child,name))return found;
    return nullptr;
}
}
void schedule_hosted_events_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot) {
    struct Run {int phase=0,ticks=0,frames=0;QString employee;pgp::core::Cents cash{0},personal{0};};
    const auto run=std::make_shared<Run>();
    auto* timer=new QTimer(&window);timer->setInterval(100);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,paths,screenshot] {
        const auto capture=[&](const QString& name){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+name+".png");};
        const auto fail=[&](const char* reason){std::cerr<<"Gatherings phase "<<run->phase<<": "<<reason<<" · "<<campaign.notice().toStdString()<<'\n';(void)capture("failure");timer->stop();QCoreApplication::exit(3);};
        if(++run->ticks>500){fail("Timed out");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())return;
        const auto item=[&](const QString& name){return find(window.contentItem(),name);};
        const auto press=[&](const QString& name){auto* control=item(name);if(!control || !control->isEnabled())return false;control->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,Qt::Key_Space);return true;};
        if(auto* panel=item("personalLifePanel");panel && panel->property("transitioning").toBool())return;
        const auto& state=campaign.snapshot()->checkpoint.world.simulation;
        const auto owned=[&]() -> const auto& {return *state.succession->personal_life;};
        switch(run->phase) {
        case 0: {
            if(!press("menuNewGameButton")){fail("New Game");return;}
            auto* story=campaign.founding();story->setField("family","rich");story->setField("leg","political");
            pgp::bridge::complete_formative_fixture(*story);
            story->setBeat("home");
            if(!story->contactHomeOffer("alder").isEmpty() || !story->respondHomeOffer("alder","accept").isEmpty()){fail("Accept an actual appointment");return;}
            campaign.startFounderCampaign();break;
        }
        case 1:
            run->employee=QString::fromStdString(state.management->people->employees.front().terms.profile.id);
            if(!press("hqTalkButton")){fail("Meet a colleague at HQ");return;}break;
        case 2:
            if(!press("contactPerson_"+run->employee) || !press("exchangeContactButton")){fail("Exchange contact details");return;}break;
        case 3:
            if(!press("hqCloseTalk")){fail("Close the conversation");return;}break;
        case 4:
            if(item("hqCloseTalk"))return;
            if(!press("personalLifeTab")){fail("Open personal life");return;}break;
        case 5:
            if(!press("personalPage_events")){fail("Open the gatherings calendar");return;}break;
        case 6:
            if(!press("gatheringGuest_"+run->employee) || !press("gatheringGuest_opening/alder")){fail("Choose employee and opening contacts");return;}
            run->cash=state.cash;run->personal=state.succession->personal_cash;break;
        case 7:
            if(!press("gatheringBook")){fail("Review the quoted gathering");return;}run->frames=0;break;
        case 8:
            if(++run->frames<3)return;
            if(!capture("booking") || !press("gatheringConfirm")){fail("Confirm personal payment and invitations");return;}break;
        case 9:
            if(item("gatheringConfirm"))return;
            if(owned().events.size()!=1 || owned().events.front().guests.size()!=2 || state.cash!=run->cash ||
                state.succession->personal_cash!=pgp::core::Cents{run->personal.value()-38000} || !press("gatheringCancel_1")) {
                fail("The invitation must use personal savings and open cancellation review");return;
            }break;
        case 10:
            if(!press("gatheringConfirm")){fail("Cancel and refund");return;}break;
        case 11:
            if(item("gatheringConfirm"))return;
            if(owned().events.front().outcome!="cancelled" || state.succession->personal_cash!=pgp::core::Cents{run->personal.value()-9500} ||
                !press("gatheringBook")){fail("The refund must permit a new booking");return;}break;
        case 12:
            if(!press("gatheringConfirm")){fail("Book the replacement gathering");return;}break;
        case 13:
            if(item("gatheringConfirm"))return;
            if(!press("gatheringAdvance")){fail("Advance to the event");return;}break;
        case 14: {
            if(owned().events.back().outcome=="scheduled")return;
            const auto& event=owned().events.back();
            pgp::persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
            if(event.outcome!="hosted" || !event.guests[0].attended || !event.guests[1].attended ||
                pgp::core::workplace_moods(state).at(run->employee.toStdString()).gatherings!=4 ||
                current.resume(*campaign.snapshot()->content)!=std::optional{campaign.snapshot()->checkpoint}) {
                fail("The completed gathering must record attendance, mood and an exact reload");return;
            }
            campaign.setTextScalePercent(200);break;
        }
        case 15:
            if(window.visibility()!=QWindow::Windowed || window.size()!=QSize{650,480}){window.showNormal();window.resize(650,480);return;}
            if(campaign.textScalePercent()!=200){fail("The compact check needs actual 200% text");return;}
            if(auto* record=item("gatheringRecord_2"))record->forceActiveFocus(Qt::TabFocusReason);
            else {fail("The completed event must be visible in the calendar");return;}
            run->frames=0;break;
        case 16:
            if(++run->frames<4)return;
            if(!capture("hosted-compact") || !press("gatheringRead_2")){fail("Read the evening from the compact calendar");return;}
            run->frames=0;break;
        case 17:
            if(++run->frames<4)return;
            if(item("gatheringConfirm") || !capture("evening-compact")){fail("The completed evening needs a readable report");return;}
            std::cout<<"Known guest selection, priced booking, cancellation, replacement, advance to gathering, attendance, mood, compact 200% text and fresh reload passed\n";
            timer->stop();QCoreApplication::exit(0);return;
        }
        ++run->phase;
    });
    timer->start();
}
