#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/authority.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/runtime/society.hpp>
#include <pgp/session/campaign.hpp>
#include <pgp/session/milestone.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>

namespace {
using namespace pgp;
using namespace core;
using Json=nlohmann::json;
constexpr std::string_view europe="region.europe";
constexpr std::string_view service="product.education_model";
constexpr std::string_view report="evidence.institutions";
unsigned checks=0;
void check(bool condition,const char* message) {
    ++checks;
    if(!condition) throw std::runtime_error(message);
}
const runtime::WorldState& world(const runtime::Runtime& game) {return game.current_state().world;}
const SimulationState& state(const runtime::Runtime& game) {return world(game).simulation;}
const AuthorityState& institutions(const runtime::Runtime& game) {return *world(game).authority;}
const GovernanceState& board(const runtime::Runtime& game) {return *state(game).management->governance;}
const CapacityProject& project(const runtime::Runtime& game,std::string_view id) {
    const auto& projects=state(game).capacity->projects;
    const auto row=std::ranges::find(projects,id,&CapacityProject::id);
    if(row==projects.end()) throw std::runtime_error("Missing funded project "+std::string(id));
    return *row;
}
void apply(runtime::Runtime& game,CommandAction action) {
    const auto result=game.submit({state(game).revision,std::move(action)});
    if(!result.accepted()) throw std::runtime_error(iso_date(state(game).date)+" · "+result.explanation);
}
void refuse(runtime::Runtime& game,CommandAction action) {
    const auto before=game.checkpoint();
    check(!game.submit({state(game).revision,std::move(action)}).accepted() && before==game.checkpoint(),
        "A refused institutional action changed the campaign.");
}
void advance(runtime::Runtime& game,OrdinalDay to) {
    while(state(game).date<to) {
        if(const auto warning=liquidity_report(*state(game).liquidity).pending_warning) apply(game,AcknowledgeLiquidityWarning{*warning});
        if(const auto* alert=required_site_alert(state(game))) apply(game,AcknowledgeSiteAlert{alert->id});
        if(required_management_notice(state(game))) {
            const auto& notices=state(game).management->notices;
            const auto row=std::ranges::find_if(notices,[](const auto& n){return n.required && !n.acknowledged;});
            apply(game,AcknowledgeManagementNotice{row->id});
            continue;
        }
        if(required_governance_notice(state(game))) {
            const auto row=std::ranges::find_if(board(game).disputes,[](const auto& d){return !d.acknowledged;});
            apply(game,RespondCorporateDispute{row->id,false});
            continue;
        }
        if(runtime::required_public_case(world(game))) {
            const auto row=std::ranges::find_if(institutions(game).cases,[](const auto& c){return !c.acknowledged;});
            apply(game,AcknowledgePublicCase{row->id});
            continue;
        }
        apply(game,AdvanceDay{});
    }
}
runtime::Runtime reload(const runtime::Runtime& game) {
    const auto& p=game.program();
    try {
        auto loaded=persistence::decode_save(p,persistence::encode_save(p,game.checkpoint()));
        check(loaded.checkpoint==game.checkpoint(),"Institutional state changed on exact reload.");
        return {p,std::move(loaded.checkpoint)};
    } catch(const std::exception& error) {
        throw std::runtime_error("Reload on "+iso_date(state(game).date)+": "+error.what());
    }
}
void reject_forgery(const runtime::Runtime& game,auto edit) {
    auto bytes=Json::parse(persistence::encode_save(game.program(),game.checkpoint()));
    edit(bytes["checkpoint"]["world"]);bytes.erase("sha256");bytes["sha256"]=persistence::sha256(bytes.dump());
    bool rejected=false;
    try {(void)persistence::decode_save(game.program(),bytes.dump());}catch(const persistence::PersistenceError&) {rejected=true;}
    check(rejected,"A correctly checksummed forged authority record was accepted.");
}
runtime::CompiledProgram program() {
    auto source=Json::parse(persistence::read_bounded(PGP_AUTHORITY_SOURCE,content::maximum_document_bytes));
    Json governance;
    for(const auto& row:source["definitions"]) if(row["id"]=="scenario.world_reduced") governance=row["management"]["governance"];
    for(auto& executive:governance["executives"]) {
        if(executive["id"]=="executive.research") executive["promise_term_days"]=30;
        if(executive["id"]=="executive.product") {
            executive["promise_term_days"]=30;
            executive["publication_research"]="research.held_out_evaluation";
        }
    }
    for(auto& row:source["definitions"]) {
        if(row["id"]=="scenario.first_playable") {
            row["flows"]={"authority.public_review","authority.monthly_review","authority.executive_disclosure"};
            row["management"]["governance"]=governance;
        }
        // Exercise a resistance escalation within one settled month.
        if(row["kind"]=="institutional_authority") {
            row["catalog"]["resistance_growth_bp"]=1500;
            row["catalog"]["resistance_threshold_bp"]=3000;
        }
    }
    const auto registry=content::foundation_registry();
    return runtime::compile_collection(content::load_collection(source.dump(),registry),registry);
}
runtime::Runtime begin(const runtime::CompiledProgram& program) {
    runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.first_playable","US-WA")};
    check(game.prepare_opening(state(game).revision).accepted(),"Institutional opening did not prepare.");
    return game;
}
std::uint64_t latest_public_delivery(const runtime::Runtime& game) {
    std::uint64_t tasks=0;
    for(const auto& row:state(game).market->days.back().discretionary)
        if(row.product_id==service && row.segment_id=="segment.europe.public_services") tasks+=row.tasks;
    return tasks;
}
RequestInstitutionalMandate role(std::string offer,std::uint64_t promise=0) {
    return {std::move(offer),std::string(europe),std::string(service),"lease.initial","",1,promise};
}
void public_paths(const runtime::CompiledProgram& p) {
    auto game=begin(p);
    refuse(game,SubmitPublicTestimony{std::string(report),0});
    apply(game,InvestEquity{"engineering.capital",Cents{650000000},BasisPoints{500},"authority_test.capital"});
    apply(game,BuyData{"data.open_documents"});
    apply(game,StartResearch{"research.held_out_evaluation",{"lease.initial",MilliCUPerDay{20000},2}});
    apply(game,StartResearch{"research.reproducible_tooling",{"lease.initial",MilliCUPerDay{10000},1}});
    apply(game,StartTraining{{"model.institutions","recipe.document_baseline","",{"data.open_documents"},"lease.initial",MilliCUPerDay{60000},3,MilliCU{3600000}}});
    advance(game,OrdinalDay{60});
    apply(game,StartEvaluation{std::string(report),"model.institutions","evaluation.held_out",{"lease.initial",MilliCUPerDay{20000},1}});
    advance(game,OrdinalDay{64});
    apply(game,DeployModel{std::string(service),"model.institutions",std::string(report),"",false,std::string(europe)});
    apply(game,BuyMediaCampaign{std::string(europe),std::string(service),std::string(report),2});
    check(runtime::authority_snapshot(world(game),state(game).date).population_coverage==0,"Advertising bought governing authority.");
    refuse(game,MakePublicPromise{PublicPromiseKind::public_work,"region.india",std::string(service)});
    const auto identity=runtime::public_identity(world(game),state(game).date);
    apply(game,SubmitPublicTestimony{std::string(report),0});
    const auto disclosed=runtime::public_identity(world(game),state(game).date);
    check(disclosed[0]<identity[0] && disclosed[1]>identity[1],"Investors and researchers reacted as a single reputation score.");
    refuse(game,SubmitPublicTestimony{std::string(report),0});
    apply(game,MakePublicPromise{PublicPromiseKind::public_work,std::string(europe),std::string(service)});
    apply(game,MakePublicPromise{PublicPromiseKind::publication,"","research.held_out_evaluation"});
    apply(game,MakePublicPromise{PublicPromiseKind::publication,"","research.reproducible_tooling"});
    refuse(game,role("institution.delegation",1));
    advance(game,OrdinalDay{94});
    check(institutions(game).promises[0].met_on.has_value() && institutions(game).promises[0].progress>=institutions(game).catalog.promise_tasks,
        "Actual European public work did not satisfy the commitment.");
    check(institutions(game).cases.size()==2 && public_case_phase(institutions(game).cases.front(),state(game).date,institutions(game).catalog)=="concern",
        "Unpublished commitments failed to open concrete concerns.");
    refuse(game,AdvanceDay{});
    apply(game,SubmitPublicTestimony{std::string(report),1});
    refuse(game,NegotiatePublicCase{1,"lease.initial"});
    const auto formal=advance_days(state(game).date,institutions(game).catalog.concern_period);
    advance(game,formal);
    check(runtime::required_public_case(world(game)),"Formal recommendation did not renew the notice.");
    const auto fee=institutions(game).catalog.remedy_fee;
    auto cash=state(game).cash;
    apply(game,NegotiatePublicCase{1,"lease.initial"});
    check(state(game).cash==checked_unit_subtract(cash,fee) && project(game,"authority.remedy/1").staff>0,
        "Negotiation did not pay for and reserve real work.");
    refuse(game,NegotiatePublicCase{1,"lease.initial"});
    refuse(game,CancelProject{"authority.remedy/1"});
    refuse(game,WithdrawOperations{});
    game=reload(game);
    apply(game,PublishResearch{"research.held_out_evaluation"});
    check(institutions(game).cases[0].resolved_on==state(game).date &&
        project(game,"authority.remedy/1").status==ProjectStatus::cancelled,"Actual publication failed to resolve and release review work.");
    advance(game,institutions(game).cases[1].deadline);
    check(institutions(game).cases[1].disclosed_on==state(game).date,"An unfulfilled promise did not reach its warned disclosure.");
    apply(game,PublishResearch{"research.reproducible_tooling"});
    check(institutions(game).promises[2].closed_on==state(game).date && !institutions(game).promises[2].met_on,
        "Publishing after the disclosure retroactively kept a broken promise.");
    game=reload(game);

    advance(game,to_ordinal({2014,5,1}));
    const auto ordinary_staff=state(game).capacity->available_staff;
    cash=state(game).cash;
    apply(game,role("institution.standards"));
    apply(game,role("institution.delegation",1));
    const auto& catalog=institutions(game).catalog;
    const auto fees=checked_unit_add(authority_offer(catalog,"institution.standards").setup,authority_offer(catalog,"institution.delegation").setup);
    check(state(game).capacity->available_staff==ordinary_staff+2 && state(game).cash==checked_unit_subtract(cash,fees),
        "Institutional applications did not hire and pay their exact funded teams.");
    refuse(game,role("institution.standards"));
    refuse(game,CancelProject{"authority.operation/1"});
    refuse(game,AddRecurringObligation{{"authority.payroll/99",Cents{1},state(game).date,{},"Forged contract payroll"}});
    refuse(game,DirectRegionalProcurement{1,true});
    const auto decision=institutions(game).mandates.front().decision_on;
    check(session::plan_milestone(game.checkpoint(),"institutions").deadline==decision,"Institutional fast-forward skipped a pending decision.");
    game=reload(game);
    advance(game,decision);
    const auto region=society_region_index(world(game).society->catalog,europe);
    auto scores=runtime::authority_snapshot(world(game),decision);
    check(scores.soft[region]==7000 && scores.hard[region]==0 &&
        scores.population_coverage==world(game).society->catalog.regions[region].population_weight &&
        scores.output_coverage==world(game).society->catalog.regions[region].output_weight,
        "Real regional grants did not produce the expected weighted coverage.");
    const auto limit=institutions(game).catalog.procurement_floor;
    check(runtime::society_evidence(state(game),report,0,decision)->lower[0]<limit,"Procurement fixture already meets the higher floor.");
    check(latest_public_delivery(game)>0,"No European public work before procurement.");
    apply(game,DirectRegionalProcurement{1,true});
    refuse(game,DirectRegionalProcurement{1,false});
    auto shortfall=reload(game);
    advance(shortfall,to_ordinal({2014,6,1}));
    check(std::ranges::count_if(institutions(shortfall).cases,[](const auto& row){
        return row.kind==PublicCaseKind::delivery && !row.resolved_on;
    })==2,"A month of blocked public work failed to open delivery cases for both grants.");
    apply(shortfall,DirectRegionalProcurement{1,false});
    advance(shortfall,advance_days(state(shortfall).date,institutions(shortfall).catalog.concern_period));
    check(std::ranges::all_of(institutions(shortfall).cases,[](const auto& row){
        return row.kind!=PublicCaseKind::delivery || row.resolved_on.has_value();
    }),"Restored service and real operating work failed to remedy both delivery shortfalls.");
    (void)reload(shortfall);
    advance(game,advance_days(decision,institutions(game).catalog.directive_spacing));
    check(latest_public_delivery(game)==0,"Binding procurement failed to stop real deliveries below the evidence floor.");
    apply(game,DirectRegionalProcurement{1,false});
    advance(game,advance_days(state(game).date,Days{1}));
    check(latest_public_delivery(game)>0,"Eligible public work did not resume after procurement was withdrawn.");
    apply(game,RelinquishInstitutionalMandate{1});
    check(state(game).capacity->available_staff==ordinary_staff+1 &&
        runtime::authority_snapshot(world(game),state(game).date).soft[region]==4000,"Relinquishing a grant kept its score or contract team.");
    game=reload(game);
    reject_forgery(game,[](Json& w){w["authority"]["mandates"][1]["reviews"][0]["score_bp"]=10000;});

    apply(game,role("institution.coercive"));
    advance(game,institutions(game).mandates.back().decision_on);
    scores=runtime::authority_snapshot(world(game),state(game).date);
    check(scores.hard[region]>6000 && scores.soft[region]==4000,"The funded hard role did not carry separate control and resistance.");
    advance(game,to_ordinal({2014,7,1}));
    const auto resistance=std::ranges::find_if(institutions(game).cases,[](const auto& c){return c.kind==PublicCaseKind::resistance;});
    check(resistance!=institutions(game).cases.end(),"Coercion did not generate a concrete resistance case.");
    const auto id=resistance->id;
    const auto opened=resistance->opened_on;
    const auto deadline=resistance->deadline;
    apply(game,SubmitPublicTestimony{std::string(report),id});
    advance(game,advance_days(opened,institutions(game).catalog.concern_period));
    apply(game,NegotiatePublicCase{id,"lease.initial"});
    const auto extended=advance_days(deadline,institutions(game).catalog.extension);
    game=reload(game);
    advance(game,extended);
    check(institutions(game).cases[id-1].disclosed_on==extended &&
        institutions(game).mandates.back().revoked_on==extended &&
        state(game).capacity->available_staff==ordinary_staff+1 &&
        runtime::authority_snapshot(world(game),extended).hard[region]==0,"Unresolved resistance failed to remove hard authority and its operating team.");
    advance(game,advance_days(extended,Days{1}));
    check(latest_public_delivery(game)==0,"Resistance sanctions did not affect actual market settlement.");
    game=reload(game);
    reject_forgery(game,[](Json& w){w["authority"]["months"].back()["population_coverage_bp"]=10000;});
    const auto expiry=institutions(game).mandates[1].ends_before;
    advance(game,expiry);
    check(state(game).capacity->available_staff==ordinary_staff && runtime::authority_snapshot(world(game),expiry).soft[region]==0,
        "Expired authority left continuing decision rights or paid staff.");
    (void)reload(game);

    const BuildOwnedSite build{"institutions","Public computing site","US-WA",20000,CoolingMode::air};
    const auto site=preview_site_build(state(game),build);
    apply(game,build);
    apply(game,OrderOwnedHardware{"institutions","institutions","rack.reference"});
    apply(game,HireSiteOperators{"institutions","institutions",1});
    apply(game,ScheduleSiteAppointment{"institutions","institutions",std::max(site.built_on,site.grid_on),AppointmentMode::remote});
    apply(game,DeployModel{"product.document_classification","model.institutions",std::string(report),"lease.initial",false,"region.us"});
    advance(game,advance_days(std::max(site.built_on,site.grid_on),Days{40}));
    auto infrastructure=role("institution.infrastructure");
    infrastructure.region="region.us";
    infrastructure.service="product.document_classification";
    infrastructure.site="institutions";
    refuse(game,infrastructure);
    infrastructure.source="owned/institutions";
    apply(game,infrastructure);
    advance(game,institutions(game).mandates.back().decision_on);
    const auto usa=society_region_index(world(game).society->catalog,"region.us");
    check(runtime::authority_snapshot(world(game),state(game).date).soft[usa]==3000 &&
        project(game,institutions(game).mandates.back().project).source_id==infrastructure.source,
        "Infrastructure authority did not bind an operating owned site and its actual work.");
    (void)reload(game);
}
void executive_paths(const runtime::CompiledProgram& p) {
    auto game=begin(p);
    const auto ordinary_staff=state(game).capacity->available_staff;
    apply(game,AppointExecutive{"executive.research",true});
    apply(game,StartResearch{"research.reproducible_tooling",{"lease.initial",MilliCUPerDay{10000},1}});
    advance(game,OrdinalDay{30});
    const auto dispute=board(game).disputes.front().id;
    const auto original_deadline=board(game).disputes.front().deadline;
    refuse(game,NegotiateExecutiveDispute{dispute,"lease.initial"});
    const auto& terms=*state(game).management->catalog.governance;
    advance(game,advance_days(state(game).date,terms.concern_period));
    const auto cash=state(game).cash;
    apply(game,NegotiateExecutiveDispute{dispute,"lease.initial"});
    check(board(game).disputes.front().deadline>original_deadline &&
        state(game).cash==checked_unit_subtract(cash,institutions(game).catalog.remedy_fee),"Executive negotiation did not fund a bounded extension.");
    const auto deadline=board(game).disputes.front().deadline;
    const auto review=board(game).disputes.front().review_project;
    refuse(game,NegotiateExecutiveDispute{dispute,"lease.initial"});
    refuse(game,CancelProject{review});
    auto failed=reload(game);
    apply(game,PublishResearch{"research.reproducible_tooling"});
    check(board(game).disputes.front().status==CorporateDisputeStatus::resolved &&
        !board(game).appointments.front().departed_on && project(game,review).status==ProjectStatus::cancelled,
        "Publishing during the extension did not preserve the appointment and release review staff.");
    (void)reload(game);
    advance(failed,deadline);
    const auto events=runtime::public_identity_events(world(failed),deadline);
    check(board(failed).appointments.front().departed_on==deadline &&
        state(failed).capacity->available_staff==ordinary_staff &&
        std::ranges::any_of(events,[&](const auto& event){return event.day==deadline && event.effect==3;}),
        "A failed executive response did not produce the documented departure and public disclosure.");
    (void)reload(failed);

    auto interrupted=begin(p);
    apply(interrupted,AppointExecutive{"executive.product",false});
    advance(interrupted,OrdinalDay{5});
    apply(interrupted,AppointExecutive{"executive.research",false});
    const auto concern=state(interrupted).management->catalog.governance->concern_period;
    advance(interrupted,advance_days(OrdinalDay{35},concern));
    check(board(interrupted).disputes.size()==2,"Concurrent executive promises lost their distinct cases.");
    const auto first_deadline=board(interrupted).disputes.front().deadline;
    const auto second=board(interrupted).disputes.back().id;
    apply(interrupted,NegotiateExecutiveDispute{second,"lease.initial"});
    const auto work=board(interrupted).disputes.back().review_project;
    advance(interrupted,first_deadline);
    check(project(interrupted,work).stopped_on==first_deadline && !board(interrupted).disputes.back().resolved_on,
        "A real executive departure did not interrupt the other review while preserving its deadline.");
    (void)reload(interrupted);
}
}

int main() {
    try {
        const auto p=program();
        public_paths(p);
        executive_paths(p);
        std::cout<<"Authority paths passed ("<<checks<<" checks).\n";
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
