#include <pgp/session/scenario_run.hpp>
#include <pgp/persistence/sha256.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace {
using namespace pgp;
using Json = nlohmann::json;
std::size_t checks = 0;
void check(bool condition, const char* explanation) {
    ++checks;
    if (!condition) throw std::runtime_error(explanation);
}
std::string read_fixture() {
    std::ifstream file(PGP_BUSINESS_FIXTURE_PATH,std::ios::binary);
    if (!file) throw std::runtime_error("Business scenario fixture is missing");
    return {std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
}
runtime::CompiledProgram compile(std::string_view source) {
    const auto registry = content::foundation_registry();
    return runtime::compile_collection(content::load_collection(source,registry),registry,runtime::supported_effects());
}
void malformed(const Json& input) {
    bool rejected = false;
    try { (void)compile(input.dump()); } catch (const std::exception&) { rejected = true; }
    check(rejected,"Invalid business plan was accepted");
}
void content_boundaries(const std::string& source,const runtime::CompiledProgram& program) {
    auto input = Json::parse(source);
    input["definitions"][0]["business_plan"]["unsupported"] = 1; malformed(input);
    input = Json::parse(source);
    input["definitions"][0]["business_plan"]["training"]["data_license_cents"] = 1200000.0; malformed(input);
    input = Json::parse(source);
    input["definitions"][0]["business_plan"]["training"]["requested_milli_CU_per_day"] = 100000; malformed(input);
    input = Json::parse(source);
    input["definitions"][0]["business_plan"]["expansion_on"] = "2017-01-01"; malformed(input);
    input = Json::parse(source);
    input["definitions"][0]["business_plan"]["rescue_proceeds_cents"] = 50000001;
    check(persistence::save_identity(compile(input.dump())).content_hash != persistence::save_identity(program).content_hash,
          "Authored business policy is absent from canonical content identity");
}
void solvent_three_years(const runtime::CompiledProgram& program) {
    const auto uninterrupted = session::run_business_scenario(program,"scenario.business","US-CA",session::BusinessStrategy::expansion);
    check(uninterrupted.stop == session::BusinessStop::finished,"Solvent expansion strategy failed to finish the three-year scenario");
    const auto& state = uninterrupted.checkpoint.world.simulation;
    check(state.date == core::to_ordinal({2017,1,1}) && state.phase == core::DayPhase::finished,
          "Scenario did not stop at the exclusive three-year ending");
    check(state.liquidity->status == core::LiquidityStatus::operating && state.liquidity->workouts.empty(),
          "A funded expansion strategy entered an unserviced-liability workout");
    const auto accounts = core::accounts_report(*state.accounts);
    check(accounts.cash == core::Cents{124'693'549} && state.cash == accounts.cash,
          "Three-year opening plus net cash postings did not match hand-worked profit");
    check(accounts.revenue == core::Cents{429'677'419} && accounts.expenses == core::Cents{424'983'870} &&
          accounts.payables == core::Cents{0} && accounts.receivables == core::Cents{0} && accounts.debt == core::Cents{0},
          "Contract revenue, support and baseline/license costs failed three-year reconciliation");
    const auto market = core::market_report(*state.market);
    check(market.delivered_tasks == 5'240'000 && market.outside_tasks == 0,
          "Solvent strategy lost actual useful work or created demand before authored expansion");
    check(uninterrupted.months.size() == 36,"Three-year runner must report all 36 actual calendar months");
    core::Cents reported_revenue{0},reported_expenses{0};
    std::uint64_t reported_tasks = 0;
    for (const auto& month : uninterrupted.months) {
        reported_revenue = core::checked_unit_add(reported_revenue,month.revenue);
        reported_expenses = core::checked_unit_add(reported_expenses,month.expenses);
        reported_tasks += month.delivered_tasks;
        check(month.cash.value() > 0 && month.payables == core::Cents{0},
              "Solvent policy report contains unpaid bills or nonpositive cash");
    }
    check(reported_revenue == accounts.revenue && reported_expenses == accounts.expenses && reported_tasks == market.delivered_tasks,
          "Monthly reports do not sum to the authoritative accounting/work history");
    const auto& plan = *std::get<content::ScenarioDefinition>(program.collection.find("scenario.business")->data).business_plan;
    const auto training = std::find_if(state.capacity->projects.begin(),state.capacity->projects.end(),
        [&](const auto& row) { return row.id == plan.training.id; });
    check(training != state.capacity->projects.end() && training->status == core::ProjectStatus::completed &&
          training->completed_work == core::MilliCU{3'600'000},"Expansion bypassed its finite training prerequisite");
    std::cout << "Three-year expansion finished: cash " << accounts.cash.value()
              << ", delivered tasks " << market.delivered_tasks << std::endl;

    const auto midpoint = core::to_ordinal({2015,7,1});
    auto partial = session::run_business_scenario(program,"scenario.business","US-CA",session::BusinessStrategy::expansion,
        session::BusinessRunOptions{midpoint,{},false});
    check(partial.stop == session::BusinessStop::requested_boundary && partial.checkpoint.world.simulation.date == midpoint,
          "Scenario stop boundary crossed work that should still be unconsumed");
    const auto bytes = persistence::encode_save(program,partial.checkpoint);
    auto loaded = persistence::decode_save(program,bytes).checkpoint;
    check(loaded == partial.checkpoint,"Scenario midpoint save changed exact business or warning state");
    auto resumed = session::run_business_scenario(program,"scenario.business","US-CA",session::BusinessStrategy::expansion,{},std::move(loaded));
    check(resumed.stop == session::BusinessStop::finished && resumed.checkpoint == uninterrupted.checkpoint &&
          resumed.months == uninterrupted.months,"Saved scenario continuation differs from uninterrupted three-year execution");
    check(persistence::encode_save(program,resumed.checkpoint) == persistence::encode_save(program,uninterrupted.checkpoint),
          "Equivalent three-year states have different canonical save bytes");
    std::cout << "Midpoint save continuation matches uninterrupted final state" << std::endl;
}
void workout_closure_and_rescue(const runtime::CompiledProgram& program) {
    const auto warning = session::run_business_scenario(program,"scenario.business","US-CA",session::BusinessStrategy::baseline);
    check(warning.stop == session::BusinessStop::warning,"Baseline strategy should stop at its first required liquidity warning");
    const auto& pending = warning.checkpoint.world.simulation;
    check(pending.paused && pending.liquidity->status == core::LiquidityStatus::workout &&
          core::liquidity_report(*pending.liquidity).pending_warning.has_value(),"Baseline driver bypassed its explicit insolvency notice");
    std::cout << "Baseline warned on " << core::iso_date(pending.date) << std::endl;
    const auto& workout = pending.liquidity->workouts.back();
    check(workout.deadline == core::advance_days(workout.warned_on,core::Days{30}),
          "Warning did not preserve its exact 30-day workout deadline");
    auto continued = persistence::decode_save(program,persistence::encode_save(program,warning.checkpoint)).checkpoint;
    const auto closed = session::run_business_scenario(program,"scenario.business","US-CA",session::BusinessStrategy::baseline,
        session::BusinessRunOptions{{},{},true},continued);
    check(closed.stop == session::BusinessStop::closed && closed.checkpoint.world.simulation.phase == core::DayPhase::closed,
          "Acknowledged but unresolved baseline insolvency did not close the company");
    const auto& ending = *closed.checkpoint.world.simulation.liquidity;
    check(ending.workouts.back().closed_on == workout.deadline &&
          ending.workouts.back().deadline == workout.deadline,"Continuing/reloading reset or shortened the warned response window");
    const auto accounts = core::accounts_report(*closed.checkpoint.world.simulation.accounts);
    check(ending.exposure.unpaid_payables.value() > 0 && accounts.payables >= ending.exposure.unpaid_payables &&
          core::liquidity_exposure(*closed.checkpoint.world.simulation.accounts,ending.checked_through) == ending.exposure,
          "Closure erased outstanding supplier liabilities");
    const auto boundary = core::advance_days(pending.date,core::Days{1});
    const auto rescued = session::run_business_scenario(program,"scenario.business","US-CA",session::BusinessStrategy::rescue,
        session::BusinessRunOptions{boundary,{},false},continued);
    const auto& rescued_state = rescued.checkpoint.world.simulation;
    check(rescued.stop == session::BusinessStop::requested_boundary && rescued_state.liquidity->status == core::LiquidityStatus::operating &&
          rescued_state.liquidity->workouts.back().resolved_on.has_value(),"Recorded equity rescue did not cure actual overdue obligations");
    const auto rescued_accounts = core::accounts_report(*rescued_state.accounts);
    // March service has already accrued after rescue, but those invoices are
    // not due until month end. Recovery settles overdue bills without erasing
    // the new, unconsumed payment term or treating future dues as insolvency.
    const auto exposure = core::liquidity_report(*rescued_state.liquidity);
    check(exposure.unpaid_payables == core::Cents{0} && exposure.matured_principal == core::Cents{0} &&
          std::all_of(rescued_state.accounts->invoices.begin(),rescued_state.accounts->invoices.end(),[&](const auto& invoice) {
              return invoice.kind != core::InvoiceKind::payable || !rescued_state.accounts->closed_through ||
                     invoice.due > *rescued_state.accounts->closed_through || invoice.settled == invoice.amount;
          }) && rescued_accounts.equity == core::Cents{170'000'000} &&
          rescued_accounts.revenue == core::market_report(*rescued_state.market).customer_charges,
          "Rescue failed to settle overdue supplier bills or incorrectly recognized equity as product revenue");
}
}
int main(int argc,char* argv[]) {
    try {
        const std::string_view mode = argc == 1 ? "both" : std::string_view{argv[1]};
        check(argc <= 2 && (mode == "both" || mode == "--solvent-only" || mode == "--recovery-only"),
              "Unknown business scenario test selection");
        const auto source = read_fixture(); const auto program = compile(source);
        content_boundaries(source,program);
        if (mode != "--recovery-only") solvent_three_years(program);
        if (mode != "--solvent-only") workout_closure_and_rescue(program);
        std::cout << checks << " three-year business scenario checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
