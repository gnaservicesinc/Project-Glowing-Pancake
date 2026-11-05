#include "../content/slice_fixture.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace pgp;
using Json = nlohmann::json;
unsigned checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void apply(runtime::Runtime& engine, core::CommandAction action) {
    const auto result = engine.submit({engine.checkpoint().world.simulation.revision, std::move(action)});
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
void refuse(runtime::Runtime& engine, core::CommandAction action) {
    const auto before = engine.checkpoint();
    check(!engine.submit({before.world.simulation.revision, std::move(action)}).accepted(), "Invalid lab command was accepted");
    check(engine.checkpoint() == before, "Refused lab command changed resources, RNG or lineage");
}
void days(runtime::Runtime& engine, unsigned count) { while (count-- != 0) apply(engine, core::AdvanceDay{}); }
const core::LabState& lab(const runtime::Runtime& engine) { return *engine.checkpoint().world.simulation.lab; }
core::TrainingRequest training(std::string id, std::vector<std::string> data = {"data.licensed_documents"}) {
    return {std::move(id), "recipe.document_baseline", "", std::move(data), "lease.initial", core::MilliCUPerDay{60000}, 4, core::MilliCU{3600000}};
}
runtime::CompiledProgram compile(Json doc) {
    pgp_test::scenario_doc(doc)["management"]["setbacks"]=Json::array(); // Isolate lab arithmetic; management integration owns dated outages.
    const auto registry = content::foundation_registry();
    return runtime::compile_collection(content::load_collection(doc.dump(),registry),registry,runtime::supported_effects());
}
runtime::Runtime engine(const runtime::CompiledProgram& program) {
    auto initial=runtime::begin_scenario(program,"scenario.first_playable","US-CA");
    initial.world.enabled_flows=std::vector<std::string>{}; // Isolate lab behavior from optional opportunity choices.
    runtime::Runtime result{program,std::move(initial)};
    const auto ready = result.prepare_opening(result.checkpoint().world.simulation.revision);
    if (!ready.accepted()) throw std::runtime_error(ready.explanation);
    return result;
}
void malformed_save(const runtime::CompiledProgram& program, Json doc) {
    doc.erase("sha256"); doc["sha256"] = persistence::sha256(doc.dump());
    bool rejected = false;
    try { (void)persistence::decode_save(program,doc.dump()); } catch (const persistence::PersistenceError&) { rejected = true; }
    check(rejected,"Rehashed inconsistent lab save was accepted");
}
void content_boundaries(const Json& source, const runtime::CompiledProgram& program) {
    for (unsigned mutation = 0; mutation < 7; ++mutation) {
        auto doc = source;
        auto& catalog = pgp_test::scenario_doc(doc)["lab"];
        if (mutation == 0) catalog["data"][0]["unique_units"] = 1.0;
        if (mutation == 1) catalog["data"].push_back(catalog["data"][0]);
        if (mutation == 2) catalog["recipes"][0]["compute_scale_milli_CU"] = 0;
        if (mutation == 3) catalog["recipes"][0]["requires_research"] = Json::array({"research.missing"});
        if (mutation == 4) catalog["data"][0]["unknown"] = true;
        if (mutation == 5) catalog["evaluations"][0]["uncertainty_bp"] = 10001;
        if (mutation == 6) pgp_test::definition(doc,"research.efficient_image_features")["prerequisites"]["all_of"] = Json::array({pgp_test::definition(doc,"research.efficient_image_features")["id"]});
        bool rejected = false;
        try { (void)compile(doc); } catch (const std::exception&) { rejected = true; }
        check(rejected,"Malformed authored lab data was accepted");
    }
    auto changed = source;
    pgp_test::scenario_doc(changed)["lab"]["data"][0]["price_cents"] = 1;
    check(persistence::save_identity(compile(changed)).content_hash != persistence::save_identity(program).content_hash,
          "Lab balance data was omitted from canonical content identity");
}
void arithmetic_case(const runtime::CompiledProgram& program) {
    auto catalog = std::get<content::ScenarioDefinition>(program.collection.definitions.at("scenario.first_playable").data).lab.value();
    catalog.data.resize(1); auto& data = catalog.data.front();
    data.unique_units = 10; data.quality = core::BasisPoints{5000};
    data.relevance = data.coverage = {core::BasisPoints{5000},core::BasisPoints{5000}};
    auto& recipe = catalog.recipes.front();
    recipe.compute_scale = core::MilliCU{100}; recipe.data_scale = {1,1};
    recipe.baseline = {core::BasisPoints{1000},core::BasisPoints{1000}};
    recipe.gain = {core::BasisPoints{6000},core::BasisPoints{6000}};
    recipe.fit = {core::BasisPoints{5000},core::BasisPoints{5000}};
    core::LabModel model; model.recipe_id = recipe.id; model.data_ids = {data.id}; model.consumed_work = core::MilliCU{100};
    const auto value = core::model_capability(catalog,model);
    check(value[0].value() == 2500 && value[1].value() == 2500,
          "Hand case: 10*0.5^3 rounds once to one data unit; half saturation/fit yields 2500 bp");
    data.commercial_use = !data.commercial_use; data.rights = "Different independent use rights";
    check(core::model_capability(catalog,model) == value,"Data rights became a hidden mathematical capability penalty");
}
void complete_loop(const runtime::CompiledProgram& program) {
    auto game = engine(program);
    check(lab(game).catalog.data.size() == 4 && lab(game).catalog.research.size() == 6,"Slice lacks its four offers or six research nodes");
    const auto initial_random = game.checkpoint().world.simulation.random;
    const auto preview_state = game.checkpoint();
    const auto preview = core::preview_training(lab(game), training("preview"));
    check(preview.minimum_days.value() == 60 && preview.unowned_data_cost.value() == 1200000 && !preview.licensed,
          "Training preview omitted time, license cost or uncertainty");
    check(preview.lower[0] < preview.upper[0] && preview.bottlenecks[1] == "data coverage" && game.checkpoint() == preview_state,
          "Preview changed state or concealed the weak data subgroup");
    refuse(game,core::StartTraining{training("unlicensed")});
    apply(game,core::BuyData{"data.licensed_documents"});
    refuse(game,core::BuyData{"data.licensed_documents"});
    refuse(game,core::StartResearch{"research.sequence_modeling",{"lease.initial",core::MilliCUPerDay{20000},2}});
    apply(game,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20000},2}});
    apply(game,core::StartTraining{training("baseline")});
    refuse(game,core::CloneModel{"baseline","too_early"});
    refuse(game,core::StartEvaluation{"too_early","baseline","evaluation.held_out",{"lease.initial",core::MilliCUPerDay{20000},1}});
    days(game,30);
    runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,game.checkpoint())).checkpoint};
    for (unsigned i=30; i<60; ++i) {
        days(game,1); days(resumed,1);
        check(game.checkpoint() == resumed.checkpoint(),"In-progress training reload diverged in work, cash, capability or RNG");
    }
    check(lab(game).models.front().consumed_work.value() == 3600000 && lab(game).research.front().completed,
          "Training/research did not finish from actual finite work");
    check(game.checkpoint().world.simulation.random == initial_random,"Deterministic training or research consumed evaluation RNG");
    const auto capability = lab(game).models.front().capability;
    check(capability[0].value() == 7000 && capability[1].value() == 2526,"Known resource-to-capability result changed");
    apply(game,core::CloneModel{"baseline","clone1"}); apply(game,core::CloneModel{"clone1","clone2"});
    check(lab(game).models.back().capability == capability && lab(game).models.back().consumed_work.value() == 3600000,
          "Repeated checkpoint cloning added free capability or work");
    auto continuation = training("continued"); continuation.parent_id = "clone2"; continuation.work = core::MilliCU{60000};
    apply(game,core::StartTraining{continuation}); days(game,1);
    check(lab(game).models.back().consumed_work.value() == 3660000,"Continuation added the parent score instead of cumulative actual work");
    const auto before_test = lab(game).models.front();
    apply(game,core::StartEvaluation{"holdout","baseline","evaluation.held_out",{"lease.initial",core::MilliCUPerDay{20000},1}});
    days(game,2);
    runtime::Runtime test_reload{program,persistence::decode_save(program,persistence::encode_save(program,game.checkpoint())).checkpoint};
    days(game,1); days(test_reload,1);
    check(game.checkpoint() == test_reload.checkpoint(),"Pending held-out evaluation rerolled after reload");
    check(lab(game).models.front() == before_test,"Buying evaluation increased actual capability");
    const auto evidence = lab(game).evaluations.front();
    check(evidence.completed && evidence.findings.size() == 1 && evidence.findings[0].starts_with("scanned_tables:"),
          "Held-out test failed to reveal the concrete scanned-table limitation");
    for (std::size_t j=0; j<core::lab_domain_count; ++j)
        check(evidence.lower[j] <= capability[j] && evidence.upper[j] >= capability[j],"Independent evidence interval missed its modeled true value");
    const auto report = core::Simulation{game.checkpoint().world.simulation}.snapshot();
    check(report->lab && report->lab->evidence.front().coverage[1].value() == 10000,"Immutable player report lost subgroup evidence");
    auto duplicate_data = training("duplicate",{"data.licensed_documents","data.licensed_documents"});
    refuse(game,core::StartTraining{duplicate_data});
    apply(game,core::BuyData{"data.consented_scans"});
    auto free_data = training("free_targeted",{"data.consented_scans"}); free_data.parent_id = "clone2";
    refuse(game,core::StartTraining{free_data});
    apply(game,core::StartTraining{training("targeted",{"data.licensed_documents","data.consented_scans"})});
    days(game,60);
    apply(game,core::StartEvaluation{"targeted_holdout","targeted","evaluation.held_out",{"lease.initial",core::MilliCUPerDay{20000},1}});
    days(game,3);
    check(lab(game).evaluations.back().completed && lab(game).evaluations.back().findings.empty() &&
          lab(game).evaluations.back().lower[1].value() >= 5500,"Targeted data and paid retraining did not produce independently verified recovery");
    auto cancelled = training("cancelled"); apply(game,core::StartTraining{cancelled}); days(game,1);
    const auto spent = game.checkpoint().world.simulation.cash;
    apply(game,core::CancelProject{"lab/train/cancelled"});
    check(lab(game).models.back().consumed_work.value() == 60000 && game.checkpoint().world.simulation.cash == spent,
          "Cancellation refunded consumed work or prior costs");
    apply(game,core::CloneModel{"cancelled","partial_checkpoint"});
    const auto saved = Json::parse(persistence::encode_save(program,game.checkpoint()));
    for (unsigned mutation=0; mutation<6; ++mutation) {
        auto corrupt = saved; auto& sim = corrupt["checkpoint"]["world"]["simulation"];
        if (mutation == 0) sim["lab"]["models"][0]["consumed_work_milli_CU"] = 7200000;
        if (mutation == 1) sim["lab"]["evaluations"][0]["measured_bp"][1] = 9999;
        if (mutation == 2) sim["lab"]["catalog"]["recipes"][0]["gain_bp"]["documents"] = 0;
        if (mutation == 3) sim["lab"]["models"][1]["parent_id"] = "clone2";
        if (mutation == 4) { sim.erase("lab"); sim.erase("lab_mode"); }
        if (mutation == 5) sim["lab"]["research_stream_origin"] = 1;
        malformed_save(program,std::move(corrupt));
    }
    const auto exact = persistence::decode_save(program,saved.dump()).checkpoint;
    check(exact == game.checkpoint(),"Completed evidence, licenses and checkpoint lineage failed exact save round-trip");
}
void unlocks_and_controls(const runtime::CompiledProgram& program) {
    auto game = engine(program);
    apply(game,core::BuyData{"data.open_documents"});
    auto efficient = training("efficient",{"data.open_documents"}); efficient.recipe_id="recipe.efficient";
    efficient.work=core::MilliCU{60000};
    refuse(game,core::StartTraining{efficient});
    // A failed payment must also roll back the capacity reservation made first.
    apply(game,core::ReserveFunds{"Protected runway",core::Cents{119999999}});
    refuse(game,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20000},2}});
    apply(game,core::CancelReservation{1});
    apply(game,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20000},2}});
    const auto interrupted=lab(game).research.front().project_id;
    apply(game,core::CancelProject{interrupted});
    apply(game,core::StartResearch{"research.held_out_evaluation",{"lease.initial",core::MilliCUPerDay{20000},2}});
    days(game,3);
    for (const auto id : {"research.reproducible_tooling","research.efficient_image_features","research.data_deduplication",
                          "research.sequence_modeling","research.efficient_serving"}) {
        apply(game,core::StartResearch{id,{"lease.initial",core::MilliCUPerDay{20000},2}}); days(game,3);
    }
    check(std::count_if(lab(game).research.begin(),lab(game).research.end(),[](const auto& run){return run.completed;}) == 6,
          "All six research nodes and a paid retry must be reachable through normal commands");
    apply(game,core::StartTraining{efficient}); days(game,1);
    const auto true_model=lab(game).models.front();
    apply(game,core::StartEvaluation{"headline","efficient","evaluation.headline",{"lease.initial",core::MilliCUPerDay{30000},1}});
    days(game,1);
    check(lab(game).evaluations.back().measured[1].value() == 0 && lab(game).evaluations.back().upper[1].value() == 0,
          "Headline-only evaluation revealed an untested subgroup");
    apply(game,core::StartEvaluation{"independent","efficient","evaluation.held_out",{"lease.initial",core::MilliCUPerDay{30000},1}});
    days(game,2);
    apply(game,core::StartEvaluation{"audit","efficient","evaluation.audit",{"lease.initial",core::MilliCUPerDay{30000},1}});
    days(game,4);
    const auto& ordinary=lab(game).evaluations[1]; const auto& audit=lab(game).evaluations[2];
    check(audit.upper[0].value()-audit.lower[0].value() < ordinary.upper[0].value()-ordinary.lower[0].value(),
          "More suitable independent testing failed to narrow evidence uncertainty");
    check(lab(game).models.front() == true_model,"Repeated paid tests changed actual model capability");
    const auto saved=persistence::encode_save(program,game.checkpoint());
    check(persistence::decode_save(program,saved).checkpoint == game.checkpoint(),"Retried research or audited evidence did not persist");
    // Temporary compute loss slows the same finite experiment without a reroll.
    auto slow=training("outage",{"data.open_documents"}); slow.work=core::MilliCU{120000};
    apply(game,core::StartTraining{slow});
    apply(game,core::SetEffectiveCapacity{"lease.initial",core::MilliCUPerDay{0}}); days(game,2);
    check(lab(game).models.back().consumed_work.value() == 0,"Outage created free training work");
    apply(game,core::SetEffectiveCapacity{"lease.initial",core::MilliCUPerDay{100000}}); days(game,2);
    check(lab(game).models.back().consumed_work.value() == 120000,"Restored source failed to resume the same experiment");
}
}
int main() {
    try {
        std::ifstream input(PGP_LAB_FIXTURE_PATH); const std::string bytes{std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{}};
        const auto source = Json::parse(bytes); const auto program = compile(source);
        content_boundaries(source,program); arithmetic_case(program); complete_loop(program); unlocks_and_controls(program);
        std::cout << checks << " lab content, resource, lineage, evidence and reload checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
