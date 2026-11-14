#include "collaboration_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/core/workplace_research.hpp>
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
void schedule_collaboration_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct Run {int phase=0,ticks=0,frames=0,days=0;bool resumed=false,profile_opened=false,profile_checked=false;std::uint64_t inspection_revision=0;std::string contact,agreement;std::filesystem::path save;core::Cents cash{0};};
    auto run=std::make_shared<Run>();run->save=paths.saves_directory/"research-jam-ui.pgpsave";auto* timer=new QTimer(&window);timer->setInterval(100);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,screenshot,paths]{
        const auto fail=[&](const std::string& why){std::cerr<<"Research jam UI "<<run->phase<<": "<<why<<" · "<<campaign.notice().toStdString()<<'\n';if(!screenshot.isEmpty())window.grabWindow().save(screenshot+".failure.png");timer->stop();QCoreApplication::exit(3);};
        if(++run->ticks>850){fail("Timeout");return;}if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())return;
        const auto item=[&](const QString& name){return visible(window.contentItem(),name);};
        const auto press=[&](const QString& name){auto* c=item(name);if(!c || !c->isEnabled())return false;c->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,Qt::Key_Space);return true;};
        const auto reveal=[&](QQuickItem* target){if(!target)return;for(auto* p=target->parentItem();p;p=p->parentItem())if(p->property("contentY").isValid()){const auto y=target->mapToItem(p,QPointF(0,0)).y()+p->property("contentY").toDouble();p->setProperty("contentY",std::clamp(y,0.0,std::max(0.0,p->property("contentHeight").toDouble()-p->height())));break;}};
        const auto capture=[&](const char* suffix){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+suffix+".png");};
        auto* world=campaign.world();const auto& s=campaign.snapshot()->checkpoint.world.simulation;
        try {switch(run->phase) {
        case 0:{
            if(campaign.currentCampaign()) {
                if(campaign.companyName()!="Jam Session" || !campaign.snapshot()->checkpoint.world.competition || campaign.snapshot()->checkpoint.world.competition->collaborations.size()!=1){fail("A different campaign is active; preserve it");return;}
                const auto& agreement=campaign.snapshot()->checkpoint.world.competition->collaborations.front();run->contact=agreement.contact;run->agreement=agreement.id;run->resumed=true;run->phase=13;return;
            }
            const auto& program=*campaign.snapshot()->content;const auto& definition=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
            content::FoundingSelection f;f.family="rich";f.advantages={"political"};f.route="hired_ceo";f.office="shared";f.researcher="none";
            for(const auto& scene:definition.founding->scenes)f.choices[scene.id]=scene.choices.front().id;
            runtime::ScenarioOptions options;options.founding=f;options.seed=719;options.identity=runtime::CampaignIdentity{"Jam Session","Alex Vale","they/them"};
            auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);initial.world.enabled_flows=std::vector<std::string>{};
            runtime::Runtime fixture{program,initial};auto result=fixture.prepare_opening(initial.world.simulation.revision);if(!result.accepted())throw std::runtime_error(result.explanation);
            const auto act=[&](core::CommandAction action){const auto r=fixture.submit({fixture.current_state().world.simulation.revision,std::move(action)});if(!r.accepted())throw std::runtime_error(r.explanation);};
            // Buy and issue a real laptop if this opening's research-qualified
            // employee does not yet have a usable workstation.
            const auto& opening=fixture.current_state().world.simulation;
            if(std::ranges::none_of(core::research_workers(opening),[](const auto& worker){return worker.unavailable.empty();})) {
                std::string learner;for(const auto& e:opening.management->people->employees)if(core::person_has_skill(opening,e.terms.profile.id,"ml_research")){learner=e.terms.profile.id;break;}
                if(learner.empty())throw std::runtime_error("No opening qualified researcher");
                act(core::ManageHeadquarters{core::BuyHQEquipment{"laptop"}});const auto laptop=fixture.current_state().world.simulation.infrastructure->workplace->headquarters->items.back().id;
                act(core::AdvanceDay{});act(core::AdvanceDay{});act(core::ManageHeadquarters{core::AssignHQLaptop{laptop,learner}});
            }
            initial=fixture.current_state();
            std::optional<runtime::Checkpoint> selected;
            for(const auto& rival:initial.world.competition->companies) {
                if(!rival.simulation.management || !rival.simulation.management->people)continue;
                for(const auto& worker:core::research_workers(rival.simulation))if(worker.unavailable.empty()) {
                auto trial=initial;auto& network=*trial.world.simulation.management->people->social;const auto contact=rival.definition_id+"/"+worker.id;
                auto link=core::SocialLink{contact,"founder",700,50,trial.world.simulation.date,trial.world.simulation.date,{},"Mature professional friendship UI fixture",true};if(link.first>link.second)std::swap(link.first,link.second);network.links.push_back(link);
                if(runtime::collaboration_quote(trial.world,program,trial.world.company_id,{contact,"research.reproducible_tooling",{},{},core::MilliCUPerDay{4000}}).blocker.empty()){selected=std::move(trial);run->contact=contact;break;}
                }
                if(selected)break;
            }
            if(selected)initial=std::move(*selected);
            if(run->contact.empty())throw std::runtime_error("No feasible mature-friendship research fixture");
            // Only friendship is seeded. The pact, payments, work, publication
            // and reload below go through the real game and persistence paths.
            persistence::save_file(program,initial,run->save);campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdString(run->save.string())));break;}
        case 1:if(item("menuResumeButton")){if(!press("menuResumeButton")){fail("Resume loaded fixture");return;}return;}if(campaign.companyName()!="Jam Session"){fail("Loaded actual fixture");return;}world->navigate("workspace","hq","laptop","people","");break;
        case 2:if(auto* close=item("birthKeepsakeClose")){close->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Space);}if(!press("peopleSocialTab")){fail("Social circle");return;}break;
        case 3:{const auto people=world->relationships().value("people").toList();int index=-1;for(qsizetype i=0;i<people.size();++i)if(people[i].toMap().value("id").toString().toStdString()==run->contact)index=static_cast<int>(i);auto* selector=item("socialPerson");if(!selector || index<0){fail("Research friend selection");return;}selector->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Home);for(int i=0;i<index;++i)QTest::keyClick(&window,Qt::Key_Down);break;}
        case 4:if(!press("researchJamToggle")){fail("Open planning board");return;}run->frames=0;break;
        case 5:
            reveal(item("researchJamBoard"));if(run->frames++<4)return;
            if(!run->profile_checked) {
                if(!run->profile_opened){run->inspection_revision=s.revision;if(!press("researchJamPartner")){fail("Inspect partner portrait");return;}run->profile_opened=true;return;}
                QObject* dialog=nullptr;for(auto* candidate:window.findChildren<QObject*>("employeeProfileDialog"))if(candidate->property("visible").toBool())dialog=candidate;
                if(!dialog || dialog->property("person").toMap().value("id").toString().toStdString()!=run->contact || s.revision!=run->inspection_revision){fail("Read-only partner profile");return;}
                QMetaObject::invokeMethod(dialog,"close");run->profile_checked=true;return;
            }
            if(!capture("planner") || !item("researchJamReview") || !item("researchJamReview")->isEnabled()){fail("Feasible live planning board");return;}if(!press("researchJamReview")){fail("Review pact");return;}run->cash=s.cash;break;
        case 6:if(!press("researchJamConfirm")){fail("Sign actual joint pact");return;}break;
        case 7:if(campaign.snapshot()->checkpoint.world.competition->collaborations.size()!=1 || s.cash>=run->cash){fail("Actual agreement and paid company cash");return;}run->agreement=campaign.snapshot()->checkpoint.world.competition->collaborations.front().id;campaign.setTextScalePercent(200);window.showNormal();window.resize(650,480);run->frames=0;break;
        case 8:reveal(item("researchJamTopic"));if(run->frames++<5)return;if(!capture("choices-compact")){fail("Compact choices");return;}reveal(item("researchJamQuote"));run->frames=0;break;
        case 9:if(run->frames++<4)return;if(!capture("quote-compact")){fail("Compact pact explanation");return;}campaign.step();break;
        case 10:campaign.step();break;
        case 11:campaign.saveTo(QUrl::fromLocalFile(QString::fromStdString(run->save.string())));break;
        case 12:if(persistence::load_file(*campaign.snapshot()->content,run->save).checkpoint!=campaign.snapshot()->checkpoint){fail("Exported pact checkpoint differs");return;}std::cout<<"Paid joint research and exact exported checkpoint retained for actual process restart.\n";timer->stop();QCoreApplication::exit(0);return;
        case 13:if(item("menuResumeButton")){if(!press("menuResumeButton")){fail("Resume reloaded pact");return;}return;}if(campaign.snapshot()->checkpoint.world.competition->collaborations.size()!=1){fail("Saved agreement reload");return;}campaign.setTextScalePercent(200);window.showNormal();window.resize(650,480);world->navigate("workspace","hq","laptop","people","");break;
        case 14:if(!item("socialPerson") && !press("peopleSocialTab")){fail("Reopen social circle");return;}break;
        case 15:{const auto people=world->relationships().value("people").toList();int index=-1;for(qsizetype i=0;i<people.size();++i)if(people[i].toMap().value("id").toString().toStdString()==run->contact)index=static_cast<int>(i);auto* selector=item("socialPerson");if(!selector || index<0){fail("Reloaded friend");return;}selector->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Home);for(int i=0;i<index;++i)QTest::keyClick(&window,Qt::Key_Down);break;}
        case 16:if(runtime::collaboration_status(campaign.snapshot()->checkpoint.world,campaign.snapshot()->checkpoint.world.competition->collaborations.front())!="completed"){if(++run->days>30){fail("Joint work completion");return;}if(!campaign.canAdvance()){fail("Unexpected work block");return;}campaign.step();return;}run->frames=0;break;
        case 17:reveal(item("researchJamStatus_"+QString::fromStdString(run->agreement)));if(run->frames++<4)return;if(!capture("complete-compact") || !press("researchJamPublish_"+QString::fromStdString(run->agreement))){fail("Actual completed result and joint publication review");return;}break;
        case 18:if(!press("researchJamPublishConfirm")){fail("Publish joint result");return;}break;
        case 19:if(!campaign.snapshot()->checkpoint.world.competition->collaborations.front().published_on){fail("Actual joint publication");return;}campaign.setTextScalePercent(100);window.resize(1280,800);run->frames=0;break;
        case 20:reveal(item("researchJamStatus_"+QString::fromStdString(run->agreement)));if(run->frames++<5)return;if(!capture("published")){fail("Publication capture");return;}if(!run->resumed){fail("Missing actual process restart");return;}std::cout<<"Research jam UI passed: explicit mature-friendship fixture; live planning and profile inspection, actual bilateral paid work, compact choices, exact export, actual process restart and joint publication.\n";timer->stop();QCoreApplication::exit(0);return;
        }++run->phase;}catch(const std::exception& e){fail(e.what());}
    });timer->start();
}
