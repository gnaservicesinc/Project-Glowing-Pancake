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
    struct State {
        int phase=0,ticks=0,days=0,settle=0,reported=-1,previewPhase=0;
        std::uint64_t familyRevision=0;
        pgp::core::Cents personalBeforeVisit{0}, companyBeforeVisit{0};
    };auto state=std::make_shared<State>();
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
            founding->setField("investment","790000");
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
                if(!QMetaObject::invokeMethod(view,"mapFrom3DScene",Q_RETURN_ARG(QVector3D,a),Q_ARG(QVector3D,(QVector3D{static_cast<float>(100-pgp::core::current_headquarters(s).width*50),48,static_cast<float>(150-pgp::core::current_headquarters(s).depth*50)}))) ||
                   !QMetaObject::invokeMethod(view,"mapFrom3DScene",Q_RETURN_ARG(QVector3D,b),Q_ARG(QVector3D,(QVector3D{static_cast<float>(200-pgp::core::current_headquarters(s).width*50),48,static_cast<float>(150-pgp::core::current_headquarters(s).depth*50)})))) {fail("Native room projection");return;}
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
                    QMetaObject::invokeMethod(view,"mapFrom3DScene",Q_RETURN_ARG(QVector3D,v),Q_ARG(QVector3D,(QVector3D{static_cast<float>(150-pgp::core::current_headquarters(s).width*50),0,static_cast<float>(50-pgp::core::current_headquarters(s).depth*50)})));
                    QMetaObject::invokeMethod(view,"mapFrom3DScene",Q_RETURN_ARG(QVector3D,b),Q_ARG(QVector3D,(QVector3D{static_cast<float>(pgp::core::current_headquarters(s).width*50-50),0,static_cast<float>(pgp::core::current_headquarters(s).depth*50-50)})));
                    validPoint=view->mapToScene({v.x(),v.y()});invalidPoint=view->mapToScene({b.x(),b.y()});
                } else {
                    const auto cell=view->property("cell").toReal(),left=view->property("leftEdge").toReal(),top=view->property("topEdge").toReal();
                    validPoint=view->mapToScene({left+cell*1.2,top+cell*0.2});invalidPoint=view->mapToScene({left+cell*(pgp::core::current_headquarters(s).width-0.8),top+cell*(pgp::core::current_headquarters(s).depth-0.8)});
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
            if(!press("hqCancelPreview")) {fail("Finish compact placement");return;}
            if(!press("hqDoneArranging")) {
                if(!press("hqFurnishButton")) {fail("Finish furnishing before opening My life");return;}
            }
            state->familyRevision=s.revision;
            state->settle=0;state->phase=31;break;
        }
        case 26:
            if(++state->settle<10)return;
            if(!press("hqReturnHome")) {fail("Open the reviewed relocation after visiting family home");return;}state->phase=28;break;
        case 27:
            if(!press("hqReturnHome")) {fail("The family home must remain available after commercial relocation");return;}++state->phase;break;
        case 28:
            if(!capture(".home-return-review") || !press("hqConfirmPurchase")) {fail("Home return terms");return;}++state->phase;break;
        case 29:
            if(pgp::core::current_headquarters(s).kind!="home" || pgp::core::current_headquarters(s).rooms!=pgp::core::headquarters_home(s).plan.rooms || world.state_id!="state.us_ca" || !press("closeDeviceButton")) {fail("Actual family home restored in California");return;}
            state->settle=0;++state->phase;break;
        case 30: {
            if(++state->settle<10)return;
            const auto& program=*campaign.snapshot()->content;pgp::persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
            if(current.resume(program)!=std::optional{campaign.snapshot()->checkpoint} || !capture(".returned-home")) {fail("Home recovery must render and reload exactly");return;}
            std::cout<<"HQ UI passed: paid supplies, placement, commercial lease, owned relocation, family rooms and keepsake, reviewed personal visit, arrival and return at 200% text, moving the company home and fresh resume\n";
            timer->stop();QCoreApplication::exit(0);break;
        }
        case 31:
            if(!press("personalLifeTab")) {fail("Open My life from commercial HQ");return;}++state->phase;break;
        case 32:
            if(!press("personalPage_family")) {fail("Open the family home independently of HQ");return;}state->settle=0;++state->phase;break;
        case 33:
            if(++state->settle<5)return;
            if(!press("familyBirthRecord")) {fail("Open the framed record from the childhood bedroom");return;}state->settle=0;++state->phase;break;
        case 34: {
            if(++state->settle<5)return;
            const auto* record=item("familyCertificate");
            if(!record || !record->isVisible() || record->mapToScene({record->width(),0}).x()>window.width() ||
                !capture(".family-keepsake-compact")) {fail("The family record must fit at 200% text");return;}
            QTest::keyClick(&window,Qt::Key_Escape);state->settle=0;++state->phase;break;
        }
        case 35:
            if(++state->settle<5)return;
            if(!press("familyRoom_3")) {fail("Select a family workspace by keyboard");return;}state->settle=0;++state->phase;break;
        case 36: {
            if(++state->settle<5)return;
            auto* plan=item("familyWorkspacePlan");
            const auto family=campaign.world()->familyHome();
            const auto home=pgp::core::headquarters_home(s);
            if(s.revision!=state->familyRevision || pgp::core::current_headquarters(s).kind!="owned" || family.value("atHome").toBool() ||
                !family.value("items").toList().empty() || family.value("rooms").toList().size()!=static_cast<qsizetype>(home.plan.rooms.size()) ||
                !plan || !plan->isVisible() || plan->property("highlightedRoom").toInt()!=3 ||
                campaign.world()->draft("personal/family-room","")!="3") {fail("The family plan must show its own saved rooms while the commercial HQ stays in place");return;}
            plan->forceActiveFocus(Qt::OtherFocusReason);
            state->settle=0;++state->phase;break;
        }
        case 37: {
            if(++state->settle<5)return;
            auto* plan=item("familyWorkspacePlan");const auto* viewport=item("managementScroll");
            if(!plan || !viewport || plan->mapToItem(viewport,{0,plan->height()}).y()>viewport->height()+1 ||
                !capture(".family-workspaces-compact")) {fail("Show the complete family plan at 200% text");return;}
            const auto garage=pgp::core::headquarters_home(s).plan.rooms.front();
            const auto cell=plan->property("cell").toReal();
            const auto point=plan->mapToScene({plan->property("leftEdge").toReal()+(garage.x+0.5)*cell,
                plan->property("topEdge").toReal()+(garage.y+0.5)*cell});
            QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,point.toPoint());
            ++state->phase;break;
        }
        case 38:
            if(campaign.world()->draft("personal/family-room","")!="1" || !press("familyVisitReview")) {
                fail("Select a room on the plan and review a trip home");return;
            }
            state->personalBeforeVisit=s.succession->personal_cash;
            state->companyBeforeVisit=s.cash;
            state->settle=0;++state->phase;break;
        case 39:
            if(++state->settle<5)return;
            if(s.revision!=state->familyRevision || !capture(".family-visit-review") || !press("familyVisitConfirm")) {fail("Review the personal fare and dates before booking");return;}
            ++state->phase;break;
        case 40: {
            const auto* trip=pgp::core::pending_family_visit(s);
            if(!trip || s.cash!=state->companyBeforeVisit || s.succession->personal_cash!=pgp::core::checked_unit_subtract(state->personalBeforeVisit,trip->fare) ||
                !press("familyVisitCancel")) {fail("Booking must use personal cash and offer a refund before departure");return;}
            ++state->phase;break;
        }
        case 41:
            if(pgp::core::pending_family_visit(s) || s.succession->personal_cash!=state->personalBeforeVisit || !press("familyVisitReview")) {fail("Cancel and rebook a fully refunded visit");return;}
            state->settle=0;++state->phase;break;
        case 42:
            if(++state->settle<5)return;
            if(!press("familyVisitConfirm")) {fail("Confirm the second reviewed fare");return;}
            ++state->phase;break;
        case 43: {
            const auto* trip=pgp::core::pending_family_visit(s);
            if(!trip) {fail("Keep the booked family visit");return;}
            if(s.date<trip->arrives_on) {if(!press("familyVisitAdvance"))fail("Advance to departure or arrival");return;}
            if(!pgp::core::executive_location(s).family_home || campaign.world()->atHeadquarters() || !press("familyRoom_0")) {fail("Arrive at the family home while the company stays in its office");return;}
            state->settle=0;++state->phase;break;
        }
        case 44: {
            if(++state->settle<5)return;
            pgp::persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
            auto* status=item("familyVisitStatus");
            if(!status || !status->isVisible() || !campaign.world()->location().contains("Family home") ||
                current.resume(*campaign.snapshot()->content)!=std::optional{campaign.snapshot()->checkpoint}) {fail("Show the saved arrival in the family home and world location");return;}
            status->forceActiveFocus(Qt::OtherFocusReason);
            if(!capture(".family-visit-arrival") || !press("familyVisitAdvance")) {fail("Advance from the family visit toward headquarters");return;}
            ++state->phase;break;
        }
        case 45:
            if(pgp::core::pending_family_visit(s)) {if(!press("familyVisitAdvance"))fail("Finish the return leg");return;}
            if(!campaign.world()->atHeadquarters() || pgp::core::current_headquarters(s).kind!="owned" || !press("familyHomeRealtor")) {
                fail("Return to the same company office and separately open the realtor");return;
            }
            campaign.setTextScalePercent(100);window.resize(1280,720);state->settle=0;state->phase=26;break;
        }
    });timer->start();
}
