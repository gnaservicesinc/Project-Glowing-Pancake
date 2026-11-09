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
        hq(core::BuyHQEquipment{"evaluation"});
        advance(10);
        refuse(core::StartResearch{specialized, local}, "evaluation station");
        hq(core::PlaceHQEquipment{3, 0, 0, 2, false});
        apply(core::StartResearch{specialized, local});
        exact_save();
        std::cout << "Named labor, prerequisites, equipment, paid compute, independent work budgets, power loss and "
                     "fresh saves passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
