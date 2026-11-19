#include <pgp/core/research_data.hpp>
#include <pgp/core/workplace_research.hpp>
#include <pgp/persistence/save.hpp>
#include <pgp/runtime/scenario.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool okay, const char *message) {
    if (!okay)
        throw std::runtime_error(message);
}
} // namespace

int main() {
    using namespace pgp;
    try {
        const auto registry = content::foundation_registry();
        const auto program = runtime::compile_collection(
            content::load_collection(persistence::read_bounded(PGP_RESEARCH_SOURCE, content::maximum_document_bytes),
                                     registry),
            registry);
        const auto &scenario =
            std::get<content::ScenarioDefinition>(program.collection.find("scenario.founder_campaign")->data);
        content::FoundingSelection founder;
        founder.family = "rich";
        founder.route = "bootstrap";
        founder.office = "home";
        for (const auto &scene : scenario.founding->scenes)
            founder.choices.emplace(scene.id, scene.choices.front().id);
        runtime::ScenarioOptions options;
        options.founding = founder;
        options.identity =
            runtime::CampaignIdentity{"Test research", founder.first_name + " " + founder.last_name, "they/them"};
        auto initial = runtime::begin_scenario(program, "scenario.founder_campaign", "US-CA",
                                               core::ObjectiveSelection{core::ObjectiveKind::survival, {}}, options);
        initial.world.enabled_flows = std::vector<std::string>{};
        runtime::Runtime game{program, initial};
        const auto state = [&]() -> const auto & { return game.current_state().world.simulation; };
        const auto submit = [&](core::CommandAction action) {
            return game.submit({state().revision, std::move(action)});
        };
        const auto apply = [&](core::CommandAction action) {
            const auto result = submit(std::move(action));
            if (!result.accepted())
                throw std::runtime_error(result.explanation);
        };
        const auto refuse = [&](core::CommandAction action, std::string_view because) {
            const auto before = game.current_state();
            const auto result = submit(std::move(action));
            if (result.accepted() || result.explanation.find(because) == std::string::npos ||
                game.current_state() != before)
                throw std::runtime_error("Expected a refusal mentioning " + std::string(because) +
                                         ", got: " + result.explanation);
        };
        const auto hq = [&](core::HeadquartersAction action) { apply(core::ManageHeadquarters{std::move(action)}); };
        const auto advance = [&](int days) {
            for (int i = 0; i < days; ++i)
                apply(core::AdvanceDay{});
        };
        const auto exact_save = [&] {
            check(
                persistence::decode_save(program, persistence::encode_save(program, game.current_state())).checkpoint ==
                    game.current_state(),
                "Workplace research must survive a fresh save exactly.");
        };
        const auto project = [&](const std::string &id) -> const core::CapacityProject & {
            return *std::ranges::find(state().capacity->projects, id, &core::CapacityProject::id);
        };

        check(game.prepare_opening(state().revision).accepted(), "The fresh opening must prepare.");
        const auto researcher = state().management->people->employees.front().terms.profile.id;
        const core::LabAllocation local{"hq.equipment/2", core::MilliCUPerDay{4000}, 1, {researcher}};
        const core::LabAllocation cloud{"lease/offer.supplier_small", core::MilliCUPerDay{30000}, 1, {researcher}};
        const auto root = "research.reproducible_tooling";
        const auto successor = "research.sequence_modeling";
        const auto specialized = "research.held_out_evaluation";
        check(state().capacity->sources.empty(), "The empty garage has no compute.");
        refuse(core::StartResearch{root, local}, "desk");
        apply(core::AcceptBusinessOffer{"offer.supplier_small"});
        check(state().capacity->sources.front().nominal == core::MilliCUPerDay{30000},
              "Signed rental supplies its actual server capacity.");
        refuse(core::StartResearch{root, cloud}, "desk");
        hq(core::BuyHQEquipment{"desk"});
        hq(core::BuyHQEquipment{"desktop"});
        advance(3);
        hq(core::PlaceHQEquipment{1, 0, 0, 0, false});
        hq(core::AssignHQDesk{1, "founder"});
        refuse(core::StartResearch{root, cloud}, "powered computer");
        hq(core::PlaceHQEquipment{2, 1, 0, 0, false});
        refuse(core::StartResearch{successor, local}, "prerequisites");
        refuse(core::StartResearch{specialized, local}, "evaluation station");
        const auto cash = state().cash;
        apply(core::StartResearch{root, local});
        check(state().cash == cash, "Research has no arbitrary start charge.");
        const auto run = state().lab->research.back().project_id;
        advance(1);
        check(project(run).completed_staff_days == 1 && project(run).completed_work.value() > 0,
              "An equipped person and local machine supply real work.");
        hq(core::PowerHQEquipment{2, false});
        check(project(run).status == core::ProjectStatus::paused, "Losing the workstation pauses research.");
        const auto before = project(run);
        advance(2);
        check(project(run) == before, "A paused project consumes neither time nor compute.");
        refuse(core::ResumeProject{run}, "powered computer");
        exact_save();
        hq(core::PowerHQEquipment{2, true});
        apply(core::ResumeProject{run});
        advance(6);
        check(project(run).completed_staff_days == project(run).required_staff_days &&
                  project(run).status == core::ProjectStatus::active,
              "A desktop can satisfy labor days before finishing the required experiments.");
        for (int n = 0; n < 30 && project(run).status == core::ProjectStatus::active; ++n)
            advance(1);
        check(project(run).status == core::ProjectStatus::completed && state().lab->research.back().completed,
              "The technology unlocks after both real resource goals complete.");
        apply(core::StartResearch{successor, cloud});
        const auto fast = state().lab->research.back().project_id;
        advance(4);
        check(project(fast).completed_work == project(fast).total_work &&
                  project(fast).status == core::ProjectStatus::active && project(fast).completed_staff_days == 4,
              "Fast paid compute cannot substitute for the remaining staff-days.");
        advance(2);
        check(project(fast).status == core::ProjectStatus::completed,
              "Both labor and experimental compute settle before completion.");
        const auto efficient="research.efficient_serving";
        const auto batched="research.compute.batched_serving";
        const auto low_precision="research.compute.low_precision";
        refuse(core::StartResearch{batched,cloud},"technology levels");
        apply(core::StartResearch{efficient,cloud});
        advance(6);
        refuse(core::StartResearch{low_precision,cloud},"technology levels");
        refuse(core::StartResearch{batched,cloud},"Compact server rack");
        hq(core::BuyHQEquipment{"rack"});
        advance(7);
        hq(core::PlaceHQEquipment{3,0,0,2,false});
        const auto inference=[&](core::OrdinalDay day) {
            const auto& lab=*state().lab;
            const auto recipe=std::ranges::find(lab.catalog.recipes,"recipe.efficient",&core::TrainingRecipe::id);
            return core::research_inference_cost(lab,*state().capacity,*recipe,day);
        };
        const auto before_innovation=state().date;
        const auto funds=state().cash;
        apply(core::StartResearch{batched,cloud});
        check(state().cash==core::checked_unit_subtract(funds,core::Cents{80000}),"Innovation pays its authored project fee.");
        check(inference(state().date)==core::MilliCU{12},"An unfinished innovation cannot discount a deployment.");
        advance(12);
        check(inference(state().date)==core::MilliCU{10} && inference(before_innovation)==core::MilliCU{12},
              "New deployments receive the completed level once and historical releases retain their original compute.");
        refuse(core::StartResearch{batched,cloud},"limit");
        apply(core::StartResearch{low_precision,cloud});
        apply(core::CancelProject{state().lab->research.back().project_id});
        hq(core::BuyHQEquipment{"evaluation"});
        advance(10);
        refuse(core::StartResearch{specialized, local}, "evaluation station");
        hq(core::PlaceHQEquipment{4, 0, 0, 3, false});
        apply(core::StartResearch{specialized, local});
        {
            const std::string active_project = state().lab->research.back().project_id;
            auto hw = state();
            std::string founder_id = "founder";
            for (const auto &emp : hw.management->people->employees)
                if (emp.terms.profile.role == "Founder / researcher") {founder_id = emp.terms.profile.id; break;}
            auto alloc = local;
            alloc.staff = 1;
            alloc.researchers = {founder_id};
            hw.management->people->talent->founder_genius = true;
            hw.management->people->talent->founder_skills = {"soldering"};
            const auto workers = core::research_workers(hw, active_project);
            const auto founder_w = std::ranges::find(workers, founder_id, &core::ResearchWorker::id);
            if (founder_w == workers.end() || !founder_w->unavailable.empty())
                throw std::runtime_error("Soldering plus Actual Genius must qualify for hardware lab work: " +
                                         (founder_w == workers.end() ? std::string{"missing founder"} : founder_w->unavailable));
            if (!core::research_workplace_blocker(hw, specialized, alloc, active_project).empty())
                throw std::runtime_error("Soldering plus Actual Genius must be unblocked when required lab equipment is placed.");
            if (core::research_equipment(hw, batched).empty() &&
                core::research_workplace_blocker(hw, batched, alloc, active_project).empty())
                throw std::runtime_error("Soldering plus Actual Genius must not qualify for non-equipment theory research.");
            auto missing_eq = hw;
            for (auto &item : missing_eq.infrastructure->workplace->headquarters->items)
                if (item.offer == "evaluation") item.premises = 0;
            if (core::research_workplace_blocker(missing_eq, specialized, alloc, active_project).empty())
                throw std::runtime_error("Hardware specialist qualification must still enforce placed HQ equipment prerequisites.");
            hw.management->people->talent->founder_genius = false;
            hw.management->people->talent->founder_skills = {"cpu_design"};
            if (!core::research_workplace_blocker(hw, specialized, alloc, active_project).empty())
                throw std::runtime_error("Hardware engineering skill (cpu_design) must qualify when equipment is placed.");
        }
        // Settle the active specialized research project before F-08D/E checks.
        for (int n = 0; n < 30 && project(state().lab->research.back().project_id).status == core::ProjectStatus::active; ++n)
            advance(1);

        // F-08D & F-08E: Broad 120+ catalog, data-free vs data-dependent research, 6 data paths, processing non-laundering,
        // creative workforce, PoC production/refusal/continuation, compound AND-of-ORs, and commercial tech licensing.
        {
            check(state().lab->catalog.research.size() >= 120, "Catalog must provide 120+ validated research topics across domains.");

            // 1. Commercial Technology Licensing vs Internal R&D:
            // License data_deduplication (instead of researching it internally) to unlock downstream empirical research.
            check(!core::has_research_access(*state().lab, *state().capacity, "research.data_deduplication", state().date),
                  "Deduplication is not yet known or licensed.");
            apply(core::LicenseTechnology{"license.data_deduplication"});
            // Before integration_days (2 days), license is not yet active.
            check(!core::has_research_access(*state().lab, *state().capacity, "research.data_deduplication", state().date),
                  "Technology license requires its integration window before becoming active.");
            advance(2);
            check(core::has_research_access(*state().lab, *state().capacity, "research.data_deduplication", state().date),
                  "Active technology license satisfies prerequisite access within its term.");
            // Cannot publish a licensed technology as an original discovery!
            refuse(core::PublishResearch{"research.data_deduplication"}, "completed, previously private research result");

            // 2. Data-free theoretical research vs Data-dependent empirical research:
            // research.data.information_theoretic_pruning is data-free and runs with zero owned datasets.
            check(state().lab->licenses.empty(), "No datasets owned yet.");
            apply(core::StartResearch{"research.data.information_theoretic_pruning", cloud});
            for (int n = 0; n < 25 && project(state().lab->research.back().project_id).status == core::ProjectStatus::active; ++n)
                advance(1);
            check(state().lab->research.back().completed, "Data-free theoretical research completes with zero datasets owned.");

            // research.data.empirical_scaling_laws requires an owned dataset (>= 50,000 units, >= 5,000 bp quality).
            refuse(core::StartResearch{"research.data.empirical_scaling_laws", cloud}, "dataset");
            apply(core::BuyData{"data.public_civic_registry_clean"});
            auto empirical_alloc = cloud;
            empirical_alloc.data_ids = {"data.public_civic_registry_clean"};
            apply(core::StartResearch{"research.data.empirical_scaling_laws", empirical_alloc});
            for (int n = 0; n < 25 && project(state().lab->research.back().project_id).status == core::ProjectStatus::active; ++n)
                advance(1);
            check(state().lab->research.back().completed, "Data-dependent research completes once qualifying dataset is supplied.");

            {
                runtime::Runtime panel{program, game.current_state()};
                const auto snapshot = [&]() -> const auto& { return panel.current_state().world.simulation; };
                const auto act = [&](core::CommandAction action) {
                    return panel.submit({snapshot().revision, std::move(action)});
                };
                const std::string id = "research.data.longitudinal_sampling";
                check(act(core::StartResearch{id, empirical_alloc}).accepted(), "A qualified panel can collect its first wave.");
                for (int n = 0; n < 30 && !snapshot().lab->research.back().completed; ++n)
                    check(act(core::AdvanceDay{}).accepted(), "Observation research progresses with real staff and compute.");
                const auto& world = snapshot();
                const auto& lab = *world.lab;
                const auto& cap = *world.capacity;
                const auto& topic = *std::ranges::find(lab.catalog.research, id, &core::LabResearch::id);
                check(lab.research.back().completed, "The first panel produces a settled result.");
                const auto& work = *std::ranges::find(cap.projects, lab.research.back().project_id, &core::CapacityProject::id);
                const auto interval = topic.technical_effect.observation_interval_days;
                const auto opens = core::advance_days(*work.stopped_on, core::Days{interval});
                check(interval && core::advance_days(scenario.starts_on, core::Days{interval * (topic.repeat_limit - 1)}) > scenario.ends_before,
                      "Even immediate results and unlimited hiring cannot exhaust the study in one founder campaign.");
                check(!core::research_followup_blocker(lab, cap, topic, core::advance_days(*work.stopped_on, core::Days{interval-1})).empty() &&
                      core::research_followup_blocker(lab, cap, topic, opens).empty(), "Follow-up window opens on its exact date.");
                check(!core::research_data_blocker(lab, cap, opens, topic, empirical_alloc.data_ids).empty(),
                      "A second wave must bring new source material.");
                check(core::research_effect_waves(lab, cap, topic, work.starts_on) == 0 &&
                      core::research_effect_waves(lab, cap, topic, world.date) == 1 &&
                      core::cumulative_research_effect(360, 3) == 490, "Historical research gains respect dates and diminish per wave.");
                const auto saved = panel.current_state();
                check(!act(core::StartResearch{id, empirical_alloc}).accepted() && panel.current_state() == saved,
                      "An early follow-up cannot reserve staff, compute or cash.");
                check(persistence::decode_save(program, persistence::encode_save(program, saved)).checkpoint == saved,
                      "Observation windows and results survive a fresh save.");
                check(act(core::BuyData{"data.company_internal_logs"}).accepted(), "Acquire a later field sample.");
                const auto& later = snapshot();
                const auto& followup = *std::ranges::find(later.lab->catalog.research, id, &core::LabResearch::id);
                check(core::research_data_blocker(*later.lab, *later.capacity, opens, followup, {"data.company_internal_logs"}).empty(),
                      "A sample delivered after the first result qualifies for the next wave.");
            }

            // 3. Six data acquisition paths + Dataset Processing (volume vs quality, non-laundering of rights/provenance/controversy):
            apply(core::BuyData{"data.torrent_shadow_archive"});
            apply(core::BuyData{"data.company_internal_logs"});
            apply(core::BuyData{"data.licensed_editorial_gold"});
            apply(core::BuyData{"data.licensed_bulk_syndicate_noisy"});
            apply(core::BuyData{"data.public_common_dump_noisy"});
            const auto &torrent_raw = core::lab_data(*state().lab, "data.torrent_shadow_archive");
            check(torrent_raw.third_party_controversy_risk && !torrent_raw.commercial_use &&
                      torrent_raw.provenance_confidence_bp.value() < 5000,
                  "Torrent archive carries controversy risk, low provenance confidence, and no commercial clearance.");
            const core::ProcessDataset filter_req{"data.torrent_shadow_archive", "filter", "data.torrent_filtered", cloud};
            const auto proc_preview = core::preview_data_processing(*state().lab, *state().capacity, filter_req);
            check(proc_preview.estimated_unique_units < torrent_raw.unique_units &&
                      proc_preview.estimated_quality > torrent_raw.quality &&
                      !proc_preview.commercial_use && proc_preview.third_party_controversy_risk &&
                      proc_preview.provenance_confidence == torrent_raw.provenance_confidence_bp,
                  "Processing preview must show volume-for-quality tradeoff and unchanged rights/provenance/controversy.");
            apply(filter_req);
            for (int n = 0; n < 20 && project(state().lab->processed.back().project_id).status == core::ProjectStatus::active; ++n)
                advance(1);
            const auto &torrent_filtered = core::lab_data(*state().lab, "data.torrent_filtered");
            check(torrent_filtered.unique_units < torrent_raw.unique_units &&
                      torrent_filtered.quality > torrent_raw.quality &&
                      !torrent_filtered.commercial_use && torrent_filtered.third_party_controversy_risk &&
                      torrent_filtered.provenance_confidence_bp == torrent_raw.provenance_confidence_bp &&
                      torrent_filtered.parent_dataset_id == "data.torrent_shadow_archive",
                  "Completed dataset processing must never launder rights, provenance confidence, or controversy risk.");
            check(core::dataset_lineage_has_controversy(*state().lab, "data.torrent_filtered"),
                  "Lineage controversy check must trace back through processed datasets.");

            // 4. Dedicated Creative Workforce & In-House Creative Data Production:
            std::string creative_worker;
            for (const auto &emp : state().management->people->employees) {
                if (emp.terms.profile.creative >= 40) {
                    creative_worker = emp.terms.profile.id;
                    break;
                }
            }
            if (creative_worker.empty()) {
                for (const auto &app : state().management->people->applicants) {
                    if (app.profile.creative >= 40) {
                        apply(core::HireEmployee{app.profile.id});
                        creative_worker = app.profile.id;
                        advance(8);
                        break;
                    }
                }
            }
            check(!creative_worker.empty(), "At least one employee or hireable applicant must qualify for creative production.");
            core::LabAllocation creative_alloc = cloud;
            creative_alloc.staff = 1;
            creative_alloc.researchers = {creative_worker};
            apply(core::StartCreativeProduction{"data.creative_docs_v1", "documents", 25000, creative_alloc});
            for (int n = 0; n < 15 && project(state().lab->creative_jobs.back().project_id).status == core::ProjectStatus::active; ++n)
                advance(1);
            const auto &creative_corpus = core::lab_data(*state().lab, "data.creative_docs_v1");
            check(creative_corpus.source_path == "in_house_creative" && creative_corpus.commercial_use &&
                      creative_corpus.open_weight_release && !creative_corpus.third_party_controversy_risk &&
                      creative_corpus.provenance_confidence_bp == core::BasisPoints{10000} &&
                      creative_corpus.quality.value() >= 8000 && creative_corpus.quality.value() <= 9700,
                  "In-house creative data must be clean, commercially cleared, controversy-free, and skill-scaled below 100%.");

            // 5. Proof-of-Concept (PoC) Research Outcome -> Deploy Refusal -> Production Continuation:
            apply(core::StartResearch{"research.ml.rapid_prototyping_poc", cloud});
            for (int n = 0; n < 20 && project(state().lab->research.back().project_id).status == core::ProjectStatus::active; ++n)
                advance(1);
            const auto poc_it = std::ranges::find(state().lab->models, "poc.research.ml.rapid_prototyping_poc", &core::LabModel::id);
            check(poc_it != state().lab->models.end() && poc_it->is_proof_of_concept &&
                      poc_it->origin_research_id == "research.ml.rapid_prototyping_poc",
                  "Completing PoC research must materialize a real PoC model artifact.");
            const std::string poc_id = poc_it->id;
            const auto &first_service_id = state().deployments->catalog.front().product.id;
            refuse(core::DeployModel{first_service_id, poc_id, "eval.poc.research.ml.rapid_prototyping_poc",
                                     "lease/offer.supplier_small", false},
                   "Proof-of-concept");
            // Continue PoC into a full production training run with clean creative data!
            apply(core::StartTraining{{"model.from_poc_prod", "recipe.sequence", poc_id, {"data.creative_docs_v1"},
                                       "lease/offer.supplier_small", core::MilliCUPerDay{30000}, 1, core::MilliCU{250000}}});
            const auto &prod_model = state().lab->models.back();
            check(!prod_model.is_proof_of_concept && prod_model.parent_id == poc_id &&
                      prod_model.inherited_work.value() > 0,
                  "Production continuation from PoC must preserve lineage and grant bounded head-start work.");

            // 6. Compound AND-of-OR prerequisites (A AND (B OR C) AND (D OR E)):
            // License state_space_recurrence (Branch B) while completely skipping attention_systems (Branch C)!
            check(!core::has_research_access(*state().lab, *state().capacity, "research.attention_systems", state().date),
                  "Branch C (attention_systems) is completely skipped.");
            apply(core::LicenseTechnology{"license.state_space_recurrence"});
            advance(5);
            check(core::has_research_access(*state().lab, *state().capacity, "research.ml.state_space_recurrence", state().date),
                  "Branch B (state_space_recurrence) is active via license.");
            // research.ml.unified_reasoning requires reproducible_tooling AND (attention_systems OR state_space_recurrence) AND (held_out_evaluation OR workflow_verification).
            check(core::research_creativity_threshold(*state().lab, *state().capacity, "research.ml.unified_reasoning", state().date) == 0,
                  "Compound AND-of-OR prerequisites must unlock unified_reasoning via Branch B while skipping Branch C!");

            // 7. Product technical profile & provenance scrutiny event:
            const auto clean_prof = core::product_technical_profile(state(), "model.from_poc_prod");
            check(clean_prof.clean_provenance, "Model trained on in-house creative data must report clean provenance.");
            apply(core::CancelProject{"lab/train/model.from_poc_prod"});
            apply(core::StartTraining{{"model.from_processed_noisy", "recipe.sequence", "", {"data.torrent_filtered"},
                                       "lease/offer.supplier_small", core::MilliCUPerDay{10000}, 1, core::MilliCU{250000}}});
            const auto noisy_prof = core::product_technical_profile(state(), "model.from_processed_noisy");
            check(!noisy_prof.clean_provenance,
                  "Model trained on filtered noisy web dump must still carry unconsented provenance risk.");
            auto story_checkpoint = game.current_state();
            story_checkpoint.world.enabled_flows = std::vector<std::string>{"story.provenance_scrutiny_audit"};
            runtime::Runtime story_game{program, story_checkpoint};
            check(story_game.submit({story_game.current_state().world.simulation.revision, core::AdvanceDay{}}).accepted(),
                  "Advancing day with provenance scrutiny flow enabled must succeed.");
            const bool scrutiny_fired = std::ranges::any_of(story_game.current_state().runtime.presentations, [](const auto &msg) {
                return msg.flow_id == "story.provenance_scrutiny_audit";
            });
            check(scrutiny_fired, "Training on controversial data must trigger story.provenance_scrutiny_audit.");
        }
        exact_save();
        std::cout << "Named labor, prerequisites, equipment, paid compute, independent work budgets, power loss and "
                     "innovation fees, equipment, strict level prerequisites, dated serving savings and fresh saves passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
