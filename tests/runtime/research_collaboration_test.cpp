#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/core/workplace_research.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {void check(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}}
int main(){using namespace pgp;try{
    auto document=nlohmann::json::parse(persistence::read_bounded(PGP_COLLABORATION_SOURCE,content::maximum_document_bytes));
    for(auto& d:document["definitions"])if(d["id"]=="scenario.founder_campaign"){d["competition"]["domestic"]=2;d["competition"]["international"]=0;d["competition"]["heavyweights"]=0;}
    const auto registry=content::foundation_registry();const auto program=runtime::compile_collection(content::load_collection(document.dump(),registry),registry);
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
    content::FoundingSelection founder;founder.family="rich";founder.leg="political";founder.route="hired_ceo";founder.office="shared";founder.researcher="none";
    for(const auto& scene:scenario.founding->scenes)founder.choices[scene.id]=scene.choices.front().id;
    runtime::ScenarioOptions options;options.founding=founder;options.seed=719;options.identity=runtime::CampaignIdentity{"Research Jams","Alex Vale","they/them"};
    auto initial=runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",core::ObjectiveSelection{core::ObjectiveKind::survival,{}},options);
    initial.world.enabled_flows=std::vector<std::string>{};runtime::Runtime opening{program,initial};check(opening.prepare_opening(initial.world.simulation.revision).accepted(),"Opening");initial=opening.current_state();
    const auto& peer=initial.world.competition->companies.front();std::string contact;
    for(const auto& worker:core::research_workers(peer.simulation))if(worker.unavailable.empty()){contact=peer.definition_id+"/"+worker.id;break;}
    check(!contact.empty(),"Qualified partner available");
    runtime::CollaborationRequest request{contact,"research.reproducible_tooling",{}, {},core::MilliCUPerDay{4000}};
    const auto cold=runtime::collaboration_quote(initial.world,program,initial.world.company_id,request);check(cold.blocker.find("friendship")!=std::string::npos,"Cold acquaintances cannot spend a partner's time");
    // Explicit mature-friendship fixture. Geographic-social tests establish
    // natural friendship growth; this check isolates its research benefit.
    auto friendship=core::SocialLink{contact,"founder",700,50,initial.world.simulation.date,initial.world.simulation.date,{},"Established professional friendship fixture",true};
    if(friendship.first>friendship.second)std::swap(friendship.first,friendship.second);
    initial.world.simulation.management->people->social->links.push_back(friendship);runtime::validate_checkpoint(program,initial);
    const auto original=initial;const auto quote=runtime::collaboration_quote(initial.world,program,initial.world.company_id,request);
    if(!quote.blocker.empty())throw std::runtime_error("Joint preview: "+quote.blocker);check(initial==original && quote.work_credit==core::BasisPoints{2000},"Preview is read-only with authored work savings");
    // Refusal from a real reservation command, preserving accounts and people.
    // Pure formula projection; world publication provenance is checked by the
    // actual joint-publication path below, not claimed by this isolated input.
    auto calculation=initial.world.simulation;
    core::acquire_research_insight(*calculation.lab,calculation.date,{request.topic,"calculation.fixture",core::OrdinalDay{calculation.date.value()-1},calculation.date,core::BasisPoints{2500}});
    const core::ResearchCooperation calculation_pact{"research-jam/calculation","calculation.partner",core::BasisPoints{2000}};
    (void)core::start_funded_research(calculation,request.topic,quote.first,core::Cents{0},&calculation_pact);
    check(calculation.capacity->projects.back().total_work==core::MilliCU{45000},"Public and joint experiment credits take the best saving instead of stacking");
    runtime::Runtime poor{program,initial};auto reserved=poor.submit({poor.current_state().world.simulation.revision,core::ReserveFunds{"Protected fixture funds",poor.current_state().world.simulation.cash,true}});check(reserved.accepted(),"Reserve real company funds");
    const auto before_poor=poor.current_state();check(!poor.collaborateResearch(before_poor.world.simulation.revision,request).accepted() && poor.current_state()==before_poor,"Unfunded collaboration refuses atomically");
    runtime::Runtime game{program,initial};const auto cash=initial.world.simulation.cash,partner_cash=peer.simulation.cash;
    const auto partner_fee=core::research_cash_fee(peer.simulation,request.topic);
    const auto started=game.collaborateResearch(game.current_state().world.simulation.revision,request);if(!started.accepted())throw std::runtime_error(started.explanation);
    const auto state=[&]()->const auto&{return game.current_state().world.simulation;};
    const auto& agreement=game.current_state().world.competition->collaborations.front();const auto agreement_id=agreement.id;const auto first_project=agreement.first_project;
    const auto& partner_after=game.current_state().world.competition->companies.front().simulation;
    check(state().cash==core::checked_unit_subtract(cash,quote.total_fee) && partner_after.cash==core::checked_unit_subtract(core::checked_unit_add(partner_cash,quote.coordination_fee),partner_fee),"Both cash legs and both research fees are real");
    const auto work=[&](const core::SimulationState& s,std::string_view id)->const core::CapacityProject&{return *std::ranges::find(s.capacity->projects,id,&core::CapacityProject::id);};
    check(work(state(),first_project).required_staff_days==5 && work(state(),first_project).total_work==core::MilliCU{48000},"Joint work reduces independent labor and experiment budgets without granting completion");
    check(state().lab->research.back().cooperation && partner_after.lab->research.back().cooperation,"Both named runs retain the same agreement");
    const auto before_repeat=game.current_state();check(!game.collaborateResearch(state().revision,request).accepted() && game.current_state()==before_repeat,"A second jam cannot steal committed resources or duplicate payment");
    check(!game.publishCollaboration(state().revision,agreement_id).accepted(),"No joint publication before both results exist");
    const auto exact=[&]{check(persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint==game.current_state(),"Exact joint research save");};exact();
    auto corrupt=game.current_state();corrupt.world.competition->collaborations.front().coordination_fee=core::Cents{1};bool rejected=false;
    try{runtime::validate_checkpoint(program,corrupt);}catch(const std::exception&){rejected=true;}check(rejected,"Forged coordination terms are rejected");
    corrupt=game.current_state();corrupt.world.competition->companies.front().simulation.lab->research.back().cooperation.reset();rejected=false;
    try{runtime::validate_checkpoint(program,corrupt);}catch(const std::exception&){rejected=true;}check(rejected,"A joint credit cannot lose its actual partner run");
    runtime::Runtime cancelled{program,game.current_state()};check(cancelled.submit({cancelled.current_state().world.simulation.revision,core::CancelProject{first_project}}).accepted(),"Ordinary cancellation applies");
    check(runtime::collaboration_status(cancelled.current_state().world,cancelled.current_state().world.competition->collaborations.front())=="cancelled" && !cancelled.publishCollaboration(cancelled.current_state().world.simulation.revision,agreement_id).accepted(),"Cancelled work cannot become a joint paper");
    const auto apply=[&](core::CommandAction action){const auto result=game.submit({state().revision,std::move(action)});if(!result.accepted())throw std::runtime_error(result.explanation);};
    apply(core::AdvanceDay{});apply(core::AdvanceDay{});
    const auto desk=core::assigned_hq_desk(state(),quote.first.researchers.front());std::uint32_t computer=0;
    for(const auto& item:state().infrastructure->workplace->headquarters->items)if(item.mount==desk && item.offer=="desktop")computer=item.id;
    check(computer!=0,"The joint researcher has a real computer");apply(core::ManageHeadquarters{core::PowerHQEquipment{computer,false}});
    check(work(state(),first_project).status==core::ProjectStatus::paused,"Losing the computer pauses ordinary joint research");
    const auto earned=work(state(),first_project).completed_work;apply(core::AdvanceDay{});check(work(state(),first_project).completed_work==earned,"Paused collaboration earns no experiment work");exact();
    runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,game.current_state())).checkpoint};
    for(auto* live:{&game,&resumed})for(core::CommandAction action:std::vector<core::CommandAction>{core::ManageHeadquarters{core::PowerHQEquipment{computer,true}},core::ResumeProject{first_project}}){const auto r=live->submit({live->current_state().world.simulation.revision,action});if(!r.accepted())throw std::runtime_error(r.explanation);}
    for(int day=0;day<25 && runtime::collaboration_status(game.current_state().world,game.current_state().world.competition->collaborations.front())!="completed";++day)
        for(auto* live:{&game,&resumed}){const auto r=live->submit({live->current_state().world.simulation.revision,core::AdvanceDay{}});if(!r.accepted())throw std::runtime_error(r.explanation);}
    check(game.current_state()==resumed.current_state(),"Joint research resumes identically after reload");
    check(runtime::collaboration_status(game.current_state().world,game.current_state().world.competition->collaborations.front())=="completed","Both labs actually finish their research");
    auto next_topic=request;next_topic.topic="research.sequence_modeling";
    check(runtime::collaboration_quote(game.current_state().world,program,game.current_state().world.company_id,next_topic).blocker.find("Give this partnership time")!=std::string::npos,"A completed partnership still respects its authored cooldown");
    const auto published=game.publishCollaboration(state().revision,agreement_id);if(!published.accepted())throw std::runtime_error(published.explanation);
    for(const auto* s:{&state(),&game.current_state().world.competition->companies.front().simulation})check(std::ranges::any_of(s->lab->publications,[&](const auto& paper){return paper.research_id==request.topic;}),"Both employers have an actual public result");exact();
    // The same world operation is selected by a normal weekly rival review.
    auto industry=original;std::vector<std::string> peers;
    for(const auto& c:industry.world.competition->companies){for(const auto& worker:core::research_workers(c.simulation))if(worker.unavailable.empty()){peers.push_back(c.definition_id+"/"+worker.id);break;}}
    check(peers.size()==2,"Two individually simulated rival researchers");std::sort(peers.begin(),peers.end());industry.world.simulation.management->people->social->links.push_back({peers[0],peers[1],700,50,state().date, state().date,{},"Established industry friendship fixture",false});
    auto& link=industry.world.simulation.management->people->social->links.back();link.met_on=industry.world.simulation.date;link.last_contact=industry.world.simulation.date;
    runtime::Runtime ai{program,industry};bool autonomous=false;
    for(int day=0;day<16 && !autonomous;++day){const auto r=ai.submit({ai.current_state().world.simulation.revision,core::AdvanceDay{}});if(!r.accepted())throw std::runtime_error(r.explanation);for(const auto& c:ai.current_state().world.competition->collaborations)if(c.first_company!=ai.current_state().world.company_id && c.second_company!=ai.current_state().world.company_id)autonomous=true;}
    check(autonomous,"A weekly rival strategy actually signs a funded collaboration through its own employees' connection");
    check(persistence::decode_save(program,persistence::encode_save(program,ai.current_state())).checkpoint==ai.current_state(),"Autonomous rival collaboration saves exactly");
    std::cout<<"Research collaboration: trust and funds, bilateral payment, named work/compute, real savings, pause/cancel, shared publication, weekly rival cooperation and exact reload passed.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
