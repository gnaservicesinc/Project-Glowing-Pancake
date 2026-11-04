#include "integration_fixture.hpp"

#include <pgp/persistence/save.hpp>
#include <pgp/persistence/sha256.hpp>
#include <nlohmann/json.hpp>

#include <iostream>

using namespace pgp;
namespace {
using Json = nlohmann::json;
std::size_t checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void apply(runtime::Runtime& engine, core::CommandAction action) {
    integration_fixture::require_accepted(engine.submit({engine.checkpoint().world.simulation.revision, std::move(action)}));
}
void refused_unchanged(runtime::Runtime& engine, core::CommandAction action) {
    const auto before = engine.checkpoint();
    check(!engine.submit({before.world.simulation.revision, std::move(action)}).accepted(),
          "Invalid capacity command was accepted");
    check(engine.checkpoint() == before, "Rejected capacity command changed authoritative state");
}
std::string rehash(Json body) {
    body.erase("sha256");
    body["sha256"] = persistence::sha256(body.dump());
    return body.dump();
}
void invalid_save(const runtime::CompiledProgram& program, const Json& body) {
    bool rejected = false;
    try { (void)persistence::decode_save(program, rehash(body)); }
    catch (const persistence::PersistenceError&) { rejected = true; }
    check(rejected, "Malformed capacity save was accepted");
}
runtime::Checkpoint scenario(const runtime::CompiledProgram& program) {
    auto checkpoint = runtime::begin_scenario(program, "scenario.foundation", "US-CA");
    checkpoint.world.enabled_flows = std::vector<std::string>{};
    return checkpoint;
}
core::CapacityProject training() {
    core::CapacityProject value;
    value.id = "train";
    value.source_id = "lease.initial";
    value.requested = core::MilliCUPerDay{60000};
    value.staff = 4;
    value.total_work = core::MilliCU{3600000};
    value.cause = "Train finite candidate";
    return value;
}
core::CapacityProject serving() {
    auto value = training();
    value.id = "serve";
    value.kind = core::ProjectKind::serving;
    value.requested = core::MilliCUPerDay{20000};
    value.staff = 1;
    value.total_work = core::MilliCU{0};
    value.tasks_per_day = 1000;
    value.inference_milli_CU_per_task = core::MilliCU{20};
    value.monthly_support_cost = core::Cents{500000};
    value.cause = "Serve useful tasks with separate support";
    return value;
}
const core::CapacityProject& project(const runtime::Checkpoint& state, std::string_view id) {
    const auto& projects = state.world.simulation.capacity->projects;
    const auto found = std::find_if(projects.begin(), projects.end(), [&](const auto& row) { return row.id == id; });
    if (found == projects.end()) throw std::runtime_error("Missing project");
    return *found;
}
void license(runtime::Runtime& engine) {
    apply(engine, core::ReserveFunds{"Delivered data license", core::Cents{1'200'000}, false});
    apply(engine, core::ConsumeReservation{1, core::Cents{1'200'000}});
}
void matched_sixty_day_loop(const runtime::CompiledProgram& program) {
    runtime::Runtime engine{program, scenario(program)};
    integration_fixture::prepare(engine);
    check(engine.checkpoint().world.simulation.capacity.has_value(), "Scenario must initialize its authored capacity");
    const auto opening = core::capacity_report(*engine.checkpoint().world.simulation.capacity);
    check(opening.nominal == core::MilliCUPerDay{100000} && opening.effective == opening.nominal &&
          opening.available_staff == 6, "Scenario capacity must use its 100 CU lease and six staff");
    license(engine);
    apply(engine, core::ReserveProject{serving()});
    apply(engine, core::ReserveProject{training()});
    const auto first = engine.checkpoint();
    check(first.world.simulation.cash == core::Cents{118'800'000},
          "Capacity reservations must not charge leased compute or recognize a second license expense");
    auto excess = training();
    excess.id = "excess";
    excess.requested = core::MilliCUPerDay{20001};
    excess.staff = 1;
    refused_unchanged(engine, core::ReserveProject{excess});
    for (unsigned day = 0; day < 30; ++day) integration_fixture::step(engine);
    const auto saved = engine.checkpoint();
    runtime::Runtime resumed{program, persistence::decode_save(program, persistence::encode_save(program, saved)).checkpoint};
    check(saved == resumed.checkpoint(), "Capacity reservations and daily work must round-trip exactly");
    for (unsigned day = 30; day < 60; ++day) {
        integration_fixture::step(engine);
        integration_fixture::step(resumed);
        check(engine.checkpoint() == resumed.checkpoint(), "Reload must not duplicate work, support costs or capacity");
        if (day == 58) {
            const auto report = core::accounts_report(*engine.checkpoint().world.simulation.accounts);
            check(report.cash == core::Cents{96'200'000} && report.expenses == core::Cents{23'800'000},
                  "Two calendar months must charge compute lease once and separate $5000/month serving support");
        }
    }
    const auto final = resumed.checkpoint();
    check(final.world.simulation.date == core::OrdinalDay{60} && project(final, "train").completed_work == core::MilliCU{3600000} &&
          project(final, "train").status == core::ProjectStatus::completed && project(final, "serve").useful_tasks == 60000,
          "Shared 20 CU serving and 60 CU training must complete exactly after 60 days");
    check(final.world.simulation.cash == core::Cents{96'200'000} &&
          final.world.simulation.random == first.world.simulation.random,
          "Capacity work created a compute bill or consumed RNG");
    const auto snapshot = core::Simulation{final.world.simulation}.snapshot();
    check(snapshot->capacity.has_value() && snapshot->capacity->reserved == core::MilliCUPerDay{20000},
          "Immutable capacity report must release completed training reservation");
    auto corrupt = Json::parse(persistence::encode_save(program, final));
    corrupt["checkpoint"]["world"]["simulation"]["capacity"]["unsupported"] = 1;
    invalid_save(program, corrupt);
    corrupt = Json::parse(persistence::encode_save(program, final));
    corrupt["checkpoint"]["world"]["simulation"].erase("capacity");
    invalid_save(program, corrupt);
    corrupt = Json::parse(persistence::encode_save(program, final));
    corrupt["checkpoint"]["world"]["simulation"]["capacity"] = nullptr;
    invalid_save(program, corrupt);
    check(resumed.checkpoint() == final, "Corrupt save refusal changed live capacity");
}
void outage_and_irreversible_cancellation(const runtime::CompiledProgram& program) {
    runtime::Runtime engine{program, scenario(program)};
    integration_fixture::prepare(engine);
    license(engine);
    apply(engine, core::ReserveProject{training()});
    apply(engine, core::ReserveProject{serving()});
    apply(engine, core::SetEffectiveCapacity{"lease.initial", core::MilliCUPerDay{40000}});
    integration_fixture::step(engine);
    const auto interrupted = engine.checkpoint();
    check(project(interrupted, "serve").useful_tasks == 1000 && project(interrupted, "train").completed_work == core::MilliCU{20000},
          "Outage must preserve serving priority in shared daily work");
    const auto loaded = persistence::decode_save(program, persistence::encode_save(program, interrupted)).checkpoint;
    engine = runtime::Runtime{program, loaded};
    apply(engine, core::SetEffectiveCapacity{"lease.initial", core::MilliCUPerDay{100000}});
    apply(engine, core::CancelProject{"train"});
    apply(engine, core::CancelProject{"serve"});
    const auto cancelled = engine.checkpoint();
    check(cancelled.world.simulation.cash == core::Cents{118'800'000} &&
          project(cancelled, "train").completed_work == core::MilliCU{20000} &&
          core::capacity_report(*cancelled.world.simulation.capacity).reserved == core::MilliCUPerDay{0},
          "Cancellation must release allocations while retaining license cost and completed work");
    refused_unchanged(engine, core::CancelProject{"train"});
    for (unsigned day = 1; day < 31; ++day) integration_fixture::step(engine);
    const auto report = core::accounts_report(*engine.checkpoint().world.simulation.accounts);
    check(report.cash == core::Cents{107'983'871},
          "Cancelled support must charge only its one active day, alongside one monthly baseline");
    check(project(engine.checkpoint(), "train").completed_work == core::MilliCU{20000} &&
          project(engine.checkpoint(), "serve").useful_tasks == 1000,
          "Cancelled projects must preserve results without progressing again");
}
void same_day_cancellation_and_legacy(const runtime::CompiledProgram& program) {
    runtime::Runtime engine{program, scenario(program)};
    integration_fixture::prepare(engine);
    apply(engine, core::ReserveProject{serving()});
    apply(engine, core::CancelProject{"serve"});
    for (unsigned day = 0; day < 31; ++day) integration_fixture::step(engine);
    check(engine.checkpoint().world.simulation.cash == core::Cents{109'200'000} &&
          project(engine.checkpoint(), "serve").completed_work == core::MilliCU{0},
          "Unused same-day cancellation must not charge support or invent useful work");
    auto legacy = Json::parse(persistence::encode_save(program, scenario(program)));
    legacy["checkpoint"]["world"]["simulation"].erase("capacity");
    legacy["checkpoint"]["world"]["simulation"].erase("capacity_mode");
    const auto restored = persistence::decode_save(program, rehash(legacy)).checkpoint;
    check(restored.world.simulation.accounts.has_value() && !restored.world.simulation.capacity,
          "Accounts-only older saves must not fabricate projects, staff or physical capacity");
}
}
int main() {
    try {
        const auto program = integration_fixture::program(PGP_RUNTIME_FIXTURE_PATH, false);
        matched_sixty_day_loop(program);
        outage_and_irreversible_cancellation(program);
        same_day_cancellation_and_legacy(program);
        std::cout << checks << " capacity command/save integration checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
