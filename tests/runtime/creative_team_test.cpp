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
        check(changed.creativity==6 && changed.compatibility_bp==fifty.compatibility_bp,"A unique trait adds one creative perspective and cannot erase existing shared-trait gains.");
        clones.back().traits[1]="Interest: painting";
        const auto fewer_matches=core::team_dynamics(team);
        check(fewer_matches.compatibility_bp<changed.compatibility_bp,"A replaced shared trait stops contributing compatibility.");
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
        {
            runtime::Runtime furnished{program,game.current_state()};
            const auto state=[&]() -> const auto& {return furnished.current_state().world.simulation;};
            const auto apply=[&](core::CommandAction action) {
                const auto result=furnished.submit({state().revision,std::move(action)});
                if(!result.accepted())throw std::runtime_error(result.explanation);
            };
            const auto action=[&](core::HeadquartersAction a){apply(core::ManageHeadquarters{std::move(a)});};
            const auto id=s.management->people->employees.front().terms.profile.id;
            const auto first_item=static_cast<std::uint32_t>(s.infrastructure->workplace->headquarters->items.size()+1);
            const auto initial_mood=core::workplace_moods(s).at(id);
            check(!initial_mood.temperament.empty() && initial_mood.ceo_prestige>0 && initial_mood.amenities==0,"The team must show its own temperament and the CEO's opening reputation.");
            action(core::BuyHQEquipment{"plant"});action(core::BuyHQEquipment{"plant"});action(core::BuyHQEquipment{"aquarium"});
            for(int day=0;day<2;++day) apply(core::AdvanceDay{});
            check(core::workplace_moods(state()).at(id).amenities==0,"Stored amenities cannot improve mood.");
            action(core::PlaceHQEquipment{first_item,0,8,0,false});
            action(core::PlaceHQEquipment{first_item+1,0,8,1,false});
            action(core::PlaceHQEquipment{first_item+2,0,8,2,false});
            const auto before_preview=furnished.current_state();
            const auto improved=core::workplace_moods(state()).at(id);
            check(improved.amenities==5 && improved.score>initial_mood.score && improved.accessible_amenities.size()==2,"Amenities must improve mood once per installed type.");
            check(furnished.current_state()==before_preview,"Mood previews must not mutate campaign state.");
            action(core::PowerHQEquipment{first_item+2,false});
            check(core::workplace_moods(state()).at(id).amenities==2,"A powered amenity must lose its contribution when switched off.");
            action(core::AssignHQDesk{core::assigned_hq_desk(state(),id),""});
            check(core::workplace_moods(state()).at(id).amenities==0,"Workers without assigned HQ desks cannot claim the office amenity benefit.");
            const auto saved=persistence::decode_save(program,persistence::encode_save(program,furnished.current_state()));
            check(saved.checkpoint==furnished.current_state() && core::workplace_moods(saved.checkpoint.world.simulation).at(id).amenities==0,"The same mood must be derived after reload.");
        }
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
