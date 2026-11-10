#include <pgp/core/people.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {void check(bool okay,const char* why) {if(!okay)throw std::runtime_error(why);}}
int main() {
    using namespace pgp;
    try {
        std::vector<core::PersonProfile> clones(50);
        std::vector<const core::PersonProfile*> team;
        for(std::size_t i=0;i<clones.size();++i) {
            auto& p=clones[i];p.id="person/"+std::to_string(i);p.gender="nonbinary";p.education="Independent study";p.preferred_mission="research";p.traits={"Social: introverted","Interest: music"};team.push_back(&p);
        }
        const auto one=core::team_dynamics(std::span(team).first(1)),fifty=core::team_dynamics(team);
        check(one.creativity==5 && fifty.creativity==one.creativity && fifty.compatibility_bp==10000,"Fifty identical people must have one person's creativity and maximum shared-trait compatibility.");
        clones.back().traits.push_back("Interest: art");const auto changed=core::team_dynamics(team);
        check(changed.creativity==6 && changed.compatibility_bp<10000,"A new trait adds exactly one creative perspective, with no additional shared-trait gain.");
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_CREATIVE_SOURCE,content::maximum_document_bytes),registry),registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection f;f.family="rich";f.leg="political";f.route="hired_ceo";f.office="shared";f.researcher="none";
        for(const auto& scene:scenario.founding->scenes)f.choices[scene.id]=scene.choices.front().id;
        runtime::ScenarioOptions options;options.founding=f;options.seed=719;options.identity=runtime::CampaignIdentity{"Creative lab","Alex Vale","they/them"};
        auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        initial.world.enabled_flows=std::vector<std::string>{};runtime::Runtime game{program,initial};
        check(game.prepare_opening(game.current_state().world.simulation.revision).accepted(),"Opening prepares.");
        const auto& s=game.current_state().world.simulation;
        core::LabAllocation plan{"hq.equipment/2",core::MilliCUPerDay{4000},3,{}};
        for(const auto& e:s.management->people->employees)plan.researchers.push_back(e.terms.profile.id);
        const std::string topic="research.sequence_modeling";
        const auto team_value=core::research_team(s,plan.researchers);
        const auto needed=core::research_creativity_threshold(*s.lab,*s.capacity,topic,s.date);
        check(needed>0 && team_value.creativity>=needed,"The mixed team must reach the bypass threshold for this fixture.");
        auto future=*s.lab;future.catalog.creative_research.at(topic)={2030,5,2000};
        check(core::research_creativity_threshold(future,*s.capacity,topic,s.date)==300,"EEY gap must multiply the base threshold before the expected year.");
        auto solo=plan;solo.staff=1;solo.researchers.resize(1);
        const auto before=game.current_state();auto refusal=game.submit({s.revision,core::StartResearch{topic,solo}});
        check(!refusal.accepted() && before==game.current_state(),"Insufficient unique perspectives must refuse atomically.");
        auto accepted=game.submit({s.revision,core::StartResearch{topic,plan}});
        if(!accepted.accepted())throw std::runtime_error(accepted.explanation);
        const auto& after=game.current_state().world.simulation;const auto& run=after.lab->research.back();
        check(run.creativity==team_value.creativity && run.compatibility_bp==team_value.compatibility_bp,"Acceptance records actual team characteristics.");
        check(after.lab->research.size()==1 && !run.completed,"Bypassing prerequisites must start real work, not grant the technology.");
        check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Creative research and traits must reload exactly.");
        std::cout<<"Distinct-trait union, identical-team compatibility, early-year threshold, atomic refusal, prerequisite bypass and fresh reload passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
