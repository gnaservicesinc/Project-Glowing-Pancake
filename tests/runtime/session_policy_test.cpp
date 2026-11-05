#include "integration_fixture.hpp"

#include <pgp/session/campaign.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace pgp;
namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Scratch {
    std::filesystem::path path;
    Scratch() {
        path = std::filesystem::path{PGP_INTEGRATION_SCRATCH_PATH} /
            ("session-policy-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(path)) throw std::runtime_error("Owned scratch directory already exists");
    }
    ~Scratch() { std::error_code error; std::filesystem::remove_all(path, error); }
};
void policy_hooks() {
    Scratch scratch;
    auto program = integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH, false);
    auto initial = runtime::begin_scenario(program, "scenario.foundation", "US-CA");
    initial.world.enabled_flows = std::vector<std::string>{};
    const auto autosave_directory = scratch.path / "autosaves";
    session::Campaign campaign{program, initial, autosave_directory};
    require(!std::filesystem::exists(autosave_directory), "Construction must not create local files");
    require(campaign.prepare_opening(initial.world.simulation.revision).command.accepted(), "Prepare opening");
    const auto before_commit = campaign.checkpoint();
    const auto committed = campaign.submit({before_commit.world.simulation.revision,
        core::ReserveFunds{"Signed commitment", core::Cents{10}, false}});
    require(committed.command.accepted() && !committed.storage_notice, "Irreversible command accepted after recovery save");
    persistence::AutosaveRing ring{autosave_directory};
    require(ring.latest(program).checkpoint == before_commit, "Autosave must capture before irreversible terms");
    const auto stale = campaign.submit({before_commit.world.simulation.revision,
        core::ReserveFunds{"Stale commitment", core::Cents{10}, false}});
    require(!stale.command.accepted() && ring.latest(program).autosave_sequence == 1,
            "Rejected stale command creates no autosave");
    for (int day = 0; day != 31; ++day)
        require(campaign.submit({campaign.checkpoint().world.simulation.revision, core::AdvanceDay{}}).command.accepted(),
                "Advance to month boundary");
    require(campaign.checkpoint().world.simulation.date == core::OrdinalDay{31} &&
                ring.latest(program).checkpoint == campaign.checkpoint() && ring.latest(program).autosave_sequence == 2,
            "Completed January automatically saves prepared Feb1 state");

    const auto blocked_path = autosave_directory / "autosave-2.pgpsave.tmp";
    { std::ofstream blocked(blocked_path); blocked << "pending prior write"; }
    const auto before_failure = campaign.checkpoint();
    const auto refused = campaign.submit({before_failure.world.simulation.revision,
        core::ReserveFunds{"Cannot safely commit", core::Cents{10}, false}});
    require(!refused.command.accepted() && refused.storage_notice.has_value() && refused.halt_requested &&
                campaign.checkpoint() == before_failure && std::filesystem::exists(blocked_path),
            "Failed recovery autosave refuses irreversible action and preserves live state/temp");
    auto preferences = campaign.settings();
    preferences.autosave_enabled = false;
    campaign.set_settings(preferences);
    require(campaign.submit({campaign.checkpoint().world.simulation.revision,
                core::ReserveFunds{"Autosave deliberately disabled", core::Cents{10}, false}}).command.accepted() &&
                ring.latest(program).autosave_sequence == 2 && std::filesystem::exists(blocked_path),
            "Explicitly disabled autosaves perform no writes before the action");
    preferences.autosave_enabled = true;
    campaign.set_settings(preferences);
    session::OperationResult month_end;
    for (int day = 0; day != 28; ++day)
        month_end = campaign.submit({campaign.checkpoint().world.simulation.revision, core::AdvanceDay{}});
    require(month_end.command.accepted() && month_end.storage_notice.has_value() && month_end.halt_requested &&
                campaign.checkpoint().world.simulation.date == core::OrdinalDay{59},
            "Month-end write failure preserves completed play and requests driver halt");
    const auto export_path = scratch.path / "manual-export.pgpsave";
    campaign.save(export_path);
    const auto saved = persistence::load_file(program, export_path).checkpoint;
    require(saved == campaign.checkpoint(), "Manual export survives autosave failure");
    { std::ofstream corrupt(export_path, std::ios::trunc); corrupt << "broken"; }
    const auto failed_load = campaign.load(export_path);
    require(!failed_load.loaded && failed_load.notice.has_value() && campaign.checkpoint() == saved &&
                persistence::read_bounded(export_path) == "broken", "Failed load preserves live state and selected file");

    auto final_checkpoint = campaign.checkpoint();
    final_checkpoint.world.simulation.end_date = core::OrdinalDay{90}; // 2014-04-01, exclusive.
    const auto final_directory = scratch.path / "final-autosaves";
    session::Campaign final_campaign{program, final_checkpoint, final_directory};
    for (int day = 0; day != 31; ++day) {
        const auto result = final_campaign.submit({final_campaign.checkpoint().world.simulation.revision, core::AdvanceDay{}});
        require(result.command.accepted() && !result.storage_notice, "Advance through final month boundary");
    }
    require(final_campaign.checkpoint().world.simulation.phase == core::DayPhase::finished &&
                persistence::AutosaveRing{final_directory}.latest(program).checkpoint == final_campaign.checkpoint(),
            "Exclusive campaign ending saves the completed final month");

    auto midmonth = campaign.checkpoint();
    midmonth.world.simulation.end_date = core::OrdinalDay{60}; // March 2, independent of month boundary.
    const auto ending_directory = scratch.path / "midmonth-autosaves";
    session::Campaign ending{program, midmonth, ending_directory};
    require(ending.submit({ending.checkpoint().world.simulation.revision, core::AdvanceDay{}}).command.accepted() &&
                persistence::AutosaveRing{ending_directory}.latest(program).checkpoint == ending.checkpoint(),
            "A scenario ending between month boundaries still saves its final state");
}
void consumption_and_suspended_month() {
    Scratch scratch;
    auto program = integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH, false);
    auto initial = runtime::begin_scenario(program, "scenario.foundation", "US-CA");
    initial.world.enabled_flows = std::vector<std::string>{};
    core::Simulation reserved{initial.world.simulation};
    for (unsigned day = 0; day < 30; ++day)
        require(reserved.submit({reserved.snapshot()->revision, core::AdvanceDay{}}).accepted(),
                "Accrue the opening month before the Jan31 choice fixture");
    const auto reservation = reserved.submit({reserved.snapshot()->revision,
        core::ReserveFunds{"Scheduled delivery", core::Cents{100}, true}});
    require(reservation.accepted(), "Reservation setup");
    initial.world.simulation = reserved.checkpoint();
    initial.world.simulation.opening_actions.push_back({1, core::OrdinalDay{31}, *reservation.reservation_id,
                                                       core::Cents{10}, true, "Required delivery decision"});
    const auto directory = scratch.path / "autosaves";
    session::Campaign campaign{program, initial, directory};
    require(campaign.prepare_opening(initial.world.simulation.revision).command.accepted(), "Jan31 prepared");
    const auto boundary = campaign.submit({campaign.checkpoint().world.simulation.revision, core::AdvanceDay{}});
    const auto suspended = campaign.checkpoint();
    persistence::AutosaveRing ring{directory};
    require(boundary.command.accepted() && !boundary.storage_notice &&
                suspended.world.simulation.phase == core::DayPhase::awaiting_choice &&
                ring.latest(program).checkpoint == suspended,
            "Completed month autosaves even when next morning has a required core choice");
    const auto accepted = campaign.submit({suspended.world.simulation.revision, core::ResolveOpeningChoice{1, true}});
    require(accepted.command.accepted() && ring.latest(program).checkpoint == suspended &&
                ring.latest(program).autosave_sequence == 2, "Core choice is saved before consuming its commitment");
    const auto before_spend = campaign.checkpoint();
    require(campaign.submit({before_spend.world.simulation.revision,
                core::ConsumeReservation{*reservation.reservation_id, core::Cents{1}}}).command.accepted() &&
                ring.latest(program).checkpoint == before_spend && ring.latest(program).autosave_sequence == 3,
            "A reversible reservation's later irreversible consumption has its own recovery save");
}
void rejected_automatic_step_halts() {
    Scratch scratch;
    const auto original = integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH, false);
    auto collection = original.collection;
    auto& flow = std::get<content::FlowDefinition>(collection.definitions.at("flow.hidden.compute_review").data);
    content::Lifecycle scheduled{};
    scheduled.mode = content::LifecycleMode::scheduled;
    scheduled.occurrence_scope = content::Scope::company;
    scheduled.due_date = core::OrdinalDay{1};
    flow.lifecycle = scheduled;
    flow.trigger = {content::ConditionKind::clause,
        content::FactClause{"company.is_operating", content::Comparison::equal, true, content::Unit::boolean}, {}};
    flow.nodes.at("record_review").effects = {{"adjust_cash", "self", content::Scope::company,
                                               {{"amount_cents", std::int64_t{100}}}}};
    const auto program = runtime::compile_collection(collection, original.registry);
    auto initial = runtime::begin_scenario(program, "scenario.foundation", "US-CA");
    initial.world.enabled_flows = std::vector<std::string>{"flow.hidden.compute_review"};
    session::Campaign campaign{program, initial, scratch.path / "autosaves"};
    require(campaign.prepare_opening(initial.world.simulation.revision).command.accepted(), "Prepare before scheduled failure");
    const auto before = campaign.checkpoint();
    const auto refused = campaign.submit({before.world.simulation.revision, core::AdvanceDay{}});
    require(!refused.command.accepted() && refused.halt_requested && campaign.checkpoint() == before,
            "Failed automatic consequence requests halt instead of retrying the same day forever");
}
void liquidity_warning_and_closure() {
    Scratch scratch;
    const auto program = integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH,false);
    runtime::Checkpoint initial{runtime::foundation_world(core::Cents{0}),{}};
    initial.world.enabled_flows = std::vector<std::string>{};
    initial.world.simulation.accounts = core::opening_accounts(core::Cents{0});
    initial.world.simulation.liquidity = core::opening_liquidity();
    session::Campaign campaign{program,initial,scratch.path / "autosaves"};
    require(campaign.prepare_opening(initial.world.simulation.revision).command.accepted(),"Prepare zero-cash fixture");
    require(campaign.submit({campaign.checkpoint().world.simulation.revision,
        core::IssueInvoice{{"unfunded",core::InvoiceKind::payable,core::OrdinalDay{0},core::Cents{100},core::Cents{0},"Delivered service"}}}).command.accepted(),
        "Issue actual service obligation");
    const auto warning = campaign.submit({campaign.checkpoint().world.simulation.revision,core::AdvanceDay{}});
    require(warning.command.accepted() && warning.halt_requested && campaign.checkpoint().world.simulation.paused,
        "A newly warned workout stops automatic time immediately");
    require(campaign.submit({campaign.checkpoint().world.simulation.revision,core::AcknowledgeLiquidityWarning{1}}).command.accepted(),
        "Acknowledge response window explicitly");
    while (campaign.checkpoint().world.simulation.phase != core::DayPhase::closed) {
        const auto step = campaign.submit({campaign.checkpoint().world.simulation.revision,core::AdvanceDay{}});
        require(step.command.accepted() && !step.storage_notice,"Advance within warned response window without storage error");
    }
    const auto final = campaign.checkpoint();
    require(persistence::AutosaveRing{scratch.path / "autosaves"}.latest(program).checkpoint == final,
        "Closure automatically saves exact causal postmortem and unpaid balances");
    require(final.world.simulation.date == core::OrdinalDay{31},"Closure must stop at the exact 30-day response boundary");
}
void weekly_news_preserves_sources() {
    runtime::Checkpoint fixture;
    fixture.world.campaign_start = core::OrdinalDay{30};
    auto add = [&](std::uint64_t id, int date, runtime::PresentationKind kind, bool resolved) {
        runtime::Presentation presentation{};
        presentation.id = id;
        presentation.day = core::OrdinalDay{date};
        presentation.kind = kind;
        presentation.resolved = resolved;
        fixture.runtime.presentations.push_back(std::move(presentation));
    };
    using Kind = runtime::PresentationKind;
    add(1, 30, Kind::news, true);
    add(2, 36, Kind::dialogue, false);
    add(3, 36, Kind::news, true);
    add(4, 37, Kind::news, true);
    add(5, 37, Kind::result, true);
    const auto before = fixture;
    const auto grouped = session::group_weekly_news(fixture);
    require(grouped.size() == 2 && grouped.at(core::OrdinalDay{30}).size() == 2 &&
            grouped.at(core::OrdinalDay{37}).size() == 1, "News groups at exact seven-day campaign boundaries");
    require(grouped.at(core::OrdinalDay{30})[0]->id == 1 && grouped.at(core::OrdinalDay{30})[1]->id == 3 &&
            grouped.at(core::OrdinalDay{37})[0]->id == 4 && fixture == before,
            "Digest retains the exact ordered source records without grouping or mutating required choices and results");
}
} // namespace
int main() {
    try {
        policy_hooks(); consumption_and_suspended_month(); rejected_automatic_step_halts();
        liquidity_warning_and_closure(); weekly_news_preserves_sources();
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Campaign persistence policy and weekly news projection pass\n";
}
