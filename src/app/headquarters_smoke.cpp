#include "headquarters_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QVector3D>
#include <QtTest/QTest>
#include <iostream>
#include <memory>

namespace {
QQuickItem* find(QQuickItem* root,const char* name) {if(root->objectName()==QLatin1String{name}) return root;for(auto* child:root->childItems()) if(auto* result=find(child,name)) return result;return nullptr;}
bool key(QQuickWindow& window,QQuickItem* item,Qt::Key code=Qt::Key_Space) {if(!item || !item->isVisible() || !item->isEnabled()) return false;item->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,code);return true;}
}
void schedule_headquarters_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot) {
    struct State {int phase=0,ticks=0,days=0,settle=0,reported=-1,previewPhase=0;};auto state=std::make_shared<State>();
    auto* timer=new QTimer(&window);timer->setInterval(70);
    QObject::connect(timer,&QTimer::timeout,&window,[&,timer,state,paths,screenshot] {
        const auto item=[&](const char* name){return find(window.contentItem(),name);};
        const auto press=[&](const char* name){return key(window,item(name));};
        const auto fail=[&](const char* why){std::cerr<<"HQ UI phase "<<state->phase<<": "<<why<<" · "<<campaign.notice().toStdString()<<'\n';timer->stop();QCoreApplication::exit(3);};
        const auto capture=[&](const char* suffix){return screenshot.isEmpty() || window.grabWindow().save(screenshot+QLatin1String{suffix}+".png");};
        if(++state->ticks>1500) {fail("Timed out");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy()) return;
        if(state->reported!=state->phase) {state->reported=state->phase;std::cout<<"HQ UI phase "<<state->phase<<std::endl;}
        const auto& world=campaign.snapshot()->checkpoint.world;const auto& s=world.simulation;
        const auto* headquarters=s.infrastructure && s.infrastructure->workplace && s.infrastructure->workplace->headquarters?&*s.infrastructure->workplace->headquarters:nullptr;
        auto* tools=item("headquartersTools");
        const auto select=[&](int id){return tools && QMetaObject::invokeMethod(tools,"select",Q_ARG(QVariant,QVariant{id}));};
        switch(state->phase) {
        case 0: {
            if(!press("menuNewGameButton")) {fail("New Game");return;}
            auto* founding=campaign.founding();founding->setField("family","rich");founding->setField("route","bootstrap");
            for(const auto& row:founding->chapters()) {const auto chapter=row.toMap();founding->choose(chapter.value("id").toString(),chapter.value("choices").toList().front().toMap().value("id").toString());}
            campaign.startFounderCampaign();++state->phase;break;
        }
        case 1:
            if(!headquarters || !headquarters->items.empty() || pgp::core::current_headquarters(s).kind!="home") {fail("The actual garage must be empty");return;}
            if(++state->settle<12) return;state->settle=0;campaign.clearNotice();
            if(!capture(".empty-garage") || !press("hqFurnishButton") || !press("hqOpenShop")) {fail("Garage and supplier entry");return;}
            ++state->phase;break;
        case 2:
            if(!press("hqBuy_desk")) {fail("Desk supplier offer");return;}++state->phase;break;
        case 3:
            if(!press("hqConfirmPurchase")) {fail("Desk signing review");return;}++state->phase;break;
        case 4:
            if(headquarters->items.size()!=1 || !press("hqBuy_desktop")) {fail("Paid desk order and computer offer");return;}++state->phase;break;
        case 5:
            if(!capture(".supplier-review") || !press("hqConfirmPurchase")) {fail("Computer signing review");return;}++state->phase;break;
        case 6:
            if(headquarters->items.size()!=2 || !press("closeDeviceButton")) {fail("Owned purchases and room return");return;}++state->phase;break;
        case 7:
            if(state->days++<3) {campaign.step();return;}
            if(!select(1) || !press("hqPlaceAtCoordinates")) {fail("Delivered desk placement controls");return;}++state->phase;break;
        case 8:
            if(!headquarters->items[0].premises || !select(2) || !press("hqPlaceAtCoordinates")) {fail("Computer mount on real desk");return;}++state->phase;break;
        case 9:
            if(headquarters->items[1].mount!=1 || !pgp::core::hq_computer_available(s) || !select(1)) {fail("Placed computer must work");return;}
            if(!key(window,item("hqDeskPerson"),Qt::Key_End)) {fail("Desk assignment selector");return;}
            ++state->phase;break;
        case 10: {
            if(headquarters->items[0].person.empty()) {fail("Actual person assigned to desk");return;}
            if(++state->settle<8) return;state->settle=0;
            auto* view=item("hqPhysicalScene");QPointF start,end;
            if(view) {
                QVector3D a,b;
                if(!QMetaObject::invokeMethod(view,"mapFrom3DScene",Q_RETURN_ARG(QVector3D,a),Q_ARG(QVector3D,(QVector3D{static_cast<float>(100-pgp::core::current_headquarters(s).width*50),48,0}))) ||
                   !QMetaObject::invokeMethod(view,"mapFrom3DScene",Q_RETURN_ARG(QVector3D,b),Q_ARG(QVector3D,(QVector3D{static_cast<float>(200-pgp::core::current_headquarters(s).width*50),48,0})))) {fail("Native room projection");return;}
                start=view->mapToScene(QPointF{a.x(),a.y()});end=view->mapToScene(QPointF{b.x(),b.y()});
            } else {
                view=item("hqFloorPlan");if(!view) {fail("Interactive scene unavailable");return;}
                const auto cell=view->property("cell").toReal(),left=view->property("leftEdge").toReal(),top=view->property("topEdge").toReal();
                start=view->mapToScene(QPointF{left+cell*1.25,top+cell*1.25});end=view->mapToScene(QPointF{left+cell*2.25,top+cell*1.25});
            }
            if(state->previewPhase<4) {
                QPointF validPoint,invalidPoint;
                if(item("hqPhysicalScene")) {
                    QVector3D v,b;
                    QMetaObject::invokeMethod(view,"mapFrom3DScene",Q_RETURN_ARG(QVector3D,v),Q_ARG(QVector3D,(QVector3D{static_cast<float>(150-pgp::core::current_headquarters(s).width*50),0,-100})));
                    QMetaObject::invokeMethod(view,"mapFrom3DScene",Q_RETURN_ARG(QVector3D,b),Q_ARG(QVector3D,(QVector3D{static_cast<float>(250-pgp::core::current_headquarters(s).width*50),0,100})));
                    validPoint=view->mapToScene({v.x(),v.y()});invalidPoint=view->mapToScene({b.x(),b.y()});
                } else {
                    const auto cell=view->property("cell").toReal(),left=view->property("leftEdge").toReal(),top=view->property("topEdge").toReal();
                    validPoint=view->mapToScene({left+cell*1.2,top+cell*0.2});invalidPoint=view->mapToScene({left+cell*2.2,top+cell*2.2});
                }
                if(state->previewPhase==0) {
                    if(!press("hqPick_1")) {fail("Pick up illustrated equipment card");return;}
                    QTest::mouseMove(&window,validPoint.toPoint(),20);++state->previewPhase;return;
                }
                if(state->previewPhase==1) {
                    if(!tools->property("placing").toBool() || !tools->property("preview").toMap().value("valid").toBool() || !capture(".placement-valid")) {fail("Valid translucent placement preview");return;}
                    // Native terminal launches may lack OS activation. In that
                    // case exercise the equivalent visible rotation control;
                    // the active offscreen run covers the R shortcut.
                    if(window.isActive()) QTest::keyClick(&window,Qt::Key_R);
                    else if(!press("hqRotatePreview")) {fail("Visible placement rotation control");return;}
                    QTest::mouseMove(&window,invalidPoint.toPoint(),20);++state->previewPhase;return;
                }
                if(state->previewPhase==2) {
                    if(!capture(".placement-blocked") || !tools->property("rotated").toBool() || tools->property("preview").toMap().value("valid").toBool()) {
                        const auto preview=tools->property("preview").toMap();
                        std::cerr<<"Placement rotated="<<tools->property("rotated").toBool()<<" x="<<preview.value("x").toInt()<<" y="<<preview.value("y").toInt()<<" valid="<<preview.value("valid").toBool()<<" active="<<window.isActive()<<'\n';
                        fail("Rotate and invalid footprint feedback");return;
                    }
                    QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,invalidPoint.toPoint());
                    QTest::mouseClick(&window,Qt::RightButton,Qt::NoModifier,invalidPoint.toPoint());++state->previewPhase;return;
                }
                if(tools->property("placing").toBool() || headquarters->items[0].x!=0 || headquarters->items[0].rotated) {fail("Invalid placement and cancel must preserve the real object");return;}
                ++state->previewPhase;
            }
            QTest::mousePress(&window,Qt::LeftButton,Qt::NoModifier,start.toPoint());QTest::mouseMove(&window,end.toPoint(),20);QTest::mouseRelease(&window,Qt::LeftButton,Qt::NoModifier,end.toPoint());
            ++state->phase;break;
        }
        case 11:
            if(headquarters->items[0].x!=1 || headquarters->items[1].mount!=1) {fail("Pointer drag must move the actual desk and attached computer");return;}
            if(!capture(".furnished-garage") || !press("hqOpenRealtor")) {fail("Furnished room and realtor entry");return;}++state->phase;break;
        case 12:
            if(!press("hqLease_compact")) {fail("Available city office listing");return;}++state->phase;break;
        case 13:
            if(!capture(".lease-review") || !press("hqConfirmPurchase")) {fail("Named landlord and lease terms review");return;}++state->phase;break;
        case 14:
            if(pgp::core::current_headquarters(s).kind!="lease" || headquarters->items[0].premises) {fail("Move must use actual leased premises and preserve stored equipment");return;}
            if(!key(window,item("hqMarketPicker"),Qt::Key_End) || !press("hqLand_compact")) {fail("Land in the selected city");return;}++state->phase;break;
        case 15:
            if(!press("hqConfirmPurchase")) {fail("Land purchase review");return;}++state->phase;break;
        case 16:
            if(headquarters->land.size()!=1 || !press("hqTab_build") || !press("hqDesign_1")) {fail("Owned plot and building designer");return;}++state->phase;break;
        case 17:
            if(!press("hqSplitVertical") || !press("hqGetBids")) {fail("Custom room partition and three bids");return;}++state->phase;break;
        case 18:
            if(!capture(".contractor-bids") || !press("hqBid_budget")) {fail("Contractor comparison");return;}++state->phase;break;
        case 19:
            if(!press("hqConfirmPurchase")) {fail("Signed construction and authorized contingency");return;}++state->phase;break;
        case 20:
            if(headquarters->construction.size()!=1 || headquarters->construction[0].rooms.size()!=2) {fail("The signed contract must retain the actual room design");return;}
            if(!headquarters->construction[0].completed_on) {if(campaign.requiredKind()=="content") {for(const auto& choice:campaign.requiredChoices()) {const auto row=choice.toMap();if(row.value("choiceId").toString().contains("decline")) {campaign.choose(row.value("choiceId").toString());return;}}}
                if(!campaign.canAdvance()){capture(".blocked");std::cerr<<"Advance blocked: "<<campaign.requiredKind().toStdString()<<" · "<<campaign.requiredTitle().toStdString()<<" · paused "<<campaign.paused()<<" · ready "<<campaign.ready()<<"\n";fail("Construction wait cannot advance");return;}campaign.step();return;}
            if(!press("hqMoveOwned_1")) {fail("Completed building move offer");return;}++state->phase;break;
        case 21:
            if(!press("hqConfirmPurchase")) {fail("Owned-building relocation terms");return;}++state->phase;break;
        case 22: {
            if(pgp::core::current_headquarters(s).kind!="owned" || pgp::core::current_headquarters(s).rent.value()!=0 || !press("closeDeviceButton")) {fail("Owned headquarters with no rent");return;}
            const auto& program=*campaign.snapshot()->content;pgp::persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
            if(current.resume(program)!=std::optional{campaign.snapshot()->checkpoint}) {fail("Fresh current company must reload exactly");return;}
            ++state->phase;break;
        }
        case 23:
            if(++state->settle<8) return;
            if(!capture(".owned-building")) {fail("Owned building render");return;}
            campaign.setTextScalePercent(200);window.resize(650,480);state->settle=0;++state->phase;break;
        case 24:
            if(window.visibility()!=QWindow::Windowed || window.width()!=650 || window.height()!=480) {window.showNormal();window.resize(650,480);state->settle=0;return;}
            campaign.clearNotice();
            if(++state->settle<10) return;
            if(!capture(".compact-200") || !press("hqStartPlacement")) {fail("Compact high-text placement controls");return;}
            state->settle=0;++state->phase;break;
        case 25: {
            if(++state->settle<8) return;
            auto* floor=item("hqFloorPlan");if(!floor)floor=item("hqPhysicalScene");
            if(!floor || floor->height()<250 || !capture(".compact-placement")) {fail("Compact room must have space for actual placement");return;}
            std::cout<<"HQ UI passed: empty garage, paid suppliers/delivery, placement, desk assignment, pointer drag, landlord lease, city land purchase, custom rooms, contractor bids and completion, owned relocation, fresh resume\n";
            timer->stop();QCoreApplication::exit(0);break;
        }
        }
    });timer->start();
}
