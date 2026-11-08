#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/core/counties.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pgp;
void require(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
template<class F> void refuses(F f) {try {f();} catch(const std::exception&) {return;} throw std::runtime_error("Forged world state was accepted.");}
}
int main() {
    try {
        const auto registry=content::foundation_registry();
        const auto program=runtime::compile_collection(content::load_collection(persistence::read_bounded(PGP_WORLD_SOURCE,content::maximum_document_bytes),registry),registry);
        const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection selected;
        for(const auto& scene:scenario.founding->scenes) selected.choices.emplace(scene.id,scene.choices.front().id);
        selected.family="rich";selected.first_name="Alex";selected.last_name="River";
        runtime::ScenarioOptions options;options.identity=runtime::CampaignIdentity{"World test","Alex River","they/them"};options.founding=selected;
        const auto original=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
        require(original.world.simulation.management->people.has_value(),"Founder HQ needs a roster.");
        require(original.world.counties && original.world.counties->counties.size()==3143,"All 2010 US counties must be present.");
        runtime::Runtime live{program,original};
        const auto submit=[&](core::CommandAction action) {auto result=live.submit({live.current_state().world.simulation.revision,std::move(action)});require(result.accepted(),result.explanation.c_str());};
        const auto prepared=live.prepare_opening(live.current_state().world.simulation.revision);require(prepared.accepted(),prepared.explanation.c_str());
        const auto candidates=live.current_state().world.simulation.management->people->applicants;
        const auto hire=candidates.front();const auto staff=live.current_state().world.simulation.capacity->available_staff;
        for(const auto& candidate:candidates) core::validate_portrait(candidate.profile.portrait);
        submit(core::SetRecruitingTerms{core::SalaryRate{15000},"public benefit"});
        require(live.current_state().world.simulation.management->people->applicants==candidates,"Recruiting changes cannot rewrite existing offers.");
        const auto cash=live.current_state().world.simulation.cash;
        submit(core::HireEmployee{hire.profile.id});
        require(live.current_state().world.simulation.cash==core::checked_unit_subtract(cash,hire.signing),"Hiring must charge the signing cost.");
        require(live.current_state().world.simulation.capacity->available_staff==staff,"Onboarding cannot supply early capacity.");
        const auto before=live.current_state();
        require(!live.submit({before.world.simulation.revision,core::HireEmployee{hire.profile.id}}).accepted(),"A stale applicant must be refused.");
        require(before==live.current_state(),"A refused hire must not mutate payroll or RNG.");
        for(int day=0;day<35;++day) submit(core::AdvanceDay{});
        const auto& state=live.current_state().world.simulation;
        require(state.capacity->available_staff==staff+1,"Completed onboarding must add exactly one person.");
        require(state.management->people->employees.back().terms==hire,"The hired identity and terms must remain stable.");
        require(state.management->people->applicants.front().salary>hire.salary,"New cohorts must reflect the selected pay.");
        require(live.current_state().world.counties->stream!=original.world.counties->stream,"Counties must advance their independent stream monthly.");
        const auto decoded=persistence::decode_save(program,persistence::encode_save(program,live.current_state())).checkpoint;
        require(decoded==live.current_state(),"People and county state must reload exactly.");
        runtime::Runtime resumed{program,decoded};submit(core::AdvanceDay{});
        const auto next=resumed.submit({resumed.current_state().world.simulation.revision,core::AdvanceDay{}});require(next.accepted(),next.explanation.c_str());
        require(resumed.current_state()==live.current_state(),"Resumed world must keep the exact future RNG path.");
        auto forged=decoded;forged.world.simulation.management->people->employees.back().terms.salary=core::Cents{1};
        refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=decoded;forged.world.simulation.management->people->employees.back().terms.profile.portrait.parts[1]=255;
        refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=decoded;forged.world.simulation.management->people->employees.back().terms.profile.portrait.kit=9;
        refuses([&]{runtime::validate_checkpoint(program,forged);});
        forged=decoded;forged.world.counties->counties.front().population=0;
        refuses([&]{runtime::validate_checkpoint(program,forged);});
        auto positive=*original.world.counties,negative=positive;
        const auto next_month=core::advance_days(original.world.simulation.date,core::Days{31});
        core::advance_counties(positive,*scenario.counties,next_month,100,100);
        core::advance_counties(negative,*scenario.counties,next_month,-100,-100);
        require(positive.stream==negative.stream,"Economic drivers must not change random draw count.");
        for(std::size_t i=0;i<positive.counties.size();++i) require(positive.counties[i].economy>negative.counties[i].economy,"Regional/world effects must change county trends in the expected direction.");
        core::validate_counties(positive,*scenario.counties,next_month);core::validate_counties(negative,*scenario.counties,next_month);
        std::cout<<"HQ hiring/payroll/onboarding, county trends, corruption refusal and exact continuation passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
