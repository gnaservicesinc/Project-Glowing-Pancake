#include "world_smoke.hpp"
#include "campaign_controller.hpp"
#include "departure_presentation.hpp"
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
    for(auto* child:root->childItems())if(auto* item=find(child,name))return item;
    return nullptr;
}
}
void schedule_asi_leadership_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,
        pgp::persistence::LocalPaths paths,QString source,QString screenshot) {
    using namespace pgp;
    struct Run {int phase=0,ticks=0,frames=0;core::Cents cash{0};core::OrdinalDay due{0};bool restored=false;};
    const auto run=std::make_shared<Run>();auto* timer=new QTimer(&window);timer->setInterval(100);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,paths,source,screenshot] {
        const auto item=[&](const char* name){return find(window.contentItem(),QString::fromUtf8(name));};
        const auto fail=[&](const char* reason){std::cerr<<"ASI leadership "<<run->phase<<": "<<reason<<" · "<<campaign.notice().toStdString()<<'\n';if(!screenshot.isEmpty())window.grabWindow().save(screenshot+".failure.png");timer->stop();QCoreApplication::exit(3);};
        const auto press=[&](const char* name){auto* target=item(name);if(!target || !target->isEnabled()){fail(name);return false;}target->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,Qt::Key_Space);return true;};
        const auto reveal=[&](const char* name){auto* target=item(name);auto* viewport=item("managementScroll");if(target && viewport){target->forceActiveFocus();QMetaObject::invokeMethod(viewport,"reveal",Q_ARG(QVariant,QVariant::fromValue(target)));}};
        const auto capture=[&](const char* suffix){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+QString::fromUtf8(suffix)+".png");};
        if(++run->ticks>900){fail("Timed out");return;}
        if(!campaign.ready() || campaign.busy() || !window.isSceneGraphInitialized())return;
        auto* desk=campaign.leadership();const auto& s=campaign.snapshot()->checkpoint.world.simulation;
        if(run->phase>1) {
            if(campaign.requiredKind()=="leadership"){desk->acknowledge();return;}
            if(campaign.requiredKind()=="site"){campaign.acknowledgeSiteAlert();return;}
            if(campaign.requiredKind()=="management"){campaign.acknowledgeManagement();return;}
            if(campaign.requiredKind()=="liquidity"){campaign.acknowledgeWarning();return;}
            if(campaign.requiredKind()=="content"){campaign.choose(campaign.requiredChoices().back().toMap().value("choiceId").toString());return;}
        }
        switch(run->phase) {
        case 0:campaign.loadFrom(QUrl::fromLocalFile(source));++run->phase;return;
        case 1:
            if(!s.frontier || !core::earliest_frontier_qualification(*s.frontier,true)){fail("Needs a genuinely qualified saved company");return;}
            run->restored=s.succession->terms.back().asi.has_value();
            window.setProperty("menuVisible",false);window.setProperty("sessionEntered",true);window.showNormal();window.resize(1280,800);
            if(!campaign.world()->navigate("workspace","hq","laptop","leadership","")){fail("Open Leadership");return;}
            run->phase=run->restored?20:2;return;
        case 2: {
            auto* picker=item("leadershipCandidate");if(!picker){fail("Successor selector");return;}
            picker->forceActiveFocus();QTest::keyClick(&window,Qt::Key_End);
            if(picker->property("currentValue").toString()!="asi.1"){fail("Select the company's ASI by keyboard");return;}
            if(!press("leadershipRetainChair"))return;
            run->cash=s.cash;reveal("leadershipCandidate");run->frames=0;++run->phase;return;
        }
        case 3:
            if(++run->frames<5)return;if(!capture("candidate") || !press("reviewLeadershipNomination"))return;++run->phase;return;
        case 4:
            if(s.cash!=run->cash || !s.succession->plans.empty() || !desk->reviewText().contains("one board seat")){fail("Nomination review and control rights");return;}
            capture("nomination");if(press("confirmLeadershipAction"))++run->phase;return;
        case 5:if(press("reviewLeadershipSupport"))++run->phase;return;
        case 6:capture("board");if(press("confirmLeadershipAction"))++run->phase;return;
        case 7: {
            if(s.succession->plans.front().status!=core::SuccessionStatus::approved || s.cash>=run->cash){fail("Actual paid board approval");return;}
            run->due=s.succession->plans.front().handover_on;
            auto* angle=item("departureMessage");if(!angle){fail("Departure PR angle");return;}
            angle->forceActiveFocus();QTest::keyClick(&window,Qt::Key_End);
            const auto preview=desk->announcementPreview("quiet_handover","personal");
            if(preview.value("focus")!="ai_control" || !preview.value("response").toString().contains("control assessment")){fail("Control evidence must lead the coverage");return;}
            auto boundary=s;boundary.management->catalog.succession->asi->control_scrutiny_below=s.succession->plans.front().asi->control;
            const auto boundary_story=bridge::departureArticle(boundary,core::preview_founder_departure(boundary,{1,"quiet_handover","personal"}));
            if(boundary_story.value("focus")!="ai_successor"){fail("Control at the authored boundary must retain the ASI appointment lead");return;}
            reveal("departureCoveragePreview");run->frames=0;++run->phase;return;
        }
        case 8:
            if(++run->frames<5)return;capture("coverage");campaign.setTextScalePercent(200);run->frames=0;++run->phase;return;
        case 9:
            if(window.width()!=650 || window.height()!=480){window.showNormal();window.resize(650,480);run->frames=0;return;}
            if(++run->frames<6)return;reveal("departureCoveragePreview");run->frames=0;++run->phase;return;
        case 10:
            if(window.width()!=650 || window.height()!=480){window.showNormal();window.resize(650,480);run->frames=0;return;}
            if(++run->frames<5)return;
            if(window.grabWindow().size()!=QSize{650,480}){fail("Compact capture dimensions");return;}
            capture("coverage-compact");campaign.setTextScalePercent(100);run->frames=0;++run->phase;return;
        case 11:
            if(window.width()!=1280 || window.height()!=800){window.showNormal();window.resize(1280,800);run->frames=0;return;}
            if(++run->frames<5)return;if(press("reviewDepartureAnnouncement"))++run->phase;return;
        case 12:capture("announcement");if(press("confirmLeadershipAction"))++run->phase;return;
        case 13:
            if(s.succession->releases->size()!=1){fail("Actual announcement record");return;}
            campaign.setDaysPerSecond(60);if(press("advanceLeadership"))++run->phase;return;
        case 14:
            if(!campaign.paused())return;
            if(s.date<run->due){(void)press("advanceLeadership");return;}
            if(!s.succession->terms.back().asi){fail("Prepared work did not appoint the ASI");return;}
            run->phase=20;return;
        case 20:
            if(!s.succession->terms.back().asi || s.succession->terms.back().portrait || desk->overview().contains("born 0")){fail("ASI identity and executive history");return;}
            if(!desk->departureHistory().contains("ASI succession draws scrutiny")){fail("Dated coverage after appointment");return;}
            reveal("leadershipOverview");run->frames=0;++run->phase;return;
        case 21:
            if(++run->frames<5)return;capture(run->restored?"restored":"appointed");
            campaign.saveTo(QUrl::fromLocalFile(QString::fromStdString((paths.saves_directory/"asi-handoff.pgpsave").string())));++run->phase;return;
        default:
            if(!std::filesystem::is_regular_file(paths.saves_directory/"asi-handoff.pgpsave")){fail("Actual exported appointment save");return;}
            std::cout<<"ASI leadership native "<<(run->restored?"reload":"handover")<<" passed with actual independent qualification, keyboard nomination, board rights, paid preparation, control-led coverage and 650x480 at 200% text.\n";
            timer->stop();QCoreApplication::exit(0);return;
        }
    });timer->start();
}
