#include "founding_fixture.hpp"
#include "personal_life_smoke.hpp"
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
    for(auto* child:root->childItems())if(auto* match=find(child,name))return match;
    return nullptr;
}
}
void schedule_personal_life_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot) {
    struct Run {int phase=0,ticks=0,frames=0;pgp::core::Cents company{0},personal{0};};
    const auto run=std::make_shared<Run>();
    auto* timer=new QTimer(&window);timer->setInterval(100);
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,run,paths,screenshot] {
        const auto capture=[&](const QString& suffix){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+suffix+".png");};
        const auto fail=[&](const char* why){
            std::cerr<<"Personal life phase "<<run->phase<<": "<<why<<" · "<<campaign.notice().toStdString()<<'\n';
            (void)capture("failure");timer->stop();QCoreApplication::exit(3);
        };
        if(++run->ticks>450){fail("Timed out");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())return;
        const auto item=[&](const QString& name){return find(window.contentItem(),name);};
        if(auto* panel=item("personalLifePanel");panel && panel->property("transitioning").toBool())return;
        const auto press=[&](const QString& name,Qt::Key key=Qt::Key_Space){
            auto* control=item(name);if(!control || !control->isEnabled())return false;
            control->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,key);return true;
        };
        const auto& state=campaign.snapshot()->checkpoint.world.simulation;
        auto* world=campaign.world();
        switch(run->phase) {
        case 0: {
            if(!press("menuNewGameButton")){fail("New Game");return;}
            auto* story=campaign.founding();story->setField("family","rich");story->setAdvantage("genius",true);
            story->setField("route","bootstrap");story->setField("investment","100000");
            pgp::bridge::complete_formative_fixture(*story);
            campaign.startFounderCampaign();break;
        }
        case 1:
            if(world->personalLife().isEmpty() || !press("personalLifeTab")){fail("Open My life");return;}
            run->company=state.cash;run->personal=state.succession->personal_cash;break;
        case 2:
            if(!press("personalBuyHome_valley_condo")){fail("Review a personal property purchase");return;}
            run->frames=0;break;
        case 3:
            if(++run->frames<3)return;
            if(!capture("property-review") || !press("personalConfirm")){fail("Confirm the personal payment");return;}break;
        case 4:
            if(item("personalConfirm"))return;
            if(state.cash!=run->company || state.succession->personal_cash!=pgp::core::Cents{run->personal.value()-25080000} || !press("personalPage_interiors")) {
                fail("The home must spend personal savings and leave company cash intact");return;
            }break;
        case 5:
            if(!press("personalBuyDecor_sofa")){fail("Review an interior purchase");return;}break;
        case 6:
            if(!press("personalConfirm")){fail("Buy the sofa");return;}break;
        case 7:
            if(item("personalConfirm"))return;
            if(!press("personalBuyDecor_painting")){fail("Review gallery art");return;}
            run->frames=0;break;
        case 8:
            if(++run->frames<3)return;
            if(!capture("gallery-review") || !press("personalConfirm")){fail("Buy the original painting");return;}break;
        case 9:
            if(item("personalConfirm"))return;
            campaign.step();break;
        case 10:
            campaign.step();break;
        case 11:
            if(!press("personalPage_rooms")){fail("Open the owned home's floor plan");return;}break;
        case 12:
            if(!press("personalPick_1") || !press("personalRotate")){fail("Pick up and rotate the sofa");return;}
            run->frames=0;break;
        case 13: {
            if(run->frames==0)if(auto* plan=item("personalFloorPlan"))plan->forceActiveFocus(Qt::TabFocusReason);
            if(++run->frames<3)return;
            auto* plan=item("personalFloorPlan");
            if(!plan || !plan->property("preview").toMap().value("valid").toBool() || !capture("valid-placement")) {
                fail("A valid rotated preview must place at the selected coordinates");return;
            }
            const auto cell=plan->property("cell").toReal();
            const auto point=plan->mapToScene({plan->property("leftEdge").toReal()+cell*0.5,plan->property("topEdge").toReal()+cell*0.5}).toPoint();
            QTest::mouseMove(&window,point);
            QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,point);
            break;
        }
        case 14:
            if(!press("personalPick_2")){fail("Pick up the painting");return;}
            run->frames=0;break;
        case 15: {
            if(++run->frames<3)return;
            auto* plan=item("personalFloorPlan");
            if(!plan || plan->property("preview").toMap().value("valid").toBool() || !capture("blocked-placement")) {
                fail("Overlapping art must show a blocked preview");return;
            }
            if(!press("personalX",Qt::Key_Up) || !press("personalX",Qt::Key_Up) || !press("personalPlace")) {
                fail("Choose free coordinates and place the painting");return;
            }
            break;
        }
        case 16:
            campaign.setTextScalePercent(200);break;
        case 17:
            if(campaign.textScalePercent()!=200){fail("The compact check requires actual 200% text");return;}
            if(window.visibility()!=QWindow::Windowed || window.size()!=QSize{650,480}) {window.showNormal();window.resize(650,480);return;}
            if(auto* plan=item("personalFloorPlan"))plan->forceActiveFocus(Qt::TabFocusReason);
            run->frames=0;break;
        case 18: {
            if(++run->frames<4)return;
            const auto& owned=*state.succession->personal_life;
            auto* plan=item("personalFloorPlan");
            auto* viewport=item("managementScroll");
            if(!plan || !viewport || plan->mapToItem(viewport,{0,0}).y()<-1 ||
                plan->mapToItem(viewport,{0,plan->height()}).y()>viewport->height()+1) {
                fail("The entire personal floor plan must fit in the compact viewport");return;
            }
            pgp::persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
            if(owned.decor.size()!=2 || owned.decor[0].property!=1 || !owned.decor[0].rotated || owned.decor[1].property!=1 ||
                current.resume(*campaign.snapshot()->content)!=std::optional{campaign.snapshot()->checkpoint} || !capture("rooms-compact") || !press("personalSellHome")) {
                fail("The furnished home must reload and remain usable at 200% text");return;
            }
            run->company=state.cash;run->personal=state.succession->personal_cash;break;
        }
        case 19:
            if(++run->frames<7)return;
            if(!capture("sale-compact") || !press("personalConfirm")){fail("Confirm the home sale");return;}break;
        case 20:
            if(item("personalConfirm"))return;
            if(!state.succession->personal_life->properties.front().sold_on || state.succession->personal_life->decor.front().property ||
                state.succession->personal_cash!=pgp::core::Cents{run->personal.value()+22500000} || state.cash!=run->company) {
                fail("Selling must return personal money and store the contents");return;
            }
            run->personal=state.succession->personal_cash;run->frames=0;
            if(!press("personalSellDecor_2")){fail("Review selling the stored artwork");return;}
            break;
        case 21:
            if(++run->frames<3)return;
            if(!capture("art-sale-compact") || !press("personalConfirm")){fail("Confirm the artwork sale");return;}
            break;
        case 22: {
            if(item("personalConfirm"))return;
            pgp::persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
            if(!state.succession->personal_life->decor.back().sold_on ||
                state.succession->personal_cash!=pgp::core::Cents{run->personal.value()+4000000} || state.cash!=run->company ||
                item("personalSellDecor_2") || current.resume(*campaign.snapshot()->content)!=std::optional{campaign.snapshot()->checkpoint}) {
                fail("The artwork sale must pay personal savings once, leave the inventory and reload");return;
            }
            std::cout<<"Personal property purchase, separate savings, art delivery, valid/blocked placement, rotation, sale, compact 200% text and fresh reload passed\n";
            timer->stop();QCoreApplication::exit(0);return;
        }
        }
        ++run->phase;
    });
    timer->start();
}
