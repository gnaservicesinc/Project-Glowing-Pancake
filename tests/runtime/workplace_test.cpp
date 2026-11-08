#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
using namespace pgp;
void require(bool okay,const char* reason) {if(!okay) throw std::runtime_error(reason);}
template<class F> void refuses(F f) {try{f();}catch(const std::exception&){return;}throw std::runtime_error("Forged workplace state accepted");}
}
int main() {
    try {
        const auto registry=content::foundation_registry();const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_WORKPLACE_SOURCE,content::maximum_document_bytes),registry),registry);
        const auto& definition=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection selected;selected.family="rich";selected.office="home";
        for(const auto& chapter:definition.founding->scenes) selected.choices.emplace(chapter.id,chapter.choices.front().id);
        runtime::ScenarioOptions options;options.founding=selected;options.identity=runtime::CampaignIdentity{"Workplace test","Alex Vale","they/them"};
        runtime::Runtime live{program,runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options)};
        const auto opening=live.prepare_opening(live.current_state().world.simulation.revision);require(opening.accepted(),opening.explanation.c_str());
        const auto submit=[&](core::CommandAction action) {const auto result=live.submit({live.current_state().world.simulation.revision,std::move(action)});require(result.accepted(),result.explanation.c_str());};
        require(core::headquarters_seats(live.current_state().world.simulation)==2,"Home opening must have two workstations");
        auto applicants=live.current_state().world.simulation.management->people->applicants;
        submit(core::HireEmployee{applicants[0].profile.id});
        const auto full=live.current_state();
        const auto refused=live.submit({full.world.simulation.revision,core::HireEmployee{applicants[1].profile.id}});
        require(!refused.accepted() && live.current_state()==full,"Full HQ must refuse hiring without spending cash or consuming identity RNG");
        submit(core::ExpandHeadquarters{"annex_1","hq.research"});
        require(core::headquarters_seats(live.current_state().world.simulation)==2,"Room construction cannot grant early workstations");
        require(live.current_state().world.simulation.cash==core::checked_unit_subtract(full.world.simulation.cash,core::Cents{3500000}),"The room's full fit-out must be paid");
        require(persistence::decode_save(program,persistence::encode_save(program,live.current_state())).checkpoint==live.current_state(),"Unfinished rooms must reload exactly");
        for(int day=0;day<14;++day) submit(core::AdvanceDay{});
        require(core::headquarters_seats(live.current_state().world.simulation)==8,"Completed research annex must add its six real seats");
        submit(core::HireEmployee{applicants[1].profile.id});
        const core::FacilityBlueprint blueprint{"compact","Compact research branch","rack.reference",20000,1,false};
        const auto before_plan=live.current_state().world.simulation;
        submit(core::CreateFacilityBlueprint{blueprint});
        require(live.current_state().world.simulation.cash==before_plan.cash && live.current_state().world.simulation.capacity==before_plan.capacity,"Saving a plan cannot grant capacity or spend funds");
        const core::BuildFromBlueprint request{"compact","branch_a","North research branch","US-CA"};
        const auto quote=core::preview_blueprint_build(live.current_state().world.simulation,request);
        const auto before=live.current_state();submit(request);
        const auto& built=live.current_state().world.simulation;
        require(built.cash==core::checked_unit_subtract(before.world.simulation.cash,quote.total),"Blueprint must charge the complete state-specific quote");
        require(built.infrastructure->sites.size()==1 && built.infrastructure->hardware.size()==1 && built.infrastructure->hires.size()==1,"One blueprint instance needs one paid physical site, rack and operator hire");
        require(built.capacity->sources.size()==before.world.simulation.capacity->sources.size(),"A blueprint cannot supply capacity before commissioning");
        require(persistence::decode_save(program,persistence::encode_save(program,live.current_state())).checkpoint==live.current_state(),"Blueprints, orders and financial obligations must reload exactly");
        const auto full_build=live.current_state();
        require(!live.submit({built.revision,request}).accepted() && live.current_state()==full_build,"A duplicate build must roll back the complete order");
        runtime::Runtime repeat{program,full_build};const core::BuildFromBlueprint second{"compact","branch_tx","South research branch","US-TX"};
        const auto second_quote=core::preview_blueprint_build(full_build.world.simulation,second);
        const auto repeated=repeat.submit({repeat.current_state().world.simulation.revision,second});require(repeated.accepted(),repeated.explanation.c_str());
        const auto& second_state=repeat.current_state().world.simulation;
        require(second_state.infrastructure->workplace->blueprints.size()==1 && second_state.infrastructure->workplace->builds.size()==2 && second_state.infrastructure->sites.size()==2,"A saved blueprint must build a separate real facility in another state");
        require(second_state.cash==core::checked_unit_subtract(full_build.world.simulation.cash,second_quote.total),"Each repeated build must pay its own state quote");
        require(persistence::decode_save(program,persistence::encode_save(program,repeat.current_state())).checkpoint==repeat.current_state(),"Repeated blueprint bindings must retain both physical instances");
        auto protected_state=full_build.world.simulation;
        refuses([&]{core::build_from_blueprint(protected_state,{"compact","branch_b","South branch","US-CA"},protected_state.cash);});
        require(protected_state==full_build.world.simulation,"An unaffordable complete plan must preserve all funds and physical orders");
        auto forged=full_build;forged.world.simulation.infrastructure->workplace->rooms[0].ready_on=forged.world.simulation.infrastructure->workplace->starts_on;
        refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=full_build;forged.world.simulation.infrastructure->workplace->blueprints[0].operators=2;
        refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=full_build;forged.world.simulation.infrastructure->workplace->opening_office="leased";
        refuses([&]{runtime::validate_checkpoint(program,forged);});
        core::Simulation physical{built};
        const auto visit=core::advance_days(std::max({quote.built_on,quote.grid_on,quote.delivery_on,quote.operators_on}),core::Days{1});
        auto result=physical.submit({physical.checkpoint().revision,core::ScheduleSiteAppointment{"inspect_a","branch_a",visit,core::AppointmentMode::remote}});require(result.accepted(),result.explanation.c_str());
        while(physical.checkpoint().date<visit) {
            result=physical.submit({physical.checkpoint().revision,core::AdvanceDay{}});require(result.accepted(),result.explanation.c_str());
        }
        require(physical.checkpoint().infrastructure->sites[0].commissioned_on.has_value(),"The blueprint site must follow the real inspection boundary");
        require(physical.checkpoint().capacity->sources.size()==built.capacity->sources.size()+1,"Only actual commissioned hardware adds usable capacity");
        std::cout<<"Paid HQ expansion, seat-bound hiring, complete blueprints, commissioning and exact saves passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
