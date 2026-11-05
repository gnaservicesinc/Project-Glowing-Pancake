#include <pgp/session/slice_run.hpp>
#include <pgp/runtime/scenario.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
namespace {
using namespace pgp;using namespace core;using Json=nlohmann::json;unsigned checks=0;
void check(bool value,const char* reason) {++checks;if (!value) throw std::runtime_error(reason);}
runtime::CompiledProgram fixture(unsigned budget_case=0) {
    std::ifstream file{PGP_HEAVYWEIGHT_SOURCE};const std::string bytes{std::istreambuf_iterator<char>{file},{}};
    const auto registry=content::foundation_registry();auto collection=content::load_collection(bytes,registry);
    for (auto it=collection.definitions.begin();it!=collection.definitions.end();) if (std::holds_alternative<content::ScenarioDefinition>(it->second.data) && it->first!="scenario.world_competition") it=collection.definitions.erase(it);else ++it;
    auto& scenario=std::get<content::ScenarioDefinition>(collection.definitions.at("scenario.world_competition").data);
    scenario.flows.clear();scenario.management->setbacks.clear();scenario.competition->domestic=0;scenario.competition->international=0;scenario.competition->heavyweights=1;
    for (auto& offer:scenario.management->financing) offer.public_announcement=true;
    auto& rival=*std::find_if(scenario.rivals.begin(),scenario.rivals.end(),[](const auto& row){return row.planning->group=="heavyweight" && row.planning->slot==1;});
    content::HeavyweightPolicy policy;policy.review_interval=Days{7};policy.delivery_lead=Days{7};policy.cooldown=Days{14};policy.weights={0,10000,0,0};policy.capability_target=1;
    policy.specialist_id="specialist.evaluation";policy.lease_offer_id="offer.supplier_small";policy.publication_node="research.reproducible_tooling";policy.pilot_product="product.documents";
    rival.response=policy;rival.warning_lead=Days{0};rival.player_deployment_threshold=0;
    rival.planning->research={"research.held_out_evaluation"};rival.planning->recipes={"recipe.document_baseline"};rival.planning->services={"product.documents"};
    if (budget_case!=0) {rival.cash=Cents{1500000};rival.planning->reserve_floor=Cents{0};rival.response->publication_fee=Cents{0};rival.response->pilot_fee=Cents{0};}
    if (budget_case==1) for (auto& specialist:scenario.management->specialists) if (specialist.id==policy.specialist_id) specialist.signing_cost=Cents{3000000};
    return runtime::compile_collection(collection,registry,runtime::supported_effects());
}
const runtime::RivalCompany& rival(const runtime::Runtime& game) {return game.current_state().world.competition->companies.front();}
void apply(runtime::Runtime& game,CommandAction command) {
    const auto result=game.submit({game.current_state().world.simulation.revision,std::move(command)});if (!result.accepted()) throw std::runtime_error(result.explanation);
}
void step(runtime::Runtime& game,unsigned days) {
    while (days--!=0) {
        const auto& state=game.current_state().world.simulation;
        if (const auto warning=liquidity_report(*state.liquidity).pending_warning) apply(game,AcknowledgeLiquidityWarning{*warning});
        apply(game,AdvanceDay{});
    }
}
void reply(runtime::Runtime& game,std::string id,bool accept) {
    const auto result=game.respondToCompetitiveOffer(game.current_state().world.simulation.revision,id,accept);if (!result.accepted()) throw std::runtime_error(result.explanation);
}
runtime::Runtime run(const runtime::CompiledProgram& program,OrdinalDay day) {
    const auto result=session::run_competition_specialist(program,"scenario.world_competition",day);
    check(result.stop==session::SliceStop::requested_boundary,"Fixture stopped at a real company failure before the requested boundary");return runtime::Runtime{program,result.checkpoint};
}
const runtime::CompetitiveOffer& offer(const runtime::Runtime& game,runtime::CompetitiveOfferKind kind) {
    const auto& offers=game.current_state().world.competition->offers;const auto it=std::find_if(offers.begin(),offers.end(),[&](const auto& row){return row.kind==kind;});
    if (it==offers.end()) throw std::runtime_error("Expected a funded competitive agreement");return *it;
}
void roundtrip(const runtime::CompiledProgram& program,const runtime::Runtime& game) {check(persistence::decode_save(program,persistence::encode_save(program,game.checkpoint())).checkpoint==game.checkpoint(),"Heavyweight continuation did not roundtrip exactly");}
void corrupt(const runtime::CompiledProgram& program,Json document) {
    document.erase("sha256");document["sha256"]=persistence::sha256(document.dump());bool refused=false;
    try {(void)persistence::decode_save(program,document.dump());}catch(const persistence::PersistenceError&){refused=true;}
    check(refused,"A rehashed forged heavyweight continuation was accepted");
}
void cancel_player_services(runtime::Runtime& game) {
    const auto contracts=game.current_state().world.simulation.market->contracts;
    for (const auto& row:contracts) if (row.status==ContractStatus::active || row.status==ContractStatus::pending) apply(game,CancelContract{row.id});
    const auto projects=game.current_state().world.simulation.capacity->projects;
    for (const auto& row:projects) if (row.kind==ProjectKind::serving && row.status==ProjectStatus::active) apply(game,CancelProject{row.id});
}
void warning_delivery_and_reloads() {
    const auto program=fixture();auto preview=run(program,OrdinalDay{100});
    check(rival(preview).response->warned_on.has_value() && !rival(preview).planning->entered_on,"The visible warning did not precede the finite response program");
    const auto warning=*rival(preview).response->warned_on;const auto due=advance_days(warning,Days{90});auto game=run(program,OrdinalDay{due.value()-1});
    check(!rival(game).simulation.accounts && rival(game).simulation.cash==Cents{0},"A watching heavyweight spent program cash before its announced window");
    const auto saved=persistence::encode_save(program,game.checkpoint());runtime::Runtime loaded{program,persistence::decode_save(program,saved).checkpoint};
    step(game,1);step(loaded,1);check(game.checkpoint()==loaded.checkpoint(),"Saved warning rerolled or duplicated the mobilization boundary");
    const auto& state=rival(game);check(state.response->phase==runtime::HeavyweightPhase::mobilize && state.planning->entered_on==due && state.simulation.management->specialists.size()==1 && state.simulation.business_offers->accepted.size()==1,"Mobilization skipped its real paid hire/compute order");
    check(state.simulation.cash<Cents{450000000} && state.simulation.capacity->available_staff==7 && std::all_of(state.simulation.capacity->sources.begin(),state.simulation.capacity->sources.end(),[](const auto& row){return row.effective.value()==0;}),"Budgeted mobilization granted immediate compute or omitted actual spending");
    step(game,6);check(rival(game).response->phase==runtime::HeavyweightPhase::mobilize && rival(game).simulation.lab->models.empty(),"Research/training started before the paid delivery lead time");
    roundtrip(program,game);step(game,1);
    check(rival(game).response->phase==runtime::HeavyweightPhase::compete && rival(game).simulation.capacity->sources.front().effective.value()>0 && rival(game).simulation.market->products.empty(),"Delivered capacity skipped model training/evaluation or stayed unavailable");
    roundtrip(program,game);
    const auto before=game.checkpoint();const auto id=rival(game).definition_id;
    const auto observation=runtime::observe_heavyweight_pressure(before.world,program,id,before.world.simulation.date);
    check(game.checkpoint()==before && observation==runtime::observe_heavyweight_pressure(before.world,program,id,before.world.simulation.date),"Public pressure preview changed authority or consumed RNG");
    // Inspection copy: preserve public deliveries/quotes while perturbing the
    // private billing and signed-demand-floor fields that this model must omit.
    auto private_billing=before.world;
    for (auto& day:private_billing.simulation.market->days) for (auto& segment:day.segments) {
        segment.charged=checked_unit_add(segment.charged,Cents{12345});segment.demand+=500;
    }
    for (auto& contract:private_billing.simulation.market->contracts) {contract.monthly_fee=checked_unit_add(contract.monthly_fee,Cents{1000000});contract.usage_price=Cents{999};}
    check(runtime::observe_heavyweight_pressure(private_billing,program,id,before.world.simulation.date)==observation,"The response model read hidden contract billing or private demand floors");
    apply(game,InvestEquity{"undisclosed",Cents{10000},BasisPoints{1},"private.test"});
    check(runtime::observe_heavyweight_pressure(game.current_state().world,program,id,game.current_state().world.simulation.date)==observation,"The response model read undisclosed player equity or cash");
    const auto funding=std::find_if(game.current_state().world.simulation.management->catalog.financing.begin(),game.current_state().world.simulation.management->catalog.financing.end(),[](const auto& row){return row.term.value()>0;});
    if (funding==game.current_state().world.simulation.management->catalog.financing.end()) throw std::runtime_error("Missing public loan fixture");
    const auto funding_id=funding->id;apply(game,TakeFinancing{funding_id});step(game,1);
    check(runtime::observe_heavyweight_pressure(game.current_state().world,program,id,game.current_state().world.simulation.date).disclosed_rounds==observation.disclosed_rounds+1,"A genuinely announced paid financing round did not enter the public model");
    auto invalid_policy=Json::parse(content::canonical_heavyweight_policy(*runtime::rival_definition(program,id).response));invalid_policy["warning_days"]=89;bool policy_refused=false;
    try {(void)content::load_heavyweight_policy(invalid_policy.dump());}catch (const std::exception&){policy_refused=true;}
    check(policy_refused,"Content accepted an instant heavyweight warning bypass");
    auto forged=Json::parse(persistence::encode_save(program,before));forged["checkpoint"]["world"]["competition"]["companies"][0]["response"]["reviews"][0]["score"]=1;corrupt(program,forged);
    forged=Json::parse(persistence::encode_save(program,before));forged["checkpoint"]["world"]["competition"]["companies"][0]["response"]["transitions"].erase(0);corrupt(program,forged);
    forged=Json::parse(persistence::encode_save(program,before));forged["checkpoint"]["world"]["competition"]["offers"][0]["amount"]=1;corrupt(program,forged);
}
void funded_deals_and_withdrawal() {
    const auto program=fixture();auto game=run(program,OrdinalDay{180});
    check(rival(game).response->phase==runtime::HeavyweightPhase::compete && game.current_state().world.competition->offers.size()==2,"The competing program did not expose both funded offers");
    const auto paper=offer(game,runtime::CompetitiveOfferKind::publication).id;const auto pilot=offer(game,runtime::CompetitiveOfferKind::pilot).id;
    const auto before=game.checkpoint();const auto total=checked_unit_add(before.world.simulation.cash,rival(game).simulation.cash);
    const auto random=before.world.simulation.random,opponent_random=rival(game).simulation.random;
    reply(game,paper,true);check(game.current_state().world.simulation.cash==before.world.simulation.cash,"Acceptance paid an unearned publication fee");
    apply(game,PublishResearch{"research.reproducible_tooling"});
    check(offer(game,runtime::CompetitiveOfferKind::publication).status==runtime::CompetitiveOfferStatus::paid && checked_unit_add(game.current_state().world.simulation.cash,rival(game).simulation.cash)==total &&
        game.current_state().world.simulation.cash==checked_unit_add(before.world.simulation.cash,Cents{5000000}),"Publication payout minted cash or lacked the actual paper");
    check(game.current_state().world.simulation.random==random && rival(game).simulation.random==opponent_random,"Responding/publishing consumed a future random decision");
    const auto paid=game.checkpoint();check(!game.respondToCompetitiveOffer(paid.world.simulation.revision,paper,true).accepted() && game.checkpoint()==paid,"A completed agreement could be claimed again");
    check(!game.respondToCompetitiveOffer(paid.world.simulation.revision-1,pilot,true).accepted() && game.checkpoint()==paid,"A stale offer response partially committed");
    reply(game,pilot,true);step(game,5);check(offer(game,runtime::CompetitiveOfferKind::pilot).status==runtime::CompetitiveOfferStatus::active && offer(game,runtime::CompetitiveOfferKind::pilot).delivered_tasks>0,"The pilot did not track real additional customer work");
    const auto saved=persistence::encode_save(program,game.checkpoint());runtime::Runtime loaded{program,persistence::decode_save(program,saved).checkpoint};
    step(game,10);step(loaded,10);check(game.checkpoint()==loaded.checkpoint() && offer(game,runtime::CompetitiveOfferKind::pilot).status==runtime::CompetitiveOfferStatus::paid,"Active pilot progress diverged after reload or failed its actual target");
    cancel_player_services(game);
    while (rival(game).response->low_reviews==0) step(game,1);
    check(rival(game).response->phase==runtime::HeavyweightPhase::compete,"A single low review caused monthly state flicker");
    while (!rival(game).response->retired_on) step(game,1);
    const auto retired=*rival(game).response->retired_on;const auto& company=rival(game).simulation;
    check(rival(game).response->phase==runtime::HeavyweightPhase::consolidate && company.capacity->available_staff==0 && available_staff_on(*company.capacity,OrdinalDay{retired.value()-1})==7,"Consolidation lost historical staff or retained free active operations");
    const auto fixed=std::find_if(company.accounts->recurring.begin(),company.accounts->recurring.end(),[](const auto& row){return row.id.starts_with("offer.lease/");});
    check(fixed!=company.accounts->recurring.end() && fixed->ends_before>retired,"Withdrawal erased a real fixed-term lease obligation");
    roundtrip(program,game);step(game,14);
    check(rival(game).response->phase==runtime::HeavyweightPhase::watch && rival(game).response->retired_on==retired,"A withdrawn program skipped its cooldown or received a new budget");
    apply(game,DeployModel{"product.documents","specialist","delegated.specialist_checks.1","lease.initial",false});step(game,28);
    check(rival(game).response->phase==runtime::HeavyweightPhase::watch && rival(game).simulation.capacity->available_staff==0 && rival(game).response->mobilized_on==before.world.competition->companies.front().response->mobilized_on,"Renewed pressure created a subsidized replacement response program");
    roundtrip(program,game);
}
void ignored_declined_and_abandoned() {
    const auto program=fixture();auto game=run(program,OrdinalDay{180});const auto paper=offer(game,runtime::CompetitiveOfferKind::publication).id;
    reply(game,paper,false);check(offer(game,runtime::CompetitiveOfferKind::publication).status==runtime::CompetitiveOfferStatus::declined,"A player could not reject the public terms");
    step(game,31);check(offer(game,runtime::CompetitiveOfferKind::pilot).status==runtime::CompetitiveOfferStatus::expired,"Ignoring an offer did not release its real cash reservation");roundtrip(program,game);
    auto withdrawn=run(program,OrdinalDay{100});cancel_player_services(withdrawn);step(withdrawn,21);
    check(!rival(withdrawn).response->warned_on && !rival(withdrawn).planning->entered_on && !rival(withdrawn).simulation.accounts,"An abandoned warning still created an instant funded competitor");roundtrip(program,withdrawn);
    const auto poor=fixture(1);auto unfunded=run(poor,OrdinalDay{180});
    check(rival(unfunded).response->retired_on && rival(unfunded).simulation.management->specialists.empty() && rival(unfunded).simulation.cash==Cents{1500000},"An unaffordable program conjured staff, cash or a replacement subsidy");roundtrip(poor,unfunded);
    const auto insolvent=fixture(2);auto closed=run(insolvent,OrdinalDay{240});
    check(rival(closed).simulation.phase==DayPhase::closed && rival(closed).simulation.management->financing.empty(),"An insolvent program ignored real bills or received hidden financing");roundtrip(insolvent,closed);
}
void legacy_has_no_undeclared_response() {
    std::ifstream file{PGP_HEAVYWEIGHT_SOURCE};const std::string bytes{std::istreambuf_iterator<char>{file},{}};
    const auto registry=content::foundation_registry();const auto program=runtime::compile_collection(content::load_collection(bytes,registry),registry,runtime::supported_effects());
    runtime::Runtime game{program,runtime::begin_scenario(program,"scenario.first_playable","US-CA")};
    const auto prepared=game.prepare_opening(game.current_state().world.simulation.revision);if (!prepared.accepted()) throw std::runtime_error(prepared.explanation);
    auto document=Json::parse(persistence::encode_save(program,game.checkpoint()));document["checkpoint"]["world"]["competition"]["offers"]=Json::array();corrupt(program,document);
    auto invalid=game.checkpoint();invalid.world.competition->companies.front().response=runtime::HeavyweightState{};bool refused=false;
    try {runtime::validate_checkpoint(program,invalid);}catch(const std::exception&){refused=true;}
    check(refused,"Legacy competition accepted an undeclared heavyweight continuation");
}
}
int main() {try {warning_delivery_and_reloads();funded_deals_and_withdrawal();ignored_declined_and_abandoned();legacy_has_no_undeclared_response();std::cout<<checks<<" heavyweight response checks passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
