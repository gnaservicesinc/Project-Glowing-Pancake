#include "integration_fixture.hpp"

#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>

#include <iostream>

using namespace pgp;
namespace {
using Json = nlohmann::json;
std::size_t checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void apply(runtime::Runtime& engine, core::CommandAction action) {
    integration_fixture::require_accepted(engine.submit({engine.checkpoint().world.simulation.revision, std::move(action)}));
}
void refused_unchanged(runtime::Runtime& engine, core::CommandAction action) {
    const auto before = engine.checkpoint();
    check(!engine.submit({before.world.simulation.revision, std::move(action)}).accepted(),
          "Invalid accounts command was accepted");
    check(engine.checkpoint() == before, "Rejected financial command changed authoritative state");
}
std::string rehash(Json body) {
    body.erase("sha256");
    body["sha256"] = persistence::sha256(body.dump());
    return body.dump();
}
void invalid_save(const runtime::CompiledProgram& program, const Json& body) {
    bool rejected = false;
    try { (void)persistence::decode_save(program, rehash(body)); }
    catch (const persistence::PersistenceError&) { rejected = true; }
    check(rejected, "Malformed accounting save was accepted");
}
runtime::Checkpoint scenario(const runtime::CompiledProgram& program) {
    auto checkpoint = runtime::begin_scenario(program, "scenario.foundation", "US-CA");
    checkpoint.world.enabled_flows = std::vector<std::string>{};
    return checkpoint;
}
void two_months_and_interrupted_settlement(const runtime::CompiledProgram& program) {
    runtime::Runtime uninterrupted{program, scenario(program)};
    integration_fixture::prepare(uninterrupted);
    check(uninterrupted.checkpoint().world.simulation.accounts.has_value(),
          "Authored scenario failed to initialize balanced business accounts");
    apply(uninterrupted, core::ReserveFunds{"Delivered data license", core::Cents{1'200'000}, false});
    apply(uninterrupted, core::ConsumeReservation{1, core::Cents{1'200'000}});
    const auto initial = uninterrupted.checkpoint();
    const auto& accounts = *initial.world.simulation.accounts;
    const auto projected = core::project_cash(accounts, core::to_ordinal({2014, 2, 28}));
    check(projected.cash == core::Cents{97'200'000} && uninterrupted.checkpoint() == initial,
          "Two-month projection failed opening hand calculation or mutated state");
    for (unsigned day = 0; day != 30; ++day) integration_fixture::step(uninterrupted);
    const auto before_settlement = uninterrupted.checkpoint();
    check(before_settlement.world.simulation.date == core::OrdinalDay{30} &&
          before_settlement.world.simulation.cash == core::Cents{118'800'000},
          "Monthly accrual spent cash before its settlement boundary");
    const auto bytes = persistence::encode_save(program, before_settlement);
    runtime::Runtime resumed{program, persistence::decode_save(program, bytes).checkpoint};
    check(resumed.checkpoint() == before_settlement, "Monthly pending accrual did not round-trip exactly");
    check(persistence::encode_save(program, resumed.checkpoint()) == bytes,
          "Accounts save serialization must be deterministic");
    for (unsigned day = 0; day != 29; ++day) {
        integration_fixture::step(uninterrupted);
        integration_fixture::step(resumed);
        check(resumed.checkpoint() == uninterrupted.checkpoint(),
              "Resumed month settlement diverged or duplicated a charge");
    }
    const auto final = resumed.checkpoint();
    const auto report = core::accounts_report(*final.world.simulation.accounts);
    check(final.world.simulation.date == core::OrdinalDay{59} && final.world.simulation.cash == core::Cents{97'200'000} &&
          report.cash == final.world.simulation.cash && report.expenses == core::Cents{22'800'000} &&
          report.payables == core::Cents{0}, "Two months plus data license must reconcile to $972000 cash");
    check(final.world.simulation.accounts->closed_through == core::OrdinalDay{58},
          "Business daily cursor must settle Feb28 before preparing Mar1");
    check(final.world.simulation.random == initial.world.simulation.random,
          "Deterministic accounting consumed authoritative random draws");
    const auto snapshot = core::Simulation{final.world.simulation}.snapshot();
    check(snapshot->accounts.has_value() && snapshot->accounts->cash == snapshot->cash,
          "Immutable UI account report failed cash reconciliation");
    auto corrupt = Json::parse(persistence::encode_save(program, final));
    corrupt["checkpoint"]["world"]["simulation"]["cash_cents"] = 1;
    invalid_save(program, corrupt);
    corrupt = Json::parse(persistence::encode_save(program, final));
    corrupt["checkpoint"]["world"]["simulation"]["accounts"]["unsupported"] = 1;
    invalid_save(program, corrupt);
    corrupt = Json::parse(persistence::encode_save(program, final));
    corrupt["checkpoint"]["world"]["simulation"].erase("accounts");
    invalid_save(program, corrupt);
    corrupt = Json::parse(persistence::encode_save(program, final));
    corrupt["checkpoint"]["world"]["simulation"]["accounts"] = nullptr;
    invalid_save(program, corrupt);
    check(resumed.checkpoint() == final, "Rejected accounting save changed live play");
}
void protected_reservations_and_recovery(const runtime::CompiledProgram& program) {
    auto checkpoint = runtime::Checkpoint{runtime::foundation_world(core::Cents{1000}), {}};
    checkpoint.world.enabled_flows = std::vector<std::string>{};
    checkpoint.world.simulation.accounts = core::opening_accounts(core::Cents{1000});
    runtime::Runtime engine{program, checkpoint};
    integration_fixture::prepare(engine);
    apply(engine, core::ReserveFunds{"Protected training license", core::Cents{800}});
    apply(engine, core::IssueInvoice{{"supplier", core::InvoiceKind::payable, core::OrdinalDay{0},
          core::Cents{500}, core::Cents{0}, "Delivered supplier service"}});
    const auto before = engine.checkpoint();
    const auto stale = engine.submit({before.world.simulation.revision - 1,
                                      core::SettleInvoice{"supplier", core::Cents{1}, "stale.payment"}});
    check(stale.reason == runtime::RuntimeReason::stale_revision && engine.checkpoint() == before,
          "Stale payment command must preserve journal and cash");
    refused_unchanged(engine, core::SettleInvoice{"supplier", core::Cents{201}, "over.reserved"});
    integration_fixture::step(engine);
    auto report = core::accounts_report(*engine.checkpoint().world.simulation.accounts);
    check(report.cash == core::Cents{800} && report.payables == core::Cents{300} && report.expenses == core::Cents{500},
          "Automatic due payment consumed reservations or silently discarded unpaid debt");
    const auto bytes = persistence::encode_save(program, engine.checkpoint());
    engine = runtime::Runtime{program, persistence::decode_save(program, bytes).checkpoint};
    apply(engine, core::CancelReservation{1});
    integration_fixture::step(engine);
    report = core::accounts_report(*engine.checkpoint().world.simulation.accounts);
    check(report.cash == core::Cents{500} && report.payables == core::Cents{0} && report.expenses == core::Cents{500},
          "Released reservation must permit overdue payment without recognizing expense twice");
    refused_unchanged(engine, core::SettleInvoice{"supplier", core::Cents{1}, "after.paid"});
    refused_unchanged(engine, core::IssueInvoice{{"supplier", core::InvoiceKind::payable, core::OrdinalDay{2},
                      core::Cents{500}, core::Cents{0}, "Repeated supplier document"}});
}
void financing_commands(const runtime::CompiledProgram& program) {
    runtime::Runtime engine{program, scenario(program)};
    integration_fixture::prepare(engine);
    apply(engine, core::DrawDebt{{"working_capital", core::Cents{100'000}, core::BasisPoints{100},
          core::OrdinalDay{0}, core::OrdinalDay{90}, 0, {}}, "Approved working capital"});
    apply(engine, core::RepayDebt{"working_capital", core::Cents{20'000}, "principal.partial"});
    apply(engine, core::InvestEquity{"investor.new", core::Cents{30'000}, core::BasisPoints{1000}, "round.one"});
    const auto state = engine.checkpoint();
    const auto report = core::accounts_report(*state.world.simulation.accounts);
    check(report.cash == core::Cents{120'110'000} && report.debt == core::Cents{80'000} &&
          report.equity == core::Cents{120'030'000} && report.revenue == core::Cents{0},
          "Financing commands must preserve debt/equity/cash distinctions");
    refused_unchanged(engine, core::RepayDebt{"working_capital", core::Cents{80'001}, "over.principal"});
    refused_unchanged(engine, core::InvestEquity{"investor.duplicate", core::Cents{1}, core::BasisPoints{1}, "round.one"});
    check(persistence::decode_save(program, persistence::encode_save(program, state)).checkpoint == state,
          "Debt, cap table and financing receipts did not survive exact reload");
}
void old_foundation_save(const runtime::CompiledProgram& program) {
    runtime::Checkpoint checkpoint{runtime::foundation_world(core::Cents{1000}), {}};
    checkpoint.world.enabled_flows = std::vector<std::string>{};
    auto legacy = Json::parse(persistence::encode_save(program, checkpoint));
    legacy["checkpoint"]["world"]["simulation"].erase("accounts");
    legacy["checkpoint"]["world"]["simulation"].erase("accounting_mode");
    const auto restored = persistence::decode_save(program, rehash(legacy)).checkpoint;
    check(restored == checkpoint && !restored.world.simulation.accounts,
          "Legacy foundation saves must retain their state without fabricated financial history");
}
}
int main() {
    try {
        const auto program = integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH, false);
        two_months_and_interrupted_settlement(program);
        protected_reservations_and_recovery(program);
        financing_commands(program);
        old_foundation_save(program);
        std::cout << checks << " accounts command/save integration checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
