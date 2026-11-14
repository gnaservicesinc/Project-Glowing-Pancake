#include <pgp/core/workplace_research.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pgp;
void check(bool passed,const char* message) {
    if(!passed)throw std::runtime_error(message);
}
const auto& state(const runtime::Runtime& game){return game.current_state().world.simulation;}
void apply(runtime::Runtime& game,core::CommandAction action) {
    const auto result=game.submit({state(game).revision,std::move(action)});
    if(!result.accepted())throw std::runtime_error(result.explanation);
}
void advance(runtime::Runtime& game,int days) {
    for(int day=0;day<days;++day)apply(game,core::AdvanceDay{});
}
}

int main() {
    using namespace pgp;
    try {
        auto document=nlohmann::json::parse(persistence::read_bounded(PGP_MISSION_SOURCE,content::maximum_document_bytes));
        for(auto& row:document["definitions"])if(row["id"]=="scenario.founder_campaign"){
            row["competition"]["domestic"]=2;
            row["competition"]["international"]=0;
            row["competition"]["heavyweights"]=0;
        }
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(document.dump(),registry),registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection founder;
        founder.family="rich";founder.advantages={"political"};founder.route="hired_ceo";founder.office="shared";founder.researcher="none";
        for(const auto& scene:scenario.founding->scenes)founder.choices[scene.id]=scene.choices.front().id;
        runtime::ScenarioOptions options;
        options.founding=founder;
        options.seed=719;
        options.identity=runtime::CampaignIdentity{"Mission Review","Alex Vale","they/them"};
        auto opening=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        opening.world.enabled_flows=std::vector<std::string>{"life.mission.concern","life.mission.rival","life.mission.resignation"};
        runtime::Runtime game{program,opening};
        check(game.prepare_opening(state(game).revision).accepted(),"Prepare founder opening");
        advance(game,60);

        auto fixture=game.current_state();
        auto& simulation=fixture.world.simulation;
        auto& people=*simulation.management->people;
        check(people.employees.size()>=2,"Two inherited colleagues");
        const auto worker=people.employees[0].terms.profile.id;
        const auto mediator=people.employees[1].terms.profile.id;
        const auto initial_mission=std::string{core::committed_company_mission(simulation)};
        const auto changed_mission=initial_mission=="commercial"?"research":"commercial";

        // Stable employee identities, equipment, and tenure come from the
        // opening. This fixture sets personal investment and a prior friendship.
        auto& profile=people.employees[0].terms.profile;
        profile.preferred_mission=initial_mission;
        if(!core::has_skill(profile,"ml_research"))profile.skills.push_back("ml_research");
        if(!core::has_skill(people.employees[1].terms.profile,"empathy"))people.employees[1].terms.profile.skills.push_back("empathy");
        if(!core::person_has_skill(simulation,"founder","empathy"))people.talent->founder_skills.push_back("empathy");
        auto link=std::ranges::find_if(people.social->links,[&](const auto& bond){
            return (bond.first==worker && bond.second=="founder") || (bond.second==worker && bond.first=="founder");
        });
        if(link!=people.social->links.end())link->affinity=720;
        else {
            people.social->links.push_back({std::min(std::string{"founder"},worker),std::max(std::string{"founder"},worker),
                720,25,simulation.date,simulation.date,{},"Long-time colleague",true});
        }
        game=runtime::Runtime{program,fixture};
        apply(game,core::SetRecruitingTerms{state(game).management->people->offered_pay,changed_mission});
        check(core::committed_company_mission(state(game))==initial_mission,"Recruiting terms preserve the public commitment");
        check(core::mission_pivot_concerns(state(game),changed_mission).empty(),"Preference alone does not create a grievance");

        if(!core::hq_workstation_ready(state(game),worker)) {
            apply(game,core::ManageHeadquarters{core::BuyHQEquipment{"laptop"}});
            const auto laptop=state(game).infrastructure->workplace->headquarters->items.back().id;
            advance(game,2);
            apply(game,core::ManageHeadquarters{core::AssignHQLaptop{laptop,worker}});
        }
        const auto& sources=state(game).capacity->sources;
        const auto source=std::ranges::find_if(sources,[](const auto& item){return core::company_capacity(item)>=core::MilliCUPerDay{4000};});
        check(source!=sources.end(),"Powered compute is available");
        apply(game,core::StartResearch{"research.reproducible_tooling",{source->id,core::MilliCUPerDay{4000},1,{worker}}});
        check(core::mission_pivot_concerns(state(game),changed_mission).empty(),"Assignments must actually do work");
        advance(game,1);
        check(core::mission_pivot_concerns(state(game),changed_mission)==std::vector<std::string>{worker},"Preview names the employee who did the work");
        apply(game,core::ChangeCompanyMission{changed_mission});
        check(core::mission_concern(state(game),worker) && !core::is_personal_rival(state(game),"founder",worker),"Mission pivot opens a response window");
        auto checkpoint=persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint;
        check(checkpoint==game.current_state(),"Pending concern saves exactly");

        runtime::Runtime resolved{program,checkpoint};
        apply(resolved,core::RespondToMissionConcern{worker,mediator});
        advance(resolved,15);
        const auto* mediated=core::mission_concern(state(resolved),worker);
        check(mediated->settled_on && !mediated->rivalry_on && mediated->mediator==mediator,"HR mediation prevents rivalry");

        runtime::Runtime departure{program,checkpoint};
        advance(departure,14);
        check(core::is_personal_rival(state(departure),"founder",worker),"Ignored concern becomes rivalry");
        check(core::visible_traits(state(departure),profile).empty(),"Rivals hide their traits");
        check(!departure.submit({state(departure).revision,core::Socialize{worker,"hobby"}}).accepted(),"Ordinary outings cannot skip repair");
        auto without_empathy=departure.current_state();
        std::erase(without_empathy.world.simulation.management->people->talent->founder_skills,"empathy");
        runtime::Runtime unprepared{program,without_empathy};
        check(!unprepared.submit({state(unprepared).revision,core::RespondToMissionConcern{worker,{}}}).accepted(),
            "Prior friendship alone cannot end a rivalry");
        advance(departure,14);
        const auto& left=state(departure).management->people->employees.front();
        check(left.resigned && left.departed_on && !left.dismissal_severance,"Unresolved rival resigns");
        check(core::assigned_hq_desk(state(departure),worker)==0,"Resignation releases the desk");
        check(persistence::decode_save(program,persistence::encode_save(program,departure.current_state())).checkpoint==departure.current_state(),"Resignation and payroll save exactly");

        advance(game,14);
        apply(game,core::RespondToMissionConcern{worker,{}});
        check(core::mission_concern(state(game),worker)->heard_on && core::is_personal_rival(state(game),"founder",worker),"First conversation opens a repair window");
        advance(game,7);
        apply(game,core::RespondToMissionConcern{worker,{}});
        check(!core::is_personal_rival(state(game),"founder",worker) && !core::mission_concern(state(game),worker)->frenemy,"Reconciliation leaves friendship to rebuild");
        for(int week=0;week<18 && !core::mission_concern(state(game),worker)->frenemy;++week) {
            advance(game,7);
            apply(game,core::Socialize{worker,"listen"});
        }
        advance(game,1);
        check(core::mission_concern(state(game),worker)->frenemy,"Earning friendship back establishes frenemies");
        check(!state(game).management->people->employees.front().departed_on,"Repair keeps both people working together");
        apply(game,core::ChangeCompanyMission{initial_mission});
        advance(game,60);
        check(core::mission_pivot_concerns(state(game),changed_mission).empty(),"Frenemies are immune to another rivalry");
        apply(game,core::ChangeCompanyMission{changed_mission});
        check(core::mission_concern(state(game),worker)->frenemy,"A second reversal retains the frenemy history");
        check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Repaired relationship saves exactly");
        std::cout<<"Mission reversal, earned project commitment, HR, rivalry, trait privacy, resignation, empathy and frenemy immunity passed.\n";
    }catch(const std::exception& error){
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
