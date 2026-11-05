#include "business_run.hpp"
#include "content_file.hpp"

#include <pgp/session/scenario_run.hpp>
#include <pgp/runtime/scenario.hpp>

#include <ostream>
#include <stdexcept>

void run_business_file(const char* content_path, const char* strategy_name, std::ostream& output) {
    using namespace pgp;
    const auto registry = content::foundation_registry();
    const auto program = runtime::compile_collection(content::load_collection(read_content_file(content_path),registry),registry);
    const auto name = std::string_view{strategy_name};
    session::BusinessStrategy strategy;
    if (name == "expansion") strategy = session::BusinessStrategy::expansion;
    else if (name == "baseline") strategy = session::BusinessStrategy::baseline;
    else if (name == "rescue") strategy = session::BusinessStrategy::rescue;
    else throw std::invalid_argument("Business strategy must be expansion, baseline or rescue");
    // This engineering command explicitly acknowledges warnings to exercise
    // failure/recovery windows. Normal application drivers stop at the warning.
    session::BusinessRunOptions options;
    options.continue_workouts = true;
    options.reload_on = core::to_ordinal({2014,3,1});
    const auto result = session::run_business_scenario(program,"scenario.business","US-CA",strategy,options);
    output << "Business scenario policy: " << name << '\n'
           << "Through,Cash cents,Monthly revenue cents,Monthly expenses cents,Payables cents,Debt cents,Delivered tasks,Outside tasks\n";
    for (const auto& month : result.months)
        output << core::iso_date(month.through) << ',' << month.cash.value() << ',' << month.revenue.value() << ','
               << month.expenses.value() << ',' << month.payables.value() << ',' << month.debt.value() << ','
               << month.delivered_tasks << ',' << month.outside_tasks << '\n';
    const auto& state = result.checkpoint.world.simulation;
    const auto accounts = core::accounts_report(*state.accounts);
    const auto liquidity = core::liquidity_report(*state.liquidity);
    output << "Result: " << session::to_string(result.stop) << "; date: " << core::iso_date(state.date)
           << "; cash: " << accounts.cash.value() << "; payables: " << accounts.payables.value()
           << "; overdue payables: " << liquidity.unpaid_payables.value()
           << "; matured principal: " << liquidity.matured_principal.value() << '\n';
    for (const auto& workout : state.liquidity->workouts) {
        output << "Workout " << workout.id << ": warned " << core::iso_date(workout.warned_on)
               << ", deadline " << core::iso_date(workout.deadline) << ", cause " << workout.trigger.cause;
        if (workout.resolved_on) output << ", resolved " << core::iso_date(*workout.resolved_on);
        if (workout.closed_on) output << ", closed " << core::iso_date(*workout.closed_on);
        output << '\n';
    }
    if (result.reloaded_on) output << "Exact save/reload on " << core::iso_date(*result.reloaded_on) << '\n';
    output << "Engineering starting product; research/model qualification and player presentation remain M3 work.\n";
}
