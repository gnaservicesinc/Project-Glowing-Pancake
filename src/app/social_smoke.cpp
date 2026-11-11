#include "social_smoke.hpp"
#include "campaign_controller.hpp"
#include "founding_fixture.hpp"
#include <pgp/persistence/save.hpp>
#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <iostream>
namespace {QQuickItem* find(QQuickItem* root,const QString& id){if(!root->isVisible())return nullptr;if(root->objectName()==id)return root;for(auto* child:root->childItems())if(auto* value=find(child,id))return value;return nullptr;}}
void schedule_social_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths,QString screenshot) {
    struct Run {int phase=0,ticks=0,frames=0;std::string person,industry;pgp::bridge::CampaignSnapshotPtr preview;};auto run=std::make_shared<Run>();auto* timer=new QTimer(&window);timer->setInterval(100);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,screenshot]{
        const auto fail=[&](const char* why){std::cerr<<"Social UI "<<run->phase<<": "<<why<<" · "<<campaign.notice().toStdString()<<'\n';if(!screenshot.isEmpty())window.grabWindow().save(screenshot+".failure.png");timer->stop();QCoreApplication::exit(3);};
        if(++run->ticks>500){fail("Timeout");return;}if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())return;
        const auto item=[&](const QString& name){return find(window.contentItem(),name);};
        const auto press=[&](const QString& name){auto* c=item(name);if(!c || !c->isEnabled())return false;c->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,Qt::Key_Space);return true;};
        const auto reveal=[&](QQuickItem* target){if(!target)return;for(auto* p=target->parentItem();p;p=p->parentItem())if(p->property("contentY").isValid()){const auto y=target->mapToItem(p,QPointF(0,0)).y()+p->property("contentY").toDouble();p->setProperty("contentY",std::clamp(y,0.0,std::max(0.0,p->property("contentHeight").toDouble()-p->height())));break;}};
        const auto capture=[&](const char* suffix){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+suffix+".png");};
        auto* world=campaign.world();const auto& s=campaign.snapshot()->checkpoint.world.simulation;
        switch(run->phase) {
        case 0:{if(!press("menuNewGameButton")){fail("New game");return;}auto* f=campaign.founding();f->setField("family","rich");f->setField("route","hired_ceo");pgp::bridge::complete_formative_fixture(*f);campaign.startFounderCampaign();break;}
        case 1:if(auto* c=item("birthKeepsakeClose")){c->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Space);}world->navigate("workspace","hq","laptop","people","");break;
        case 2:if(!press("peopleSocialTab")){fail("Social circle tab");return;}break;
        case 3:{const auto people=world->relationships().value("people").toList();if(people.size()<2){fail("HQ people");return;}run->person=people.front().toMap().value("id").toString().toStdString();if(!press("socialChat")){fail("Take a break");return;}run->frames=0;break;}
        case 4:{const auto* bond=pgp::core::social_link(s,"founder",run->person);if(!bond || bond->affinity<=0 || item("socialChat")->isEnabled()){fail("Actual relationship and daily limit");return;}reveal(item("socialCard"));if(run->frames++<4)return;if(!capture("conversation")){fail("Conversation capture");return;}
            // Mature ties are seeded solely for graph rendering, not asserted as
            // results of this single encounter. Runtime tests cover progression.
            auto fixture=campaign.snapshot()->checkpoint;auto& social=*fixture.world.simulation.management->people->social;
            social.links.clear();std::vector<std::string> ids;for(const auto& e:s.management->people->employees)if(e.terms.profile.role!="Founder / researcher")ids.push_back(e.terms.profile.id);
            for(const auto& id:ids)social.links.push_back({id,"founder",900,60,s.date,s.date,{},"Mature friendship preview fixture",true});
            social.links.push_back({ids[0],ids[1],700,50,s.date,s.date,{},"Shared close friendship preview fixture",true});
            pgp::runtime::validate_checkpoint(*campaign.snapshot()->content,fixture);
            auto preview=std::make_shared<pgp::bridge::CampaignSnapshot>(*campaign.snapshot());preview->checkpoint=std::move(fixture);
            run->preview=preview;world->update(preview,*preview->content,false);break;}
        case 5:world->navigate("workspace","hq","laptop","people","");break;
        case 6:if(item("peopleSocialTab"))press("peopleSocialTab");if(!press("socialNetwork")){fail("Open graph");return;}run->frames=0;break;
        case 7:reveal(item("socialGraph"));if(run->frames++<5)return;if(!item("socialGraph") || world->relationships().value("edges").toList().size()<3 || !capture("graph")){fail("Known close graph");return;}if(!press("socialConnection_0")){fail("Explore a friend’s connections");return;}run->frames=0;break;
        case 8:if(run->frames++<3)return;if(!press("socialGraphHome")){fail("Return to own connections");return;}campaign.setTextScalePercent(200);window.showNormal();window.resize(650,480);run->frames=0;break;
        case 9:if(run->frames==0)world->update(run->preview,*run->preview->content,false);if(run->frames++<5)return;if(!capture("list-compact") || !press("socialPeople")){fail("Compact accessible list");return;}run->frames=0;break;
        case 10:reveal(item("socialCard"));if(run->frames++<5)return;if(!capture("person-compact")){fail("Compact person");return;}campaign.setTextScalePercent(100);window.resize(1280,800);break;
        case 11:if(!press("peopleBackToTeam")){fail("Return to team");return;}break;
        case 12:if(!press("peopleIndustryTab")){fail("Industry people tab");return;}run->frames=0;break;
        case 13:reveal(item("industryCompany"));if(run->frames++<5)return;if(world->industryPeople().size()<2 || !item("industryLocation") || !capture("industry")){fail("Major HQ roster");return;}campaign.setTextScalePercent(200);window.resize(650,480);run->frames=0;break;
        case 14:reveal(item("industryCompany"));if(run->frames++<5)return;if(!capture("industry-compact")){fail("Compact industry roster");return;}campaign.step();break;
        case 15:{const auto firms=world->industryPeople();run->industry=firms.front().toMap().value("people").toList().front().toMap().value("id").toString().toStdString();if(!press("industryHello_"+QString::fromStdString(run->industry))){fail("Say hello to a regional colleague");return;}break;}
        case 16:if(!pgp::core::social_link(s,"founder",run->industry)){fail("Actual regional acquaintance");return;}if(!press("peopleBackToTeam")){fail("Return from industry roster");return;}break;
        case 17:if(!press("peopleSocialTab")){fail("Reopen social circle");return;}break;
        case 18:{auto* selector=item("socialPerson");if(!selector){fail("Select industry contact");return;}selector->forceActiveFocus();QTest::keyClick(&window,Qt::Key_End);run->frames=0;break;}
        case 19:reveal(item("socialCard"));if(run->frames++<4)return;if(!capture("regional-contact")){fail("Regional contact card");return;}std::cout<<"Social UI passed: actual meetup, cooldown and regional introduction; seeded mature graph; compact industry and social cards.\n";timer->stop();QCoreApplication::exit(0);return;

        }++run->phase;
    });timer->start();
}
