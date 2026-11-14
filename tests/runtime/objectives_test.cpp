#include "objectives_internal.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>
#include "../content/slice_fixture.hpp"
#include <pgp/runtime/authority.hpp>
#include <pgp/session/campaign.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace {
using namespace pgp;
using namespace core;
using namespace runtime;
using Json=nlohmann::json;
constexpr auto frontier="scenario.century_frontier";
constexpr auto plateau="scenario.century_plateau";
std::size_t checks=0;
void check(bool ok,std::string_view reason) {
    if(!ok) throw std::runtime_error(std::string(reason));
    ++checks;
}
CompiledProgram compile(const Json& source) {
    const auto registry=content::foundation_registry();
    return compile_collection(content::load_collection(source.dump(),registry),registry);
}
Json source() {
    std::ifstream stream{PGP_OBJECTIVE_SOURCE};
    auto document=Json::parse(stream);
    // Keep the threshold calculations as an isolated numerical fixture.
    // Shipped player campaigns use the unscored retrospective.
    for(auto& definition:document["definitions"]) if(definition.contains("objectives"))
        definition["objectives"]["retrospective"]=false;
    // The scored fixture rewrites the century rivals' shared definitions.
    // Keep only the scenario variants that participate in those checks.
    auto& definitions=document["definitions"];
    const auto founder=std::ranges::find_if(definitions,[](const auto& row) {return row["id"]=="scenario.founder_campaign";});
    if(founder!=definitions.end()) definitions.erase(founder);
    pgp_test::prune_unused_flows(document);
    return document;
}
const SimulationState& state(const Runtime& game) {return game.checkpoint().world.simulation;}
const ObjectivesState& objectives(const Runtime& game) {return *game.checkpoint().world.objectives;}
void apply(Runtime& game,CommandAction action) {
    const auto outcome=game.submit({state(game).revision,std::move(action)});
    if(!outcome.accepted()) throw std::runtime_error(iso_date(state(game).date)+" · "+outcome.explanation);
}
void refuse(Runtime& game,CommandAction action,std::string_view reason) {
    const auto before=game.checkpoint();
    const auto result=game.submit({state(game).revision,std::move(action)});
    check(!result.accepted() && before==game.checkpoint() && result.explanation.find(reason)!=std::string::npos,
        "An inadmissible objective command changed the checkpoint or misidentified its cause.");
}
Runtime begin(const CompiledProgram& program,ObjectiveSelection selection,std::string_view id=frontier) {
    Runtime game{program,begin_scenario(program,id,"US-WA",std::move(selection))};
    const auto result=game.prepare_opening(state(game).revision);
    check(result.accepted(),result.explanation);
    return game;
}
Runtime reload(const Runtime& game,std::size_t* size=nullptr) {
    const auto& p=game.program();
    const auto bytes=persistence::encode_save(p,game.checkpoint());
    if(size) *size=bytes.size();
    auto loaded=persistence::decode_save(p,bytes);
    check(loaded.checkpoint==game.checkpoint() && persistence::encode_save(p,loaded.checkpoint)==bytes,
        "Objective or source evidence changed on save/reload.");
    return {p,std::move(loaded.checkpoint)};
}
void forged(const Runtime& game,const auto& change) {
    const auto& p=game.program();
    auto data=Json::parse(persistence::encode_save(p,game.checkpoint()));
    change(data["checkpoint"]["world"]["objectives"]);
    data.erase("sha256");data["sha256"]=persistence::sha256(data.dump());
    bool rejected=false;
    try {(void)persistence::decode_save(p,data.dump());}
    catch(const persistence::PersistenceError&) {rejected=true;}
    check(rejected,"A rehashed save forged an objective result or calendar boundary.");
}
void prepare_mandatory(Runtime& game) {
    if(const auto warning=liquidity_report(*state(game).liquidity).pending_warning) apply(game,AcknowledgeLiquidityWarning{*warning});
    while(const auto* notice=required_succession_notice(state(game))) apply(game,AcknowledgeSuccession{notice->id});
    if(const auto* notice=required_site_alert(state(game))) apply(game,AcknowledgeSiteAlert{notice->id});
    if(const auto& a=game.checkpoint().world.authority;a && required_public_case(game.checkpoint().world)) {
        std::vector<std::uint64_t> notices;
        for(const auto& row:a->cases) if(!row.acknowledged) notices.push_back(row.id);
        for(auto id:notices) apply(game,AcknowledgePublicCase{id});
    }
}
void until(Runtime& game,OrdinalDay day) {
    while(campaign_day(game.checkpoint().world)<day) {
        if(!observing_world(game.checkpoint().world)) prepare_mandatory(game);
        apply(game,AdvanceDay{});
    }
}
CompiledProgram near_window(bool catastrophic=false,bool control_loss=false) {
    auto data=source();
    const auto society=std::find_if(data["definitions"].begin(),data["definitions"].end(),[](const auto& d){return d["id"]=="society.century";});
    auto regional=*society;regional["id"]="society.fixture_objectives";
    regional["catalog"]["forum"]["first_agenda_day"]=to_ordinal({2104,1,1}).value();
    data["definitions"].push_back(std::move(regional));
    for(auto& d:data["definitions"]) if(d["id"]==frontier || d["id"]==plateau) {
        d["starts_on"]=control_loss?"2103-12-01":"2104-01-01";
        d.erase("backgrounds"); // Founder contacts belong to the original 2014 opening.
        d["society"]="society.fixture_objectives";
        if(control_loss) {
            // This charter offers a personal chair but no trustee board seat.
            auto& ownership=d["ownership_basis_points"];
            const auto founder_key=d["management"]["scoring"]["founder"].get<std::string>();
            const auto founder=ownership[founder_key].get<int>();
            ownership[founder_key]=4900;
            ownership["cofounder"]=ownership["cofounder"].get<int>()+founder-4900;
            d["management"]["scoring"]["opening_founder_share_bp"]=4900;
        }
        d["flows"]=Json::array();
        d["management"]["setbacks"]=Json::array();
        d["business_offers"]=Json::array();
        auto& leadership=d["management"]["succession"];
        leadership["founder_birth_year"]=control_loss?2034:2075;
        if(control_loss) leadership["trust_seats"]=0;
        d.erase("resilience");
        auto& rivals=d["rivals"];
        rivals.erase(std::remove_if(rivals.begin(),rivals.end(),[](const auto& row) {
            return row["id"]!="company.century.frontier";
        }),rivals.end());
        rivals.front()["start_after_days"]=0;
        rivals.front()["planning"]["slot"]=1;
        d["competition"]["domestic"]=1;
        d["competition"]["international"]=0;
        d["competition"]["heavyweights"]=0;
        if(catastrophic && d["id"]==frontier) d["flows"]={"objectives.fixture_catastrophe"};
    }
    if(catastrophic) {
        data["messages"]["objectives.fixture_cause"]="A disclosed fictional world-scale incident ended the scored campaign.";
        data["definitions"].push_back(Json::parse(R"({
          "id":"objectives.fixture_catastrophe","kind":"flow","revision":1,"scope":"world",
          "tags":["base_event"],"entry_node":"cause",
          "lifecycle":{"mode":"once","occurrence_scope":"world","max_occurrences_per_scope":1},
          "trigger":{"fact":"world.society_month_today","op":"eq","unit":"boolean","value":true},
          "nodes":{"cause":{"kind":"effect","next":"end","effects":[{"op":"record_world_catastrophe","target":"self",
            "cause_key":"objectives.fixture_cause"}]},"end":{"kind":"end"}}
        })"));
    }
    pgp_test::prune_unused_flows(data);
    return compile(data);
}
void strict_rules() {
    const auto end=to_ordinal({2114,1,1});
    check(objective_window_start(end,ObjectiveKind::monopoly)==to_ordinal({2109,1,1}) &&
        objective_window_start(end,ObjectiveKind::domination)==to_ordinal({2109,1,1}) &&
        objective_window_start(end,ObjectiveKind::utopia)==to_ordinal({2104,1,1}) &&
        objective_window_start(end,ObjectiveKind::survival)==to_ordinal({2113,1,1}) &&
        objective_month_end(to_ordinal({2100,2,28})), "The century windows or non-leap boundary shifted.");
    std::array<ObjectiveMonth,monopoly_months> months{};
    auto calendar=from_ordinal(objective_window_start(end,ObjectiveKind::monopoly));
    for(std::size_t i=0;i<months.size();++i) {
        auto& m=months[i];m.month_end=to_ordinal({calendar.year,calendar.month,days_in_month(calendar.year,calendar.month)});
        m.delivery.fill(i?DeliveredWork{86,100}:DeliveredWork{26,100});
        calendar=from_ordinal(advance_days(m.month_end,Days{1}));
    }
    const auto first=objective_window_start(end,ObjectiveKind::monopoly);
    check(monopoly_still_possible(aggregate_delivery(months,first)) && months.back().month_end==to_ordinal({2113,12,31}),
        "A weak month lost a qualifying five-year aggregate.");
    months.back().delivery[0].controlled--;
    check(!monopoly_still_possible(aggregate_delivery(months,first)),"A one-task global shortfall was rounded up.");
    RegionalDeliveredWork regions;regions.fill({10000,10000});regions[0]={5999,10000};
    check(!monopoly_still_possible(regions),"A regional shortfall was hidden by the global total.");
    std::array<std::uint64_t,society_regions> future{};future[0]=3;
    check(monopoly_still_possible(regions,future),"Possible future delivery was treated as impossible.");
    future[0]=2;
    check(!monopoly_still_possible(regions,future),"The optimistic bound rounded a regional near miss up.");
    regions[0]={};check(!monopoly_still_possible(regions),"An empty region qualified.");
    ObjectiveMonth month;month.population_coverage=8000;month.output_coverage=8000;
    month.global_welfare=utopia_global_floors;
    for(auto& region:month.regional_welfare) region.fill(7000);
    check(domination_met(month) && utopia_met(month),"Exact authority and welfare thresholds failed.");
    month.output_coverage=7999;check(!domination_met(month),"Output coverage was replaced by population coverage.");
    month.regional_welfare[7][2]=6999;check(!utopia_met(month),"Global welfare hid a regional near miss.");
    month.regional_welfare[7][2]=7000;month.global_welfare[4]=7499;
    check(!utopia_met(month),"A global welfare near miss passed.");
}
void durable_welfare(const CompiledProgram& program) {
    const auto& scenario=std::get<content::ScenarioDefinition>(program.collection.find(frontier)->data);
    const auto& catalog=*content::society_catalog(program.collection,scenario);
    check(catalog.development.has_value(),"The century society has no feasible durable development mechanism.");
    auto society=opening_society(catalog,scenario.starts_on);
    // A hand-auditable sustained high-delivery case. Every region delivers
    // 8,000 of 10,000 requested tasks; 2,000 are actual public services,
    // another 2,000 are science and 2,000 are industrial. No agents or ASI.
    std::array<RegionOutcomes,society_regions> delivered{},idle{};
    for(auto& r:delivered) {
        r.demand=10000;r.delivered=8000;r.public_demand=2500;r.public_work=2000;
        r.science_work=2000;r.industrial_work=2000;r.quality=BasisPoints{8000};r.concentration=BasisPoints{4000};
        r.compute_milli=80000;
    }
    for(auto& r:idle) {r.demand=10000;r.public_demand=2500;}
    auto day=scenario.starts_on;
    for(std::size_t i=0;i<utopia_months+catalog.lag_months+3;++i) {
        const auto date=from_ordinal(day);
        const auto end=to_ordinal({date.year,date.month,days_in_month(date.year,date.month)});
        for(;day<=end;day=advance_days(day,Days{1})) prepare_society(society,day);
        close_society_month(society,end,i<utopia_months?delivered:idle);
    }
    prepare_society(society,day);validate_society(society,day);
    const auto& history=society.months;
    check((*history[catalog.lag_months-1].durable_gains)[0]==0 &&
        (*history[catalog.lag_months].durable_gains)[0]==40 &&
        (*history[utopia_months+catalog.lag_months-1].durable_gains)[0]==4400 &&
        (*history[utopia_months+catalog.lag_months].durable_gains)[0]==4385,
        "Public gains ignored their observation lag, cap or lost delivery.");
    ObjectiveMonth scored;scored.global_welfare=global_welfare(society);
    for(std::size_t i=0;i<society_regions;++i) scored.regional_welfare[i]=welfare_indices(history.back().components[i]);
    check(utopia_met(scored),"Realistic non-ASI public, industrial and science coverage cannot reach the authored welfare thresholds.");
}
void boundaries(const CompiledProgram& p) {
    auto game=begin(p,{ObjectiveKind::utopia,{ObjectiveKind::survival}});
    const auto before=to_ordinal({2104,1,31}),after=to_ordinal({2104,2,1});
    until(game,before);
    check(!required_objective_review(game.checkpoint().world) && objectives(game).months.empty(),
        "The unclosed January morning counted as a welfare check.");
    apply(game,SetPaused{false});
    apply(game,AdvanceDay{});
    check(state(game).date==after && state(game).paused && objectives(game).months.size()==1 &&
        objectives(game).months.front().month_end==before && required_objective_review(game.checkpoint().world)==ObjectiveKind::utopia,
        "The first mandatory check did not immediately pause.");
    check(objective_result(game.checkpoint().world,ObjectiveKind::utopia)==ObjectiveResult::lost &&
        objective_result(game.checkpoint().world,ObjectiveKind::survival)==ObjectiveResult::pending &&
        !objectives(game).breaches[static_cast<std::size_t>(ObjectiveKind::monopoly)].first,
        "A welfare loss spilled into later windows or other distinctions.");
    refuse(game,AdvanceDay{},"breach");
    refuse(game,SetPaused{false},"breach");
    game=reload(game);
    forged(game,[](auto& o){o["months"][0]["global_welfare_bp"][0]=10000;});
    forged(game,[](auto& o){o["breaches"][2]["first"]=nullptr;});
    auto sandbox=game;
    apply(sandbox,EndWithPostmortem{});
    check(objectives(sandbox).end && objectives(sandbox).end->cause==CampaignEndCause::postmortem && state(sandbox).date==after,
        "The postmortem did not end at the existing breach.");
    refuse(sandbox,AdvanceDay{},"final accounts");
    sandbox=reload(sandbox);
    apply(sandbox,ContinueAfterEnding{});
    apply(sandbox,AdvanceDay{});
    check(objectives(sandbox).mode==ObjectiveMode::sandbox && objectives(sandbox).end->day==after && state(sandbox).date>after &&
        objectives(sandbox).deadline==to_ordinal({2114,1,1}) && objective_is_custom(objectives(sandbox)),
        "Sandbox rewrote the original calendar or closed result.");
    (void)reload(sandbox);

    apply(game,AcknowledgeObjectiveBreach{ObjectiveKind::utopia});
    game=reload(game);
    check(!required_objective_review(game.checkpoint().world),"An acknowledged breach reappeared after reload.");
    apply(game,SelectCampaignObjectives{{ObjectiveKind::profit,{ObjectiveKind::survival}}});
    apply(game,SelectCampaignObjectives{{ObjectiveKind::utopia,{ObjectiveKind::survival}}});
    check(objective_is_custom(objectives(game)) && objective_result(game.checkpoint().world,ObjectiveKind::utopia)==ObjectiveResult::lost,
        "Re-selecting the original objective reset custom history or feasibility.");
    until(game,to_ordinal({2104,3,1}));
    check(objectives(game).breaches[2].failed_checks==2 && !required_objective_review(game.checkpoint().world),
        "A later bad month erased the first loss or repeated a reviewed alert.");
    (void)reload(game);
    auto plateau_game=begin(p,{ObjectiveKind::survival,{ObjectiveKind::profit}},plateau);
    refuse(plateau_game,SelectCampaignObjectives{{ObjectiveKind::asi,{}}},"plateau");
    check(objective_result(plateau_game.checkpoint().world,ObjectiveKind::asi)==ObjectiveResult::unavailable,
        "The plateau advertised an impossible ASI objective.");
}
void observer(const CompiledProgram& p) {
    auto game=begin(p,{ObjectiveKind::survival,{}});
    apply(game,IssueInvoice{{"fixture.payable",InvoiceKind::payable,state(game).date,
        checked_unit_add(state(game).cash,Cents{50000}),Cents{0},"Fixture operating bill"}});
    apply(game,AdvanceDay{});
    const auto workout=state(game).liquidity->workouts.back();
    until(game,workout.deadline);
    prepare_mandatory(game);
    apply(game,AdvanceDay{});
    check(state(game).phase==DayPhase::closed && objectives(game).end->cause==CampaignEndCause::insolvency &&
        objectives(game).end->day==workout.deadline && objectives(game).end->obligations.unpaid_payables.value()>0,
        "Insolvency did not preserve its warned causal chain and actual liabilities.");
    game=reload(game);
    const auto accounts=state(game).accounts;
    const auto capacity=state(game).capacity;
    apply(game,ContinueAfterEnding{});
    const auto after=advance_days(workout.deadline,Days{31});
    until(game,after);
    const auto& world=game.checkpoint().world;
    check(observing_world(world) && campaign_day(world)==after && state(game).date==workout.deadline &&
        state(game).accounts==accounts && state(game).capacity==capacity && world.society->prepared_on==after &&
        world.competition->companies.front().simulation.date==after,
        "Observation advanced company activity, stopped rivals or lost the world calendar.");
    refuse(game,IssueInvoice{{"invented",InvoiceKind::payable,after,Cents{1},Cents{0},"Refused"}}, "Observer mode");
    (void)reload(game);
}
void effective_order(const CompiledProgram& baseline) {
    auto data=Json::parse(baseline.canonical_content_bytes);
    const auto authored=std::ranges::find_if(data["definitions"],[](const auto& d){return d["id"]==frontier;});
    const auto civic_id=authored->at("civic").get<std::string>();
    auto civic=*std::ranges::find_if(data["definitions"],[&](const auto& d){return d["id"]==civic_id;});
    civic["id"]="civic.objectives_fixture";
    (*authored)["civic"]=civic["id"];
    auto order=civic["catalog"]["policies"].front();
    order["id"]="policy.fixture_final_closure";
    order["title"]="Reviewed federal closure order";
    order["jurisdiction"]="US";order["rule"]="final_closure";order["channel"]="legacy";
    order["evidence_axis"]=0;order["evidence_floor_bp"]=0;order["exempt_reviewed_service"]=false;
    order["permit_watts"]=0;order["first_agenda_day"]=to_ordinal({2104,1,1}).value();
    order["repeat_years"]=0;order["case_strength_bp"]=10000;
    order["hearing_days"]=7;order["vote_spacing_days"]=2;order["review_days"]=2;order["review_floor_bp"]=1;
    order["implementation_days"]=3;order["term_days"]=365;
    civic["catalog"]["policies"]=Json::array({order});
    data["definitions"].push_back(std::move(civic));
    auto game=begin(compile(data),{ObjectiveKind::survival,{}});
    check(!objectives(game).end && !state(game).civic->dockets.empty(),"A proposal closed the company before the order was enforceable.");
    const auto bound=advance_days(state(game).date,Days{45});
    while(!objectives(game).end && state(game).date<bound) until(game,advance_days(state(game).date,Days{1}));
    const auto& docket=state(game).civic->dockets.front();
    check(objectives(game).end && objectives(game).end->cause==CampaignEndCause::final_order &&
        objectives(game).end->day==docket.effective_from && docket.stage==CivicStage::effective,
        "A reviewed and enforceable federal order did not close the scored institution.");
    game=reload(game);
    forged(game,[](auto& o){o["end"]["source"]="policy.invented/1";});
}
void earned_finance(const CompiledProgram& program) {
    auto world=begin_scenario(program,frontier,"US-WA").world;
    const auto day=to_ordinal({2113,12,31});
    const auto charter=*std::get<content::ScenarioDefinition>(program.collection.find(frontier)->data).objectives;
    const auto accounts=[&](std::string id,bool parent,bool trade) {
        SimulationState s;s.date=day;s.end_date=world.objectives->deadline;
        s.cash=Cents{100000};s.accounts=opening_accounts(s.cash,day,id);
        s.liquidity=opening_liquidity(day,id);
        s.capacity=opening_capacity({{"source","physical/"+id,id,"supplier",CapacitySourceKind::leased,
            MilliCUPerDay{10},MilliCUPerDay{10},0,"general",true}},1,day,id);
        s.management=world.simulation.management;s.management->catalog.scoring.founder="founder";
        s.management->catalog.succession->founder_salary=Cents{0};
        s.management->catalog.succession->chair_salary=Cents{0};
        s.management->catalog.succession->personal_cash=Cents{123};s.management->catalog.succession->personal_liabilities=Cents{45};
        if(parent) s.management->governance->opening_holdings=s.accounts->holdings;
        else {s.management->governance.reset();s.management->catalog.governance.reset();}
        initialize_succession(s);
        s.market=opening_market(day,id);
        if(trade) {
            CapacityProject work;work.id="serving";work.source_id="source";work.kind=ProjectKind::serving;work.requested=MilliCUPerDay{4};
            work.starts_on=day;work.staff=1;work.tasks_per_day=4;work.inference_milli_CU_per_task=MilliCU{1};work.cause="Delivered test service";
            reserve_project(*s.capacity,day,work);
            add_market_product(*s.market,*s.capacity,{"product","serving",Cents{1000},true});
            add_market_segment(*s.market,{"customer",10,Cents{10000},0,0,BasisPoints{10000},{{"product",100,Cents{1500},0,true}}});
        }
        record_cash(*s.accounts,day,Cents{2500},Account::revenue,"One-off external payment","fixture.oneoff");
        record_cash(*s.accounts,day,Cents{-10000},Account::assets,"Owned hardware","owned.hardware/fixture");
        add_recurring(*s.accounts,{"payroll",Cents{3100},day,std::nullopt,"Staff payroll"});
        draw_debt(*s.accounts,day,{"fixture.debt",Cents{20000},BasisPoints{0},day,advance_days(day,Days{10}),0,std::nullopt},"External debt");
        close_market_day(*s.market,*s.accounts,*s.capacity,day);
        close_accounts_day(*s.accounts,day,Cents{0});
        s.date=advance_days(day,Days{1});s.cash=accounts_report(*s.accounts).cash;
        observe_liquidity(*s.liquidity,*s.accounts,s.date);
        return s;
    };
    world.simulation=accounts(world.company_id,true,true);
    auto& rival=world.competition->companies.front();
    rival.simulation=accounts(rival.definition_id,false,true);
    auto table=objective_detail::terminal_profit(world,charter);
    check(table.size()==2 && table.front().audited_profit==Cents{3900} &&
        table.front().company_equity==Cents{114100} && table.front().legacy_wealth==Cents{62833} &&
        table.front().legacy_wealth==table.back().legacy_wealth,
        "Audited goodwill double-counted financing, cash, assets or one-off receipts, or broke a tie.");
    const auto survival=objective_detail::terminal_survival(world);
    check(survival.paid_tasks==4 && objective_detail::survival_met(survival),"Actual paid work did not support a staffed solvent company.");
    world.simulation=accounts(world.company_id,true,false);
    const auto dormant=objective_detail::terminal_survival(world);
    check(dormant.active_team && dormant.serviced_liabilities && !objective_detail::survival_met(dormant),
        "A staffed but dormant cash shell was awarded Survival.");
    table=objective_detail::terminal_profit(world,charter);
    check(table.front().legacy_wealth<table.back().legacy_wealth,"A missed operating year tied a profitable rival.");
}
// The ordinary CTest covers focused boundaries. This separate replay actually
// plays through ten exact years to exercise one winning terminal save.
void terminal_replay() {
    const auto baseline=near_window();
    auto data=Json::parse(baseline.canonical_content_bytes);
    for(auto& d:data["definitions"]) {
        if(d["id"]==frontier || d["id"]==plateau)
            for(auto& rival:d["rivals"]) rival["start_after_days"]=7000;
        if(d["id"]==frontier) {
            d["team_people"]=24;
            d["monthly_commitments_cents"]=Json::object();
            d["management"]["succession"]["founder_salary_cents"]=0;
            d["management"]["succession"]["chair_salary_cents"]=0;
            d["management"]["succession"]["founder_birth_year"]=
                2114-d["management"]["succession"]["executive_retirement_age"].get<int>();
            d.erase("civic");
            d["authority"]="authority.terminal_fixture";
        } else if(d["id"]=="society.fixture_objectives") {
            for(auto& region:d["catalog"]["regions"]) {
                region["initial_components_bp"]=std::vector<std::uint32_t>(society_components,9700);
                region["population_weight"]=region["id"]=="region.europe"?9000:100;
                region["output_weight"]=region["population_weight"];
            }
        }
    }
    const auto authority=std::ranges::find_if(data["definitions"],[](const auto& d){return d["id"]=="authority.institutions";});
    auto institutions=*authority;institutions["id"]="authority.terminal_fixture";
    for(auto& offer:institutions["catalog"]["offers"]) if(offer["id"]=="institution.standards" || offer["id"]=="institution.delegation") {
        offer["lead_days"]=7;offer["term_days"]=720;
    }
    data["definitions"].push_back(std::move(institutions));
    const auto program=compile(data);
    auto game=begin(program,{ObjectiveKind::utopia,{}});
    until(game,to_ordinal({2107,1,1}));
    apply(game,BuyData{"data.open_documents"});
    apply(game,StartResearch{"research.held_out_evaluation",{"lease.initial",MilliCUPerDay{20000},2}});
    apply(game,StartResearch{"research.reproducible_tooling",{"lease.initial",MilliCUPerDay{10000},1}});
    apply(game,StartTraining{{"model.objectives","recipe.document_baseline","",{"data.open_documents"},"lease.initial",
        MilliCUPerDay{60000},3,MilliCU{3600000}}});
    until(game,advance_days(state(game).date,Days{60}));
    apply(game,StartEvaluation{"evidence.objectives","model.objectives","evaluation.held_out",{"lease.initial",MilliCUPerDay{20000},1}});
    until(game,advance_days(state(game).date,Days{4}));
    const auto regions=game.checkpoint().world.society->catalog.regions;
    for(const auto& region:regions)
        apply(game,DeployModel{"product.public_toolkit."+region.id.substr(7),"model.objectives","evidence.objectives","",false,region.id});
    apply(game,DeployModel{"product.documents","model.objectives","evidence.objectives","lease.initial",false,"region.us"});
    apply(game,SubmitPublicTestimony{"evidence.objectives",0});
    apply(game,MakePublicPromise{PublicPromiseKind::public_work,"region.europe","product.public_toolkit.europe"});
    const auto deadline=objectives(game).deadline;
    constexpr auto term=727;
    for(int i=3;i>0;--i) {
        until(game,OrdinalDay{checked_subtract(deadline.value(),term*i)});
        apply(game,RequestInstitutionalMandate{"institution.standards","region.europe","product.public_toolkit.europe","lease.initial","",1,0});
        apply(game,RequestInstitutionalMandate{"institution.delegation","region.europe","product.public_toolkit.europe","lease.initial","",1,1});
        std::cout<<"Funded both authority terms at "<<iso_date(state(game).date)<<std::endl;
    }
    const auto last=OrdinalDay{checked_subtract(deadline.value(),1)};
    until(game,last);
    check(!objectives(game).end && objectives(game).months.size()==119,"The unclosed final day was awarded early.");
    game=reload(game);
    auto near_miss=game;
    for(const auto& m:game.checkpoint().world.authority->mandates) if(!m.staff_released_on)
        apply(near_miss,RelinquishInstitutionalMandate{m.id});
    apply(near_miss,AdvanceDay{});
    check(objectives(near_miss).breaches[1].first==last && objectives(near_miss).breaches[1].failed_checks==1 &&
        objective_result(near_miss.checkpoint().world,ObjectiveKind::domination)==ObjectiveResult::lost,
        "A single failed final authority check did not lose Domination.");
    (void)reload(near_miss);
    apply(game,AdvanceDay{});
    check(objectives(game).end && objectives(game).end->cause==CampaignEndCause::deadline && objectives(game).months.size()==120 &&
        objectives(game).months.back().month_end==last,"The terminal transaction missed its exact last work day.");
    for(const auto objective:{ObjectiveKind::monopoly,ObjectiveKind::domination,ObjectiveKind::utopia,ObjectiveKind::profit,ObjectiveKind::survival})
        check(objective_result(game.checkpoint().world,objective)==ObjectiveResult::won,
            std::string(objective_name(objective))+" did not win its exact terminal check. "+
            objective_breach_explanation(game.checkpoint().world,objective));
    std::size_t bytes=0;
    game=reload(game,&bytes);
    const auto scored=objectives(game).end;
    check(state(game).succession->terms.back().due_on==deadline,"The fixture must retire its executive at the scored boundary.");
    apply(game,ContinueAfterEnding{});
    check(state(game).succession->terms.back().starts_on==deadline && objectives(game).end==scored,
        "Continuing must open the due leadership change and preserve the scored ending.");
    prepare_mandatory(game);
    apply(game,AdvanceDay{});
    check(state(game).date==advance_days(deadline,Days{1}) && objectives(game).end->day==deadline,
        "A won century did not play into the sandbox.");
    (void)reload(game);
    std::cout<<"Terminal replay passed: all five non-race objectives, a last-day Domination loss, "<<bytes<<" save bytes, "<<checks<<" checks.\n";
}
}

int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string_view(argv[1])=="--terminal-replay") {terminal_replay();return 0;}
        const auto program=near_window();
        if(argc==3 && std::string_view(argv[1])=="--smoke-fixture") {
            const std::filesystem::path folder=argv[2];
            if(!std::filesystem::create_directories(folder/"saves")) throw std::runtime_error("Choose a fresh objective fixture folder.");
            std::ofstream(folder/"content.json")<<program.canonical_content_bytes;
            auto game=begin(program,{ObjectiveKind::utopia,{ObjectiveKind::survival}});
            until(game,to_ordinal({2104,1,31}));
            persistence::save_file(program,game.checkpoint(),folder/"saves"/"objective-start.pgpsave");
            return 0;
        }
        if(argc!=1) throw std::runtime_error("Use --smoke-fixture DIRECTORY, --terminal-replay, or no arguments.");
        strict_rules();durable_welfare(program);boundaries(program);observer(program);earned_finance(program);effective_order(program);
        auto control=begin(near_window(false,true),{ObjectiveKind::survival,{}});
        until(control,to_ordinal({2104,1,1}));
        check(objectives(control).end && objectives(control).end->cause==CampaignEndCause::institutional_control &&
            founder_role(*state(control).succession)==FounderRole::trust &&
            std::any_of(state(control).accounts->holdings.begin(),state(control).accounts->holdings.end(),[&](const auto& h){
                return h.holder==state(control).succession->founder_holder && h.shares==BasisPoints{4900};}),
            "Complete loss of directing rights erased economic ownership or let the player keep issuing commands.");
        (void)reload(control);
        auto catastrophic=begin(near_window(true),{ObjectiveKind::survival,{}});
        until(catastrophic,to_ordinal({2104,2,1}));
        check(objectives(catastrophic).end && objectives(catastrophic).end->cause==CampaignEndCause::catastrophe &&
            objective_result(catastrophic.checkpoint().world,ObjectiveKind::utopia)==ObjectiveResult::lost,
            "A world-ending incident failed to close the campaign with its authored cause.");
        (void)reload(catastrophic);
        std::cout<<checks<<" objective and continuation checks passed.\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
