#include "lab_demo.hpp"
#include "content_file.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>
#include <ostream>

namespace {
using namespace pgp;
void apply(runtime::Runtime& engine, core::CommandAction action) {
    const auto result=engine.submit({engine.checkpoint().world.simulation.revision,std::move(action)});
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
void days(runtime::Runtime& engine, unsigned count) { while (count-- != 0) apply(engine,core::AdvanceDay{}); }
void show(const runtime::Runtime& engine, std::ostream& output) {
    const auto& simulation=engine.checkpoint().world.simulation;
    const auto report=core::lab_report(*simulation.lab,*simulation.capacity);
    const auto& evidence=report.evidence.back();
    output << evidence.model_id << " on " << core::iso_date(simulation.date) << ": ";
    for (std::size_t j=0; j<core::lab_domain_count; ++j)
        output << core::lab_domain_names[j] << ' ' << evidence.lower[j].value() << ".." << evidence.upper[j].value() << " bp; ";
    output << '\n';
    for (const auto& finding:evidence.findings) output << "Finding: " << finding << '\n';
}
}
void run_lab_demo(const char* path, std::ostream& output) {
    using namespace pgp;
    const auto registry=content::foundation_registry();
    const auto program=runtime::compile_collection(content::load_collection(read_content_file(path),registry),registry,runtime::supported_effects());
    auto initial=runtime::begin_scenario(program,"scenario.first_playable","US-CA");
    initial.world.enabled_flows=std::vector<std::string>{};
    runtime::Runtime engine{program,std::move(initial)};
    const auto ready=engine.prepare_opening(engine.checkpoint().world.simulation.revision);
    if (!ready.accepted()) throw std::runtime_error(ready.explanation);
    core::TrainingRequest training{"baseline","recipe.document_baseline","",{"data.licensed_documents"},
        "lease.initial",core::MilliCUPerDay{60000},4,core::MilliCU{3600000}};
    const auto preview=core::preview_training(*engine.checkpoint().world.simulation.lab,training);
    output << "Training: " << preview.minimum_days.value() << " days, " << preview.unowned_data_cost.value()
           << " cents for the delivered license; existing 100 CU/day lease.\n";
    apply(engine,core::BuyData{"data.licensed_documents"});
    apply(engine,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20000},2}});
    apply(engine,core::StartTraining{training}); days(engine,30);
    runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,engine.checkpoint())).checkpoint};
    days(engine,30); days(resumed,30);
    if (engine.checkpoint()!=resumed.checkpoint()) throw std::runtime_error("Lab save continuation differs");
    apply(engine,core::StartEvaluation{"baseline_test","baseline","evaluation.held_out",{"lease.initial",core::MilliCUPerDay{20000},1}});
    days(engine,3); show(engine,output);
    apply(engine,core::BuyData{"data.consented_scans"});
    training.model_id="targeted"; training.data_ids.push_back("data.consented_scans");
    apply(engine,core::StartTraining{training}); days(engine,60);
    apply(engine,core::StartEvaluation{"targeted_test","targeted","evaluation.held_out",{"lease.initial",core::MilliCUPerDay{20000},1}});
    days(engine,3); show(engine,output);
    output << "Targeted data, finite retraining and fresh held-out evidence; no direct evaluation capability bonus.\n"
           << "Mid-training save/reload matched exactly. Cash: " << engine.checkpoint().world.simulation.cash.value() << " constant-2014 cents.\n";
}
