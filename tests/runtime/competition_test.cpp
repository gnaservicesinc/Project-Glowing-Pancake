#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include "../content/slice_fixture.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
using namespace pgp;using Json=nlohmann::json;unsigned checks=0;
void check(bool value,const char* message) {++checks;if (!value) throw std::runtime_error(message);}
Json source() {std::ifstream stream{PGP_COMPETITION_SOURCE};if (!stream) throw std::runtime_error("Competition content is absent");return Json::parse(stream);}
runtime::CompiledProgram compile(const Json& document) {const auto registry=content::foundation_registry();return runtime::compile_collection(content::load_collection(document.dump(),registry),registry,runtime::supported_effects());}
Json& scenario(Json& document) {for (auto& row:document.at("definitions")) if (row.at("id")=="scenario.world_competition") return row;throw std::runtime_error("Competition scenario is absent");}
Json compact() {
    auto document=source();auto& rows=document["definitions"];
    rows.erase(std::remove_if(rows.begin(),rows.end(),[](const auto& row){return row.at("kind")=="scenario" && row.at("id")!="scenario.world_competition";}),rows.end());
    auto& selected=scenario(document);selected["flows"]=Json::array();selected["management"]["setbacks"]=Json::array();
    selected["competition"]["domestic"]=2;selected["competition"]["international"]=0;selected["competition"]["heavyweights"]=0;
    for (auto& rival:selected["rivals"]) if (rival["planning"]["group"]=="domestic") rival["start_after_days"]=rival["planning"]["slot"]==1?0:14;
    pgp_test::prune_unused_flows(document);
    return document;
}
void apply(runtime::Runtime& game,core::CommandAction action) {
    const auto result=game.submit({game.current_state().world.simulation.revision,std::move(action)});if (!result.accepted()) throw std::runtime_error(result.explanation);
}
runtime::Runtime begin(const runtime::CompiledProgram& program) {
    runtime::Runtime result{program,runtime::begin_scenario(program,"scenario.world_competition","US-CA")};
    const auto ready=result.prepare_opening(result.current_state().world.simulation.revision);if (!ready.accepted()) throw std::runtime_error(ready.explanation);return result;
}
void step(runtime::Runtime& game,unsigned days) {
    while (days--!=0) {
        const auto& s=game.current_state().world.simulation;
        if (s.liquidity) if (const auto warning=core::liquidity_report(*s.liquidity).pending_warning) apply(game,core::AcknowledgeLiquidityWarning{*warning});
        if (s.management) {const auto notices=s.management->notices;for (const auto& row:notices) if (row.required && !row.acknowledged) apply(game,core::AcknowledgeManagementNotice{row.id});}
        apply(game,core::AdvanceDay{});
    }
}
void invalid_save(const runtime::CompiledProgram& program,Json document) {
    document.erase("sha256");document["sha256"]=persistence::sha256(document.dump());bool refused=false;
    try {(void)persistence::decode_save(program,document.dump());}catch (const persistence::PersistenceError&){refused=true;}
    check(refused,"A rehashed inconsistent competition save was accepted");
}
core::SimulationState seller(std::string id) {
    using namespace core;SimulationState state;state.cash=Cents{100000};state.accounts=opening_accounts(state.cash,OrdinalDay{0},id);
    state.capacity=opening_capacity({{"source","physical/"+id,id,"supplier",CapacitySourceKind::leased,MilliCUPerDay{10},MilliCUPerDay{10},0,"general",true}},1,OrdinalDay{0},id);
    state.liquidity=opening_liquidity(OrdinalDay{0},id);state.market=opening_market(OrdinalDay{0},id);
    CapacityProject work;work.id="serving";work.source_id="source";work.kind=ProjectKind::serving;work.requested=MilliCUPerDay{4};work.staff=1;work.tasks_per_day=4;work.inference_milli_CU_per_task=MilliCU{1};work.cause="Tiny shared customer case";
    reserve_project(*state.capacity,OrdinalDay{0},work);add_market_product(*state.market,*state.capacity,{"product","serving",Cents{10},true});
    add_market_segment(*state.market,{"customer",10,Cents{70},0,0,BasisPoints{10000},{{"product",100,Cents{100},0,true}}});return Simulation{std::move(state)}.checkpoint();
}
void single_customer() {
    using namespace core;runtime::WorldState world;world.simulation=seller(world.company_id);
    MarketContract contract;contract.id="priority";contract.segment_id="customer";contract.product_id="product";contract.tasks_per_day=2;contract.usage_price=Cents{10};contract.start_by=OrdinalDay{1};contract.ends_before=OrdinalDay{3};contract.cause="Signed priority work";
    Simulation signed_work{world.simulation};check(signed_work.submit({signed_work.checkpoint().revision,AcceptContract{contract}}).accepted() && signed_work.submit({signed_work.checkpoint().revision,StartContract{"priority"}}).accepted(),"The tiny contract-priority fixture was not accepted");world.simulation=signed_work.checkpoint();
    runtime::RivalCompany rival;rival.definition_id="company.rival";rival.simulation=seller(rival.definition_id);rival.planning=runtime::RivalPlanState{OrdinalDay{0},OrdinalDay{7},{}};
    world.competition=runtime::CompetitionState{};world.competition->setup=content::CompetitionSetup{};world.competition->companies.push_back(rival);
    const auto before=world;const auto program=compile(compact());const auto assignments=runtime::prepare_competition_day(world,program);
    check(world==before,"Shared opportunity preview mutated company state or RNG");
    std::uint64_t tasks=0;Cents charges{0};
    for (const auto& [owner,result]:assignments) {(void)owner;for (const auto& row:result.segments) for (const auto& delivery:row.deliveries) {tasks+=delivery.tasks;charges=checked_unit_add(charges,delivery.cost);}}
    check(tasks==5 && charges==Cents{50},"Discretionary competition did not preserve the two protected tasks and their twenty-cent charge");
    Simulation player{world.simulation},other{rival.simulation};
    check(player.submit({player.checkpoint().revision,AdvanceDay{assignments.at(world.company_id)}}).accepted() && other.submit({other.checkpoint().revision,AdvanceDay{assignments.at(rival.definition_id)}}).accepted(),"A feasible industry allocation did not settle through ordinary company commands");
    check(market_report(*player.checkpoint().market).delivered_tasks+market_report(*other.checkpoint().market).delivered_tasks==7 &&
        checked_unit_add(player.checkpoint().cash,other.checkpoint().cash)==Cents{200070},"Billed work and seller cash do not conserve the shared allocation");
    check(player.checkpoint().market->days.front().contracts.front().delivered_tasks==2,"Shared competition displaced protected signed work");
    const auto old_day=player.checkpoint().market->days.front();
    check(player.submit({player.checkpoint().revision,SetMarketPrice{"product",Cents{5}}}).accepted(),"A feasible price change was refused");
    validate_simulation(player.checkpoint());check(player.checkpoint().market->days.front()==old_day && market_price_on(*player.checkpoint().market,"product",OrdinalDay{0})==Cents{10},"Repricing rewrote past work or actual invoices");
    auto invalid=assignments.at(world.company_id);invalid.segments.front().deliveries.front().tasks=100;
    const auto stable=player.checkpoint();check(!player.submit({stable.revision,AdvanceDay{invalid}}).accepted() && player.checkpoint()==stable,"An infeasible assigned market result partially committed");

    set_market_waitlist(*world.simulation.market,*world.simulation.capacity,OrdinalDay{0},"product",0);
    const auto waiting=runtime::prepare_competition_day(world,program);
    const auto& own=waiting.at(world.company_id).segments.front();
    const auto& rival_sales=waiting.at(rival.definition_id).segments.front().deliveries;
    check(own.deliveries.empty() && own.waitlist_interest.size()==1 && own.waitlist_interest.front().tasks>0 &&
          rival_sales.size()==1 && rival_sales.front().tasks==4,
          "A waitlisted order was reserved for the player instead of leaving the rival free to sell");
    Simulation queued{world.simulation};
    check(queued.submit({queued.checkpoint().revision,AdvanceDay{waiting.at(world.company_id)}}).accepted() &&
          queued.checkpoint().market->days.front().waitlists.front().waiting_tasks==own.waitlist_interest.front().tasks &&
          queued.checkpoint().market->days.front().contracts.front().delivered_tasks==2 && queued.checkpoint().cash==Cents{100020},
          "The shared allocation lost projected interest or recognized it as a sale");
}
void industry_rounding_context() {
    using namespace core;auto state=seller("company.a");
    state.capacity->sources.front().nominal=state.capacity->sources.front().effective=MilliCUPerDay{9999};
    auto& serving=state.capacity->projects.front();serving.requested=MilliCUPerDay{9999};serving.tasks_per_day=9999;
    auto& customer=state.market->segments.front();customer.daily_tasks=9999;customer.daily_budget=Cents{999900};customer.maximum_monthly_migration=BasisPoints{3334};
    Simulation owner{std::move(state)};
    const AllocationResult equal{{{"customer",{{"product",3333,Cents{33330}}},6666,Cents{966570},{}}},1,1};
    for (unsigned day=0;day<31;++day) if (!owner.submit({owner.checkpoint().revision,AdvanceDay{equal}}).accepted()) throw std::runtime_error("Rounding fixture failed its real January settlement");
    // Three companies each delivered 3333/day from the same 9999-task pool.
    // Hamilton gives company.a 3334bp globally, but only 3333bp when the other
    // two companies are aggregated into this owner's local outside bucket.
    MarketAdoption industry{OrdinalDay{30},"customer",9999U*31U,{}};
    const std::uint64_t delivered=3333U*31U;
    const std::vector<WeightedQuota> weights{{"company.a:product",delivered},{"company.b:product",delivered},{"company.c:product",delivered},{"outside",0}};
    for (const auto& row:apportion_units(10000,weights))
        industry.shares.push_back({row.id,BasisPoints{row.units}});
    check(industry.shares.front().share==BasisPoints{3334} && owner.checkpoint().market->adoptions.front().shares.front().share==BasisPoints{6667},"The hand-auditable industry/local rounding boundary was not exercised");
    const AllocationResult expansion{{{"customer",{{"product",6667,Cents{66670}}},3332,Cents{933230},{}}},1,1};
    const auto before=owner.checkpoint();
    check(!owner.submit({before.revision,AdvanceDay{expansion}}).accepted() && owner.checkpoint()==before,"Local migration rules unexpectedly accepted the one-task industry-only quota");
    check(owner.submit({before.revision,AdvanceDay{expansion,{industry}}}).accepted() && owner.checkpoint().date==OrdinalDay{32},"Settlement used a local rounded share instead of the shared industry's actual adoption quota");
}
void roster_and_entry() {
    const auto program=compile(source());const auto opening=runtime::begin_scenario(program,"scenario.world_competition","US-CA");
    const auto& companies=opening.world.competition->companies;check(companies.size()==15,"The default roster does not schedule six domestic, six international and three heavyweights");
    unsigned domestic=0,international=0,heavy=0,entered=0;
    for (const auto& company:companies) {const auto& definition=runtime::rival_definition(program,company.definition_id);const auto& plan=*definition.planning;
        if (plan.group=="domestic") ++domestic;else if (plan.group=="international") ++international;else ++heavy;
        if (company.planning->entered_on) ++entered;else check(company.simulation.cash==core::Cents{0} && !company.simulation.accounts && !company.simulation.capacity,"A scheduled company spent capital or payroll before entry");}
    check(domestic==6 && international==6 && heavy==3 && entered==6,"Initial versus staged roster counts are incorrect");
    check(runtime::begin_scenario(program,"scenario.world_reduced","US-CA").world.competition->companies.size()==7 && runtime::begin_scenario(program,"scenario.world_expanded","US-CA").world.competition->companies.size()==20,"Reduced/expanded roster presets exceed or ignore their configured slots");
    const auto data=compact();const auto small=compile(data);auto game=begin(small);step(game,13);
    const auto pending=persistence::encode_save(small,game.checkpoint());runtime::Runtime restored{small,persistence::decode_save(small,pending).checkpoint};
    step(game,1);step(restored,1);check(game.checkpoint()==restored.checkpoint(),"A saved pending entry duplicated actions or diverged after reload");
    const auto& entrant=game.current_state().world.competition->companies.at(1);
    check(entrant.planning->entered_on==core::OrdinalDay{14} && entrant.simulation.accounts->starts_on==core::OrdinalDay{14} && entrant.planning->decisions.size()==1,"A staged entrant accrued costs before opening or skipped its first weekly review");
    check(entrant.simulation.accounts->taxes && entrant.simulation.accounts->taxes->starts_on==*entrant.planning->entered_on &&
          entrant.simulation.accounts->taxes->policy==*game.current_state().world.simulation.management->catalog.corporate_tax &&
          entrant.simulation.accounts->taxes->assessments.empty(),"The entering rival did not adopt the shared tax policy from its own opening");
    auto corrupt=Json::parse(pending);corrupt["checkpoint"]["world"]["competition"]["companies"][1]["simulation"]["cash_cents"]=1;invalid_save(small,corrupt);
    corrupt=Json::parse(pending);corrupt["checkpoint"]["world"]["competition"]["setup"]["catchup_cash_cents"]=1000;invalid_save(small,corrupt);
}
void private_information() {
    const auto program=compile(compact());auto ordinary=begin(program),private_change=begin(program);
    apply(private_change,core::InvestEquity{"private_financing",core::Cents{1000000},core::BasisPoints{100},"private_funding"});
    apply(private_change,core::BuyData{"data.uncleared_archive"});
    step(ordinary,21);step(private_change,21);
    check(ordinary.current_state().world.competition==private_change.current_state().world.competition,"Rival plans read the player's undisclosed cash, data licenses or private settings");
    const auto& world=ordinary.current_state().world;const auto random=world.simulation.random;const auto checkpoint=ordinary.checkpoint();
    (void)core::market_opportunity(*world.simulation.market,*world.simulation.capacity,world.simulation.date);
    check(ordinary.checkpoint()==checkpoint && ordinary.current_state().world.simulation.random==random,"A market preview changed authoritative state or RNG");
    core::AdvanceDay forged;forged.industry_adoptions.push_back({});
    check(!ordinary.submit({checkpoint.world.simulation.revision,forged}).accepted() && ordinary.checkpoint()==checkpoint,"A caller supplied its own industry snapshot to the authoritative runtime");
}
void budgeted_actions_and_reload() {
    const auto program=compile(compact());auto game=begin(program);const auto stable=game.checkpoint();
    check(!game.submit({stable.world.simulation.revision,core::StartResearch{"research.efficient_serving",{"lease.initial",core::MilliCUPerDay{10000},1}}}).accepted() && game.checkpoint()==stable,"Research prerequisites were bypassed or a refused action partially committed");
    step(game,180);unsigned trained=0,evaluated=0,research=0;
    for (const auto& rival:game.current_state().world.competition->companies) {
        check(!rival.planning->decisions.empty() && core::accounts_report(*rival.simulation.accounts).expenses.value()>0,"Weekly planning omitted paid staff, compute or real decisions");
        trained+=static_cast<unsigned>(rival.simulation.lab->models.size());evaluated+=static_cast<unsigned>(rival.simulation.lab->evaluations.size());research+=static_cast<unsigned>(rival.simulation.lab->research.size());
    }
    check(trained>0 && evaluated>0 && research>0,"The budgeted planner did not reach actual research, training and evaluation");
    const auto bytes=persistence::encode_save(program,game.checkpoint());runtime::Runtime loaded{program,persistence::decode_save(program,bytes).checkpoint};
    step(game,14);step(loaded,14);check(game.checkpoint()==loaded.checkpoint(),"Active weekly planning, public prices or market work diverged after reload");
    auto malformed=Json::parse(bytes);auto& company=malformed["checkpoint"]["world"]["competition"]["companies"][0];
    company["simulation_storage"]="history-rows-v99";invalid_save(program,malformed);
    malformed=Json::parse(bytes);malformed["checkpoint"]["world"]["competition"]["companies"][0]["simulation"]["accounts"]["postings"][0].push_back("unknown-column");invalid_save(program,malformed);
    malformed=Json::parse(bytes);malformed["checkpoint"]["world"]["competition"]["companies"][0]["simulation"]["accounts"]["postings"][0][2]=100000;invalid_save(program,malformed);
    malformed=Json::parse(bytes);auto& strings=malformed["checkpoint"]["world"]["competition"]["companies"][0]["simulation"]["history_strings"];strings.push_back(strings.front());invalid_save(program,malformed);
    malformed=Json::parse(bytes);malformed["checkpoint"]["world"]["competition"]["companies"][0]["planning"]["decisions"][0]["observation"]["demand_upper"]=1;invalid_save(program,malformed);
    malformed=Json::parse(bytes);malformed["checkpoint"]["world"]["scenario_id"]="scenario.absent";invalid_save(program,malformed);

    step(game, 172);
    const auto archived = persistence::encode_save(program, game.checkpoint());
    auto reopened = persistence::decode_save(program, archived);
    check(reopened.checkpoint == game.checkpoint(), "Packed company history changed the checkpoint");
    auto document = Json::parse(archived);
    auto& packed_company = document["checkpoint"]["world"]["competition"]["companies"][0];
    check(packed_company.at("simulation_storage") == "history-blocks-v3", "Long company history missed the packed save path");
    auto& postings = packed_company["simulation"]["accounts"]["postings"];
    auto& blocks = postings["blocks"];
    const auto valid_block = blocks.at(0);
    blocks[0] = "gAAA"; // A backreference with zero distance.
    invalid_save(program, document);
    blocks[0] = valid_block;
    postings["count"] = postings["count"].get<std::size_t>() + 1;
    invalid_save(program, document);
    runtime::Runtime resumed{program, std::move(reopened.checkpoint)};
    step(game, 7);
    step(resumed, 7);
    check(resumed.checkpoint() == game.checkpoint(), "Reloaded packed history changed the next weekly decision");
}
void research_diffusion() {
    auto document=compact();auto& selected=scenario(document);selected["competition"]["domestic"]=1;
    auto& receiver=selected["rivals"][0];receiver["start_after_days"]=35;receiver["planning"]["research"]=Json::array({"research.held_out_evaluation"});receiver["planning"]["priorities"][1]=1000;
    const auto program=compile(document);auto game=begin(program);
    apply(game,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{60000},2}});
    step(game,5);apply(game,core::PublishResearch{"research.held_out_evaluation"});
    const auto before=game.checkpoint();const auto duplicate=game.submit({before.world.simulation.revision,core::PublishResearch{"research.held_out_evaluation"}});
    check(!duplicate.accepted() && game.checkpoint()==before,"A repeated publication changed ownership or consumed RNG");
    step(game,29);const auto saved=persistence::encode_save(program,game.checkpoint());runtime::Runtime loaded{program,persistence::decode_save(program,saved).checkpoint};
    step(game,1);step(loaded,1);check(game.checkpoint()==loaded.checkpoint(),"Publication diffusion or entry rerolled after a save boundary");
    const auto& rival=game.current_state().world.competition->companies.front();
    check(rival.simulation.lab->insights.size()==1 && rival.simulation.lab->insights.front().credit==core::BasisPoints{2500} && rival.simulation.lab->research.size()==1 && !rival.simulation.lab->research.front().completed &&
        rival.simulation.lab->research.front().insight_credit==core::BasisPoints{2500},"A public result skipped prerequisites/completion or failed to reduce a real finite experiment");
    auto corrupt=Json::parse(persistence::encode_save(program,game.checkpoint()));corrupt["checkpoint"]["world"]["competition"]["companies"][0]["simulation"]["lab"]["insights"][0]["publisher"]="company.absent";invalid_save(program,corrupt);
    auto untouched=begin(program);const auto stable=untouched.checkpoint();
    check(!untouched.submit({stable.world.simulation.revision,core::AcquireResearchInsight{{"research.held_out_evaluation","company.absent",core::OrdinalDay{0},core::OrdinalDay{0},core::BasisPoints{2500}}}}).accepted() && untouched.checkpoint()==stable,"A forged or premature public insight committed");
}
void insolvency_and_catchup() {
    auto document=compact();auto& selected=scenario(document);selected["competition"]["domestic"]=1;selected["management"]["financing"]=Json::array();
    selected["rivals"][0]["cash_cents"]=3000000;selected["rivals"][0]["planning"]["reserve_floor_cents"]=0;
    const auto hard=compile(document);auto game=begin(hard);step(game,100);
    check(game.current_state().world.competition->companies.front().simulation.phase==core::DayPhase::closed &&
        game.current_state().world.competition->companies.front().simulation.accounts->debts.empty(),"An insolvent rival received hidden money or ignored the real closure window");
    check(persistence::decode_save(hard,persistence::encode_save(hard,game.checkpoint())).checkpoint==game.checkpoint(),"A closed rival's final accounts did not reload");
    selected["competition"]["catchup_cash_cents"]=30000000;const auto forgiving=compile(document);auto helped=begin(forgiving);step(helped,100);
    const auto& company=helped.current_state().world.competition->companies.front();
    check(company.simulation.management->financing.size()==1 && company.simulation.management->financing.front().offer_id=="finance.disclosed_catchup" && company.simulation.phase!=core::DayPhase::closed,"The disclosed finite catch-up round was missing or repeated");
    const auto investor=std::find_if(company.simulation.accounts->holdings.begin(),company.simulation.accounts->holdings.end(),[](const auto& row){return row.holder=="investor/finance.disclosed_catchup";});
    check(investor!=company.simulation.accounts->holdings.end() && investor->shares==core::BasisPoints{1500},"Catch-up proceeds bypassed real dilution and ownership accounting");
}
}
int main() {
    try {single_customer();industry_rounding_context();roster_and_entry();private_information();budgeted_actions_and_reload();research_diffusion();insolvency_and_catchup();std::cout<<checks<<" competition checks passed\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
