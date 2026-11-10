#include "contact_finance_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <algorithm>
#include <iostream>

namespace {
QQuickItem* find(QQuickItem* root,const QString& name) {
    if(!root->isVisible())return nullptr;
    if(root->objectName()==name)return root;
    for(auto* child:root->childItems())if(auto* found=find(child,name))return found;
    return nullptr;
}
}
void schedule_contact_finance_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot,bool donor) {
    struct Run {int phase=0,ticks=0,frames=0;pgp::core::Cents cash{0},personal{0},capital{0};QString motion;std::vector<pgp::core::EquityHolding> holdings;};
    const auto run=std::make_shared<Run>();
    const QString person=donor?"opening/open_science":"opening/frontier";
    const QString offer=donor?"corporate.open_field_gift":"corporate.round_large";
    auto* timer=new QTimer(&window);timer->setInterval(100);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,paths,screenshot,donor,person,offer] {
        const auto capture=[&](const QString& name){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+name+".png");};
        const auto fail=[&](const char* why){std::cerr<<"Investor phase "<<run->phase<<": "<<why<<" · "<<campaign.notice().toStdString()<<'\n';(void)capture("failure");timer->stop();QCoreApplication::exit(3);};
        if(++run->ticks>500){fail("Timed out");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())return;
        const auto item=[&](const QString& name){return find(window.contentItem(),name);};
        const auto press=[&](const QString& name,Qt::Key key=Qt::Key_Space){auto* control=item(name);if(!control || !control->isEnabled())return false;control->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,key);return true;};
        const auto& state=campaign.snapshot()->checkpoint.world.simulation;
        const auto funding=[&]{return campaign.world()->contactFunding(person);};
        switch(run->phase) {
        case 0: {
            if(!press("menuNewGameButton")){fail("New Game");return;}
            auto* story=campaign.founding();story->setField("family","rich");story->setField("leg","genius");story->setField("route","bootstrap");story->setField("office","home");
            for(const auto& value:story->chapters()) {
                const auto chapter=value.toMap();story->choose(chapter.value("id").toString(),chapter.value("choices").toList().front().toMap().value("id").toString());
            }
            story->setBeat("home");
            if(donor) {
                story->setField("legal_form","nonprofit");story->setField("route","mission_director");
                if(!story->contactHomeOffer("safety").isEmpty() || !story->respondHomeOffer("safety","accept").isEmpty()){fail("Agree actual founding donor terms");return;}
            }
            if(!story->contactHomeOffer(donor?"open_science":"frontier").isEmpty() || !story->closeHomeOffer().isEmpty()){fail("Meet the funding contact before formation");return;}
            campaign.startFounderCampaign();break;
        }
        case 1:
            campaign.world()->buyDevice("tablet",campaign.world()->reviewToken());break;
        case 2: case 3:
            campaign.step();break;
        case 4:
            if(!press("messagesTab")){fail("Open Messages");return;}break;
        case 5:
            if(!press("contactPerson_"+person)){fail("Open the continuing funding conversation");return;}break;
        case 6: {
            run->cash=state.cash;run->personal=state.succession->personal_cash;
            run->holdings=state.accounts->holdings;
            const auto& quotes=state.management->catalog.governance->financing;
            run->capital=std::ranges::find(quotes,offer.toStdString(),&pgp::core::CorporateFinanceTerms::id)->proceeds;
            if(!press("contactFundingPitch")){fail("Pitch for follow-up funding");return;}run->frames=0;break;
        }
        case 7: {
            if(++run->frames<3)return;
            const auto rows=funding().value("discussions").toList();
            if(rows.size()!=1 || rows.front().toMap().value("offers").toList().size()!=(donor?1:2) || state.cash!=run->cash ||
                !capture("offered-normal") || !press("contactFundingHold")){fail("Read actual counteroffers and hold them without payment");return;}
            run->frames=0;break;
        }
        case 8:
            if(funding().value("discussions").toList().front().toMap().value("status")!="held"){fail("Persist the held offer");return;}
            campaign.setTextScalePercent(200);break;
        case 9:
            if(window.visibility()!=QWindow::Windowed || window.size()!=QSize{650,480}){window.showNormal();window.resize(650,480);return;}
            if(campaign.textScalePercent()!=200 || !press("contactFundingReview_"+offer)){fail("Review funding terms at actual 200% text");return;}
            run->frames=0;break;
        case 10: {
            if(++run->frames<3)return;
            const auto* copy=item("contactFundingTerms");
            auto* scroll=item("contactFundingReviewScroll");
            auto* content=scroll?scroll->property("contentItem").value<QObject*>():nullptr;
            if(!copy || !copy->property("text").toString().contains(donor?"no ownership":"contributed capital") || !capture("review-compact") || !content ||
                !content->setProperty("contentY",std::max(0.,content->property("contentHeight").toDouble()-content->property("height").toDouble()))) {
                fail("Read cash, conserving dilution and the board requirement");return;
            }
            run->frames=0;break;
        }
        case 11: {
            if(++run->frames<3)return;
            const auto* copy=item("contactFundingTerms");const auto* scroll=item("contactFundingReviewScroll");
            const auto* confirm=item("contactFundingConfirm");
            if(!copy || !scroll || !confirm || !confirm->isEnabled() || copy->mapToItem(scroll,{0,copy->height()}).y()>scroll->height()+1 ||
                confirm->mapToScene({0,confirm->height()}).y()>window.height() || !capture("terms-compact") || !press("contactFundingBack")) {
                fail("Scroll through all terms and cancel with reachable controls");return;
            }break;
        }
        case 12:
            if(state.cash!=run->cash || !state.management->governance->motions.empty() ||
                !press("contactFundingReview_"+offer)){fail("Cancelling the review must preserve the offer and treasury");return;}run->frames=0;break;
        case 13:
            if(++run->frames<3)return;
            if(!press("contactFundingConfirm")){fail("Submit the reviewed terms to the board");return;}run->frames=0;break;
        case 14: {
            if(++run->frames<3)return;
            const auto* status=item("contactFundingStatus");const auto* viewport=item("managementScroll");
            if(state.management->governance->motions.size()!=1 || state.cash!=run->cash ||
                !status || !viewport || status->mapToItem(viewport,{0,status->height()}).y()>viewport->height()+1 ||
                funding().value("discussions").toList().front().toMap().value("status")!="board" || !capture("pending-compact") || !press("contactFundingBoard")) {
                fail("Submission must create an unpaid board motion and open governance");return;
            }
            run->motion=QString::fromStdString(state.management->governance->motions.back().id);break;
        }
        case 15:
            if(!press("corporatePageChoiceInput",Qt::Key_Home) || !press("corporatePageChoiceInput",Qt::Key_Down) ||
                !press("corporatePageChoiceInput",Qt::Key_Down)){fail("Open board votes");return;}
            campaign.management()->boardMotions()->setSelectedId(run->motion);break;
        case 16:
            if(!press("voteBoardYesButton")){fail("Review the actual board vote");return;}run->frames=0;break;
        case 17: {
            if(++run->frames<3)return;
            auto* dialog=window.findChild<QObject*>("corporateConfirmation");
            if(!dialog || !dialog->property("visible").toBool() || !QMetaObject::invokeMethod(dialog,"accept")) {
                fail("Approve the reviewed board vote");return;
            }break;
        }
        case 18:
            if(!state.management->governance->motions.back().executed || state.cash!=pgp::core::checked_unit_add(run->cash,run->capital) ||
                (donor && (state.accounts->holdings!=run->holdings || !pgp::core::nonprofit_organization(state))) ||
                state.succession->personal_cash!=run->personal || !press("messagesTab")) {
                fail("Approval must deliver actual company capital and leave personal cash alone");return;
            }break;
        case 19:
            if(!press("contactPerson_"+person)){fail("Return to the funded conversation");return;}run->frames=0;break;
        case 20: {
            if(++run->frames<3)return;
            pgp::persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
            const auto* status=item("contactFundingStatus");const auto* viewport=item("managementScroll");
            const auto* reply=item("contactFundingReply");
            if(funding().value("discussions").toList().front().toMap().value("status")!="funded" || !status || !viewport ||
                !reply || reply->mapToItem(viewport,{0,0}).y()>viewport->height()-64 ||
                status->mapToItem(viewport,{0,status->height()}).y()>viewport->height()+1 || !capture("funded-compact") ||
                current.resume(*campaign.snapshot()->content)!=std::optional{campaign.snapshot()->checkpoint}) {
                std::cerr<<"Reply status "<<funding().value("discussions").toList().front().toMap().value("status").toString().toStdString()
                    <<" · visible bottom "<<(status && viewport?status->mapToItem(viewport,{0,status->height()}).y():-1)
                    <<" · viewport "<<(viewport?viewport->height():-1)<<" · focused "<<(status && status->hasActiveFocus())<<'\n';
                fail("The funded reply, ownership, board seat and cash must reload at 200% text");return;
            }
            std::cout<<(donor?"Nonprofit donor proposal, gift review, unchanged endowment, trustee approval, compact reply and fresh reload passed\n":
                "Continuing investor pitch, real counteroffers, hold, review cancellation, compact scroll, actual board funding and fresh reload passed\n");
            timer->stop();QCoreApplication::exit(0);return;
        }
        }
        ++run->phase;
    });
    timer->start();
}
