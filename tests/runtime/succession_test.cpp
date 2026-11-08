#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>
#include "../content/slice_fixture.hpp"
#include <pgp/runtime/legacy.hpp>
#include <pgp/runtime/competition.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>

namespace {
using namespace pgp;using namespace core;using Json=nlohmann::json;
constexpr std::string_view scenario="scenario.century_frontier";
unsigned checks=0;
void check(bool yes,const char* reason) {++checks;if(!yes) throw std::runtime_error(reason);}
const SimulationState& state(const runtime::Runtime& game) {return game.current_state().world.simulation;}
const SuccessionState& leadership(const runtime::Runtime& game) {return *state(game).succession;}
bool fact(const runtime::Runtime& game,std::string_view id) {
    const auto& world=game.current_state().world;
    return std::get<bool>(runtime::read_fact(world,*game.program().registry.fact(id),{content::Scope::company,world.company_id}));
}
void apply(runtime::Runtime& game,CommandAction action) {
    const auto result=game.submit({state(game).revision,std::move(action)});
    if(!result.accepted()) throw std::runtime_error(iso_date(state(game).date)+" · "+result.explanation);
}
void refuse(runtime::Runtime& game,CommandAction action) {
    const auto before=game.checkpoint();const auto result=game.submit({state(game).revision,std::move(action)});
    check(!result.accepted() && game.checkpoint()==before,"A refused leadership command changed the company.");
}
void advance(runtime::Runtime& game,OrdinalDay target) {
    while(state(game).date<target) {
        if(const auto* notice=required_succession_notice(state(game))) apply(game,AcknowledgeSuccession{notice->id});
        if(const auto* alert=required_site_alert(state(game))) apply(game,AcknowledgeSiteAlert{alert->id});
        if(const auto warning=liquidity_report(*state(game).liquidity).pending_warning) apply(game,AcknowledgeLiquidityWarning{*warning});
        if(required_management_notice(state(game))) {
            const auto& rows=state(game).management->notices;
            const auto found=std::ranges::find_if(rows,[](const auto& n){return n.required && !n.acknowledged;});
            apply(game,AcknowledgeManagementNotice{found->id});
        }
        apply(game,AdvanceDay{});
    }
}
const CapacityProject& project(const runtime::Runtime& game,std::string_view id) {
    const auto& projects=state(game).capacity->projects;const auto found=std::ranges::find(projects,id,&CapacityProject::id);
    if(found==projects.end()) throw std::runtime_error("Missing funded work.");return *found;
}
void finish(runtime::Runtime& game,std::string id) {
    while(project(game,id).status==ProjectStatus::active) advance(game,advance_days(state(game).date,Days{1}));
    check(project(game,id).status==ProjectStatus::completed,"The actual paid preparation failed to finish.");
}
runtime::CompiledProgram compile(Json source) {
    const auto registry=content::foundation_registry();return runtime::compile_collection(content::load_collection(source.dump(),registry),registry);
}
runtime::CompiledProgram fixture(Json source,bool rivals=false) {
    // This fixture rewrites rival terms in its two scenarios. Do not leave a
    // third scenario retaining different terms for the same rival identities.
    auto& definitions=source["definitions"];
    definitions.erase(std::remove_if(definitions.begin(),definitions.end(),[](const auto& row){return row["id"]=="scenario.founder_campaign";}),definitions.end());
    for(auto& row:source["definitions"]) if(row["id"]==scenario || row["id"]=="scenario.century_plateau") {
        row["starts_on"]="2027-01-01";row["flows"]=Json::array();row["business_offers"]=Json::array();
        row["management"]["setbacks"]=Json::array();
        row["management"]["succession"]["founder_birth_year"]=1958;
        row["management"]["succession"]["chair_retirement_age"]=70;
        for(const auto key:{"authority","society","civic","resilience","objectives","backgrounds"}) row.erase(key);
        if(rivals) {
            auto& roster=row["rivals"];
            roster.erase(std::remove_if(roster.begin(),roster.end(),[](const auto& company) {
                return company["id"]!="company.century.frontier";
            }),roster.end());
            roster.front()["start_after_days"]=0;
            roster.front()["planning"]["slot"]=1;
            row["competition"]["domestic"]=1;
            row["competition"]["international"]=0;
            row["competition"]["heavyweights"]=0;
        }
        else {row.erase("rivals");row.erase("competition");}
    }
    pgp_test::prune_unused_flows(source);
    return compile(std::move(source));
}
runtime::Runtime begin(const runtime::CompiledProgram& p) {
    runtime::Runtime game{p,runtime::begin_scenario(p,scenario,"US-WA")};
    check(game.prepare_opening(state(game).revision).accepted(),"The leadership opening failed.");return game;
}
runtime::Runtime reload(const runtime::Runtime& game) {
    auto loaded=persistence::decode_save(game.program(),persistence::encode_save(game.program(),game.checkpoint()));
    check(loaded.checkpoint==game.checkpoint(),"Leadership, trust or company obligations changed across reload.");
    return {game.program(),std::move(loaded.checkpoint)};
}
void service(runtime::Runtime& game) {
    apply(game,StartResearch{"research.held_out_evaluation",{"lease.initial",MilliCUPerDay{40000},2}});
    finish(game,state(game).lab->research.back().project_id);
    apply(game,BuyData{"data.open_documents"});
    apply(game,StartTraining{{"model.continuity","recipe.document_baseline","",{"data.open_documents"},"lease.initial",MilliCUPerDay{60000},3,MilliCU{3600000}}});
    finish(game,state(game).lab->models.back().project_id);
    apply(game,StartEvaluation{"evidence.continuity","model.continuity","evaluation.held_out",{"lease.initial",MilliCUPerDay{30000},1}});
    finish(game,state(game).lab->evaluations.back().project_id);
    apply(game,DeployModel{"product.documents","model.continuity","evidence.continuity","lease.initial",false});
}
ProposeSuccession proposal(const runtime::Runtime& game,bool chair=true) {
    const auto day=advance_days(state(game).date,Days{104});
    const auto candidates=leadership_candidates(succession_catalog(state(game)),day);
    check(!candidates.empty(),"No eligible named successor is available at the disclosed date.");
    return {candidates.front().id,day,chair,{"lease.initial",MilliCUPerDay{10000},1}};
}
void handover(const runtime::CompiledProgram& p) {
    auto game=begin(p);const auto original_holder=leadership(game).founder_holder;
    check(required_succession_notice(state(game)) && required_succession_notice(state(game))->kind=="planning","The retirement deadline had no advance planning notice.");
    refuse(game,AdvanceDay{});apply(game,AcknowledgeSuccession{required_succession_notice(state(game))->id});
    service(game);
    const auto request=proposal(game);const auto before=game.checkpoint();const auto rng=state(game).random;
    apply(game,ProposeSuccession{request});
    check(state(game).cash==before.world.simulation.cash && state(game).accounts->holdings==before.world.simulation.accounts->holdings,
        "Nominating a successor paid money or assigned ownership.");
    refuse(game,ProposeSuccession{request});
    auto bad=request;bad.handover_on=advance_days(state(game).date,Days{1});refuse(game,bad);
    apply(game,VoteSuccession{1,true});
    const auto fee=leadership_candidate(succession_catalog(state(game)),request.candidate).signing_cost;
    check(state(game).cash==checked_unit_subtract(before.world.simulation.cash,fee) && leadership(game).plans[0].status==SuccessionStatus::approved,
        "The approved successor lacks the actual paid preparation.");
    const auto incumbent=state(game).deployments->records.front();
    const auto service_before=project(game,incumbent.project_id).useful_tasks;
    const auto terms=state(game).accounts->recurring;
    game=reload(game);advance(game,OrdinalDay{request.handover_on.value()-1});
    check(leadership(game).terms.size()==1,"Succession took effect before its reviewed date.");
    advance(game,request.handover_on);
    check(leadership(game).terms.size()==2 && founder_role(leadership(game))==FounderRole::chair &&
        leadership(game).plans[0].status==SuccessionStatus::activated && leadership(game).terms.back().person==request.candidate,
        "The prepared and approved successor failed to take office.");
    check(fact(game,"company.leadership_handover_today") && fact(game,"company.founder_is_chair"),
        "Story inspection cannot read the actual dated handover and founder role.");
    check(state(game).accounts->holdings==before.world.simulation.accounts->holdings && leadership(game).founder_holder==original_holder &&
        state(game).deployments->records.front()==incumbent && project(game,incumbent.project_id).status==ProjectStatus::active &&
        project(game,incumbent.project_id).useful_tasks>service_before,"Succession reset ownership, deployed work or existing obligations.");
    for(const auto& obligation:terms) if(!obligation.id.starts_with("leadership/")) {
        const auto& rows=state(game).accounts->recurring;
        const auto found=std::ranges::find(rows,obligation.id,&RecurringObligation::id);
        check(found!=rows.end() && *found==obligation,"A handover changed an existing company bill.");
    }
    const auto board=current_board(state(game));
    const auto founder=std::ranges::find(board,"board.founder",&BoardMember::id);
    check(founder!=board.end() && founder->seats==1 && std::ranges::any_of(board,[](const auto& b){return b.id=="leadership.ceo" && !b.player;}),
        "The chair handover failed to change actual board leverage.");
    const auto legacy=runtime::founder_legacy(game.current_state().world,game.current_state().world.company_id);
    check(legacy.cash==leadership(game).personal_cash && legacy.net_book_wealth==checked_unit_subtract(checked_unit_add(legacy.book_equity,legacy.cash),legacy.liabilities) &&
        legacy.cash.value()>0 && state(game).random==rng,"Legacy wealth duplicated company cash or succession consumed unrelated RNG.");
    apply(game,AcknowledgeSuccession{required_succession_notice(state(game))->id});
    apply(game,ProposePoolExpansion{BasisPoints{100}});apply(game,VoteBoardMotion{state(game).management->governance->motions.back().id,true});
    check(state(game).management->governance->motions.back().leadership_cursor==leadership(game).controls.size(),"A board vote lost its actual leadership rights boundary.");
    game=reload(game);
    const auto chair_end=to_ordinal({2028,1,1});advance(game,chair_end);
    check(founder_role(leadership(game))==FounderRole::trust && leadership(game).terms.size()==2 &&
        required_succession_notice(state(game))->kind=="chair_retired","The founder's chair role assumed unlimited personal longevity.");
    check(!fact(game,"company.leadership_handover_today") && !fact(game,"company.founder_is_chair"),
        "A later morning retained stale handover or personal chair facts.");
    game=reload(game);
    auto bytes=Json::parse(persistence::encode_save(p,game.checkpoint()));
    bytes["checkpoint"]["world"]["simulation"]["succession"]["personal_cash_cents"]=999999999999LL;
    bytes.erase("sha256");bytes["sha256"]=persistence::sha256(bytes.dump());
    bool rejected=false;try{(void)persistence::decode_save(p,bytes.dump());}catch(const persistence::PersistenceError&){rejected=true;}
    check(rejected,"A correctly checksummed founder cash windfall bypassed paid source validation.");
}
void cancellation(const runtime::CompiledProgram& p) {
    auto game=begin(p);apply(game,AcknowledgeSuccession{required_succession_notice(state(game))->id});service(game);
    const auto request=proposal(game,false);apply(game,ProposeSuccession{request});
    const auto cash=state(game).cash;apply(game,VoteSuccession{1,true});const auto paid=state(game).cash;
    apply(game,CancelSuccession{1});
    check(paid<cash && state(game).cash==paid && leadership(game).plans[0].status==SuccessionStatus::cancelled &&
        project(game,leadership(game).plans[0].project).status==ProjectStatus::cancelled && leadership(game).terms.size()==1,
        "Cancelling funded preparation refunded spent cash or retired the incumbent.");
    refuse(game,VoteSuccession{1,true});refuse(game,CancelSuccession{1});game=reload(game);
    auto overbooked=proposal(game,false);overbooked.allocation.daily_compute=MilliCUPerDay{300000};
    // More calendar room makes compute protection, rather than duration, bind.
    overbooked.handover_on=advance_days(state(game).date,Days{104});
    apply(game,overbooked);refuse(game,VoteSuccession{2,true});apply(game,CancelSuccession{2});
    apply(game,proposal(game,false));
    advance(game,leadership(game).plans.back().vote_before);
    check(leadership(game).plans.back().status==SuccessionStatus::expired,"An ignored successor was silently approved.");
    refuse(game,VoteSuccession{3,true});
    apply(game,proposal(game,false));
    apply(game,ReserveFunds{"Protected company funds",state(game).cash,true});refuse(game,VoteSuccession{4,true});
    check(leadership(game).terms.size()==1,"A refused unfunded transition displaced the incumbent.");
    (void)reload(game);
}
void caretaker(const runtime::CompiledProgram& p) {
    auto game=begin(p);const auto holdings=state(game).accounts->holdings;
    const auto deadline=leadership(game).terms.front().due_on;
    advance(game,OrdinalDay{deadline.value()-1});check(leadership(game).terms.size()==1,"The caretaker started before retirement.");
    advance(game,deadline);
    check(leadership(game).terms.size()==2 && leadership(game).terms.back().caretaker &&
        leadership(game).terms.back().birth_year>=1990 && founder_role(leadership(game))==FounderRole::trust &&
        state(game).accounts->holdings==holdings && required_succession_notice(state(game))->kind=="caretaker",
        "The unplanned retirement erased company control or invented a second founder stake.");
    refuse(game,AdvanceDay{});game=reload(game);
    if(game.current_state().world.competition) {
        const auto& rival=game.current_state().world.competition->companies.front().simulation;
        check(rival.succession && rival.succession->terms.size()==2 && rival.succession->terms.back().caretaker &&
            rival.succession->terms.back().starts_on==deadline && rival.succession->terms.front().name=="Lena Ortiz",
            "A competing institution retained an immortal executive or lost its original named founder.");
    }
    const auto& c=succession_catalog(state(game));
    for(const auto year:{2035,2114,2514}) {
        const auto candidates=leadership_candidates(c,to_ordinal({year,1,1}));
        check(!candidates.empty() && std::ranges::all_of(candidates,[&](const auto& person){return year-person.birth_year>=static_cast<int>(c.minimum_candidate_age) &&
            year-person.birth_year<=static_cast<int>(c.maximum_candidate_age);}),"Future candidate cohorts reuse an immortal executive.");
    }
}
void acquisition(const runtime::CompiledProgram& p) {
    auto game=begin(p);const auto& companies=game.current_state().world.competition->companies;
    check(companies.size()==1 && companies.front().simulation.succession.has_value(),"A rival lacks the same finite leadership charter.");
    const auto target=companies.front().definition_id;
    const auto seller_before=runtime::founder_legacy(game.current_state().world,target);
    apply(game,InvestEquity{"legacy.review_capital",Cents{20000000000LL},BasisPoints{1000},"legacy.review_capital"});
    check(game.proposeAcquisition(state(game).revision,target,true).accepted(),"The retained-equity acquisition could not be reviewed.");
    apply(game,VoteBoardMotion{state(game).management->governance->motions.back().id,true});
    const auto& records=game.current_state().world.competition->acquisitions;
    check(records.size()==1 && records.front().completed_on.has_value(),"The merger never committed its actual consideration.");
    const auto record=records.front();
    const auto seller=std::ranges::find(record.settlements,seller_before.holder,&runtime::SellerSettlement::holder);
    check(seller!=record.settlements.end() && seller->cash.value()>0 && seller->shares.value()>0,"The seller lacks actual cash and equity consideration.");
    const auto legacy=runtime::founder_legacy(game.current_state().world,target);
    check(legacy.cash==checked_unit_add(seller_before.cash,seller->cash) && legacy.positions.size()==1 &&
        legacy.positions.front().company==game.current_state().world.company_id && legacy.positions.front().denominator==10000U*static_cast<std::uint64_t>(record.shares.value()),
        "Acquisition erased the founding trust or duplicated retained target equity.");
    apply(game,InvestEquity{"legacy.later_capital",Cents{1000000000},BasisPoints{1000},"legacy.later_capital"});
    const auto diluted=runtime::founder_legacy(game.current_state().world,target);
    check(diluted.cash==legacy.cash && diluted.positions.front().numerator<legacy.positions.front().numerator,
        "The founder's pooled seller claim escaped later dilution or paid its sale cash twice.");
    (void)reload(game);
}
void maximum_board(Json source) {
    for(auto& row:source["definitions"]) if(row["id"]==scenario) {
        auto& catalog=row["management"]["governance"];
        for(auto& member:catalog["board"]) member["seats"]=1;
        for(unsigned i=0;i<4;++i) catalog["board"].push_back({{"id","board.additional."+std::to_string(i)},
            {"name","Additional director "+std::to_string(i)},{"seats",1},{"player",false},{"cash_floor_cents",0},{"maximum_dilution_bp",1000}});
        catalog["financing"]=Json::array();
        catalog.erase("public_listing");
        for(unsigned i=0;i<8;++i) {
            const auto id="wide.round."+std::to_string(i);
            catalog["financing"].push_back({{"id",id},{"name","Reviewed round "+std::to_string(i)},{"negotiation",id},
                {"proceeds_cents",1000000},{"covenant_cash_floor_cents",0},{"waiver_fee_cents",0},{"acquired_bp",10},
                {"monthly_interest_bp",0},{"term_days",0},{"board_seat",true}});
        }
    }
    const auto p=fixture(std::move(source));auto game=begin(p);
    for(unsigned i=0;i<8;++i) {
        apply(game,NegotiateCorporateFinance{"wide.round."+std::to_string(i)});
        apply(game,VoteBoardMotion{state(game).management->governance->motions.back().id,true});
    }
    check(current_board(state(game)).size()==16,"The maximum opening and financed board did not use its actual rights.");
    const auto request=proposal(game);apply(game,request);apply(game,VoteSuccession{1,true});advance(game,request.handover_on);
    check(current_board(state(game)).size()==17,"Succession lost a director at the supported board-size boundary.");
    apply(game,ProposePoolExpansion{BasisPoints{100}});apply(game,VoteBoardMotion{state(game).management->governance->motions.back().id,true});
    check(state(game).management->governance->motions.back().ballots.size()==17,"The post-handover corporate vote omitted a real seat.");
    game=reload(game);
    auto next=proposal(game,false);const auto available=leadership_candidates(succession_catalog(state(game)),next.handover_on);
    const auto person=std::ranges::find_if(available,[&](const auto& candidate){return candidate.id!=leadership(game).terms.back().person;});
    check(person!=available.end(),"No second successor is available for the maximum board fixture.");
    next.candidate=person->id;apply(game,next);apply(game,VoteSuccession{2,true});
    check(leadership(game).plans.back().ballots.size()==17,"The next succession vote omitted a real seat.");
    (void)reload(game);
}
void smoke_fixture(const runtime::CompiledProgram& p,const std::filesystem::path& directory) {
    if(!std::filesystem::create_directories(directory)) throw std::runtime_error("Choose a new leadership smoke directory.");
    auto game=begin(p);apply(game,AcknowledgeSuccession{required_succession_notice(state(game))->id});service(game);
    persistence::atomic_write(directory/"collection.json",p.canonical_content_bytes,[](std::string_view){throw std::runtime_error("Fixture already exists.");},content::maximum_document_bytes,false);
    persistence::save_file(p,game.checkpoint(),persistence::local_paths(directory).saves_directory/"leadership-start.pgpsave");
    std::cout<<"Prepared leadership fixture at "<<directory<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        const auto source=Json::parse(persistence::read_bounded(PGP_SUCCESSION_SOURCE,content::maximum_document_bytes));
        const auto p=fixture(source);
        if(argc==3 && std::string_view{argv[1]}=="--smoke-fixture") {smoke_fixture(p,argv[2]);return 0;}
        if(argc!=1) throw std::runtime_error("Use --smoke-fixture DIRECTORY or no arguments.");
        auto unavailable=source;
        for(auto& row:unavailable["definitions"]) if(row["id"]==scenario)
            for(auto& track:row["management"]["succession"]["tracks"]) track["first_birth_year"]=2200;
        bool refused_opening=false;try{(void)compile(unavailable);}catch(const std::exception&){refused_opening=true;}
        check(refused_opening,"The authoring compiler accepted retirement before any successor cohort could become eligible.");
        auto collision=source;
        for(auto& row:collision["definitions"]) if(row["id"]==scenario)
            row["management"]["governance"]["board"][1]["id"]="leadership.ceo";
        bool refused_seat=false;try{(void)compile(collision);}catch(const std::exception&){refused_seat=true;}
        check(refused_seat,"An authored director collided with the successor's reserved board identity.");
        const auto authored=compile(source);auto original=begin(authored);
        check(leadership(original).terms.front().person=="founder" && leadership(original).terms.front().due_on==to_ordinal({2034,1,1}),"The authored founder term differs from its finite charter.");
        const auto rivals=fixture(source,true);
        handover(p);cancellation(p);caretaker(rivals);acquisition(rivals);maximum_board(source);
        std::cout<<"Succession and founder legacy passed ("<<checks<<" checks).\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
