#include "accounts_demo.hpp"
#include "content_file.hpp"

#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>

#include <algorithm>
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

void run_capacity_demo(const char* content_path, std::ostream& output) {
    using namespace pgp;
    const auto registry = content::foundation_registry();
    const auto collection = content::load_collection(read_content_file(content_path), registry);
    const auto program = runtime::compile_collection(collection, registry, runtime::supported_effects());
    auto initial = runtime::begin_scenario(program, "scenario.foundation", "US-CA");
    initial.world.enabled_flows = std::vector<std::string>{};
    runtime::Runtime engine{program, initial};
    const auto ready = engine.prepare_opening(initial.world.simulation.revision);
    if (!ready.accepted()) throw std::runtime_error(ready.explanation);
    submit(engine, core::ReserveFunds{"data_license", core::Cents{1'200'000}});
    submit(engine, core::ConsumeReservation{1, core::Cents{1'200'000}});
    core::CapacityProject serving;
    serving.id = "serving.initial";
    serving.source_id = "lease.initial";
    serving.kind = core::ProjectKind::serving;
    serving.requested = core::MilliCUPerDay{20'000};
    serving.staff = 1;
    serving.tasks_per_day = 1000;
    serving.inference_milli_CU_per_task = core::MilliCU{20};
    serving.monthly_support_cost = core::Cents{500'000};
    serving.cause = "Engineering serving workload";
    submit(engine, core::ReserveProject{serving});
    core::CapacityProject training;
    training.id = "training.initial";
    training.source_id = "lease.initial";
    training.requested = core::MilliCUPerDay{60'000};
    training.staff = 4;
    training.total_work = core::MilliCU{3'600'000};
    training.cause = "Initial training work";
    submit(engine, core::ReserveProject{training});
    for (unsigned day = 0; day < 30; ++day) submit(engine, core::AdvanceDay{});
    const auto saved = engine.checkpoint();
    runtime::Runtime resumed{program, persistence::decode_save(program, persistence::encode_save(program, saved)).checkpoint};
    for (unsigned day = 30; day < 60; ++day) {
        submit(engine, core::AdvanceDay{});
        submit(resumed, core::AdvanceDay{});
    }
    const auto state = resumed.checkpoint().world.simulation;
    if (engine.checkpoint() != resumed.checkpoint()) throw std::runtime_error("Capacity reload diverged");
    const auto& projects = state.capacity->projects;
    const auto trained = std::find_if(projects.begin(), projects.end(), [](const auto& project) {
        return project.id == "training.initial";
    });
    const auto served = std::find_if(projects.begin(), projects.end(), [](const auto& project) {
        return project.id == "serving.initial";
    });
    if (trained == projects.end() || served == projects.end() ||
        trained->completed_work != core::MilliCU{3'600'000} || trained->status != core::ProjectStatus::completed ||
        served->useful_tasks != 60'000 || state.cash != core::Cents{96'200'000})
        throw std::runtime_error("Shared-capacity opening example does not reconcile");
    output << "Shared capacity demonstration: 60 days, exact resumed state\n"
           << "Serving: 20 CU/day, 60000 engineering tasks; training: 60 CU/day, 3600 CU complete\n"
           << "Lease: 100 CU/day; cash: 96200000 cents ($962,000)\n"
           << "Two settled months of baseline costs and separate serving support, plus one data license.\n"
           << "Customer demand, revenue and model qualification remain later work.\n";
}
