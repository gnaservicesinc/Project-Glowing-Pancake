#include "integration_fixture.hpp"

#include <pgp/persistence/save.hpp>
#include <pgp/persistence/settings.hpp>
#include <pgp/persistence/sha256.hpp>

#include <chrono>
#include <filesystem>
#include <iostream>

using namespace pgp;
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
class ScratchDirectory {
public:
    ScratchDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::path{PGP_INTEGRATION_SCRATCH_PATH} /
               ("save-resume-" + std::to_string(stamp));
        if (!std::filesystem::create_directory(path)) throw std::runtime_error("Scratch path already exists");
    }
    ~ScratchDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
    std::filesystem::path path;
};
std::size_t pending_choices(const runtime::Checkpoint& checkpoint) {
    return static_cast<std::size_t>(std::count_if(checkpoint.runtime.presentations.begin(),
        checkpoint.runtime.presentations.end(), [](const auto& row) {
            return row.kind == runtime::PresentationKind::dialogue && !row.resolved;
        }));
}
std::size_t delayed_nested(const runtime::Checkpoint& checkpoint) {
    return static_cast<std::size_t>(std::count_if(checkpoint.runtime.activations.begin(),
        checkpoint.runtime.activations.end(), [](const auto& row) {
            return row.status == runtime::ActivationStatus::delayed && row.frames.size() == 2;
        }));
}

void interrupted_sequence_and_choice() {
    const auto program = integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH, true);
    runtime::Runtime uninterrupted{program, integration_fixture::checkpoint(program)};
    integration_fixture::prepare(uninterrupted);
    const auto first_morning = uninterrupted.checkpoint();
    require(pending_choices(first_morning) == 1, "Supplier decision must block first morning");
    require(delayed_nested(first_morning) == 1, "Nested sequence must save both waiting siblings");
    require(runtime::modifier_total(first_morning.runtime,
                {content::Scope::company, first_morning.world.company_id},
                "research_coordination_basis_points") == -500, "Continuous entry installed once");

    ScratchDirectory scratch;
    const auto save = scratch.path / "interrupted.pgpsave";
    persistence::save_file(program, first_morning, save);
    const auto loaded = persistence::load_file(program, save);
    require(loaded.checkpoint == first_morning, "File reload must preserve every checkpoint field");
    runtime::Runtime resumed{program, loaded.checkpoint};
    const auto blocked = resumed.submit({resumed.checkpoint().world.simulation.revision,
                                        core::ReserveFunds{"Blocked planning", core::Cents{1}}});
    require(blocked.reason == runtime::RuntimeReason::morning_not_ready &&
                resumed.checkpoint() == first_morning, "Required flow choice gates core commands atomically");

    integration_fixture::choose_all(uninterrupted);
    integration_fixture::choose_all(resumed);
    require(uninterrupted.checkpoint() == resumed.checkpoint(), "Opening choice cannot duplicate effects on reload");
    for (int day = 0; day != 14; ++day) {
        integration_fixture::step(uninterrupted);
        integration_fixture::step(resumed);
        require(uninterrupted.checkpoint() == resumed.checkpoint(), "Every next-day checkpoint must match");
    }
    const auto nested_choice = resumed.checkpoint();
    require(nested_choice.world.simulation.date == core::OrdinalDay{14} && pending_choices(nested_choice) == 1,
            "Nested delay resumes to its exact dialogue on Jan15");
    const auto nested = std::find_if(nested_choice.runtime.activations.begin(), nested_choice.runtime.activations.end(),
        [](const auto& row) { return row.definition_id == "flow.hidden.sequence_resume"; });
    require(nested != nested_choice.runtime.activations.end() && nested->frames.size() == 2 &&
                nested->status == runtime::ActivationStatus::awaiting_choice,
            "Dialogue keeps both nested continuations");

    persistence::save_file(program, nested_choice, save);
    const auto previous = persistence::load_file(program, std::filesystem::path{save.string() + ".bak"});
    require(previous.checkpoint == first_morning, "Atomic replacement preserves previous valid save");
    resumed = runtime::Runtime{program, persistence::load_file(program, save).checkpoint};
    integration_fixture::choose_all(uninterrupted);
    integration_fixture::choose_all(resumed);
    require(resumed.checkpoint() == uninterrupted.checkpoint(), "Nested dialogue returns to identical siblings");
    const auto after_choice = resumed.checkpoint();
    const auto& decisions = after_choice.world.decisions;
    require(std::count(decisions.begin(), decisions.end(), "inner_done") == 1 &&
                std::count(decisions.begin(), decisions.end(), "outer_done") == 1 &&
                std::find(decisions.begin(), decisions.end(), "inner_done") <
                std::find(decisions.begin(), decisions.end(), "outer_done"),
            "Nested sibling effects execute exactly once in authored order");
    require(after_choice.world.simulation.random == first_morning.world.simulation.random,
            "Queries and deterministic flows must not consume RNG");

    const auto bytes = persistence::encode_save(program, after_choice);
    auto corrupt = bytes;
    corrupt[corrupt.size() / 2] = static_cast<char>(corrupt[corrupt.size() / 2] ^ 1);
    bool rejected = false;
    try { (void)persistence::decode_save(program, corrupt); }
    catch (const persistence::PersistenceError&) { rejected = true; }
    require(rejected && resumed.checkpoint() == after_choice, "Corrupt load preserves current campaign");
    integration_fixture::step(resumed);
    integration_fixture::step(uninterrupted);
    require(persistence::encode_save(program, resumed.checkpoint()) ==
                persistence::encode_save(program, uninterrupted.checkpoint()),
            "Whole next-day serialized state matches uninterrupted play");

    persistence::AutosaveRing autosaves{scratch.path / "autosaves"};
    for (unsigned i = 0; i != 4; ++i)
        require(autosaves.write(program, resumed.checkpoint(),
                    persistence::AutosaveReason::before_irreversible_commitment).has_value(), "Autosave committed");
    require(autosaves.latest(program).checkpoint == resumed.checkpoint(), "Ring restores latest real runtime state");
    const auto paths = persistence::local_paths(scratch.path / "portable");
    persistence::Settings settings;
    settings.text_scale_percent = 200;
    settings.reduced_motion = true;
    settings.autosave_enabled = false;
    persistence::save_settings(paths.settings_file, settings);
    require(persistence::load_settings(paths.settings_file) == settings, "Local settings save/reopen");
    require(!autosaves.write(program, resumed.checkpoint(),
                persistence::AutosaveReason::before_irreversible_commitment, settings.autosave_enabled),
            "Disabled autosave does not write");
    std::cout << "Nested wait/choice file reload, recovery, autosaves and next-day parity pass; SHA256 "
              << persistence::sha256(persistence::encode_save(program, resumed.checkpoint())) << '\n';
}
void source_selection_and_scenario_data() {
    auto program = integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH, false);
    auto initial = runtime::begin_scenario(program, "scenario.foundation", "US-TX");
    require(initial.world.state_id == "state.us_tx" && initial.world.simulation.cash == core::Cents{120'000'000} &&
                initial.world.root_seed == 20140101 && initial.world.enabled_flows.has_value(),
            "Scenario loads authored cash, seed, headquarters and root selection");
    initial.world.enabled_flows = std::vector<std::string>{"flow.hidden.sequence_resume"};
    runtime::Runtime isolated{program, initial};
    integration_fixture::prepare(isolated);
    require(isolated.checkpoint().runtime.activations.size() == 1 &&
                isolated.checkpoint().runtime.activations[0].definition_id == "flow.hidden.sequence_resume",
            "Disabled flow sources do not activate independently");
    bool refused = false;
    try { (void)runtime::begin_scenario(program, "scenario.foundation", "US-NY"); }
    catch (const std::invalid_argument&) { refused = true; }
    require(refused, "Unavailable headquarters refused");
}
} // namespace
int main() {
    try { source_selection_and_scenario_data(); interrupted_sequence_and_choice(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
