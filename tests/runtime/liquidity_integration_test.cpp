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
    check(!engine.submit({before.world.simulation.revision,std::move(action)}).accepted(),"Invalid liquidity command was accepted");
    check(engine.checkpoint()==before,"Rejected liquidity command changed state");
}
std::string rehash(Json value) {
    value.erase("sha256");value["sha256"]=persistence::sha256(value.dump());return value.dump();
}
void invalid_save(const runtime::CompiledProgram& program,const Json& value) {
    bool rejected=false;try {(void)persistence::decode_save(program,rehash(value));}
    catch (const persistence::PersistenceError&) {rejected=true;}
    check(rejected,"Malformed liquidity save was accepted");
}
runtime::Runtime initial(const runtime::CompiledProgram& program,core::Cents cash=core::Cents{100}) {
    runtime::Checkpoint checkpoint{runtime::foundation_world(cash),{}};
    checkpoint.world.enabled_flows=std::vector<std::string>{};
    checkpoint.world.simulation.accounts=core::opening_accounts(cash);
    checkpoint.world.simulation.liquidity=core::opening_liquidity();
    runtime::Runtime engine{program,std::move(checkpoint)};integration_fixture::prepare(engine);return engine;
}
const core::LiquidityState& liquidity(const runtime::Checkpoint& checkpoint) {
    return *checkpoint.world.simulation.liquidity;
}
void issue_bill(runtime::Runtime& engine,std::string id,core::Cents amount) {
    apply(engine,core::IssueInvoice{{std::move(id),core::InvoiceKind::payable,
        engine.checkpoint().world.simulation.date,amount,core::Cents{0},"Delivered supplier service"}});
}
void warning_reload_and_rescue(const runtime::CompiledProgram& program) {
    auto engine=initial(program);issue_bill(engine,"bill",core::Cents{200});
    check(liquidity(engine.checkpoint()).status==core::LiquidityStatus::operating,"Today's invoice should settle before insolvency is observed");
    integration_fixture::step(engine);
    const auto warning=engine.checkpoint();
    check(warning.world.simulation.date==core::OrdinalDay{1} && warning.world.simulation.paused &&
          liquidity(warning).status==core::LiquidityStatus::workout && liquidity(warning).workouts.front().deadline==core::OrdinalDay{31},
          "Unpaid close must pause next morning with exact 30-day deadline");
    check(core::liquidity_report(liquidity(warning)).pending_warning==1,"Initial warning lost its required acknowledgment");
    refused_unchanged(engine,core::AdvanceDay{});refused_unchanged(engine,core::SetPaused{false});
    refused_unchanged(engine,core::AcknowledgeLiquidityWarning{2});
    const auto bytes=persistence::encode_save(program,warning);
    runtime::Runtime resumed{program,persistence::decode_save(program,bytes).checkpoint};
    check(resumed.checkpoint()==warning,"Warning, original shortfall and deadline did not reload exactly");
    const auto stale=resumed.submit({warning.world.simulation.revision-1,core::AcknowledgeLiquidityWarning{1}});
    check(stale.reason==runtime::RuntimeReason::stale_revision && resumed.checkpoint()==warning,"Stale warning acknowledgment changed authoritative state");
    apply(engine,core::AcknowledgeLiquidityWarning{1});apply(resumed,core::AcknowledgeLiquidityWarning{1});
    for (unsigned day=0;day<3;++day) {
        integration_fixture::step(engine);integration_fixture::step(resumed);
        check(engine.checkpoint()==resumed.checkpoint(),"Acknowledged workout rerolled/reset after save");
    }
    const auto deadline=liquidity(resumed.checkpoint()).workouts.front().deadline;
    apply(resumed,core::InvestEquity{"rescuer",core::Cents{50},core::BasisPoints{100},"equity.rescue.1"});
    apply(resumed,core::SettleInvoice{"bill",core::Cents{50},"pay.partial"});
    check(liquidity(resumed.checkpoint()).status==core::LiquidityStatus::workout &&
          liquidity(resumed.checkpoint()).workouts.front().deadline==deadline,"Partial funding improperly restarted the workout timer");
    const auto day=resumed.checkpoint().world.simulation.date;
    apply(resumed,core::DrawDebt{{"bridge",core::Cents{50},core::BasisPoints{0},day,core::OrdinalDay{90},0,{}},"Approved rescue bridge"});
    apply(resumed,core::SettleInvoice{"bill",core::Cents{50},"pay.rest"});
    const auto recovered=resumed.checkpoint();
    check(liquidity(recovered).status==core::LiquidityStatus::operating && liquidity(recovered).workouts.front().resolved_on==day,
          "Actual payment after financing failed to resolve liquidity distress");
    check(core::accounts_report(*recovered.world.simulation.accounts).revenue==core::Cents{0} &&
          core::accounts_report(*recovered.world.simulation.accounts).debt==core::Cents{50},"Rescue proceeds were incorrectly treated as sales");
    check(persistence::decode_save(program,persistence::encode_save(program,recovered)).checkpoint==recovered,
          "Resolved workout history did not round-trip exactly");
    issue_bill(resumed,"new.bill",core::Cents{1});integration_fixture::step(resumed);
    check(liquidity(resumed.checkpoint()).workouts.size()==2 && liquidity(resumed.checkpoint()).workouts.back().id==2,
          "Independent later missed obligation did not receive a new stable warning identity");
    auto corrupt=Json::parse(bytes);corrupt["checkpoint"]["world"]["simulation"]["liquidity"]["workouts"][0]["deadline"]=32;
    invalid_save(program,corrupt);
    corrupt=Json::parse(bytes);corrupt["checkpoint"]["world"]["simulation"]["liquidity"]["exposure"]["unpaid_payables_cents"]=1;
    invalid_save(program,corrupt);
    corrupt=Json::parse(bytes);corrupt["checkpoint"]["world"]["simulation"].erase("liquidity");invalid_save(program,corrupt);
    corrupt=Json::parse(bytes);corrupt["checkpoint"]["world"]["simulation"]["liquidity"]=nullptr;invalid_save(program,corrupt);
    corrupt=Json::parse(bytes);corrupt["checkpoint"]["world"]["simulation"]["liquidity"]["unsupported"]=true;invalid_save(program,corrupt);
}
void exact_deadline_and_terminal_closure(const runtime::CompiledProgram& program) {
    auto engine=initial(program);issue_bill(engine,"bill",core::Cents{200});integration_fixture::step(engine);
    apply(engine,core::AcknowledgeLiquidityWarning{1});
    while (engine.checkpoint().world.simulation.date<core::OrdinalDay{31}) integration_fixture::step(engine);
    const auto deadline=engine.checkpoint();
    check(deadline.world.simulation.date==core::OrdinalDay{31} && liquidity(deadline).status==core::LiquidityStatus::workout,
          "Company closed before its deadline morning response opportunity");
    runtime::Runtime rescued{program,persistence::decode_save(program,persistence::encode_save(program,deadline)).checkpoint};
    apply(rescued,core::InvestEquity{"last_day",core::Cents{100},core::BasisPoints{100},"deadline.rescue"});
    apply(rescued,core::SettleInvoice{"bill",core::Cents{100},"deadline.payment"});integration_fixture::step(rescued);
    check(liquidity(rescued.checkpoint()).status==core::LiquidityStatus::operating,"Valid deadline-day rescue failed");
    runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,deadline)).checkpoint};
    integration_fixture::step(engine);integration_fixture::step(resumed);
    const auto closed=engine.checkpoint();
    check(closed==resumed.checkpoint() && closed.world.simulation.date==core::OrdinalDay{31} &&
          closed.world.simulation.phase==core::DayPhase::closed && liquidity(closed).status==core::LiquidityStatus::closed,
          "Unresolved workout must close on its deadline before work with exact resumed state");
    check(closed.world.simulation.accounts==deadline.world.simulation.accounts &&
          closed.world.simulation.capacity==deadline.world.simulation.capacity &&
          closed.world.simulation.market==deadline.world.simulation.market &&
          liquidity(closed).checked_through==core::OrdinalDay{30},
          "Closure must not advance accounting, market work or the date beyond 30 completed response intervals");
    check(closed.world.simulation.paused && core::accounts_report(*closed.world.simulation.accounts).payables==core::Cents{100},
          "Closure must retain unpaid liabilities and pause the company");
    refused_unchanged(engine,core::AdvanceDay{});
    refused_unchanged(engine,core::InvestEquity{"too_late",core::Cents{100},core::BasisPoints{100},"late.rescue"});
    refused_unchanged(engine,core::SetPaused{false});
    check(persistence::decode_save(program,persistence::encode_save(program,closed)).checkpoint==closed,
          "Closed postmortem checkpoint failed exact reload");
}
void maturity_and_legacy(const runtime::CompiledProgram& program) {
    auto engine=initial(program,core::Cents{1000});
    apply(engine,core::ReserveFunds{"Delivered expense",core::Cents{100},false});apply(engine,core::ConsumeReservation{1,core::Cents{100}});
    integration_fixture::step(engine);
    check(liquidity(engine.checkpoint()).status==core::LiquidityStatus::operating,"Negative profit alone caused insolvency");
    apply(engine,core::DrawDebt{{"mature",core::Cents{200},core::BasisPoints{0},core::OrdinalDay{1},core::OrdinalDay{2},0,{}},"Short bridge"});
    integration_fixture::step(engine);check(liquidity(engine.checkpoint()).status==core::LiquidityStatus::operating,"Future maturity warned too early");
    integration_fixture::step(engine);
    check(liquidity(engine.checkpoint()).exposure.matured_principal==core::Cents{200} && engine.checkpoint().world.simulation.cash==core::Cents{1100},
          "Matured unserviced debt must warn independently of available cash");
    apply(engine,core::RepayDebt{"mature",core::Cents{200},"principal.pay"});
    check(liquidity(engine.checkpoint()).status==core::LiquidityStatus::operating,"Matured principal repayment did not resolve warning");
    runtime::Checkpoint original{runtime::foundation_world(core::Cents{100}),{}};
    original.world.enabled_flows=std::vector<std::string>{};
    auto legacy=Json::parse(persistence::encode_save(program,original));
    legacy["checkpoint"]["world"]["simulation"].erase("liquidity");
    legacy["checkpoint"]["world"]["simulation"].erase("liquidity_mode");
    check(persistence::decode_save(program,rehash(legacy)).checkpoint==original,
          "Legacy checkpoint invented liquidity warnings or financial history");
}
}
int main() {
    try {
        const auto program=integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH,false);
        warning_reload_and_rescue(program);exact_deadline_and_terminal_closure(program);maturity_and_legacy(program);
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<checks<<" liquidity command/save integration checks passed\n";
}
