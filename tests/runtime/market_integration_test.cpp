#include "integration_fixture.hpp"

#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>

#include <iostream>

using namespace pgp;
namespace {
using Json=nlohmann::json;
std::size_t checks=0;
void check(bool condition,const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void apply(runtime::Runtime& engine,core::CommandAction action) {
    integration_fixture::require_accepted(engine.submit({engine.checkpoint().world.simulation.revision,std::move(action)}));
}
void refused_unchanged(runtime::Runtime& engine,core::CommandAction action) {
    const auto before=engine.checkpoint();
    check(!engine.submit({before.world.simulation.revision,std::move(action)}).accepted(),"Invalid market command was accepted");
    check(engine.checkpoint()==before,"Rejected market command changed authoritative state");
}
std::string rehash(Json value) {
    value.erase("sha256");value["sha256"]=persistence::sha256(value.dump());return value.dump();
}
void invalid_save(const runtime::CompiledProgram& program,const Json& value) {
    bool rejected=false;
    try { (void)persistence::decode_save(program,rehash(value)); }
    catch (const persistence::PersistenceError&) { rejected=true; }
    check(rejected,"Malformed market save was accepted");
}
runtime::Runtime initial(const runtime::CompiledProgram& program) {
    auto state=runtime::begin_scenario(program,"scenario.foundation","US-CA");
    state.world.enabled_flows=std::vector<std::string>{};
    runtime::Runtime engine{program,std::move(state)};
    integration_fixture::prepare(engine);
    return engine;
}
core::CapacityProject serving(std::string id="serve",core::Cents support=core::Cents{500000}) {
    core::CapacityProject row;
    row.id=std::move(id);row.source_id="lease.initial";row.kind=core::ProjectKind::serving;
    row.requested=core::MilliCUPerDay{20000};row.staff=1;row.tasks_per_day=1000;
    row.inference_milli_CU_per_task=core::MilliCU{20};row.monthly_support_cost=support;
    row.cause="Contract-capable document serving";
    return row;
}
core::CapacityProject training() {
    core::CapacityProject row;
    row.id="train";row.source_id="lease.initial";row.requested=core::MilliCUPerDay{60000};
    row.staff=4;row.total_work=core::MilliCU{3600000};row.cause="Finite training candidate";
    return row;
}
core::MarketSegment segment(std::uint64_t demand=1000,core::Cents budget=core::Cents{100000}) {
    core::MarketSegment row;
    row.id="enterprise";row.daily_tasks=demand;row.daily_budget=budget;
    row.preferences.push_back({"product.docs",100,core::Cents{20},100,true});
    return row;
}
runtime::Runtime setup(const runtime::CompiledProgram& program,bool train=true,
                       core::Cents support=core::Cents{500000},std::uint64_t demand=1000,
                       core::Cents budget=core::Cents{100000},core::Cents price=core::Cents{10}) {
    auto engine=initial(program);
    apply(engine,core::ReserveProject{serving("serve",support)});
    if (train) apply(engine,core::ReserveProject{training()});
    apply(engine,core::EnableMarket{});
    apply(engine,core::AddMarketProduct{{"product.docs","serve",price,true}});
    apply(engine,core::AddMarketSegment{segment(demand,budget)});
    return engine;
}
core::MarketContract contract() {
    core::MarketContract row;
    row.id="contract.docs";row.segment_id="enterprise";row.product_id="product.docs";
    row.tasks_per_day=1000;row.monthly_fee=core::Cents{2500000};
    row.start_by=core::OrdinalDay{0};row.ends_before=core::OrdinalDay{90};row.cause="Enterprise document commitment";
    return row;
}
const core::CapacityProject& project(const runtime::Checkpoint& state,std::string_view id) {
    const auto& rows=state.world.simulation.capacity->projects;
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    if (found==rows.end()) throw std::runtime_error("Missing capacity project");
    return *found;
}
const core::MarketContract& saved_contract(const runtime::Checkpoint& state,std::string_view id="contract.docs") {
    const auto& rows=state.world.simulation.market->contracts;
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    if (found==rows.end()) throw std::runtime_error("Missing market contract");
    return *found;
}
void two_months_and_reload(const runtime::CompiledProgram& program) {
    auto engine=setup(program);
    apply(engine,core::ReserveFunds{"Delivered data license",core::Cents{1200000},false});
    apply(engine,core::ConsumeReservation{1,core::Cents{1200000}});
    apply(engine,core::AcceptContract{contract()});apply(engine,core::StartContract{"contract.docs"});
    const auto first=engine.checkpoint();
    for (unsigned day=0;day<30;++day) integration_fixture::step(engine);
    const auto saved=engine.checkpoint();
    const auto bytes=persistence::encode_save(program,saved);
    runtime::Runtime resumed{program,persistence::decode_save(program,bytes).checkpoint};
    check(resumed.checkpoint()==saved,"Pending monthly fee/work state failed exact reload");
    check(persistence::encode_save(program,resumed.checkpoint())==bytes,"Market serialization must be deterministic");
    for (unsigned day=30;day<59;++day) {
        integration_fixture::step(engine);integration_fixture::step(resumed);
        check(engine.checkpoint()==resumed.checkpoint(),"Resumed contract duplicated or lost delivery, income or support");
    }
    const auto final=resumed.checkpoint();
    const auto accounts=core::accounts_report(*final.world.simulation.accounts);
    check(accounts.revenue==core::Cents{5000000} && accounts.cash==core::Cents{101200000} &&
          accounts.expenses==core::Cents{23800000},"Two calendar months must book $50000 contract income once and leave $1012000");
    check(project(final,"serve").useful_tasks==59000 && project(final,"train").completed_work==core::MilliCU{3540000},
          "Contract serving and training must consume their shared 20/60 CU allocations");
    check(final.world.simulation.random==first.world.simulation.random,"Market allocation consumed authoritative RNG");
    const auto report=core::market_report(*final.world.simulation.market);
    check(report.delivered_tasks==59000 && report.customer_charges==core::Cents{5000000} && report.active_contracts==1,
          "Immutable market report differs from actual delivered/billed work");
    check(!final.world.simulation.market->adoptions.empty(),"Month-end installed shares were not persisted");
    const auto snapshot=core::Simulation{final.world.simulation}.snapshot();
    check(snapshot->market && snapshot->market->delivered_tasks==report.delivered_tasks,"UI market snapshot lacks committed report");
    auto corrupt=Json::parse(persistence::encode_save(program,final));
    corrupt["checkpoint"]["world"]["simulation"]["market"]["unsupported"]=1;invalid_save(program,corrupt);
    corrupt=Json::parse(persistence::encode_save(program,final));
    corrupt["checkpoint"]["world"]["simulation"].erase("market");invalid_save(program,corrupt);
    corrupt=Json::parse(persistence::encode_save(program,final));
    corrupt["checkpoint"]["world"]["simulation"]["market"]=nullptr;invalid_save(program,corrupt);
    corrupt=Json::parse(persistence::encode_save(program,final));
    corrupt["checkpoint"]["world"]["simulation"]["market"]["days"][0]["contracts"][0]["delivered_tasks"]=999;
    invalid_save(program,corrupt);
    corrupt=Json::parse(persistence::encode_save(program,final));
    for (auto& row:corrupt["checkpoint"]["world"]["simulation"]["capacity"]["days"][0]["projects"])
        if (row["project_id"]=="serve") row.erase("protected_tasks");
    invalid_save(program,corrupt);
    check(resumed.checkpoint()==final,"Invalid save handling changed live market state");
}
void fee_usage_budget_and_actual_work(const runtime::CompiledProgram& program) {
    auto engine=setup(program,false,core::Cents{0},10,core::Cents{200},core::Cents{11});
    auto terms=contract();terms.tasks_per_day=5;terms.monthly_fee=core::Cents{2800};terms.usage_price=core::Cents{20};
    apply(engine,core::AcceptContract{terms});apply(engine,core::StartContract{terms.id});
    auto excess=terms;excess.id="overcommitted";excess.tasks_per_day=6;
    refused_unchanged(engine,core::AcceptContract{excess});
    const auto before=engine.checkpoint();
    const auto stale=engine.submit({before.world.simulation.revision-1,core::SetMarketProductAvailable{"product.docs",false}});
    check(stale.reason==runtime::RuntimeReason::stale_revision && engine.checkpoint()==before,"Stale market command changed eligibility");
    integration_fixture::step(engine);
    const auto state=engine.checkpoint();const auto& closed=state.world.simulation.market->days.back();
    check(closed.contracts.size()==1 && closed.contracts.front().gross_fee==core::Cents{90} &&
          closed.contracts.front().usage_charge==core::Cents{100},"Contract actual-month fee and usage must be distinct");
    check(closed.segments.front().charged==core::Cents{190} && closed.segments.front().unspent==core::Cents{10} &&
          closed.segments.front().outside_tasks==5,"Fixed fee and usage spent the same segment budget twice or lost refused demand");
    check(project(state,"serve").useful_tasks==5 && project(state,"serve").completed_work==core::MilliCU{100},
          "Unused planned serving capacity invented tasks without customer demand");
    check(core::accounts_report(*state.world.simulation.accounts).revenue==core::Cents{190},"Undelivered usage generated product revenue");
    auto discretionary=setup(program,false,core::Cents{0},10,core::Cents{200},core::Cents{11});
    integration_fixture::step(discretionary);
    auto corrupt=Json::parse(persistence::encode_save(program,discretionary.checkpoint()));
    corrupt["checkpoint"]["world"]["simulation"]["market"]["segments"][0]["preferences"][0]["eligible"]=false;
    invalid_save(program,corrupt);
    corrupt=Json::parse(persistence::encode_save(program,discretionary.checkpoint()));
    corrupt["checkpoint"]["world"]["simulation"]["market"]["segments"][0]["preferences"][0]["willingness_to_pay_cents"]=0;
    invalid_save(program,corrupt);
}
void contract_priority_credits_and_deadline(const runtime::CompiledProgram& program) {
    auto engine=setup(program,false,core::Cents{0});
    auto other=serving("other",core::Cents{0});
    apply(engine,core::ReserveProject{other});
    apply(engine,core::AddMarketProduct{{"product.other","other",core::Cents{0},true}});
    auto terms=contract();terms.missing_task_penalty=core::Cents{1};
    apply(engine,core::AcceptContract{terms});apply(engine,core::StartContract{terms.id});
    apply(engine,core::SetEffectiveCapacity{"lease.initial",core::MilliCUPerDay{10000}});
    integration_fixture::step(engine);
    const auto state=engine.checkpoint();const auto& result=state.world.simulation.market->days.back().contracts.front();
    check(result.delivered_tasks==500 && result.gross_fee==core::Cents{80645} && result.service_credit==core::Cents{40322} &&
          result.penalty==core::Cents{500},"Outage credits/penalties must use missing actual tasks and actual-calendar-month fees");
    check(project(state,"serve").useful_tasks==500 && project(state,"other").useful_tasks==0,
          "Discretionary serving displaced protected contractual work");
    check(core::accounts_report(*state.world.simulation.accounts).revenue==core::Cents{40323},
          "Service credit failed to reduce the owed fixed fee exactly once");
    const auto reloaded=persistence::decode_save(program,persistence::encode_save(program,state)).checkpoint;
    check(reloaded==state,"Outage/credit receipts failed exact reload");

    auto pending=setup(program,false,core::Cents{0});
    terms=contract();terms.monthly_fee=core::Cents{0};terms.start_by=core::OrdinalDay{1};terms.missed_start_penalty=core::Cents{1000};
    apply(pending,core::SetMarketProductAvailable{"product.docs",false});
    apply(pending,core::AcceptContract{terms});
    integration_fixture::step(pending);integration_fixture::step(pending);
    check(saved_contract(pending.checkpoint()).status==core::ContractStatus::missed,
          "Unstarted agreement did not expire at its disclosed start deadline");
    check(core::market_report(*pending.checkpoint().world.simulation.market).penalties==core::Cents{1000},
          "Missed deadline penalty was not recorded once");
    refused_unchanged(pending,core::StartContract{"contract.docs"});
    const auto missed=pending.checkpoint();
    pending=runtime::Runtime{program,persistence::decode_save(program,persistence::encode_save(program,missed)).checkpoint};
    integration_fixture::step(pending);
    check(core::market_report(*pending.checkpoint().world.simulation.market).penalties==core::Cents{1000},
          "Missed deadline penalty replayed on a later day");

    auto cancelled=setup(program,false,core::Cents{0});
    terms=contract();terms.monthly_fee=core::Cents{0};terms.start_by=core::OrdinalDay{10};
    terms.cancellation_penalty=core::Cents{1234};
    apply(cancelled,core::SetMarketProductAvailable{"product.docs",false});
    apply(cancelled,core::AcceptContract{terms});apply(cancelled,core::CancelContract{terms.id});
    refused_unchanged(cancelled,core::CancelContract{terms.id});
    integration_fixture::step(cancelled);
    check(saved_contract(cancelled.checkpoint()).status==core::ContractStatus::cancelled &&
          cancelled.checkpoint().world.simulation.cash==core::Cents{119998766},
          "Cancellation must preserve its agreed penalty without free revenue or repeat charges");
}
void installed_incumbency_and_legacy(const runtime::CompiledProgram& program) {
    auto engine=initial(program);
    apply(engine,core::ReserveProject{serving("serve",core::Cents{0})});
    apply(engine,core::ReserveProject{serving("other",core::Cents{0})});
    apply(engine,core::EnableMarket{});
    apply(engine,core::AddMarketProduct{{"product.docs","serve",core::Cents{0},true}});
    apply(engine,core::AddMarketProduct{{"product.other","other",core::Cents{0},false}});
    auto demand=segment();demand.preferences.front().willingness_to_pay=core::Cents{0};
    demand.preferences.push_back({"product.other",150,core::Cents{0},100,true});
    apply(engine,core::AddMarketSegment{demand});
    for (unsigned day=0;day<31;++day) integration_fixture::step(engine);
    const auto monthly=engine.checkpoint();
    const auto& adoption=monthly.world.simulation.market->adoptions.back();
    const auto incumbent=std::find_if(adoption.shares.begin(),adoption.shares.end(),[](const auto& row){return row.product_id=="product.docs";});
    check(incumbent!=adoption.shares.end() && incumbent->share==core::BasisPoints{10000},"Installed share must derive from actual incumbent useful work");
    engine=runtime::Runtime{program,persistence::decode_save(program,persistence::encode_save(program,monthly)).checkpoint};
    apply(engine,core::SetMarketProductAvailable{"product.other",true});integration_fixture::step(engine);
    const auto switched=engine.checkpoint();
    const auto& delivered=switched.world.simulation.market->days.back().discretionary;
    const auto newcomer=std::find_if(delivered.begin(),delivered.end(),[](const auto& row){return row.product_id=="product.other";});
    check(newcomer!=delivered.end() && newcomer->tasks==200,"Switching cost must use the saved incumbent share instead of penalizing every vendor equally");

    auto legacy=Json::parse(persistence::encode_save(program,monthly));
    legacy["checkpoint"]["world"]["simulation"].erase("market");
    legacy["checkpoint"]["world"]["simulation"].erase("market_mode");
    const auto restored=persistence::decode_save(program,rehash(legacy)).checkpoint;
    check(!restored.world.simulation.market && restored.world.simulation.capacity,
          "Legacy capacity-only save must keep explicit market absence without fabricated income");
    auto engineering=initial(program).checkpoint();
    auto old=Json::parse(persistence::encode_save(program,engineering));
    old["checkpoint"]["world"]["simulation"].erase("market");
    old["checkpoint"]["world"]["simulation"].erase("market_mode");
    check(persistence::decode_save(program,rehash(old)).checkpoint==engineering,"Original capacity fixture compatibility changed");
}
}
int main() {
    try {
        const auto program=integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH,false);
        two_months_and_reload(program);fee_usage_budget_and_actual_work(program);
        contract_priority_credits_and_deadline(program);installed_incumbency_and_legacy(program);
        std::cout << checks << " market command/save integration checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n';return 1; }
}
