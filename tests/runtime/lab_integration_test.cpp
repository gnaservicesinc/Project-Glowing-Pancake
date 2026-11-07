#include "../content/slice_fixture.hpp"
#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <pgp/runtime/scenario.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

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
    for (unsigned mutation = 0; mutation < 9; ++mutation) {
        auto doc = source;
        auto& catalog = pgp_test::scenario_doc(doc)["lab"];
        if (mutation == 0) catalog["data"][0]["unique_units"] = 1.0;
        if (mutation == 1) catalog["data"].push_back(catalog["data"][0]);
        if (mutation == 2) catalog["recipes"][0]["compute_scale_milli_CU"] = 0;
        if (mutation == 3) catalog["recipes"][0]["requires_research"] = Json::array({"research.missing"});
        if (mutation == 4) catalog["data"][0]["unknown"] = true;
        if (mutation == 5) catalog["evaluations"][0]["uncertainty_bp"] = 10001;
        if (mutation == 6) pgp_test::definition(doc,"research.efficient_image_features")["prerequisites"]["all_of"] = Json::array({pgp_test::definition(doc,"research.efficient_image_features")["id"]});
        if (mutation == 7) catalog["data"][0]["coverage_bp"]["unknown_domain"] = 5000;
        if (mutation == 8) for (auto& recipe:catalog["recipes"])
            if (recipe["id"]=="recipe.sequence") recipe["data_scale"].erase("software_reasoning");
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
    data.relevance[0]=data.relevance[1]=data.coverage[0]=data.coverage[1]=core::BasisPoints{5000};
    auto& recipe = catalog.recipes.front();
    recipe.compute_scale = core::MilliCU{100}; recipe.data_scale = {1,1};
    recipe.baseline[0]=recipe.baseline[1]=core::BasisPoints{1000};
    recipe.gain[0]=recipe.gain[1]=core::BasisPoints{6000};
    recipe.fit[0]=recipe.fit[1]=core::BasisPoints{5000};
    core::LabModel model; model.recipe_id = recipe.id; model.data_ids = {data.id}; model.consumed_work = core::MilliCU{100};
    const auto value = core::model_capability(catalog,model);
    check(value[0].value() == 2500 && value[1].value() == 2500,
          "Hand case: 10*0.5^3 rounds once to one data unit; half saturation/fit yields 2500 bp");
    data.commercial_use = !data.commercial_use; data.rights = "Different independent use rights";
    check(core::model_capability(catalog,model) == value,"Data rights became a hidden mathematical capability penalty");
}
void complete_loop(const runtime::CompiledProgram& program) {
    auto game = engine(program);
    check(lab(game).catalog.data.size() == 9 && lab(game).catalog.research.size() == 6,
        "Slice lacks introductory and specialist offers or the six research nodes");
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

void specialist_services(const runtime::CompiledProgram& program) {
    using namespace core;
    auto opening=engine(program);
    const LabAllocation research{"lease.initial",MilliCUPerDay{20000},2};
    for (const auto id:{"research.held_out_evaluation","research.reproducible_tooling","research.efficient_image_features",
                        "research.sequence_modeling","research.efficient_serving"}) {
        apply(opening,StartResearch{id,research});
        days(opening,3);
    }
    apply(opening,BuyData{"data.open_documents"});
    const auto train_and_measure=[&](std::string recipe,std::vector<std::string> mixture,std::size_t target) {
        auto game=opening;
        auto request=training("specialist",std::move(mixture));
        request.recipe_id=std::move(recipe);
        const auto forecast=preview_training(lab(game),request);
        auto general=request;
        general.data_ids={"data.open_documents"};
        const auto without_specialist_data=preview_training(lab(game),general);
        check(forecast.lower[target]>without_specialist_data.upper[target] &&
              forecast.lower[0]==without_specialist_data.lower[0],
            "Specialist data failed to improve the intended capability independently");
        for (const auto& id:request.data_ids) if (id!="data.open_documents") apply(game,BuyData{id});
        apply(game,StartTraining{request});
        days(game,60);
        const LabAllocation evaluation{"lease.initial",MilliCUPerDay{20000},1};
        apply(game,StartEvaluation{"intro","specialist","evaluation.held_out",evaluation});
        apply(game,StartEvaluation{"suite","specialist","evaluation.capability_suite",evaluation});
        days(game,6);
        check(lab(game).evaluations.front().upper[target]==BasisPoints{0} &&
            lab(game).evaluations.back().lower[target]>=forecast.lower[target],
            "The authored capability suite failed to measure the new domain");
        return game;
    };
    const auto launch=[&](runtime::Runtime game,std::string product,std::size_t axis,bool local,bool review) {
        DeployModel request{product,"specialist","intro",local?"":"lease.initial",review,"region.europe"};
        const auto before=game.checkpoint();
        const auto refused=game.submit({before.world.simulation.revision,request});
        check(!refused.accepted() && refused.explanation.find(lab_domain_labels[axis])!=std::string::npos &&
              game.checkpoint()==before,"An introductory report qualified unmeasured specialist work");
        request.evidence_id="suite";
        apply(game,request);
        days(game,1);
        const auto& market=*game.checkpoint().world.simulation.market;
        check(std::ranges::any_of(market.days.back().discretionary,[&](const auto& sale) {
            return sale.product_id==product && sale.tasks>0;
        }),"Qualified specialist service failed to serve its regional customers");
        const auto bytes=persistence::encode_save(program,game.checkpoint());
        check(persistence::decode_save(program,bytes).checkpoint==game.checkpoint(),
            "The expanded capability evidence and production release did not reload exactly");
        if (axis==5) {
            auto corrupt=Json::parse(bytes);
            auto& evidence=corrupt["checkpoint"]["world"]["simulation"]["lab"]["evaluations"];
            evidence[1]["lower_bp"][axis]=10000;
            malformed_save(program,std::move(corrupt));
        }
    };
    const auto sequence=train_and_measure("recipe.sequence",{"data.open_documents","data.software_tasks","data.consented_dialogues"},2);
    launch(sequence,"product.coding_api",2,false,false);
    launch(sequence,"product.social_companion",4,false,false);
    launch(train_and_measure("recipe.efficient",{"data.open_documents","data.science_problems"},3),
        "product.research_toolkit",3,true,false);
    auto robotics=train_and_measure("recipe.image_features",{"data.open_documents","data.consented_scans","data.robotics_rollouts"},5);
    launch(robotics,"product.robotics_agent",5,false,true);
    apply(robotics,BuyData{"data.workflow_checks"});
    const GenerateDataset workflow{"synthetic.workflow","synthesis.workflow","specialist","suite","data.workflow_checks",
        20000,{"lease.initial",MilliCUPerDay{100000},2}};
    apply(robotics,workflow);
    days(robotics,5);
    const auto& workflow_output=lab(robotics).generated.back().output;
    check(workflow_output && workflow_output->unique_units>0 && workflow_output->coverage[5].value()>0 &&
        workflow_output->coverage[0]==BasisPoints{0},
        "The workflow generation recipe lost its independent verification or target-domain coverage");
    const auto workflow_save=persistence::encode_save(program,robotics.checkpoint());
    check(persistence::decode_save(program,workflow_save).checkpoint==robotics.checkpoint(),
        "Verified workflow data did not reopen with its teacher and source lineage");
}

void synthetic_data(const runtime::CompiledProgram& program) {
    using namespace core;
    auto game=engine(program);
    const auto simulation=[&]() -> const SimulationState& { return game.current_state().world.simulation; };
    const LabAllocation research{"lease.initial",MilliCUPerDay{20000},2};
    for (const auto id:{"research.held_out_evaluation","research.reproducible_tooling"}) {
        apply(game,StartResearch{id,research});
        days(game,3);
    }
    apply(game,BuyData{"data.open_documents"});
    apply(game,StartTraining{training("teacher",{"data.open_documents"})});
    days(game,60);
    const LabAllocation assessment{"lease.initial",MilliCUPerDay{20000},1};
    apply(game,StartEvaluation{"teacher.holdout","teacher","evaluation.held_out",assessment});
    apply(game,StartEvaluation{"teacher.headline","teacher","evaluation.headline",assessment});
    days(game,3);
    GenerateDataset request{"synthetic.verified","synthesis.document","teacher","teacher.holdout","data.licensed_documents",
        20000,{"lease.initial",MilliCUPerDay{100000},2}};
    refuse(game,request);
    apply(game,BuyData{"data.licensed_documents"});
    auto bad=request;
    bad.verification_id="data.open_documents";
    refuse(game,bad);
    bad=request;
    bad.evidence_id="teacher.headline";
    refuse(game,bad);
    bad=request;
    bad.attempted_units=200001;
    refuse(game,bad);
    const auto before_preview=game.checkpoint();
    const auto quote=preview_synthesis(lab(game),*simulation().capacity,simulation().date,request);
    check(quote.fee==Cents{100000} && quote.work==MilliCU{440000} && quote.minimum_days==Days{5} &&
        quote.retained_lower>0 && quote.retained_upper>=quote.retained_lower &&
        quote.commercial_use && !quote.open_weight_release && game.checkpoint()==before_preview,
        "Generation preview lost its paid work, conservative yield, rights or read-only behavior");
    const auto original_catalog=lab(game).catalog;
    const auto cash=simulation().cash;
    apply(game,request);
    check(simulation().cash==checked_unit_subtract(cash,quote.fee) && lab(game).generated.size()==1 &&
        !lab(game).generated.front().output && lab(game).catalog==original_catalog,
        "Starting synthesis published data early, changed authored terms or lost its fee");
    const auto project_id=lab(game).generated.front().project_id;
    const auto& projects=simulation().capacity->projects;
    const auto generation=std::ranges::find(projects,project_id,&CapacityProject::id);
    check(generation!=projects.end() && generation->kind==ProjectKind::data_generation &&
        generation->total_work==quote.work && generation->requested==request.allocation.daily_compute &&
        generation->staff==request.allocation.staff,"Generation omitted its exact finite staff and compute commitment");
    auto student=training("student",{"data.open_documents",request.id});
    student.work=MilliCU{6000000};
    refuse(game,StartTraining{student});
    runtime::Runtime resumed{program,persistence::decode_save(program,persistence::encode_save(program,game.checkpoint())).checkpoint};
    auto paused=game;
    days(paused,1);
    const auto paused_work=[&]() -> const CapacityProject& {
        return *std::ranges::find(paused.current_state().world.simulation.capacity->projects,project_id,&CapacityProject::id);
    };
    const auto spent_work=paused_work().completed_work;
    const auto pause_cash=paused.current_state().world.simulation.cash;
    apply(paused,PauseProject{project_id});
    check(paused.current_state().world.simulation.cash==pause_cash,"Pausing synthesis refunded its nonrefundable fee");
    days(paused,2);
    check(paused_work().completed_work==spent_work && !lab(paused).generated.front().output,
        "Paused synthesis consumed work or materialized an unfinished corpus");
    paused=runtime::Runtime{program,persistence::decode_save(program,persistence::encode_save(program,paused.checkpoint())).checkpoint};
    apply(paused,ResumeProject{project_id});
    days(paused,4);
    check(lab(paused).generated.front().output && paused_work().completed_work==quote.work,
        "Resumed synthesis failed to finish its original paid work");
    check(persistence::decode_save(program,persistence::encode_save(program,paused.checkpoint())).checkpoint==paused.checkpoint(),
        "Synthesis pause history or its completed output did not reopen exactly");
    auto cancelled=game;
    days(cancelled,1);
    const auto cancel_cash=cancelled.current_state().world.simulation.cash;
    apply(cancelled,CancelProject{project_id});
    check(!lab(cancelled).generated.front().output && cancelled.current_state().world.simulation.cash==cancel_cash,
        "Cancellation published a partial dataset or refunded the spent fee");
    days(cancelled,5);
    check(!lab(cancelled).generated.front().output,"Cancelled synthesis materialized output after its former end date");
    auto retry=request;
    retry.id="synthetic.retry";
    apply(cancelled,retry);
    check(lab(cancelled).generated.size()==2 && !lab(cancelled).generated.back().output,
        "Cancelling a generation job prevented a fresh paid retry");
    check(persistence::decode_save(program,persistence::encode_save(program,cancelled.checkpoint())).checkpoint==cancelled.checkpoint(),
        "Cancelled generation and its funded retry did not reopen exactly");
    for (unsigned day=0;day<5;++day) {
        days(game,1);
        days(resumed,1);
        check(game.checkpoint()==resumed.checkpoint(),"Pending synthesis changed output or costs across reload");
    }
    const auto& output=lab(game).generated.front().output;
    check(output && output->id==request.id && output->unique_units>=quote.retained_lower &&
        output->unique_units<=quote.retained_upper && output->commercial_use && !output->open_weight_release &&
        output->coverage[1]==BasisPoints{0} && lab(game).catalog==original_catalog,
        "Completed synthesis lost verified yield, source rights, domain scope or catalog immutability");
    const auto report=lab_report(lab(game),*simulation().capacity);
    check(std::ranges::find(report.data_offers,request.id,&DataOffer::id)!=report.data_offers.end() &&
        std::ranges::find(report.licenses,request.id,&DataLicense::offer_id)!=report.licenses.end(),
        "Completed generated data was absent from the training selection");
    auto plain=student;
    plain.data_ids={"data.open_documents"};
    check(preview_training(lab(game),student).lower[0]>preview_training(lab(game),plain).lower[0],
        "Verified synthetic data did not contribute to a fresh model's data-limited capability");
    apply(game,CloneModel{"teacher","teacher.clone"});
    apply(game,StartEvaluation{"clone.holdout","teacher.clone","evaluation.held_out",assessment});
    days(game,3);
    auto duplicate=request;
    duplicate.id="synthetic.clone";
    duplicate.teacher_id="teacher.clone";
    duplicate.evidence_id="clone.holdout";
    refuse(game,duplicate);
    auto diversity=game;
    for (const auto& [id,work]:std::array<std::pair<const char*,std::int64_t>,2>{
            std::pair{"teacher.cap_a",60000},std::pair{"teacher.cap_b",120000}}) {
        auto continuation=training(id,{"data.open_documents"});
        continuation.parent_id="teacher";
        continuation.work=MilliCU{work};
        apply(diversity,StartTraining{continuation});
        days(diversity,static_cast<unsigned>(work/60000));
        apply(diversity,StartEvaluation{std::string(id)+".holdout",id,"evaluation.held_out",assessment});
        days(diversity,3);
    }
    auto fill=request;
    fill.id="synthetic.diversity";
    fill.teacher_id="teacher.cap_a";
    fill.evidence_id="teacher.cap_a.holdout";
    fill.attempted_units=200000;
    apply(diversity,fill);
    auto over_budget=request;
    over_budget.id="synthetic.over_budget";
    over_budget.teacher_id="teacher.cap_b";
    over_budget.evidence_id="teacher.cap_b.holdout";
    over_budget.attempted_units=30001;
    const auto before_budget=diversity.checkpoint();
    const auto refused_budget=diversity.submit({before_budget.world.simulation.revision,over_budget});
    check(!refused_budget.accepted() && refused_budget.explanation.find("diversity")!=std::string::npos &&
        diversity.checkpoint()==before_budget,
        "Distinct teacher checkpoints bypassed the verifier's shared diversity budget");
    check(persistence::decode_save(program,persistence::encode_save(program,diversity.checkpoint())).checkpoint==diversity.checkpoint(),
        "The shared diversity commitment did not survive exact pending-job reload");
    apply(game,StartTraining{student});
    days(game,100);
    apply(game,StartEvaluation{"student.holdout","student","evaluation.held_out",assessment});
    days(game,3);
    refuse(game,DeployModel{"product.education_model","student","student.holdout","",false,"region.europe"});
    apply(game,DeployModel{"product.documents","student","student.holdout","lease.initial",false});
    days(game,1);
    check(std::ranges::any_of(simulation().market->days.back().discretionary,[](const auto& sale) {
        return sale.product_id=="product.documents" && sale.tasks>0;
    }),"A trained and measured synthetic-data student failed to serve customer work");
    const auto saved=persistence::encode_save(program,game.checkpoint());
    check(persistence::decode_save(program,saved).checkpoint==game.checkpoint(),
        "Generated lineage, student evaluation and deployed service did not reopen exactly");
    auto forged=Json::parse(saved);
    auto& retained=forged["checkpoint"]["world"]["simulation"]["lab"]["generated"][0]["output"]["unique_units"];
    retained=retained.get<std::uint64_t>()+1;
    malformed_save(program,std::move(forged));
}
void acquired_data(const runtime::CompiledProgram& program) {
    using namespace core;
    auto game=engine(program);
    const auto state=[&]() -> const SimulationState& {return game.current_state().world.simulation;};
    const auto save=[&](const runtime::Runtime& current) {
        const auto bytes=persistence::encode_save(program,current.checkpoint());
        check(persistence::decode_save(program,bytes).checkpoint==current.checkpoint(),
            "Collected data, consent and project history must reopen exactly");
        return bytes;
    };
    const StartDataAcquisition commission{"collected.consented_tables",{"lease.initial",MilliCUPerDay{20000},2}};
    const StartDataAcquisition feedback{"collected.enterprise_feedback",{"lease.initial",MilliCUPerDay{10000},1}};
    refuse(game,feedback);
    refuse(game,BuyData{commission.offer_id});
    const auto before=game.checkpoint();
    const auto quote=preview_data_acquisition(state(),commission);
    check(quote.fee==Cents{450000} && quote.work==MilliCU{100000} && quote.minimum_days==Days{5} &&
        game.checkpoint()==before,"Commissioning preview omitted paid terms or changed the campaign");
    const auto cash=state().cash;
    apply(game,commission);
    const auto project=lab(game).acquisitions.front().project_id;
    check(state().cash==checked_unit_subtract(cash,quote.fee) &&
        !lab(game).acquisitions.front().output && !data_available_on(lab(game),*state().capacity,commission.offer_id),
        "Commissioned data became usable before the work completed or skipped its paid fee");
    refuse(game,StartTraining{training("pending_commission",{commission.offer_id})});
    refuse(game,commission);
    auto cancelled=game;
    apply(cancelled,CancelProject{project});
    check(cancelled.current_state().world.simulation.cash==state().cash && !lab(cancelled).acquisitions.front().output,
        "Cancelling released a corpus or refunded its collection fee");
    apply(cancelled,commission);
    save(cancelled);
    days(game,1);
    apply(game,PauseProject{project});
    days(game,2);
    check(!lab(game).acquisitions.front().output,"Paused collection delivered before its work finished");
    game=runtime::Runtime{program,persistence::decode_save(program,save(game)).checkpoint};
    apply(game,ResumeProject{project});
    days(game,4);
    const auto& corpus=lab_data(lab(game),commission.offer_id);
    check(corpus.unique_units==300000 && corpus.open_weight_release &&
        data_available_on(lab(game),*state().capacity,commission.offer_id)==state().date,
        "Consented commission lost its record count, rights or availability date");
    refuse(game,commission);
    apply(game,BuyData{"data.licensed_documents"});
    const LabAllocation assessment{"lease.initial",MilliCUPerDay{20000},2};
    apply(game,StartResearch{"research.held_out_evaluation",assessment});
    apply(game,StartTraining{training("collected_teacher",{"data.licensed_documents",commission.offer_id})});
    days(game,60);
    apply(game,StartEvaluation{"collected_holdout","collected_teacher","evaluation.held_out",assessment});
    days(game,3);
    apply(game,DeployModel{"product.documents","collected_teacher","collected_holdout","lease.initial",false});
    const auto start=state().date;
    const auto collection=preview_data_acquisition(state(),feedback);
    auto empty=game;
    apply(empty,feedback);
    const auto service=std::ranges::find(state().market->products,"product.documents",&MarketProduct::id);
    apply(empty,CancelProject{service->capacity_project_id});
    days(empty,10);
    check(lab(empty).acquisitions.back().settled_deliveries==0 && !lab(empty).acquisitions.back().output,
        "A feedback agreement invented records without an eligible delivery");
    save(empty);
    apply(game,AcceptBusinessOffer{"offer.enterprise_service"});
    const auto cash_before=state().cash;
    apply(game,feedback);
    check(state().cash==checked_unit_subtract(cash_before,collection.fee) && !lab(game).acquisitions.back().output,
        "Opted-in collection did not reserve paid work or published early");
    const auto feedback_work=lab(game).acquisitions.back().project_id;
    days(game,3);
    apply(game,PauseProject{feedback_work});
    days(game,2);
    apply(game,ResumeProject{feedback_work});
    runtime::Runtime resumed{program,persistence::decode_save(program,save(game)).checkpoint};
    days(game,7); days(resumed,7);
    check(game.checkpoint()==resumed.checkpoint(),"Opted-in feedback diverged after a pending save reload");
    std::uint64_t settled=0;
    for(const auto& row:state().market->days) if(row.day>=start && row.day<collection.collection_ends_before) {
        for(const auto& sale:row.discretionary)
            if(sale.product_id=="product.documents" && sale.segment_id=="segment.enterprise") settled+=sale.tasks;
        for(const auto& contract:row.contracts) settled+=contract.delivered_tasks;
    }
    const auto& delivered=lab(game).acquisitions.back();
    check(settled>0 && delivered.settled_deliveries==settled && delivered.output &&
        delivered.output->unique_units==std::min<std::uint64_t>(12000,settled*4/10) &&
        delivered.output->commercial_use && !delivered.output->open_weight_release,
        "Feedback exceeded its dated customer consent, counted unrelated tasks or lost redistribution restrictions");
    refuse(game,feedback);
    apply(game,StartTraining{training("feedback_student",{"data.licensed_documents",feedback.offer_id})});
    auto forged=Json::parse(save(game));
    auto& records=forged["checkpoint"]["world"]["simulation"]["lab"]["acquisitions"];
    records[1]["settled_deliveries"]=settled+1;
    malformed_save(program,std::move(forged));
}

runtime::CompiledProgram workload_fixture(Json source) {
    auto& scenario=pgp_test::scenario_doc(source);
    scenario["management"]["staff_workload"]={{"high_load_bp",9500},{"recovery_load_bp",7500},
        {"daily_strain_bp",2500},{"daily_recovery_bp",1500},{"strain_tolerance_bp",0},{"maximum_output_loss_bp",4000}};
    for(auto& offer:scenario["business_offers"])
        if(offer["id"]=="offer.enterprise_service") offer["tasks_per_day"]=1800;
    return compile(std::move(source));
}
runtime::Runtime workload_game(const runtime::CompiledProgram& program) {
    using namespace core;
    auto game=engine(program);
    apply(game,BuyData{"data.licensed_documents"});
    const LabAllocation assessment{"lease.initial",MilliCUPerDay{20000},2};
    apply(game,StartResearch{"research.held_out_evaluation",assessment});
    days(game,3);
    apply(game,StartTraining{training("workload_teacher")});
    days(game,60);
    apply(game,StartEvaluation{"workload_holdout","workload_teacher","evaluation.held_out",assessment});
    days(game,3);
    apply(game,DeployModel{"product.documents","workload_teacher","workload_holdout","lease.initial",false});
    apply(game,AcceptBusinessOffer{"offer.enterprise_service"});
    auto student=training("workload_student");
    student.staff=5;
    apply(game,StartTraining{student});
    return game;
}
void staff_health(const Json& source) {
    using namespace core;
    const auto program=workload_fixture(source);
    auto game=workload_game(program);
    const auto state=[&]() -> const SimulationState& {return game.current_state().world.simulation;};
    const auto capacity=[&]() -> const CapacityState& {return *state().capacity;};
    const std::string student="lab/train/workload_student";
    const auto work=[&]() -> const CapacityProject& {
        return *std::ranges::find(capacity().projects,student,&CapacityProject::id);
    };
    const auto contract=[&]() -> const ContractDayResult& {return state().market->days.back().contracts.front();};
    check(capacity().workload && capacity().workload->strain==BasisPoints{0} &&
        capacity_report(capacity()).assigned_staff==6,
        "Rested staff began with strain or an uncommitted full allocation");
    days(game,1);
    check(work().completed_work==MilliCU{60000} && capacity().workload->strain==BasisPoints{2500} &&
        contract().delivered_tasks==1800 && staff_output_factor(capacity())==BasisPoints{9000},
        "Settled load should affect the following day, not work already recorded");
    days(game,1);
    check(work().completed_work==MilliCU{114000} && contract().delivered_tasks==1800,
        "Staff output reduced promised priority before discretionary tasks");
    days(game,1);
    check(work().completed_work==MilliCU{162000} && capacity().workload->strain==BasisPoints{7500} &&
        contract().promised_tasks==1800 && contract().delivered_tasks==1600 && contract().service_credit.value()>0 &&
        capacity_report(capacity()).assigned_staff==6,
        "Sustained strain did not reduce actual project work and create the ordinary signed service credit");
    const auto encoded=persistence::encode_save(program,game.checkpoint());
    auto loaded=persistence::decode_save(program,encoded);
    check(loaded.checkpoint==game.checkpoint(),"Staff strain and actual work did not reopen exactly");
    runtime::Runtime resumed{program,std::move(loaded.checkpoint)};
    auto hired=game;
    apply(hired,HireSpecialist{"specialist.evaluation"});
    days(hired,1);
    check(hired.current_state().world.simulation.capacity->workload->strain==BasisPoints{7500},
        "An added hire erased existing strain or made a neutral workload worsen");
    for(auto* current:{&game,&resumed}) {
        apply(*current,PauseProject{student});
        days(*current,5);
    }
    check(game.checkpoint()==resumed.checkpoint() && capacity().workload->strain==BasisPoints{0} &&
        staff_output_factor(capacity())==BasisPoints{10000} && work().completed_work==MilliCU{162000},
        "Low staffing load must restore output gradually while paused work stays fixed");
    apply(game,HireSpecialist{"specialist.evaluation"});
    apply(game,ResumeProject{student});
    days(game,1);
    check(work().completed_work==MilliCU{222000} && capacity().workload->strain==BasisPoints{0} &&
        contract().delivered_tasks==1800 && contract().service_credit==Cents{0},
        "Recovered staff could not maintain normal production with added headroom");
    auto forged=Json::parse(encoded);
    forged["checkpoint"]["world"]["simulation"]["capacity"]["workload"]["strain_bp"]=0;
    malformed_save(program,std::move(forged));
}

void research_setbacks(const runtime::CompiledProgram& program) {
    using namespace core;
    constexpr auto topic = "research.efficient_image_features";
    constexpr auto safeguard = "research.reproducible_tooling";
    const LabAllocation allocation{"lease.initial",MilliCUPerDay{10000},3};
    auto game = engine(program);
    auto safe = game;
    auto uncancelled = game;
    const auto random = game.checkpoint().world.simulation.random;
    const auto cash = [](const auto& active) {return active.checkpoint().world.simulation.cash;};
    const auto work = [](const auto& active) -> const CapacityProject& {
        const auto& state = active.checkpoint().world.simulation;
        return *std::ranges::find(state.capacity->projects,lab(active).research.back().project_id,&CapacityProject::id);
    };
    apply(game,StartResearch{topic,allocation});
    days(game,1);
    apply(game,CancelProject{work(game).id});
    check(!lab(game).research.back().failed && !lab(game).research.back().completed &&
        work(game).completed_work == MilliCU{10000}, "Cancellation resolved an outcome or erased actual work");
    apply(game,StartResearch{topic,allocation});
    check(lab(game).research.back().retry_credit == BasisPoints{0} && work(game).total_work == MilliCU{60000},
        "Cancellation gave a free credit");
    refuse(game,StartResearch{topic,allocation});
    days(game,5);
    runtime::Runtime reopened{program,persistence::decode_save(program,persistence::encode_save(program,game.checkpoint())).checkpoint};
    days(game,1); days(reopened,1);
    check(game.checkpoint() == reopened.checkpoint() && work(game).status == ProjectStatus::completed &&
        lab(game).research.back().failed && !lab(game).research.back().completed,
        "A settled setback must preserve actual work without creating knowledge, including after reload");
    check(game.checkpoint().world.simulation.random == random, "Experiment outcomes consumed another stream's draw");
    apply(uncancelled,StartResearch{topic,allocation}); days(uncancelled,6);
    check(lab(uncancelled).research.back().failed, "Cancelling an attempt changed the seeded outcome");
    refuse(game,PublishResearch{topic});
    apply(game,BuyData{"data.open_documents"});
    auto image = training("image",{"data.open_documents"});
    image.recipe_id = "recipe.image_features";
    refuse(game,StartTraining{image});
    const auto before_retry = cash(game);
    apply(game,StartResearch{topic,{"lease.initial",MilliCUPerDay{10000},1}});
    check(cash(game) == checked_unit_subtract(before_retry,Cents{50000}) &&
        lab(game).research.back().retry_credit == BasisPoints{2500} && work(game).total_work == MilliCU{60000},
        "A retry must pay the fee and meet the staff floor even when evidence reduces the experiment budget");
    apply(game,CancelProject{work(game).id});
    apply(game,StartResearch{topic,allocation});
    check(work(game).total_work == MilliCU{45000} && lab(game).research.back().retry_credit == BasisPoints{2500},
        "Retained evidence did not shorten eligible work or grew through cancellation");
    const auto retry_save = persistence::encode_save(program,game.checkpoint());
    runtime::Runtime retry{program,persistence::decode_save(program,retry_save).checkpoint};
    days(game,5); days(retry,5);
    check(game.checkpoint() == retry.checkpoint() && lab(game).research.back().completed &&
        !lab(game).research.back().failed, "Paid corrected retry did not produce an exact saved unlock");
    apply(game,PublishResearch{topic});
    apply(game,StartTraining{image});
    refuse(game,StartResearch{topic,allocation});

    apply(safe,StartResearch{safeguard,allocation}); days(safe,6);
    apply(safe,StartResearch{topic,allocation}); days(safe,6);
    check(lab(safe).research.back().completed && !lab(safe).research.back().failed,
        "Completed safeguard did not prevent the first setback");
    auto forged=Json::parse(retry_save);
    auto& saved_runs=forged["checkpoint"]["world"]["simulation"]["lab"]["research"];
    saved_runs[1].erase("failed");
    saved_runs[1]["completed"]=true;
    malformed_save(program,forged);
    forged=Json::parse(retry_save);
    forged["checkpoint"]["world"]["simulation"]["lab"]["research"].back()["retry_credit"]=1500;
    malformed_save(program,std::move(forged));
}

}
int main() {
    try {
        std::ifstream input(PGP_LAB_FIXTURE_PATH); const std::string bytes{std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{}};
        const auto source = Json::parse(bytes); const auto program = compile(source);
        content_boundaries(source,program); arithmetic_case(program); complete_loop(program); unlocks_and_controls(program);
        specialist_services(program);
        synthetic_data(program);
        acquired_data(program);
        staff_health(source);
        research_setbacks(program);
        std::cout << checks << " lab content, resource, lineage, evidence and reload checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
