#include "world_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/current_campaign.hpp>
#include <pgp/persistence/sha256.hpp>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QWheelEvent>
#include <iostream>

namespace {
QQuickItem* button(QQuickItem* root,const QString& text) {
    if(root->isVisible() && root->metaObject()->indexOfProperty("down")>=0 && root->property("text").toString()==text) return root;
    for(auto* child:root->childItems()) if(auto* found=button(child,text)) return found;
    return nullptr;
}
QQuickItem* find(QQuickItem* root,const QString& name) {
    if(root->isVisible() && root->objectName()==name) return root;
    for(auto* child:root->childItems()) if(auto* found=find(child,name)) return found;
    return nullptr;
}
bool press(QQuickWindow& window,QQuickItem* item) {
    if(!item || !item->isEnabled()) return false;
    item->forceActiveFocus(Qt::TabFocusReason);
    QKeyEvent down{QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier},up{QEvent::KeyRelease,Qt::Key_Space,Qt::NoModifier};
    QCoreApplication::sendEvent(&window,&down);QCoreApplication::sendEvent(&window,&up);return true;
}
}
void schedule_content_recovery_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString source,QString screenshot) {
    using namespace pgp;
    const auto registry=content::foundation_registry();
    auto program=std::make_shared<const runtime::CompiledProgram>(runtime::compile_collection(content::load_collection(persistence::read_bounded(std::filesystem::path{source.toStdU16String()},content::maximum_document_bytes),registry),registry));
    persistence::CurrentCampaign current{paths.saves_directory/"continuing"};
    const auto expected=current.resume(*program);
    if(!expected) throw std::runtime_error("Content recovery smoke needs an existing isolated company");
    const auto fingerprint=current.fingerprint();
    const auto packet=current.content().packet(persistence::sha256(program->canonical_content_bytes));
    struct State{int step=0,ticks=0,settle=0;bool resumed=false,scrolled=false;};auto state=std::make_shared<State>();
    auto* timer=new QTimer(&window);timer->setInterval(200);
    QObject::connect(timer,&QTimer::timeout,&window,[&,state,timer,program,expected,fingerprint,packet,paths,source,screenshot] {
        const auto fail=[&](const char* message){std::cerr<<message<<'\n';timer->stop();QCoreApplication::exit(3);};
        const auto capture=[&](const QString& suffix){if(!screenshot.isEmpty()) window.grabWindow().save(screenshot+suffix+".png");};
        if(++state->ticks>180){fail("Content recovery interface timed out");return;}
        if(!campaign.ready() || campaign.busy()) return;
        if(state->step==0) {
            state->resumed=campaign.currentCampaign();
            if(state->resumed) {state->step=5;return;}
            if(!campaign.storageBlocked() || !campaign.needsOriginalContent() || campaign.canStartNew()) {fail("Missing original rules must retain a blocked company");return;}
            if(!window.property("menuVisible").toBool()) {fail("Recovery must remain reachable from startup menu");return;}
            capture("-missing");campaign.reviewOriginalContent(QUrl::fromLocalFile(source));state->step=1;return;
        }
        if(state->step==1 || state->step==3) {
            auto* dialog=window.findChild<QObject*>("originalContentReviewDialog");
            if(campaign.originalContentReview().isEmpty() || !dialog || !dialog->property("visible").toBool()) return;
            persistence::CurrentCampaign untouched{paths.saves_directory/"continuing"};
            if(untouched.fingerprint()!=fingerprint || std::filesystem::exists(packet)) {fail("Content review must not write company or content files");return;}
            window.resize(650,480);capture(state->step==1?"-review":"-confirm");
            if(state->step==3) {
                auto* scroll=find(window.contentItem(),"originalContentScroll");
                if(!scroll){fail("Original content review needs a scroll area");return;}
                if(!state->scrolled) {
                    const auto position=scroll->mapToScene(QPointF{scroll->width()/2,scroll->height()/2});
                    QWheelEvent wheel{position,window.mapToGlobal(position),QPoint{},QPoint{0,-960},Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false};
                    QCoreApplication::sendEvent(&window,&wheel);state->scrolled=true;return;
                }
                if(++state->settle<4) return;
                auto* viewport=qvariant_cast<QObject*>(scroll->property("contentItem"));
                const auto offset=scroll->property("scrollOffset").toReal();
                const auto maximum=viewport?viewport->property("contentHeight").toReal()-viewport->property("height").toReal():0;
                std::cout<<"Review scroll: "<<offset<<" of "<<maximum<<'\n';
                if(offset<=0 || offset>maximum+1){fail("Compact original content review must stay within its text");return;}
                capture("-review-scrolled");
            }
            if(!press(window,button(window.contentItem(),state->step==1?QStringLiteral("Cancel"):QStringLiteral("OK")))) {fail("Original rules review needs keyboard confirmation and cancellation");return;}
            ++state->step;return;
        }
        if(state->step==2) {
            if(!campaign.originalContentReview().isEmpty() || !campaign.needsOriginalContent()) {fail("Cancelling review must retain recovery state");return;}
            campaign.reviewOriginalContent(QUrl::fromLocalFile(source));state->step=3;return;
        }
        if(state->step==4) {if(!campaign.currentCampaign()) {fail("Confirmed original rules must resume the company");return;}state->step=5;return;}
        if(state->step==5) {
            if(!campaign.currentCampaign() || campaign.storageBlocked() || campaign.needsOriginalContent() || !campaign.paused() || campaign.campaignContentVersion()!=QString::fromStdString(program->collection.content_version) || campaign.snapshot()->checkpoint!=*expected) {fail("Original source, pending decisions, people and exact state must resume paused");return;}
            persistence::CurrentCampaign unchanged{paths.saves_directory/"continuing"};if(unchanged.fingerprint()!=fingerprint) {fail("Original company and journal must remain unchanged");return;}
            capture("-resumed");campaign.saveTo(QUrl::fromLocalFile(QString::fromStdU16String((paths.saves_directory/"source-check.pgpsave").u16string())));state->step=6;return;
        }
        if(state->step==6) {
            if(persistence::load_file(*program,paths.saves_directory/"source-check.pgpsave").checkpoint!=*expected) {fail("Export must use resumed original source");return;}
            std::cout<<"Original rules review, keyboard cancel/confirm, exact paused resume and source-aware export passed; resumed="<<state->resumed<<'\n';timer->stop();QCoreApplication::exit(0);
        }
    });timer->start();
}
