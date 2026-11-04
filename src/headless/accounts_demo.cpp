#include "accounts_demo.hpp"
#include "content_file.hpp"

#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>

#include <ostream>
#include <stdexcept>

namespace {
void submit(pgp::runtime::Runtime& engine, pgp::core::CommandAction action) {
    const auto result = engine.submit({engine.checkpoint().world.simulation.revision, std::move(action)});
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
}

void run_accounts_demo(const char* content_path, std::ostream& output) {
    using namespace pgp;
    const auto registry = content::foundation_registry();
    const auto collection = content::load_collection(read_content_file(content_path), registry);
    const auto program = runtime::compile_collection(collection, registry, runtime::supported_effects());
    auto initial = runtime::begin_scenario(program, "scenario.foundation", "US-CA");
    // Isolate the authored opening accounts from the unrelated engineering
    // story fixtures. This command is an accounting example, not a strategy.
    initial.world.enabled_flows = std::vector<std::string>{};
    runtime::Runtime engine{program, initial};
    const auto ready = engine.prepare_opening(initial.world.simulation.revision);
    if (!ready.accepted()) throw std::runtime_error(ready.explanation);
    submit(engine, core::ReserveFunds{"data_license", core::Cents{1'200'000}});
    submit(engine, core::ConsumeReservation{1, core::Cents{1'200'000}});
    for (unsigned day = 0; day < 30; ++day) submit(engine, core::AdvanceDay{});
    const auto saved = engine.checkpoint();
    runtime::Runtime resumed{program, persistence::decode_save(program, persistence::encode_save(program, saved)).checkpoint};
    for (unsigned day = 30; day < 59; ++day) {
        submit(engine, core::AdvanceDay{});
        submit(resumed, core::AdvanceDay{});
    }
    if (engine.checkpoint() != resumed.checkpoint()) throw std::runtime_error("Accounting reload diverged");
    const auto state = resumed.checkpoint().world.simulation;
    const auto report = core::accounts_report(*state.accounts);
    if (report.cash != core::Cents{97'200'000} || report.expenses != core::Cents{22'800'000} ||
        report.revenue != core::Cents{0} || report.payables != core::Cents{0})
        throw std::runtime_error("Opening accounting example does not reconcile");
    output << "Opening accounts demonstration: " << core::iso_date(initial.world.simulation.date)
           << " through " << core::iso_date(state.date) << '\n'
           << "Cash: 97200000 constant-2014 cents ($972,000)\n"
           << "Expenses: 22800000 cents; revenue: 0; unpaid payables: 0\n"
           << "Two calendar months at $108,000 plus one $12,000 license.\n"
           << "Saved before January settlement; resumed ledger matches exactly.\n";
}
