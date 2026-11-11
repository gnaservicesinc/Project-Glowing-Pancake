#include "mission_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/core/workplace_research.hpp>
#include <pgp/persistence/save.hpp>
#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <algorithm>
#include <iostream>

namespace {
QQuickItem* visible_item(QQuickItem* root,const QString& name) {
    if(!root->isVisible())return nullptr;
    if(root->objectName()==name)return root;
    for(auto* child:root->childItems())if(auto* found=visible_item(child,name))return found;
    return nullptr;
}
}

void schedule_mission_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct Walkthrough {int step=0,ticks=0,frames=0;std::string employee;};
    auto run=std::make_shared<Walkthrough>();
    auto* timer=new QTimer(&window);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,screenshot,paths]{
        const auto fail=[&](const std::string& error){
            std::cerr<<"Mission UI "<<run->step<<": "<<error<<" · "<<campaign.notice().toStdString()<<'\n';
            if(!screenshot.isEmpty())window.grabWindow().save(screenshot+".failure.png");
            timer->stop();
            QCoreApplication::exit(3);
        };
        if(++run->ticks>300){fail("Timed out");return;}
        if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())return;
        const auto press=[&](const QString& name){
            auto* button=visible_item(window.contentItem(),name);
            if(!button || !button->isEnabled())return false;
            button->forceActiveFocus(Qt::TabFocusReason);
            QTest::keyClick(&window,Qt::Key_Space);
            return true;
        };
        const auto capture=[&](const char* suffix){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+suffix+".png");};
        auto* world=campaign.world();
        try {
            switch(run->step) {
            case 0: {
                if(campaign.currentCampaign()){fail("Use a fresh portable root");return;}
                const auto& program=*campaign.snapshot()->content;
                const auto& definition=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
                content::FoundingSelection founder;
                founder.family="rich";founder.leg="political";founder.route="hired_ceo";founder.office="shared";founder.researcher="none";
                for(const auto& scene:definition.founding->scenes)founder.choices[scene.id]=scene.choices.front().id;
                runtime::ScenarioOptions options;
                options.founding=founder;options.seed=719;options.identity=runtime::CampaignIdentity{"Mission Review","Alex Vale","they/them"};
                auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
                initial.world.enabled_flows=std::vector<std::string>{};
                auto& s=initial.world.simulation;
                auto& people=*s.management->people;
                if(people.employees.size()<2)throw std::runtime_error("Two colleagues required");
                auto& worker=people.employees.front().terms.profile;
                run->employee=worker.id;
                worker.preferred_mission=std::string{core::committed_company_mission(s)};
                if(!core::has_skill(worker,"ml_research"))worker.skills.push_back("ml_research");
                auto& mediator=people.employees[1].terms.profile;
                if(!core::has_skill(mediator,"empathy"))mediator.skills.push_back("empathy");
                runtime::Runtime fixture{program,initial};
                if(!fixture.prepare_opening(s.revision).accepted())throw std::runtime_error("Prepare opening");
                const auto state=[&]() -> const auto& {return fixture.current_state().world.simulation;};
                const auto apply=[&](core::CommandAction action){
                    const auto result=fixture.submit({state().revision,std::move(action)});
                    if(!result.accepted())throw std::runtime_error(result.explanation);
                };
                if(!core::hq_workstation_ready(state(),run->employee)) {
                    apply(core::ManageHeadquarters{core::BuyHQEquipment{"laptop"}});
                    const auto laptop=state().infrastructure->workplace->headquarters->items.back().id;
                    apply(core::AdvanceDay{});
                    apply(core::AdvanceDay{});
                    apply(core::ManageHeadquarters{core::AssignHQLaptop{laptop,run->employee}});
                }
                const auto& sources=state().capacity->sources;
                const auto source=std::ranges::find_if(sources,[](const auto& row){return core::company_capacity(row)>=core::MilliCUPerDay{4000};});
                if(source==sources.end())throw std::runtime_error("No powered compute");
                apply(core::StartResearch{"research.reproducible_tooling",{source->id,core::MilliCUPerDay{4000},1,{run->employee}}});
                while(state().date<core::advance_days(s.date,core::Days{60}))apply(core::AdvanceDay{});
                const auto save=paths.saves_directory/"mission-ui.pgpsave";
                persistence::save_file(program,fixture.current_state(),save);
                campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdString(save.string())));
                break;
            }
            case 1:
                if(visible_item(window.contentItem(),"menuResumeButton")){press("menuResumeButton");return;}
                break;
            case 2:
                if(auto* close=visible_item(window.contentItem(),"birthKeepsakeClose")){
                    close->forceActiveFocus();
                    QTest::keyClick(&window,Qt::Key_Space);
                }
                world->setDraft("people/page","social");
                world->setDraft("social/person",QString::fromStdString(run->employee));
                world->navigate("workspace","hq","laptop","people","");
                break;
            case 3:
                campaign.setTextScalePercent(200);
                window.showNormal();
                window.resize(650,480);
                break;
            case 4:
                if(window.width()!=650 || window.height()!=480){window.showNormal();window.resize(650,480);return;}
                if(!press("reviewCompanyMission")){fail("Mission control");return;}
                break;
            case 5:
                if(!capture("review") || !press("confirmCompanyMission")){fail("Mission confirmation");return;}
                break;
            case 6: {
                if(window.width()!=650 || window.height()!=480){window.showNormal();window.resize(650,480);return;}
                const auto& s=campaign.snapshot()->checkpoint.world.simulation;
                const auto* concern=core::mission_concern(s,run->employee);
                if(!concern || concern->rivalry_on){fail("Expected an open mission concern");return;}
                if(!press("mediateMissionConcern")){fail("Mediator control");return;}
                break;
            }
            case 7: {
                if(window.width()!=650 || window.height()!=480){window.showNormal();window.resize(650,480);return;}
                const auto& s=campaign.snapshot()->checkpoint.world.simulation;
                const auto* concern=core::mission_concern(s,run->employee);
                if(!concern || !concern->settled_on || concern->mediator.empty()){fail("Mediation did not settle the concern");return;}
                auto* relationship=visible_item(window.contentItem(),"socialRelationship");
                if(!relationship){fail("Relationship card after mediation");return;}
                for(auto* ancestor=relationship->parentItem();ancestor;ancestor=ancestor->parentItem()) {
                    if(!ancestor->property("contentY").isValid())continue;
                    const auto top=relationship->mapToItem(ancestor,QPointF(0,0)).y()+ancestor->property("contentY").toDouble();
                    ancestor->setProperty("contentY",std::max(0.0,top));
                    break;
                }
                if(++run->frames<3)return;
                if(!capture("mediated")){fail("Capture");return;}
                std::cout<<"Mission review and HR mediation passed at 650x480 with 200% text.\n";
                timer->stop();
                QCoreApplication::exit(0);
                return;
            }
            }
            ++run->step;
        }catch(const std::exception& error){fail(error.what());}
    });
    timer->start(100);
}
