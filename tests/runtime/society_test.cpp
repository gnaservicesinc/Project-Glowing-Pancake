#include <pgp/runtime/scenario.hpp>
#include <pgp/runtime/society.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/session/milestone.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <numeric>

namespace {
using namespace pgp;
using namespace core;
using Json=nlohmann::json;
int checks=0;
void check(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
const runtime::WorldState& world(const runtime::Runtime& game) {return game.current_state().world;}
const SimulationState& state(const runtime::Runtime& game) {return world(game).simulation;}
void apply(runtime::Runtime& game,CommandAction action) {
    const auto result=game.submit({state(game).revision,std::move(action)});
    if(!result.accepted()) throw std::runtime_error(iso_date(state(game).date)+" · "+result.explanation);
}
void refuse(runtime::Runtime& game,CommandAction action) {
    const auto original=game.checkpoint();
    check(!game.submit({state(game).revision,std::move(action)}).accepted() && game.checkpoint()==original,
        "A refused society action changed the campaign.");
}
void advance(runtime::Runtime& game,OrdinalDay to) {
    while(state(game).date<to) {
        if(const auto alert=liquidity_report(*state(game).liquidity).pending_warning) apply(game,AcknowledgeLiquidityWarning{*alert});
        if(const auto* alert=required_site_alert(state(game))) apply(game,AcknowledgeSiteAlert{alert->id});
        if(required_management_notice(state(game))) {
            const auto& notices=state(game).management->notices;
            const auto row=std::find_if(notices.begin(),notices.end(),[](const auto& alert){return alert.required && !alert.acknowledged;});
            apply(game,AcknowledgeManagementNotice{row->id});continue;
        }
        apply(game,AdvanceDay{});
    }
}
runtime::Runtime begin(const runtime::CompiledProgram& program,std::string_view scenario="scenario.first_playable") {
    runtime::Runtime game{program,runtime::begin_scenario(program,scenario,"US-WA")};
    check(game.prepare_opening(state(game).revision).accepted(),"The society opening did not prepare.");return game;
}
runtime::Runtime reload(const runtime::Runtime& game) {
    const auto bytes=persistence::encode_save(game.program(),game.checkpoint());
    auto loaded=persistence::decode_save(game.program(),bytes);
    check(loaded.checkpoint==game.checkpoint(),"Regional society changed on exact save reload.");
    return {game.program(),std::move(loaded.checkpoint)};
}
void forged(const runtime::Runtime& game,auto change) {
    auto data=Json::parse(persistence::encode_save(game.program(),game.checkpoint()));
    change(data["checkpoint"]["world"]);data.erase("sha256");data["sha256"]=persistence::sha256(data.dump());
    bool rejected=false;try {(void)persistence::decode_save(game.program(),data.dump());}catch(const persistence::PersistenceError&) {rejected=true;}
    check(rejected,"A correctly checksummed forged society record was accepted.");
}
runtime::CompiledProgram program() {
    auto source=Json::parse(persistence::read_bounded(PGP_SOCIETY_SOURCE,content::maximum_document_bytes));
    for(auto& row:source["definitions"]) if(row["kind"]=="scenario") row["flows"]={"society.monthly_briefing","society.forum_briefing"};
    const auto registry=content::foundation_registry();
    return runtime::compile_collection(content::load_collection(source.dump(),registry),registry);
}
void hand_audit(SocietyCatalog catalog) {
    for(auto& row:catalog.regions) {row.initial.fill(5000);row.adaptation=BasisPoints{5000};}
    auto society=opening_society(catalog,OrdinalDay{0});
    std::array<RegionOutcomes,society_regions> january{},empty{};
    for(auto& row:january) {
        row.demand=10000;row.delivered=10000;row.public_demand=2000;row.public_work=2000;
        row.science_work=1000;row.industrial_work=2000;row.agent_work=1000;row.compute_milli=40000000;
        row.quality=BasisPoints{7000};row.concentration=BasisPoints{5000};
    }
    for(auto day=OrdinalDay{0};day<=to_ordinal({2014,4,1});day=advance_days(day,Days{1})) {
        if(day.value()>0) {
            const auto previous=OrdinalDay{day.value()-1};const auto civil=from_ordinal(previous);
            if(civil.day==days_in_month(civil.year,civil.month)) close_society_month(society,previous,civil.month==1?january:empty);
        }
        prepare_society(society,day);
    }
    check(society.months[0].components[0][0]==5000 && society.months[1].components[0][0]==5000,
        "Welfare moved before its two-month observation lag.");
    const auto& march=society.months[2].components[0];
    check(march[0]==6625 && march[1]==5700 && march[2]==6667,
        "The hand-calculated food, housing and access transition differs from half-up adaptation.");
    check(welfare_indices(march)[0]==6330 && global_welfare(society)[0]==6330 && minimum_welfare(society)[0]==6330,
        "Equal component weights or population/minimum reporting changed.");
    validate_society(society,to_ordinal({2014,4,1}));
    auto inequality=catalog;
    for(std::size_t i=0;i<society_regions;++i) inequality.regions[i].initial.fill(i==0?0:10000);
    const auto unequal=opening_society(inequality,OrdinalDay{0});
    check(global_welfare(unequal)[0]==8200 && minimum_welfare(unequal)[0]==0,
        "The population-weighted value hid a low region or used output weights.");
    auto token=opening_society(catalog,OrdinalDay{0});
    auto tiny=january;
    for(auto& row:tiny) {
        row.demand=1'000'000'000;row.delivered=1;row.public_demand=100'000'000;row.public_work=0;
        row.science_work=row.industrial_work=row.agent_work=0;row.compute_milli=20;
        row.quality=BasisPoints{10000};row.concentration=BasisPoints{10000};
    }
    close_society_month(token,to_ordinal({2014,1,31}),tiny);
    close_society_month(token,to_ordinal({2014,2,28}),empty);
    close_society_month(token,to_ordinal({2014,3,31}),empty);
    check(token.months.back().components.front()==catalog.regions.front().initial,
        "A token delivery awarded region-wide quality or concentration effects without meaningful adoption.");
    catalog.forum.quorum=8;
    auto shortfall=opening_society(catalog,OrdinalDay{0});
    const auto ballot=advance_days(OrdinalDay{0},catalog.forum.hearing);
    for(auto day=OrdinalDay{0};day<=ballot;day=advance_days(day,Days{1})) prepare_society(shortfall,day);
    check(shortfall.rounds.front().members.size()==8 && std::none_of(shortfall.rounds.front().members.begin(),shortfall.rounds.front().members.end(),
        [](const auto& row){return row.effective_from.has_value();}),"A standards vote implemented without quorum.");
}
std::uint64_t preference_weight(const runtime::Runtime& game) {
    const auto& s=state(game);
    const auto opportunity=market_opportunity(*s.market,*s.capacity,s.date,nullptr,runtime::society_market_conditions(world(game),s,s.date));
    for(const auto& segment:opportunity.segments) if(segment.id=="segment.europe.public_services")
        for(const auto& edge:segment.edges) if(edge.product_id=="product.education_model") return edge.weight;
    throw std::runtime_error("The expected local education opportunity is absent.");
}
void blocked_promises(const runtime::Runtime& game) {
    Simulation company{state(game)};
    const auto submit=[&](CommandAction action) {
        const auto result=company.submit({company.checkpoint().revision,std::move(action)});
        if(!result.accepted()) throw std::runtime_error(result.explanation);
    };
    submit(DeployModel{"product.documents","model.society","evidence.society","lease.initial",false,"region.us"});
    MarketContract contract;contract.id="contract.restricted";contract.product_id="product.documents";contract.segment_id="segment.enterprise";
    contract.tasks_per_day=10;contract.monthly_fee=Cents{3100};contract.missing_task_penalty=Cents{5};
    contract.accepted_on=state(game).date;contract.start_by=state(game).date;contract.ends_before=advance_days(state(game).date,Days{30});contract.cause="Signed restricted-service fixture";
    submit(AcceptContract{contract});submit(StartContract{contract.id});
    auto market=*company.checkpoint().market;auto accounts=*company.checkpoint().accounts;auto capacity=*company.checkpoint().capacity;
    MarketInfluences influences;
    for(const auto& segment:market.segments) influences.push_back({"product.documents",segment.id,BasisPoints{0},false});
    close_market_day(market,accounts,capacity,state(game).date,true,nullptr,nullptr,{},influences);
    const auto& result=market.days.back().contracts.front();
    check(result.promised_tasks==10 && result.delivered_tasks==0 && result.gross_fee==Cents{100} &&
        result.service_credit==Cents{100} && result.penalty==Cents{50},"Restricted signed work escaped its exact credit and missed-task penalty.");
    check(std::none_of(market.days.back().discretionary.begin(),market.days.back().discretionary.end(),
        [](const auto& row){return row.product_id=="product.documents" && row.tasks>0;}),"A restricted service bypassed the discretionary market gate.");
}
void paid_actions_and_outcomes(const runtime::CompiledProgram& program) {
    auto game=begin(program);
    check(world(game).society->rounds.size()==1 && world(game).society->months.empty(),"The opening forum or monthly cursor is missing.");
    refuse(game,BuyMediaCampaign{"region.europe","product.education_model","made-up",2});
    refuse(game,EndorseForumStandard{1,"region.europe","made-up",true});
    AdvanceDay forged_day;forged_day.market_influences.push_back({"any","any",BasisPoints{3000},true});refuse(game,forged_day);
    check(session::plan_milestone(game.checkpoint(),"society").deadline==to_ordinal({2014,2,1}),"Regional fast-forward missed the next observation boundary.");
    const auto initial=global_welfare(*world(game).society);
    apply(game,BuyData{"data.open_documents"});
    apply(game,StartResearch{"research.held_out_evaluation",{"lease.initial",MilliCUPerDay{20000},2}});
    apply(game,StartResearch{"research.reproducible_tooling",{"lease.initial",MilliCUPerDay{10000},1}});
    apply(game,StartTraining{{"model.society","recipe.document_baseline","",{"data.open_documents"},"lease.initial",MilliCUPerDay{60000},3,MilliCU{3600000}}});
    advance(game,OrdinalDay{60});
    apply(game,StartEvaluation{"evidence.society","model.society","evaluation.held_out",{"lease.initial",MilliCUPerDay{20000},1}});
    advance(game,OrdinalDay{64});
    apply(game,DeployModel{"product.education_model","model.society","evidence.society","",false,"region.europe"});
    blocked_promises(game);
    check(global_welfare(*world(game).society)==initial,"Spending on research and deployment directly awarded welfare.");
    const auto before=game.checkpoint();const auto weight=preference_weight(game);
    apply(game,ReserveFunds{"Protect media cash",state(game).cash,true});
    refuse(game,BuyMediaCampaign{"region.europe","product.education_model","evidence.society",2});
    apply(game,CancelReservation{1});
    const auto cash=state(game).cash;const auto random=state(game).random;
    const auto quote=runtime::preview_media_campaign(world(game),{"region.europe","product.education_model","evidence.society",2});
    check(game.checkpoint().world.society->campaigns.empty() && quote.awareness==BasisPoints{1800},"A media preview changed state or its quoted first return.");
    apply(game,BuyMediaCampaign{"region.europe","product.education_model","evidence.society",2});
    check(preference_weight(game)>weight && state(game).random==random && state(game).lab==before.world.simulation.lab &&
        world(game).society->months==before.world.society->months && state(game).cash.value()==cash.value()-quote.paid.value(),
        "Advertising failed to change awareness or changed evidence, welfare, RNG or the exact fee.");
    apply(game,BuyMediaCampaign{"region.europe","product.education_model","evidence.society",2});
    apply(game,BuyMediaCampaign{"region.europe","product.education_model","evidence.society",2});
    const auto& campaigns=world(game).society->campaigns;
    check(campaigns[1].awareness.value()==900 && campaigns[2].awareness.value()==600 &&
        campaign_awareness(*world(game).society,"region.europe","product.education_model",state(game).date)==BasisPoints{3000},
        "Repeat media did not diminish or total customer awareness escaped its cap.");
    check(campaign_awareness(*world(game).society,"region.europe","product.education_model",OrdinalDay{campaigns.front().ends_before.value()-1}).value()>0 &&
        campaign_awareness(*world(game).society,"region.europe","product.education_model",campaigns.front().ends_before).value()==0,
        "Media awareness lost its exclusive expiration boundary.");
    refuse(game,BuyMediaCampaign{"region.europe","product.education_model","evidence.society",5});
    refuse(game,BuyMediaCampaign{"region.invalid","product.education_model","evidence.society",2});
    apply(game,DeployModel{"product.education_model","model.society","evidence.society","",false,"region.europe"});
    (void)reload(game);
    apply(game,StartTraining{{"model.weak","recipe.document_baseline","",{"data.open_documents"},"lease.initial",MilliCUPerDay{60000},3,MilliCU{1800000}}});
    advance(game,OrdinalDay{94});
    apply(game,StartEvaluation{"evidence.weak","model.weak","evaluation.held_out",{"lease.initial",MilliCUPerDay{20000},1}});
    advance(game,OrdinalDay{98});
    check(runtime::society_evidence(state(game),"evidence.weak",0,state(game).date)->lower[0]<world(game).society->catalog.forum.evidence_floor,
        "The short-run fixture unexpectedly satisfies the standards threshold.");
    apply(game,DeployModel{"product.workflow_agent","model.weak","evidence.weak","lease.initial",true,"region.europe"});
    apply(game,EndorseForumStandard{1,"region.india","evidence.society",true});
    refuse(game,EndorseForumStandard{1,"region.india","evidence.society",false});
    apply(game,StartEvaluation{"evidence.headline","model.society","evaluation.headline",{"lease.initial",MilliCUPerDay{20000},1}});
    advance(game,OrdinalDay{100});
    refuse(game,EndorseForumStandard{1,"region.us","evidence.headline",true});
    const auto stale=game.checkpoint();
    check(!game.submit({state(game).revision-1,EndorseForumStandard{1,"region.china","evidence.society",true}}).accepted() &&
        stale==game.checkpoint(),"A stale forum review changed the world.");
    auto paused=reload(game);
    advance(game,to_ordinal({2014,5,1}));advance(paused,to_ordinal({2014,5,1}));
    check(game.checkpoint()==paused.checkpoint(),"A pending forum ballot rerolled or duplicated after reload.");
    const auto& round=world(game).society->rounds.front();
    check(round.members.size()==8 && round.members[5].supports && !round.members[3].supports,
        "The India endorsement did not affect its member vote or the declined member changed.");
    check(round.members[4].effective_from==advance_days(round.ballot_on,Days{45}) &&
        round.members[7].effective_from==advance_days(round.ballot_on,Days{75}),"Members lost their individual implementation delays.");
    refuse(game,EndorseForumStandard{1,"region.us","evidence.society",true});
    check(campaign_awareness(*world(game).society,"region.europe","product.education_model",state(game).date)==BasisPoints{0},
        "An expired media campaign kept its market preference boost.");
    check(world(game).society->months[2].outcomes[4].public_work>0 &&
        world(game).society->months[3].components[4]==world(game).society->months[2].components[4],
        "Public-service work was not observed or reached welfare before its lag.");
    advance(game,to_ordinal({2014,6,14}));
    check(!forum_enforced(*world(game).society,"region.europe",state(game).date),"Standards access was restricted before the member's implementation date.");
    auto pending=reload(game);
    advance(game,to_ordinal({2014,6,15}));advance(pending,to_ordinal({2014,6,15}));
    check(game.checkpoint()==pending.checkpoint() && forum_enforced(*world(game).society,"region.europe",state(game).date) &&
        !forum_enforced(*world(game).society,"region.us",state(game).date) &&
        !forum_enforced(*world(game).society,"region.china",state(game).date),"Regional enforcement escaped its accepting member or effective date.");
    advance(game,to_ordinal({2014,6,16}));
    const auto& blocked_day=state(game).market->days.back();
    check(std::none_of(blocked_day.discretionary.begin(),blocked_day.discretionary.end(),[](const auto& row){return row.product_id=="product.workflow_agent" && row.tasks>0;}),
        "The effective forum rule failed to stop actual agent work.");
    const DeployModel blocked{"product.workflow_agent","model.weak","evidence.weak","lease.initial",true,"region.europe"};
    check(!runtime::society_launch_block(&*world(game).society,state(game),blocked,state(game).date).empty(),"A weak agent report bypassed the effective evidence floor.");
    refuse(game,blocked);
    auto foreign=blocked;foreign.region_id="region.china";
    check(runtime::society_launch_block(&*world(game).society,state(game),foreign,state(game).date).empty(),"A declined forum member inherited another region's access law.");
    auto exact_floor=*world(game).society;
    exact_floor.catalog.forum.evidence_floor=runtime::society_evidence(state(game),"evidence.society",0,state(game).date)->lower[0];
    check(runtime::society_launch_block(&exact_floor,state(game),{"product.workflow_agent","model.society","evidence.society","lease.initial",true,"region.europe"},state(game).date).empty(),
        "An independent lower bound exactly on the threshold failed eligibility.");
    apply(game,DeployModel{"product.workflow_agent","model.society","evidence.society","lease.initial",true,"region.europe"});
    advance(game,advance_days(state(game).date,Days{1}));
    check(std::any_of(state(game).market->days.back().discretionary.begin(),state(game).market->days.back().discretionary.end(),
        [](const auto& row){return row.product_id=="product.workflow_agent" && row.tasks>0 && row.segment_id.starts_with("segment.europe.");}),
        "A qualifying independent report and redeployment did not restore actual regional delivery.");
    check(world(game).society->months[4].components[4]!=world(game).society->months[3].components[4] &&
        world(game).society->months[4].audiences[1]>5000,"Actual delayed services did not change welfare and research reputation.");
    forged(game,[](auto& w){w["society"]["months"][2]["outcomes"][4]["public_work"]=0;});
    forged(game,[](auto& w){auto& demand=w["society"]["months"].back()["outcomes"][4]["demand"];demand=demand.template get<std::uint64_t>()+1;});
    forged(game,[](auto& w){w["society"]["months"][4]["components_bp"][4][0]=10000;});
    forged(game,[](auto& w){w["society"]["rounds"][0]["members"][3]["effective_from"]=0;});
    forged(game,[](auto& w){w["society"]["campaigns"][0]["paid_cents"]=1;});
    forged(game,[](auto& w){w["society"]["campaigns"][0]["unknown"]=true;});
    forged(game,[](auto& w){for(auto& row:w["simulation"]["accounts"]["postings"]) if(row["key"].template get<std::string>().starts_with("society.media/")) {row["key"]="orphaned-payment";break;}});
    forged(game,[](auto& w){w.erase("society");});
    const auto education=std::find_if(state(game).market->products.begin(),state(game).market->products.end(),
        [](const auto& row){return row.id=="product.education_model";});
    apply(game,CancelProject{education->capacity_project_id});
    (void)reload(game);
}
void shared_roster(const runtime::CompiledProgram& program) {
    auto game=begin(program,"scenario.world_competition");
    advance(game,to_ordinal({2014,7,1}));
    const auto& latest=world(game).society->months.back();
    const auto total=std::accumulate(latest.outcomes.begin(),latest.outcomes.end(),std::uint64_t{0},[](auto sum,const auto& row){return sum+row.delivered;});
    check(total>0 && std::all_of(latest.outcomes.begin(),latest.outcomes.end(),[](const auto& row){return row.delivered<=row.demand;}),
        "The shared roster did not contribute actual work or duplicated regional customer demand.");
    (void)reload(game);
}
void contracted_standard(const runtime::CompiledProgram& original) {
    auto collection=original.collection;
    std::get<SocietyCatalog>(collection.definitions.at("society.global").data).forum.channel=ProductChannel::business;
    const auto program=runtime::compile_collection(std::move(collection),original.registry);
    auto game=begin(program);
    apply(game,BuyData{"data.open_documents"});
    apply(game,StartResearch{"research.held_out_evaluation",{"lease.initial",MilliCUPerDay{20000},2}});
    apply(game,StartTraining{{"model.contract","recipe.document_baseline","",{"data.open_documents"},"lease.initial",MilliCUPerDay{60000},3,MilliCU{2100000}}});
    advance(game,OrdinalDay{35});
    apply(game,StartEvaluation{"evidence.contract","model.contract","evaluation.held_out",{"lease.initial",MilliCUPerDay{20000},1}});
    advance(game,OrdinalDay{39});
    apply(game,DeployModel{"product.document_classification","model.contract","evidence.contract","lease.initial",false,"region.europe"});
    MarketContract promise;promise.id="contract.standard";promise.product_id="product.document_classification";promise.segment_id="segment.europe.enterprise";
    promise.tasks_per_day=10;promise.monthly_fee=Cents{3100};promise.missing_task_penalty=Cents{5};promise.accepted_on=state(game).date;
    promise.start_by=state(game).date;promise.ends_before=advance_days(state(game).date,Days{180});promise.cause="Business standard transition fixture";
    apply(game,AcceptContract{promise});apply(game,StartContract{promise.id});
    advance(game,to_ordinal({2014,6,14}));auto pending=reload(game);
    advance(game,to_ordinal({2014,6,15}));
    check(state(game).market->days.back().contracts.front().delivered_tasks==10,"The pre-implementation business contract did not deliver.");
    advance(game,to_ordinal({2014,6,16}));advance(pending,to_ordinal({2014,6,16}));
    const auto& claim=state(game).market->days.back().contracts.front();
    check(game.checkpoint()==pending.checkpoint() && claim.delivered_tasks==0 && claim.service_credit==claim.gross_fee &&
        claim.service_credit.value()>0 && claim.penalty==Cents{50},"An effective business standard bypassed exact promise credits, penalties or pending save replay.");
    (void)reload(game);
}
}
int main() {
    try {
        const auto p=program();
        hand_audit(std::get<SocietyCatalog>(p.collection.definitions.at("society.global").data));
        paid_actions_and_outcomes(p);contracted_standard(p);shared_roster(p);
        std::cout<<checks<<" society lag, forum, media and source-reconciled save checks passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
