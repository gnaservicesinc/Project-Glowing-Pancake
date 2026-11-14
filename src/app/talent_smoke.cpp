#include "founding_fixture.hpp"
#include "talent_smoke.hpp"
#include "campaign_controller.hpp"
#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <iostream>
#include <memory>
namespace {
QQuickItem* visible(QQuickItem* root,const QString& name){if(!root->isVisible())return nullptr;if(root->objectName()==name)return root;for(auto* child:root->childItems())if(auto* found=visible(child,name))return found;return nullptr;}
}
void schedule_talent_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,QString screenshot){
    struct Run{int phase=0,ticks=0,days=0,frames=0;};auto run=std::make_shared<Run>();auto* timer=new QTimer(&window);timer->setInterval(100);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,screenshot]{
        const auto fail=[&](const char* why){std::cerr<<"Talent UI phase "<<run->phase<<": "<<why<<" · "<<campaign.notice().toStdString()<<'\n';timer->stop();QCoreApplication::exit(3);};
        const auto item=[&](const QString& name){return visible(window.contentItem(),name);};
        const auto press=[&](const QString& name){auto* c=item(name);if(!c || !c->isEnabled())return false;c->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,Qt::Key_Space);return true;};
        const auto reveal=[&](QQuickItem* target){if(!target)return;for(auto* p=target->parentItem();p;p=p->parentItem())if(p->property("contentY").isValid()) {const auto y=target->mapToItem(p,QPointF(0,0)).y()+p->property("contentY").toDouble();p->setProperty("contentY",std::clamp(y,0.0,std::max(0.0,p->property("contentHeight").toDouble()-p->height())));break;}};
        const auto capture=[&](const char* suffix){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+suffix+".png");};
        if(++run->ticks>600){fail("Timeout");return;}if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())return;
        auto* world=campaign.world();const auto& s=campaign.snapshot()->checkpoint.world.simulation;
        switch(run->phase){
        case 0:{if(!press("menuNewGameButton")){fail("New Game");return;}auto* f=campaign.founding();f->setField("family","rich");f->setAdvantage("genius",true);f->setField("route","bootstrap");f->setField("investment","100000");pgp::bridge::complete_formative_fixture(*f);campaign.startFounderCampaign();break;}
        case 1:world->buyHQEquipment("desk",world->reviewToken());break;
        case 2:world->buyHQEquipment("desktop",world->reviewToken());break;
        case 3:if(run->days++<3){campaign.step();return;}world->placeHQEquipment(1,0,0,false,0,world->reviewToken());break;
        case 4:world->placeHQEquipment(2,0,0,false,1,world->reviewToken());break;
        case 5:world->assignHQDesk(1,"founder",world->reviewToken());break;
        case 6:if(auto* close=item("birthKeepsakeClose")){close->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Space);}world->navigate("workspace","hq","laptop","office","software");break;
        case 7:if(!item("softwarePrototype")){fail("Software tab");return;}break;
        case 8:if(!press("softwareWorker_founder")){fail("Assign founder");return;}break;
        case 9:reveal(item("softwarePrototype"));if(!capture("studio") || !press("softwareBuild")){fail("Live preview and review");return;}break;
        case 10:if(!press("softwareConfirm")){fail("Confirm development");return;}break;
        case 11:if(!s.management->people->talent || s.management->people->talent->software.size()!=1){fail("Actual project");return;}world->navigate("workspace","hq","laptop","people","");break;
        case 12:if(!press("peopleUniversityTab")){fail("University tab");return;}run->frames=0;break;
        case 13:{reveal(item("universityPersonalMoney"));if(run->frames++<4)return;if(!capture("university-wallet")){fail("Money illustration capture");return;}auto* c=item("universityCourse");if(!c){fail("Course selector");return;}c->setProperty("currentIndex",1);break;}
        case 14:if(!press("universityCompany")){fail("Sponsor course");return;}break;
        case 15:if(!press("universityConfirm")){fail("Confirm tuition");return;}break;
        case 16:if(s.management->people->talent->courses.size()!=1){fail("Actual course");return;}window.showNormal();window.resize(650,480);break;
        case 17:if(run->frames++<6)return;if(!capture("university-compact")){fail("Capture compact university");return;}world->navigate("workspace","hq","laptop","office","software");break;
        case 18:if(!item("softwarePrototype") || !item("softwareWorker_founder")->property("checked").toBool()){fail("Reopen studio with saved team draft");return;}reveal(item("softwarePrototype"));run->frames=0;break;
        case 19:if(run->frames++<6)return;if(!capture("studio-compact")){fail("Capture compact studio");return;}std::cout<<"Talent UI passed: rendered studio, qualified team, actual development, sponsored tuition and compact layouts.\n";timer->stop();QCoreApplication::exit(0);return;
        }++run->phase;
    });timer->start();
}
