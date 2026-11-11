#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/core/workplace_research.hpp>
#include "../../src/runtime/competition_internal.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
int main(){using namespace pgp;try {
    auto doc=nlohmann::json::parse(persistence::read_bounded(PGP_INDUSTRY_SOURCE,content::maximum_document_bytes));
    for(auto& definition:doc["definitions"])if(definition["id"]=="scenario.founder_campaign") {
        definition["competition"]["domestic"]=2;definition["competition"]["international"]=0;definition["competition"]["heavyweights"]=0;
    }
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(doc.dump(),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection f;f.family="rich";f.route="bootstrap";f.office="home";f.personal_investment_cents=10000000;
    for(const auto& scene:scenario.founding->scenes)f.choices.emplace(scene.id,scene.choices.front().id);
    runtime::ScenarioOptions options;options.founding=f;options.identity=runtime::CampaignIdentity{"Industry Test","Alex Vale","they/them"};
    auto checkpoint=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
    checkpoint.world.enabled_flows=std::vector<std::string>{};runtime::Runtime game{program,checkpoint};
    check(game.prepare_opening(game.current_state().world.simulation.revision).accepted(),"Opening prepared");
    const auto& world=game.current_state().world;
    check(world.competition->companies.size()==2,"Bounded active roster");
    for(const auto& rival:world.competition->companies) {
        const auto& s=rival.simulation;check(s.management->people.has_value(),"Major HQ has individual people");
        check(s.management->people->employees.size()==4,"Opening roster conserves funded staff");
        check(core::current_headquarters(s).rent.value()>0,"Major HQ pays actual property rent");
        for(const auto& e:s.management->people->employees) {
            check(e.terms.profile.orientation!="unrecorded","Identity fixed at generation");
            check(core::hq_workstation_ready(s,e.terms.profile.id),"Each opening employee has an actual powered workstation");
            check(core::employee_monthly_salary(s,e).value()>0,"Named employee has funded payroll");
        }
        core::validate_simulation(s);
    }
    // The ten-person late entrant must fit a real authored opening plan too.
    const auto& large=runtime::rival_definition(program,"company.century.frontier");
    auto large_hq=runtime::competition_detail::opening_rival(world,program,large,world.simulation.date);
    check(large_hq.management->people->employees.size()==10,"Larger major-company roster");
    core::validate_simulation(large_hq);
    const auto& original=world.competition->companies.front().simulation;
    core::Simulation control{original};
    const auto submit=[&](core::CommandAction action){const auto result=control.submit({control.checkpoint().revision,std::move(action)});if(!result.accepted())throw std::runtime_error(result.explanation);};
    const auto& person=original.management->people->employees.front().terms.profile;
    std::string skill;for(const auto& course:core::talent_rules(original)->skills)if(course.teachable && course.prerequisite.empty() && !core::has_skill(person,course.id)){skill=course.id;break;}
    const auto cash=control.checkpoint().cash;submit(core::ManageTalent{core::EnrollSkill{person.id,skill,"company"}});
    check(control.checkpoint().cash<cash,"Rival courses use the same actual company tuition transaction");
    const auto save=[&]{check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Exact industry save reload");};
    save();
    for(int day=0;day<35;++day) {
        const auto result=game.submit({game.current_state().world.simulation.revision,core::AdvanceDay{}});if(!result.accepted())throw std::runtime_error(result.explanation);
    }
    bool research=false;for(const auto& rival:game.current_state().world.competition->companies)for(const auto& run:rival.simulation.lab->research)if(!run.researchers.empty())research=true;
    check(research,"Industry planner actually assigns named qualified researchers");save();
    const auto reloaded=persistence::decode_save(program,persistence::encode_save(program,game.current_state()));
    runtime::Runtime resumed{program,reloaded.checkpoint};
    for(auto* live:{&game,&resumed}) {
        const auto result=live->submit({live->current_state().world.simulation.revision,core::AdvanceDay{}});
        if(!result.accepted())throw std::runtime_error(result.explanation);
    }
    check(game.current_state()==resumed.current_state(),"Industry people, work and RNG continue identically after reload");
    std::cout<<"Industry people: funded HQ rosters, stable identities, workstations, rent, tuition, larger plans, actual named research and fresh saves passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
