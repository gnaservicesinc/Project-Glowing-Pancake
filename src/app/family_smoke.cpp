#include "family_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/family.hpp>
#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <algorithm>
#include <iostream>
namespace {
QQuickItem* visible(QQuickItem* root,const QString& name){if(!root->isVisible())return nullptr;if(root->objectName()==name)return root;for(auto* child:root->childItems())if(auto* found=visible(child,name))return found;return nullptr;}
}
void schedule_family_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct Run {int phase=0,ticks=0,frames=0,days=0;bool resumed=false;std::filesystem::path save;core::Cents cash{0};std::string applicant;};
    auto run=std::make_shared<Run>();run->save=paths.saves_directory/"family-ui.pgpsave";auto* timer=new QTimer(&window);timer->setInterval(100);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,screenshot,paths]{
        const auto fail=[&](const std::string& why){std::cerr<<"Family UI "<<run->phase<<": "<<why<<" · "<<campaign.notice().toStdString()<<'\n';if(!screenshot.isEmpty())window.grabWindow().save(screenshot+".failure.png");timer->stop();QCoreApplication::exit(3);};
        if(++run->ticks>1200){fail("Timeout");return;}if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())return;
        const auto item=[&](const QString& name){return visible(window.contentItem(),name);};
        if(auto* panel=item("personalLifePanel");panel && panel->property("transitioning").toBool())return;
        const auto press=[&](const QString& name){auto* c=item(name);if(!c || !c->isEnabled())return false;c->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,Qt::Key_Space);return true;};
        const auto reveal=[&](QQuickItem* target){if(!target)return;target->forceActiveFocus(Qt::OtherFocusReason);for(auto* p=target->parentItem();p;p=p->parentItem())if(p->property("contentY").isValid()){const auto y=target->mapToItem(p,QPointF(0,0)).y()+p->property("contentY").toDouble();p->setProperty("contentY",std::clamp(y,0.0,std::max(0.0,p->property("contentHeight").toDouble()-p->height())));break;}};
        const auto on_screen=[&](QQuickItem* target){if(!target)return false;for(auto* p=target->parentItem();p;p=p->parentItem())if(p->clip() && !QRectF(0,0,p->width(),p->height()).adjusted(-1,-1,1,1).contains(target->mapRectToItem(p,target->boundingRect())))return false;return true;};
        const auto capture=[&](const char* suffix){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+suffix+".png");};
        auto* world=campaign.world();const auto& w=campaign.snapshot()->checkpoint.world;
        try {switch(run->phase) {
        case 0:{
            if(campaign.currentCampaign()) {
                if(campaign.companyName()!="Family Chapters" || !w.family || w.family->pending.size()!=1){fail("A different campaign is active; preserve it");return;}run->resumed=true;run->phase=9;return;
            }
            const auto& program=*campaign.snapshot()->content;const auto& definition=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
            content::FoundingSelection f;f.family="rich";f.leg="political";f.route="hired_ceo";f.office="shared";f.researcher="none";
            for(const auto& scene:definition.founding->scenes)f.choices[scene.id]=scene.choices.front().id;
            runtime::ScenarioOptions options;options.founding=f;options.seed=719;options.identity=runtime::CampaignIdentity{"Family Chapters","Alex Vale","they/them"};
            auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);initial.world.enabled_flows=std::vector<std::string>{};
            runtime::Runtime fixture{program,initial};const auto result=fixture.prepare_opening(initial.world.simulation.revision);if(!result.accepted())throw std::runtime_error(result.explanation);initial=fixture.current_state();
            if(!initial.world.family)throw std::runtime_error("Missing authored family rules");initial.world.family->stream=1;
            // Only the repeatable consent stream is seeded. Families, named
            // parents, choices, arrivals and recommendations use real systems.
            persistence::save_file(program,initial,run->save);campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdString(run->save.string())));break;}
        case 1:if(item("menuResumeButton")){press("menuResumeButton");return;}break;
        case 2:if(auto* close=item("birthKeepsakeClose")){close->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Space);}if(!press("personalLifeTab")){fail("Open My life");return;}break;
        case 3:if(!press("personalPage_album")){fail("Family album tab");return;}break;
        case 4:reveal(item("familyPlan_adopt"));run->frames=0;break;
        case 5:if(run->frames++<4)return;if(!capture("planner") || !press("familyPlanReview")){fail("Reviewed adoption choice");return;}break;
        case 6:if(!press("familyConfirm")){fail("Actual family conversation");return;}run->cash=w.simulation.cash;break;
        case 7:if(w.family->pending.size()!=1 || w.family->members.size()!=2 || w.simulation.cash!=run->cash){fail("An agreed plan saves a real waiting period without invented company charges");return;}campaign.saveTo(QUrl::fromLocalFile(QString::fromStdString(run->save.string())));break;
        case 8:if(persistence::load_file(*campaign.snapshot()->content,run->save).checkpoint!=campaign.snapshot()->checkpoint){fail("Exact family export");return;}std::cout<<"Reviewed adoption and actual pending plan exported for process restart.\n";timer->stop();QCoreApplication::exit(0);return;
        case 9:if(item("menuResumeButton")){press("menuResumeButton");return;}if(!press("personalLifeTab")){fail("Reopen My life");return;}break;
        case 10:if(!item("familyPerson") && !press("personalPage_album")){fail("Reload family album");return;}break;
        case 11:if(!w.family->pending.empty()){if(++run->days>95 || !campaign.canAdvance()){fail("Actual adoption waiting period");return;}campaign.step();return;}if(w.family->members.size()!=3){fail("One actual new family member");return;}break;
        case 12:{auto* select=item("familyPerson");if(!select){fail("Family member selector");return;}select->forceActiveFocus();QTest::keyClick(&window,Qt::Key_End);run->frames=0;break;}
        case 13:reveal(item("familyMemberCard"));if(run->frames++<5)return;if(!capture("arrival") || !world->familyAlbum().value("members").toList().back().toMap().value("portrait").toString().contains("sprout")){fail("Age-appropriate childhood marker and actual arrival");return;}campaign.setTextScalePercent(200);window.showNormal();window.resize(650,480);run->frames=0;break;
        case 14:if(window.width()!=650 || window.height()!=480){window.showNormal();window.resize(650,480);run->frames=0;return;}reveal(item("familyMemberCard"));if(run->frames++<5)return;if(!capture("child-compact")){fail("Compact childhood card");return;}if(auto* select=item("familyPerson")){select->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Home);}else{fail("Adult family selector");return;}break;
        case 15:reveal(item("familyApplicationReview"));if(!press("familyApplicationReview")){fail("Invite an adult family application");return;}break;
        case 16:if(!press("familyConfirm")){fail("Reviewed application invitation");return;}break;
        case 17:if(w.simulation.management->people->queued_applicants.size()!=1){fail("Real waiting-list candidate");return;}run->applicant=w.simulation.management->people->queued_applicants.front().profile.id;if(!w.simulation.management->people->queued_applicants.front().recommendation){fail("Named recommendation source");return;}world->setDraft("people/page","applicants");world->setDraft("people/applicantFilter","recommended");world->navigate("workspace","hq","laptop","people","");break;
        case 18:if(!press("applicantsRecommended")){fail("Employee recommended candidates group");return;}run->frames=0;break;
        case 19: {
            if(window.width()!=650 || window.height()!=480){window.showNormal();window.resize(650,480);run->frames=0;return;}
            const auto name="employeeRecommendation_"+QString::fromStdString(run->applicant);
            auto* recommendation=item(name);
            reveal(recommendation);
            if(!on_screen(recommendation)) {
                if(run->frames++<30)return;
                fail(recommendation?"Compact recommendation is clipped":"Compact recommendation label is missing");
                return;
            }
            if(!capture("recommended-compact")){fail("Compact recommendation capture");return;}
            campaign.setTextScalePercent(100);window.resize(1280,800);run->frames=0;break;
        }
        case 20:reveal(item("employeeRecommendation_"+QString::fromStdString(run->applicant)));if(run->frames++<5)return;if(!on_screen(item("employeeRecommendation_"+QString::fromStdString(run->applicant))) || !capture("recommended")){fail("Recommendation source capture");return;}if(!run->resumed){fail("Missing real process restart");return;}std::cout<<"Family UI passed: seeded consent stream only; real adoption choice, exact pending export, process restart, actual 90-day arrival, childhood marker, compact album, voluntary ordinary application queue and Employee recommended candidates with named provenance.\n";timer->stop();QCoreApplication::exit(0);return;
        }++run->phase;}catch(const std::exception& e){fail(e.what());}
    });timer->start();
}
