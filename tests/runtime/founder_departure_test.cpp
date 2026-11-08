#include <pgp/runtime/scenario.hpp>
#include <pgp/runtime/authority.hpp>
#include <pgp/runtime/objectives.hpp>
#include <pgp/persistence/save.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>

namespace {
using namespace pgp;using namespace core;
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
const SimulationState& state(const runtime::Runtime& game) {return game.current_state().world.simulation;}
void apply(runtime::Runtime& game,CommandAction action) {
    const auto result=game.submit({state(game).revision,std::move(action)});
    if(!result.accepted()) throw std::runtime_error(result.explanation);
}
void refuse(runtime::Runtime& game,CommandAction action) {
    const auto before=game.current_state();const auto result=game.submit({state(game).revision,std::move(action)});
    check(!result.accepted() && game.current_state()==before,"A refused departure changed the company.");
}
void exact(runtime::Runtime& game) {
    const auto loaded=persistence::decode_save(game.program(),persistence::encode_save(game.program(),game.current_state()));
    check(loaded.checkpoint==game.current_state(),"Departure or ending changed across exact reload.");
    game=runtime::Runtime{game.program(),loaded.checkpoint};
}
void day(runtime::Runtime& game) {
    for(int tries=0;tries<8;++tries) {
        const auto& rows=game.current_state().runtime.presentations;
        const auto pending=std::ranges::find_if(rows,[](const auto& p){return !p.resolved && !p.choices.empty();});
        if(pending==rows.end()) break;
        const auto choice=std::ranges::find(pending->choices,std::string{"decline"},&runtime::PresentedChoice::id);
        check(choice!=pending->choices.end(),"Unexpected decision in focused retirement fixture.");
        check(game.choose(state(game).revision,pending->id,choice->id).accepted(),"Could not resolve retirement fixture decision.");
    }
    while(const auto* n=required_succession_notice(state(game))) apply(game,AcknowledgeSuccession{n->id});
    if(const auto* n=required_site_alert(state(game))) apply(game,AcknowledgeSiteAlert{n->id});
    apply(game,AdvanceDay{});
}
template<class F> void rejects(F f) {try {f();}catch(const std::exception&){return;}throw std::runtime_error("Forged departure proof accepted.");}
}
int main() {
    try {
        const auto registry=content::foundation_registry();
        const auto source=content::load_collection(persistence::read_bounded(PGP_DEPARTURE_SOURCE,content::maximum_document_bytes),registry);
        const auto program=runtime::compile_collection(source,registry);
        const auto& definition=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection selection;selection.family="rich";selection.office="leased";
        for(const auto& scene:definition.founding->scenes) selection.choices.emplace(scene.id,scene.choices.front().id);
        runtime::ScenarioOptions options;options.founding=selection;options.seed=19972015;
        options.identity=runtime::CampaignIdentity{"Handover test","Alex Vale","they/them"};
        runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",ObjectiveSelection{ObjectiveKind::survival,{}},options)};
        check(game.prepare_opening(state(game).revision).accepted(),"Founder opening failed.");
        refuse(game,FinishFounderRun{});refuse(game,AnnounceFounderDeparture{1,"continuity"});
        const auto due=advance_days(state(game).date,Days{104});
        const auto candidates=leadership_candidates(succession_catalog(state(game)),due);
        check(!candidates.empty(),"A founder must have a senior successor at the 2015 opening.");
        apply(game,ProposeSuccession{candidates.front().id,due,true,{"lease.initial",MilliCUPerDay{10000},1}});
        refuse(game,AnnounceFounderDeparture{1,"continuity"});apply(game,VoteSuccession{1,true});
        refuse(game,AnnounceFounderDeparture{1,"unknown"});
        auto reserved=game;apply(reserved,ReserveFunds{"Protected company funds",state(reserved).cash,true});
        refuse(reserved,AnnounceFounderDeparture{1,"continuity"});
        const auto cash=state(game).cash;const auto random=state(game).random;
        apply(game,AnnounceFounderDeparture{1,"continuity"});
        check(checked_unit_subtract(cash,state(game).cash)==Cents{25000} && state(game).random==random,"Announcement payment or RNG changed incorrectly.");
        refuse(game,AnnounceFounderDeparture{1,"open_record"});refuse(game,FinishFounderRun{});exact(game);
        const auto announced=game.current_state();
        auto cancelled=game;apply(cancelled,CancelSuccession{1});refuse(cancelled,FinishFounderRun{});exact(cancelled);
        const auto failed_events=runtime::public_identity_events(cancelled.current_state().world,state(cancelled).date);
        check(std::ranges::any_of(failed_events,[](const auto& e){return e.cause.find("cancelled")!=std::string::npos && e.direct_effect && (*e.direct_effect)[4]<0;}),"Breaking a public handover must affect actual audience confidence.");
        while(state(game).date<due) day(game);
        check(state(game).succession->plans.front().status==SuccessionStatus::activated && state(game).succession->terms.size()==2,"Retirement requires actual paid preparation and handover.");
        check(state(game).succession->terms.back().portrait.has_value(),"The appointed CEO must retain vector part IDs in the leadership record.");
        refuse(game,FinishFounderRun{});apply(game,RetireFounderChair{});
        const auto& world=game.current_state().world;auto without=world;without.simulation.succession->releases->clear();
        const auto with_scores=runtime::public_identity(world,state(game).date),without_scores=runtime::public_identity(without,state(game).date);
        check(with_scores[4]==without_scores[4]+150,"Completed handover confidence must use the authored message effect.");
        auto forged=announced;forged.world.simulation.succession->releases->front().message="open_record";
        rejects([&]{runtime::validate_checkpoint(program,forged);});
        forged=announced;forged.world.simulation.succession->releases->clear();rejects([&]{runtime::validate_checkpoint(program,forged);});
        const auto scratch=std::filesystem::path{PGP_DEPARTURE_SCRATCH}/("retirement-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        persistence::AutosaveRing ring{scratch};
        rejects([&]{(void)ring.write(program,game.current_state(),persistence::AutosaveReason::campaign_end);});
        auto bad_portrait=game.current_state();bad_portrait.world.simulation.succession->terms.back().portrait->parts[0]=6;rejects([&]{runtime::validate_checkpoint(program,bad_portrait);});
        const auto founder_accounts=state(game).accounts;apply(game,FinishFounderRun{});
        const auto end_save=ring.write(program,game.current_state(),persistence::AutosaveReason::campaign_end);
        check(end_save && persistence::load_file(program,*end_save).checkpoint==game.current_state(),"A prepared-morning retirement ending must autosave and reload.");
        std::filesystem::remove_all(scratch);
        check(runtime::objective_time_stopped(game.current_state().world) && game.current_state().world.objectives->end->cause==runtime::CampaignEndCause::retirement && state(game).accounts==founder_accounts,"Retirement must preserve company accounts and stop scored time.");
        refuse(game,AdvanceDay{});refuse(game,FinishFounderRun{});exact(game);
        forged=game.current_state();forged.world.objectives->end->source="2";rejects([&]{runtime::validate_checkpoint(program,forged);});
        const auto ending=game.current_state().world.objectives->end;apply(game,ContinueAfterEnding{});day(game);
        check(game.current_state().world.objectives->end==ending,"Following the successor cannot rewrite the founder ending.");exact(game);
        auto legacy=source;std::get<content::ScenarioDefinition>(legacy.definitions.at("scenario.founder_campaign").data).management->succession->departure.reset();
        const auto old=runtime::compile_collection(legacy,registry);
        runtime::Runtime old_game{old,runtime::begin_scenario(old,"scenario.founder_campaign","US-CA",ObjectiveSelection{ObjectiveKind::survival,{}},options)};
        check(old_game.prepare_opening(state(old_game).revision).accepted() && !state(old_game).succession->releases,"Legacy charters must retain their original save shape.");exact(old_game);
        std::cout<<"Paid founder release, real handover, public confidence, protected funds, cancellation, ending, sandbox and exact/legacy persistence passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
