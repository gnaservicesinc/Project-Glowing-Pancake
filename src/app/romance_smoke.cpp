#include "romance_smoke.hpp"
#include "campaign_controller.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/romance.hpp>
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
void schedule_romance_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths paths,QString screenshot) {
    using namespace pgp;
    struct Run {int phase=0,ticks=0,frames=0,days=0;bool resumed=false;std::string contact;std::filesystem::path save;core::OrdinalDay began{0};};
    auto run=std::make_shared<Run>();run->save=paths.saves_directory/"romance-ui.pgpsave";auto* timer=new QTimer(&window);timer->setInterval(100);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,screenshot,paths]{
        const auto fail=[&](const std::string& why){std::cerr<<"Romance UI "<<run->phase<<": "<<why<<" · "<<campaign.notice().toStdString()<<'\n';if(!screenshot.isEmpty())window.grabWindow().save(screenshot+".failure.png");timer->stop();QCoreApplication::exit(3);};
        if(++run->ticks>600){fail("Timeout");return;}if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())return;
        const auto item=[&](const QString& name){return visible(window.contentItem(),name);};
        const auto press=[&](const QString& name){auto* c=item(name);if(!c || !c->isEnabled())return false;c->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,Qt::Key_Space);return true;};
        const auto reveal=[&](QQuickItem* target){if(!target)return;for(auto* p=target->parentItem();p;p=p->parentItem())if(p->property("contentY").isValid()){const auto y=target->mapToItem(p,QPointF(0,0)).y()+p->property("contentY").toDouble();p->setProperty("contentY",std::clamp(y,0.0,std::max(0.0,p->property("contentHeight").toDouble()-p->height())));break;}};
        const auto capture=[&](const char* suffix){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+suffix+".png");};
        auto* world=campaign.world();const auto& w=campaign.snapshot()->checkpoint.world;
        try {switch(run->phase) {
        case 0:{
            if(campaign.currentCampaign()) {
                if(campaign.companyName()!="Personal Chapters" || !runtime::romantic_partner(w,"founder")){fail("A different campaign is active; preserve it");return;}
                const auto* bond=runtime::romantic_partner(w,"founder");run->contact=bond->first=="founder"?bond->second:bond->first;run->began=bond->started_on;run->resumed=true;run->phase=12;return;
            }
            const auto& program=*campaign.snapshot()->content;const auto& definition=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
            content::FoundingSelection f;f.family="rich";f.advantages={"political"};f.route="hired_ceo";f.office="shared";f.researcher="none";f.gender="woman";f.orientation="bisexual";
            for(const auto& scene:definition.founding->scenes)f.choices[scene.id]=scene.choices.front().id;
            runtime::ScenarioOptions options;options.founding=f;options.seed=719;options.identity=runtime::CampaignIdentity{"Personal Chapters","Alex Vale","she/her"};
            auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);initial.world.enabled_flows=std::vector<std::string>{};
            runtime::Runtime fixture{program,initial};const auto result=fixture.prepare_opening(initial.world.simulation.revision);if(!result.accepted())throw std::runtime_error(result.explanation);initial=fixture.current_state();
            if(!initial.world.romance)throw std::runtime_error("Missing authored romance rules");
            for(const auto& [id,person]:initial.world.simulation.management->people->industry)if(person.profile.romantic && core::mutual_romantic_interest("woman",initial.world.romance->founder.preferences,person.profile.gender,person.profile.romantic->preferences,800)){run->contact=id;break;}
            if(run->contact.empty())throw std::runtime_error("No generated compatible contact");
            // Seed only friendship and a repeatable consent stream. Invitations,
            // dates, preferences and persistence go through actual game controls.
            auto bond=core::SocialLink{run->contact,"founder",800,60,initial.world.simulation.date,initial.world.simulation.date,{},"Mature friendship UI fixture",true};if(bond.first>bond.second)std::swap(bond.first,bond.second);initial.world.simulation.management->people->social->links.push_back(bond);initial.world.romance->stream=1;
            persistence::save_file(program,initial,run->save);campaign.loadFrom(QUrl::fromLocalFile(QString::fromStdString(run->save.string())));break;}
        case 1:if(item("menuResumeButton")){press("menuResumeButton");return;}world->navigate("workspace","hq","laptop","people","");break;
        case 2:if(auto* c=item("birthKeepsakeClose")){c->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Space);}if(!item("socialPerson") && !press("peopleSocialTab")){fail("Social circle");return;}break;
        case 3:{const auto people=world->relationships().value("people").toList();int index=-1;for(qsizetype i=0;i<people.size();++i)if(runtime::romance_person_id(w,people[i].toMap().value("id").toString().toStdString())==run->contact)index=static_cast<int>(i);auto* selector=item("socialPerson");if(!selector || index<0){fail("Contact selection");return;}selector->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Home);for(int i=0;i<index;++i)QTest::keyClick(&window,Qt::Key_Down);break;}
        case 4:if(!press("romanceToggle")){fail("Personal chapter");return;}run->frames=0;break;
        case 5:reveal(item("romanceBoard"));if(run->frames++<4)return;if(!capture("invitation") || !press("romance_ask")){fail("Invitation review");return;}break;
        case 6:if(!press("romanceConfirm")){fail("Send actual invitation");return;}break;
        case 7:if(!runtime::romantic_partner(w,"founder")){fail("Saved yes creates actual dating");return;}run->began=runtime::romantic_partner(w,"founder")->started_on;if(!press("romancePreferences")){fail("Open own preferences");return;}break;
        case 8:if(!item("romancePreference_nonbinary") || !press("romancePreference_nonbinary")){fail("Edit explicit attraction preference");return;}campaign.setTextScalePercent(200);window.showNormal();window.resize(650,480);run->frames=0;break;
        case 9:if(run->frames++<5)return;if(!capture("preferences-compact") || !press("romancePreferencesSave")){fail("Save preference choice");return;}break;
        case 10:if(!w.romance || w.romance->founder.preferences.genders.size()!=2){fail("Actual preference saved");return;}campaign.saveTo(QUrl::fromLocalFile(QString::fromStdString(run->save.string())));break;
        case 11:if(persistence::load_file(*campaign.snapshot()->content,run->save).checkpoint!=campaign.snapshot()->checkpoint){fail("Exact exported personal history");return;}std::cout<<"Invitation, actual saved answer and edited preferences exported for process restart.\n";timer->stop();QCoreApplication::exit(0);return;
        case 12:if(item("menuResumeButton")){press("menuResumeButton");return;}campaign.setTextScalePercent(200);window.showNormal();window.resize(650,480);world->navigate("workspace","hq","laptop","people","");break;
        case 13:if(!item("socialPerson") && !press("peopleSocialTab")){fail("Reload social circle");return;}break;
        case 14:{const auto people=world->relationships().value("people").toList();int index=-1;for(qsizetype i=0;i<people.size();++i)if(runtime::romance_person_id(w,people[i].toMap().value("id").toString().toStdString())==run->contact)index=static_cast<int>(i);auto* selector=item("socialPerson");if(!selector || index<0){fail("Reloaded partner");return;}selector->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Home);for(int i=0;i<index;++i)QTest::keyClick(&window,Qt::Key_Down);break;}
        case 15:if(!press("romanceToggle")){fail("Reopen personal chapter");return;}break;
        case 16:if(runtime::campaign_day(w)<core::advance_days(run->began,core::Days{7})){if(++run->days>8 || !campaign.canAdvance()){fail("Date calendar progression");return;}campaign.step();return;}run->frames=0;break;
        case 17:reveal(item("romance_listen"));if(run->frames++<4)return;if(!capture("dating-compact") || !press("romance_listen")){fail("Date after actual weekly cooldown");return;}break;
        case 18:if(!press("romanceConfirm")){fail("Confirm time together");return;}break;
        case 19:if(runtime::romantic_partner(w,"founder")->dates!=1){fail("Actual date earns closeness");return;}if(!press("socialNetwork")){fail("Relationship graph");return;}campaign.setTextScalePercent(100);window.resize(1280,800);run->frames=0;break;
        case 20:reveal(item("socialGraph"));if(run->frames++<5)return;if(!capture("known-map")){fail("Known romantic edge capture");return;}if(!run->resumed){fail("Missing process restart");return;}std::cout<<"Romance UI passed: explicit friendship/consent-stream fixture; reviewed invitation, actual yes, independent preferences, compact 200% controls, exact export, process restart, actual weekly date and known-relationship map.\n";timer->stop();QCoreApplication::exit(0);return;
        }++run->phase;}catch(const std::exception& e){fail(e.what());}
    });timer->start();
}
