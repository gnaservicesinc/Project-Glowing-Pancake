#include "market_demo.hpp"
#include "content_file.hpp"

#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>

#include <ostream>
#include <stdexcept>

namespace {
void apply(pgp::runtime::Runtime& engine, pgp::core::CommandAction action) {
    const auto result = engine.submit({engine.checkpoint().world.simulation.revision,std::move(action)});
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
}
void run_market_demo(const char* content_path, std::ostream& output) {
    using namespace pgp;
    const auto registry = content::foundation_registry();
    const auto collection = content::load_collection(read_content_file(content_path),registry);
    const auto program = runtime::compile_collection(collection,registry,runtime::supported_effects());
    auto initial = runtime::begin_scenario(program,"scenario.enterprise","US-CA");
    if (!initial.world.simulation.market) throw std::runtime_error("Enterprise scenario has no market setup");
    const auto* research = program.collection.find("research.foundation");
    if (!research || !std::holds_alternative<content::ResearchDefinition>(research->data))
        throw std::runtime_error("Enterprise example requires its engineering research workload");
    const auto& definition = std::get<content::ResearchDefinition>(research->data);
    runtime::Runtime engine{program,std::move(initial)};
    const auto ready = engine.prepare_opening(engine.checkpoint().world.simulation.revision);
    if (!ready.accepted()) throw std::runtime_error(ready.explanation);
    apply(engine,core::ReserveFunds{"Engineering data license",definition.cash});
    apply(engine,core::ConsumeReservation{1,definition.cash});
    core::CapacityProject training;
    training.id = "train.foundation"; training.source_id = "lease.initial";
    training.requested = core::MilliCUPerDay{60'000}; training.staff = 4;
    training.total_work = definition.work; training.cause = "Engineering training workload";
    apply(engine,core::ReserveProject{training});
    for (unsigned day = 0; day < 30; ++day) apply(engine,core::AdvanceDay{});
    const auto saved = engine.checkpoint();
    runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,saved)).checkpoint};
    for (unsigned day = 30; day < 59; ++day) {
        apply(engine,core::AdvanceDay{}); apply(resumed,core::AdvanceDay{});
    }
    const auto state = resumed.checkpoint().world.simulation;
    const auto report = core::market_report(*state.market);
    if (resumed.checkpoint() != engine.checkpoint() || state.cash != core::Cents{101'200'000} ||
        report.delivered_tasks != 59'000 || report.customer_charges != core::Cents{5'000'000} || report.outside_tasks != 0)
        throw std::runtime_error("Enterprise accounting/work example did not reconcile exactly");
    output << "Enterprise contracts demonstration through 2014-03-01\n"
           << "Delivered tasks: 59000; outside: 0; customer charges: 5000000 cents\n"
           << "Cash: 101200000 cents ($1,012,000); exact resumed contract/work/adoption state\n"
           << "Two months of $25,000 fees and separate $5,000 support; lease and data license charged once.\n"
           << "Engineering starting product; model qualification and the three-year strategy remain later work.\n";
}
