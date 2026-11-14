#include "social_smoke.hpp"
#include "campaign_controller.hpp"
#include "founding_fixture.hpp"
#include <pgp/persistence/save.hpp>
#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QTest>
#include <iostream>
#include <algorithm>
namespace {QQuickItem* find(QQuickItem* root,const QString& id){if(!root->isVisible())return nullptr;if(root->objectName()==id)return root;for(auto* child:root->childItems())if(auto* value=find(child,id))return value;return nullptr;}}
void schedule_social_smoke(QQuickWindow& window,pgp::bridge::CampaignController& campaign,pgp::persistence::LocalPaths,QString screenshot) {
    struct Run {int phase=0,ticks=0,frames=0;pgp::core::OrdinalDay study_until{0};std::string person,industry,student,course;pgp::core::Cents company_cash{0},personal_cash{0};pgp::bridge::CampaignSnapshotPtr preview;};auto run=std::make_shared<Run>();auto* timer=new QTimer(&window);timer->setInterval(100);
    QObject::connect(timer,&QTimer::timeout,&window,[&,run,timer,screenshot]{
        const auto fail=[&](const char* why){std::cerr<<"Social UI "<<run->phase<<": "<<why<<" · "<<campaign.notice().toStdString()<<'\n';if(!screenshot.isEmpty())window.grabWindow().save(screenshot+".failure.png");timer->stop();QCoreApplication::exit(3);};
        if(++run->ticks>500){fail("Timeout");return;}if(!window.isSceneGraphInitialized() || !campaign.ready() || campaign.busy())return;
        const auto item=[&](const QString& name){return find(window.contentItem(),name);};
        const auto press=[&](const QString& name){auto* c=item(name);if(!c || !c->isEnabled())return false;c->forceActiveFocus(Qt::TabFocusReason);QTest::keyClick(&window,Qt::Key_Space);return true;};
        const auto reveal=[&](QQuickItem* target){if(!target)return;for(auto* p=target->parentItem();p;p=p->parentItem())if(p->property("contentY").isValid()){const auto y=target->mapToItem(p,QPointF(0,0)).y()+p->property("contentY").toDouble();p->setProperty("contentY",std::clamp(y,0.0,std::max(0.0,p->property("contentHeight").toDouble()-p->height())));break;}};
        const auto capture=[&](const char* suffix){return screenshot.isEmpty() || window.grabWindow().save(screenshot+"."+suffix+".png");};
        auto* world=campaign.world();const auto& s=campaign.snapshot()->checkpoint.world.simulation;
        switch(run->phase) {
        case 0:{if(!press("menuNewGameButton")){fail("New game");return;}auto* f=campaign.founding();f->setField("family","rich");f->setAdvantage("genius",true);f->setField("route","hired_ceo");pgp::bridge::complete_formative_fixture(*f);campaign.startFounderCampaign();break;}
        case 1:if(auto* c=item("birthKeepsakeClose")){c->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Space);}world->navigate("workspace","hq","laptop","people","");break;
        case 2:if(!press("peopleSocialTab")){fail("Social circle tab");return;}break;
        case 3:{const auto people=world->relationships().value("people").toList();if(people.size()<2){fail("HQ people");return;}run->person=people.front().toMap().value("id").toString().toStdString();if(!press("socialChat")){fail("Take a break");return;}run->frames=0;break;}
        case 4:{const auto* bond=pgp::core::social_link(s,"founder",run->person);if(!bond || bond->affinity<=0 || item("socialChat")->isEnabled()){fail("Actual relationship and daily limit");return;}reveal(item("socialCard"));if(run->frames++<4)return;if(!capture("conversation")){fail("Conversation capture");return;}
            // Mature ties are seeded solely for graph rendering, not asserted as
            // results of this single encounter. Runtime tests cover progression.
            auto fixture=campaign.snapshot()->checkpoint;auto& social=*fixture.world.simulation.management->people->social;
            social.links.clear();std::vector<std::string> ids;for(const auto& e:s.management->people->employees)if(e.terms.profile.role!="Founder / researcher")ids.push_back(e.terms.profile.id);
            for(const auto& id:ids)social.links.push_back({id,"founder",900,60,s.date,s.date,{},"Mature friendship preview fixture",true});
            social.links.push_back({ids[0],ids[1],700,50,s.date,s.date,{},"Shared close friendship preview fixture",true});
            pgp::runtime::validate_checkpoint(*campaign.snapshot()->content,fixture);
            auto preview=std::make_shared<pgp::bridge::CampaignSnapshot>(*campaign.snapshot());preview->checkpoint=std::move(fixture);
            run->preview=preview;world->update(preview,*preview->content,false);break;}
        case 5:world->navigate("workspace","hq","laptop","people","");break;
        case 6:if(item("peopleSocialTab"))press("peopleSocialTab");if(!press("socialNetwork")){fail("Open graph");return;}run->frames=0;break;
        case 7:reveal(item("socialGraph"));if(run->frames++<5)return;if(!item("socialGraph") || world->relationships().value("edges").toList().size()<3 || !capture("graph")){fail("Known close graph");return;}if(!press("socialConnection_0")){fail("Explore a friend’s connections");return;}run->frames=0;break;
        case 8:if(run->frames++<3)return;if(!press("socialGraphHome")){fail("Return to own connections");return;}campaign.setTextScalePercent(200);window.showNormal();window.resize(650,480);run->frames=0;break;
        case 9:if(run->frames==0)world->update(run->preview,*run->preview->content,false);if(run->frames++<5)return;if(!capture("list-compact") || !press("socialPeople")){fail("Compact accessible list");return;}run->frames=0;break;
        case 10:reveal(item("socialCard"));if(run->frames++<5)return;if(!capture("person-compact")){fail("Compact person");return;}campaign.setTextScalePercent(100);window.resize(1280,800);break;
        case 11:if(!press("peopleBackToTeam")){fail("Return to team");return;}break;
        case 12:if(!press("peopleIndustryTab")){fail("Industry people tab");return;}run->frames=0;break;
        case 13:reveal(item("industryCompany"));if(run->frames++<5)return;if(world->industryPeople().size()<2 || !item("industryLocation") || !capture("industry")){fail("Major HQ roster");return;}campaign.setTextScalePercent(200);window.resize(650,480);run->frames=0;break;
        case 14:reveal(item("industryCompany"));if(run->frames++<5)return;if(!capture("industry-compact")){fail("Compact industry roster");return;}if(auto* selector=item("industryCompany")){selector->forceActiveFocus();QTest::keyClick(&window,Qt::Key_End);}else {fail("Select overseas HQ");return;}run->frames=0;break;
        case 15:if(run->frames++<4)return;{
            const auto overseas=world->industryPeople().back().toMap();const auto contact=overseas.value("people").toList().front().toMap().value("id").toString();
            if(overseas.value("state").toString().startsWith("US-") || !item("industryLocation")->property("text").toString().contains(overseas.value("stateName").toString()) || !item("industryHello_"+contact) || item("industryHello_"+contact)->isEnabled() || !capture("overseas-compact")){fail("Overseas public roster and distant-contact limit");return;}
            auto* selector=item("industryCompany");selector->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Home);break;
        }
        case 16:campaign.step();break;
        case 17:{const auto firms=world->industryPeople();run->industry=firms.front().toMap().value("people").toList().front().toMap().value("id").toString().toStdString();if(!press("industryHello_"+QString::fromStdString(run->industry))){fail("Say hello to a regional colleague");return;}break;}
        case 18:if(!pgp::core::social_link(s,"founder",run->industry)){fail("Actual regional acquaintance");return;}if(!press("peopleBackToTeam")){fail("Return from industry roster");return;}break;
        case 19:if(!press("peopleSocialTab")){fail("Reopen social circle");return;}break;
        case 20:{auto* selector=item("socialPerson");if(!selector){fail("Select industry contact");return;}selector->forceActiveFocus();QTest::keyClick(&window,Qt::Key_End);run->frames=0;break;}
        case 21:reveal(item("socialCard"));if(run->frames++<4)return;if(!capture("regional-contact")){fail("Regional contact card");return;}campaign.setTextScalePercent(100);window.resize(1280,800);if(!press("peopleBackToTeam")){fail("Return for employee study");return;}break;
        case 22:if(!press("peopleUniversityTab")){fail("Employee university");return;}break;
        case 23:{
            const auto university=world->talent();const auto students=university.value("people").toList(),skills=university.value("skills").toList();
            bool selected=false;for(qsizetype i=1;i<students.size() && !selected;++i)for(qsizetype j=0;j<skills.size() && !selected;++j) {
                const auto student=students[i].toMap().value("id").toString(),course=skills[j].toMap().value("id").toString();
                const auto definition=std::ranges::find(pgp::core::talent_rules(s)->skills,course.toStdString(),&pgp::core::SkillDefinition::id);
                if(!definition->teachable || definition->tuition_cents<=0 || (!definition->engineering && !definition->research) || !definition->prerequisite.empty() || pgp::core::person_has_skill(s,student.toStdString(),course.toStdString()))continue;
                run->student=student.toStdString();run->course=course.toStdString();const auto select=[&](const char* name,qsizetype index){auto* control=item(name);control->forceActiveFocus();QTest::keyClick(&window,Qt::Key_Home);for(qsizetype step=0;step<index;++step)QTest::keyClick(&window,Qt::Key_Down);};select("universityStudent",i);select("universityCourse",j);selected=true;
            }
            if(!selected){fail("Select professional self-study");return;}run->frames=0;break;}
        case 24:reveal(item("universityStudySavings"));if(run->frames++<4)return;if(!item("universityStudySavings") || item("universityPersonal")->isEnabled() || !capture("learning-jar-saving")){fail("Learning jar saving and refusal");return;}run->study_until=pgp::core::advance_days(s.date,pgp::core::Days{32});break;
        case 25:
            if(campaign.requiredKind()=="content") {const auto choices=campaign.requiredChoices();QString choice;for(const auto& row:choices){const auto id=row.toMap().value("choiceId").toString();if(choice.isEmpty() || id.contains("decline",Qt::CaseInsensitive))choice=id;}if(choice.isEmpty()){fail("Required choice while waiting for payday");return;}campaign.choose(choice);return;}
            if(s.date<run->study_until){if(!campaign.canAdvance()){fail("Unexpected block before payday");return;}campaign.step();return;}run->frames=0;break;
        case 26:reveal(item("universityPersonal"));if(run->frames++<4)return;if(!press("universityPersonal")){fail("Ask employee to self-fund");return;}run->company_cash=s.cash;run->personal_cash=s.succession->personal_cash;break;
        case 27:if(!press("universityConfirm")){fail("Confirm voluntary employee tuition");return;}break;
        case 28:if(s.cash!=run->company_cash || s.succession->personal_cash!=run->personal_cash || !std::ranges::any_of(s.management->people->talent->courses,[&](const auto& c){return c.person==run->student && c.skill==run->course && c.payer=="personal";})){fail("Employee tuition separation and course");return;}campaign.setTextScalePercent(200);window.resize(650,480);run->frames=0;break;
        case 29:reveal(item("universityStudySavings"));if(run->frames++<5)return;if(!capture("learning-jar-compact")){fail("Compact learning jar");return;}reveal(item("universitySelfStudyStatus"));run->frames=0;break;
        case 30:if(run->frames++<4)return;if(!capture("self-study-compact")){fail("Compact self-study controls");return;}std::cout<<"Social UI passed: actual meetup, regional greeting, overseas rosters, employee-funded tuition from paid wages and separate company/founder cash; compact 200% layouts. Mature friendship graph uses an explicit preview fixture.\n";timer->stop();QCoreApplication::exit(0);return;

        }++run->phase;
    });timer->start();
}
