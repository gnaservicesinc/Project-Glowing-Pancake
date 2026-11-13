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
    if(core::required_governance_notice(state(game))) {
        const auto& disputes=state(game).management->governance->disputes;
        const auto next=std::ranges::find_if(disputes,[](const auto& row){return !row.acknowledged;});
        apply(game,RespondCorporateDispute{next->id,false});
    }
    apply(game,AdvanceDay{});
}
template<class F> void rejects(F f) {try {f();}catch(const std::exception&){return;}throw std::runtime_error("Forged departure proof accepted.");}
}
int main() {
    try {
        const auto registry=content::foundation_registry();
        auto source=content::load_collection(persistence::read_bounded(PGP_DEPARTURE_SOURCE,content::maximum_document_bytes),registry);
        // Keep the actual paid transition, with a bounded 44-day focused fixture.
        auto& fixture=std::get<content::ScenarioDefinition>(source.definitions.at("scenario.founder_campaign").data);
        auto& transition=*fixture.management->succession;
        transition.transition_days=Days{30};transition.transition_work=MilliCU{300000};
        fixture.management->governance->executives.front().promise_term=Days{30};
        const auto program=runtime::compile_collection(source,registry);
        const auto& definition=std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection selection;selection.family="rich";selection.route="hired_ceo";selection.office="shared";
        for(const auto& scene:definition.founding->scenes) selection.choices.emplace(scene.id,scene.choices.front().id);
        runtime::ScenarioOptions options;options.founding=selection;options.seed=19972015;
        options.identity=runtime::CampaignIdentity{"Handover test","Alex Vale","they/them"};
        runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.founder_campaign","US-CA",ObjectiveSelection{ObjectiveKind::survival,{}},options)};
        check(game.prepare_opening(state(game).revision).accepted(),"Founder opening failed.");
        apply(game,AcceptBusinessOffer{"offer.supplier_small"});
        refuse(game,FinishFounderRun{});refuse(game,AnnounceFounderDeparture{1,"continuity"});
        const auto due=advance_days(state(game).date,Days{44});
        const auto candidates=leadership_candidates(succession_catalog(state(game)),due);
        check(!candidates.empty(),"A founder must have a senior successor at the 2015 opening.");
        const auto& catalog=succession_catalog(state(game));
        const auto* outside=outside_leadership_candidate(catalog,"outside.kiran-desai");
        check(outside && catalog.outside_candidates->size()==2,"The founder campaign must offer two named outside nominees.");
        const auto nominee=*outside;
        const auto available=leadership_candidates(state(game),due);
        const auto aged_out=leadership_candidates(state(game),to_ordinal({nominee.birth_year+static_cast<int>(catalog.maximum_candidate_age)+1,1,1}));
        check(std::ranges::count(available,nominee.id,&LeadershipCandidate::id)==1 &&
            std::ranges::none_of(aged_out,[&](const auto& person){return person.id==nominee.id;}) &&
            std::ranges::none_of(candidates,[](const auto& person){return person.id.starts_with("outside.");}),
            "Outside nominees must be eligible on the handover date and excluded from automatic caretaker appointments.");
        if(!active_charter(state(game)) || active_charter(state(game))->id!="charter.independent_lab") {
            apply(game,ProposeCharter{"charter.independent_lab"});
            apply(game,VoteBoardMotion{state(game).management->governance->motions.back().id,true});
        }
        check(active_charter(state(game)) && active_charter(state(game))->id=="charter.independent_lab",
            "The charter floor fixture needs the independent laboratory charter.");
        const LabAllocation allocation{"lease/offer.supplier_small",MilliCUPerDay{10000},1};
        refuse(game,ProposeSuccession{"outside.unknown",due,true,allocation});
        apply(game,ProposeSuccession{nominee.id,due,true,allocation});
        refuse(game,AnnounceFounderDeparture{1,"continuity"});
        auto under_floor=state(game);
        const auto target_cash=checked_unit_add(active_charter(under_floor)->cash_floor,Cents{1});
        record_cash(*under_floor.accounts,under_floor.date,checked_unit_subtract(target_cash,under_floor.cash),
            Account::expenses,"Fixture cash expense","fixture.charter-floor");
        under_floor.cash=accounts_report(*under_floor.accounts).cash;
        vote_succession(under_floor,{1,true},Cents{0});
        const auto& blocked=under_floor.succession->plans.front();
        check(blocked.status==SuccessionStatus::rejected && blocked.project.empty() && under_floor.cash==target_cash,
            "A succession that breaches the current charter's cash floor must lose board support without a preparation fee.");
        const auto before_vote=state(game).cash;
        apply(game,VoteSuccession{1,true});
        const auto& decision=state(game).succession->plans.front();
        check(decision.status==SuccessionStatus::approved && decision.charter_cursor==state(game).management->governance->charters.size() &&
            checked_unit_subtract(before_vote,state(game).cash)==nominee.signing_cost,"An approved outside nominee must use the actual charter and preparation fee.");
        const auto outside_reaction=[](const auto& event){return event.cause=="Appointed an outside chief executive";};
        const auto pending_events=runtime::public_identity_events(game.current_state().world,state(game).date);
        check(std::ranges::none_of(pending_events,outside_reaction),"A board vote must not grant the outside appointment's audience effects.");
        exact(game);
        auto missing_charter=game.checkpoint();
        missing_charter.world.simulation.succession->plans.front().charter_cursor.reset();
        rejects([&]{runtime::validate_checkpoint(program,missing_charter);});
        const auto recorded_vote=state(game).succession->plans.front();
        auto amended=game;
        apply(amended,ProposeCharter{"charter.customer_partnership"});
        apply(amended,VoteBoardMotion{state(amended).management->governance->motions.back().id,true});
        check(active_charter(state(amended))->id=="charter.customer_partnership" &&
            state(amended).succession->plans.front()==recorded_vote,"A later charter amendment must preserve the earlier board decision.");
        exact(amended);
        refuse(game,AnnounceFounderDeparture{1,"unknown"});
        refuse(game,AnnounceFounderDeparture{1,"continuity","unknown"});
        const auto before_preview=game.current_state();
        const auto preview=preview_founder_departure(state(game),{1,"profitable_exit","personal"});
        check(preview.reason=="personal" && preview.evidence && departure_assessment(state(game),preview)=="challenged" &&
            game.current_state()==before_preview,"Statement preview must use the company record without mutating it.");
        auto challenged=game;
        apply(challenged,AnnounceFounderDeparture{1,"profitable_exit","responsibility"});
        const auto challenged_release=challenged.current_state();
        const auto confidence=runtime::public_identity_events(challenged_release.world,state(challenged).date);
        check(std::ranges::any_of(confidence,[](const auto& event){return event.cause.starts_with("Reporters challenged") && event.direct_effect && (*event.direct_effect)[4]<0;}),
            "A contradicted resignation claim must affect actual audience confidence.");
        exact(challenged);
        auto changed_evidence=challenged_release;changed_evidence.world.simulation.succession->releases->front().evidence->profit=Cents{1};
        rejects([&]{runtime::validate_checkpoint(program,changed_evidence);});
        // Isolate dated press evidence from unrelated publication prerequisites.
        auto press=state(challenged);
        record_cash(*press.accounts,press.date,Cents{100000000},Account::revenue,"Later payment","fixture.later-payment");
        press.cash=accounts_report(*press.accounts).cash;
        press.lab->publications.push_back({"fixture.same-day",press.date});
        validate_founder_departure(press);
        check(departure_assessment(press,press.succession->releases->front())=="challenged",
            "Later revenue must not rewrite the original resignation coverage.");
        auto profitable=state(game);
        record_cash(*profitable.accounts,profitable.date,Cents{100000000},Account::revenue,"Earlier payment","fixture.earlier-payment");
        profitable.cash=accounts_report(*profitable.accounts).cash;
        check(departure_assessment(profitable,preview_founder_departure(profitable,{1,"profitable_exit","personal"}))=="supported",
            "A profitable company must support the corresponding statement.");
        auto same_day=state(game);same_day.lab->publications.push_back({"fixture.same-day",same_day.date});
        check(departure_assessment(same_day,preview_founder_departure(same_day,{1,"research_record","direction"}))=="challenged",
            "A publication on the announcement day must not become earlier evidence.");
        auto published=state(game);published.lab->publications.push_back({"fixture.earlier",OrdinalDay{published.date.value()-1}});
        check(departure_assessment(published,preview_founder_departure(published,{1,"research_record","direction"}))=="supported",
            "An earlier publication must support the research claim.");
        const auto delivery=preview_founder_departure(state(game),{1,"delivery_record","personal"});
        check(departure_assessment(state(game),delivery)=="unsubstantiated",
            "An empty contract register does not substantiate an on-time delivery record.");
        auto delivered=state(game);
        MarketContract pilot;pilot.id="fixture.on_time";pilot.status=ContractStatus::completed;pilot.stopped_on=delivered.date;
        delivered.market->contracts.push_back(pilot);
        check(departure_assessment(delivered,preview_founder_departure(delivered,{1,"delivery_record","personal"}))=="supported",
            "A settled contract with no missed start supports the claim.");
        pilot.id="fixture.missed";pilot.status=ContractStatus::missed;pilot.stopped_on=OrdinalDay{delivered.date.value()-1};
        delivered.market->contracts.push_back(pilot);
        check(departure_assessment(delivered,preview_founder_departure(delivered,{1,"delivery_record","personal"}))=="challenged",
            "An actual missed start contradicts the claim regardless of other completed contracts.");
        auto disclosure=game;
        apply(disclosure,AppointExecutive{"executive.research",false});
        const auto pre_dispute=preview_founder_departure(state(disclosure),{1,"publication_record","direction"});
        check(departure_assessment(state(disclosure),pre_dispute)=="supported" && !pre_dispute.evidence->public_dispute,
            "An appointment with an unexpired commitment is not a public dispute.");
        const auto departure_day=advance_days(state(disclosure).date,Days{60});
        while(state(disclosure).date<OrdinalDay{departure_day.value()-1}) day(disclosure);
        const auto before_disclosure=preview_founder_departure(state(disclosure),{1,"publication_record","direction"});
        check(!before_disclosure.evidence->public_dispute,
            "A formal concern before its response deadline must not be described as a departure.");
        while(state(disclosure).date<departure_day) day(disclosure);
        if(const auto* notice=required_succession_notice(state(disclosure))) apply(disclosure,AcknowledgeSuccession{notice->id});
        if(core::required_governance_notice(state(disclosure)))
            apply(disclosure,RespondCorporateDispute{state(disclosure).management->governance->disputes.front().id,false});
        apply(disclosure,AnnounceFounderDeparture{1,"publication_record","direction"});
        const auto& press_release=state(disclosure).succession->releases->back();
        check(press_release.evidence->public_dispute==state(disclosure).management->governance->disputes.front().id &&
            departure_assessment(state(disclosure),press_release)=="challenged" &&
            departure_assessment(state(disclosure),pre_dispute)=="supported",
            "The confirmed public departure should shape coverage without rewriting earlier evidence.");
        const auto reaction=runtime::public_identity_events(disclosure.current_state().world,state(disclosure).date);
        check(std::ranges::any_of(reaction,[](const auto& row){
            return row.cause.starts_with("Reporters challenged") && row.direct_effect && (*row.direct_effect)[4]<0;
        }) && std::ranges::none_of(reaction,[](const auto& row){
            return row.cause.starts_with("Completed the publicly announced") && row.direct_effect && (*row.direct_effect)[4]>0;
        }),"A completed handover should not reward a contradicted claim.");
        exact(disclosure);
        auto forged_press=disclosure.checkpoint();
        forged_press.world.simulation.succession->releases->back().evidence->public_dispute.reset();
        rejects([&]{runtime::validate_checkpoint(program,forged_press);});
        auto reserved=game;apply(reserved,ReserveFunds{"Protected company funds",state(reserved).cash,true});
        refuse(reserved,AnnounceFounderDeparture{1,"continuity"});
        auto quiet=reserved;const auto quiet_cash=state(quiet).cash;
        apply(quiet,AnnounceFounderDeparture{1,"quiet_handover","personal"});
        check(state(quiet).cash==quiet_cash,"A quiet announcement must remain available without an announcement fee.");exact(quiet);
        const auto cash=state(game).cash;const auto random=state(game).random;
        apply(game,AnnounceFounderDeparture{1,"continuity"});
        check(checked_unit_subtract(cash,state(game).cash)==Cents{25000} && state(game).random==random,"Announcement payment or RNG changed incorrectly.");
        refuse(game,AnnounceFounderDeparture{1,"open_record"});refuse(game,FinishFounderRun{});exact(game);
        const auto announced=game.current_state();
        auto cancelled=game;apply(cancelled,CancelSuccession{1});refuse(cancelled,FinishFounderRun{});exact(cancelled);
        const auto failed_events=runtime::public_identity_events(cancelled.current_state().world,state(cancelled).date);
        check(std::ranges::any_of(failed_events,[](const auto& e){return e.cause.find("cancelled")!=std::string::npos && e.direct_effect && (*e.direct_effect)[4]<0;}),"Breaking a public handover must affect actual audience confidence.");
        check(std::ranges::none_of(failed_events,outside_reaction),"A cancelled outside appointment must not affect audiences as a completed handover.");
        while(state(game).date<due) day(game);
        check(state(game).succession->plans.front().status==SuccessionStatus::activated && state(game).succession->terms.size()==2,"Retirement requires actual paid preparation and handover.");
        check(state(game).succession->terms.back().portrait.has_value(),"The appointed CEO must retain vector part IDs in the leadership record.");
        const auto board=current_board(state(game));
        const auto& recurring=state(game).accounts->recurring;
        const auto salary=std::ranges::find(recurring,"leadership/ceo.2",&RecurringObligation::id);
        const auto appointments=runtime::public_identity_events(game.current_state().world,due);
        const auto appointment=std::ranges::find_if(appointments,outside_reaction);
        check(state(game).succession->terms.back().person==nominee.id && salary!=recurring.end() &&
            salary->starts_on==due && salary->monthly_amount==nominee.monthly_salary &&
            std::ranges::any_of(board,[&](const auto& member){return member.id=="leadership.ceo" && member.seats==1 && member.name.starts_with(nominee.name);}) &&
            state(game).accounts->holdings==announced.world.simulation.accounts->holdings,
            "The outside appointment must transfer the CEO seat and pay its agreed salary while preserving ownership.");
        check(std::ranges::count_if(appointments,outside_reaction)==1 && appointment->day==due &&
            appointment->subject==nominee.name && appointment->direct_effect==nominee.appointment_effect,
            "One dated outside-appointment reaction must follow the actual handover.");
        check(runtime::completed_founder_departure(game.current_state().world,state(game).date).has_value(),"A retained chair must still be allowed to close their life story.");
        auto kept_chair=game;const auto retained=state(kept_chair).succession;
        apply(kept_chair,FinishFounderRun{});
        check(state(kept_chair).succession==retained,"Ending by choice must preserve the chair and ownership record.");exact(kept_chair);
        day(game);
        check(!game.current_state().world.objectives->end && founder_role(*state(game).succession)==FounderRole::chair,
            "Continuing after resignation must advance the same life without forcing an ending.");exact(game);
        apply(game,RetireFounderChair{});
        const auto& world=game.current_state().world;auto without=world;without.simulation.succession->releases->clear();
        const auto with_scores=runtime::public_identity(world,state(game).date),without_scores=runtime::public_identity(without,state(game).date);
        const auto memory=world.authority->catalog.identity_memory.value();
        check(with_scores[4]==without_scores[4]+150*(memory-1)/memory,"Completed handover confidence must use the authored effect and its dated decay.");
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
        refuse(game,ContinueAfterEnding{});
        auto legacy=source;std::get<content::ScenarioDefinition>(legacy.definitions.at("scenario.founder_campaign").data).management->succession->departure.reset();
        const auto old=runtime::compile_collection(legacy,registry);
        runtime::Runtime old_game{old,runtime::begin_scenario(old,"scenario.founder_campaign","US-CA",ObjectiveSelection{ObjectiveKind::survival,{}},options)};
        check(old_game.prepare_opening(state(old_game).revision).accepted() && !state(old_game).succession->releases,"Legacy charters must retain their original save shape.");exact(old_game);
        std::cout<<"Outside appointment, charter ballots, dated resignation claims, audience effects, paid handover, continuing life and exact persistence passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
