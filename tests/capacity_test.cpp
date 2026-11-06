#include <pgp/core/capacity.hpp>

#include <algorithm>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

using namespace pgp::core;
namespace {
std::size_t checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class Operation> void refuses_unchanged(CapacityState& state, Operation operation) {
    const auto before = state;
    bool rejected = false;
    try { operation(); } catch (const std::exception&) { rejected = true; }
    check(rejected, "Invalid capacity operation was accepted");
    check(state == before, "Rejected capacity operation changed state");
}
CapacitySource source() {
    CapacitySource value;
    value.id = "lease";
    value.physical_id = "provider.slice";
    value.owner = "company.player";
    value.provider = "supplier.cloud";
    value.nominal = MilliCUPerDay{100000};
    value.effective = value.nominal;
    value.memory_mib = 16384;
    return value;
}
CapacityProject training(std::string id = "train") {
    CapacityProject value;
    value.id = std::move(id);
    value.source_id = "lease";
    value.requested = MilliCUPerDay{60000};
    value.staff = 4;
    value.minimum_memory_mib = 8192;
    value.total_work = MilliCU{3600000};
    value.cause = "Train finite candidate";
    return value;
}
CapacityProject serving() {
    auto value = training("serve");
    value.kind = ProjectKind::serving;
    value.requested = MilliCUPerDay{20000};
    value.staff = 1;
    value.total_work = MilliCU{0};
    value.tasks_per_day = 1000;
    value.inference_milli_CU_per_task = MilliCU{20};
    value.monthly_support_cost = Cents{500000};
    value.cause = "Deliver useful customer tasks";
    return value;
}
const CapacityProject& project(const CapacityState& state, std::string_view id) {
    const auto found = std::find_if(state.projects.begin(), state.projects.end(),
        [&](const auto& row) { return row.id == id; });
    if (found == state.projects.end()) throw std::runtime_error("Missing capacity project");
    return *found;
}
void inference_and_finite_work() {
    check(required_inference(1000, MilliCU{20}) == MilliCU{20000},
          "1000 tasks at 20 milliCU must reserve 20 CU");
    check(maximum_tasks(MilliCU{19999}, MilliCU{20}) == 999,
          "Inference capacity must floor whole useful tasks");
    bool rejected = false;
    try { (void)required_inference(std::numeric_limits<std::uint64_t>::max(), MilliCU{2}); }
    catch (const std::exception&) { rejected = true; }
    check(rejected, "Inference multiplication must reject overflow");
    rejected = false;
    try { (void)maximum_tasks(MilliCU{100}, MilliCU{0}); }
    catch (const std::exception&) { rejected = true; }
    check(rejected, "Zero-cost inference must not create unlimited useful work");

    auto state = opening_capacity({source()}, 6);
    reserve_project(state, OrdinalDay{0}, serving());
    reserve_project(state, OrdinalDay{0}, training());
    const auto report = capacity_report(state);
    check(report.reserved == MilliCUPerDay{80000} && report.protected_serving == MilliCUPerDay{20000} &&
          report.assigned_staff == 5 && report.available_staff == 6,
          "Serving and training must share the same staff and capacity pool");
    auto too_large = training("oversubscribe");
    too_large.requested = MilliCUPerDay{20001};
    too_large.staff = 1;
    refuses_unchanged(state, [&] { reserve_project(state, OrdinalDay{0}, too_large); });
    auto too_many_staff = training("staff");
    too_many_staff.requested = MilliCUPerDay{1};
    too_many_staff.staff = 2;
    refuses_unchanged(state, [&] { reserve_project(state, OrdinalDay{0}, too_many_staff); });
    auto incompatible = training("memory");
    incompatible.requested = MilliCUPerDay{1};
    incompatible.staff = 1;
    incompatible.minimum_memory_mib = 16385;
    refuses_unchanged(state, [&] { reserve_project(state, OrdinalDay{0}, incompatible); });
    incompatible.minimum_memory_mib = 1;
    incompatible.interconnect = "special";
    refuses_unchanged(state, [&] { reserve_project(state, OrdinalDay{0}, incompatible); });
    for (std::int64_t day = 0; day < 60; ++day) {
        close_capacity_day(state, OrdinalDay{day});
        const auto& usage = state.days.back().sources.front();
        check(usage.used <= MilliCU{usage.effective.value()} && usage.used == MilliCU{80000},
              "Daily work exceeded capacity or lost the 20 CU serving / 60 CU training split");
    }
    check(project(state, "train").completed_work == MilliCU{3600000} &&
          project(state, "train").status == ProjectStatus::completed &&
          project(state, "serve").useful_tasks == 60000,
          "60 days must complete 3600 CU training and deliver 60000 useful tasks");
    check(capacity_report(state).reserved == MilliCUPerDay{20000},
          "Completed training must release its future compute reservation");
    refuses_unchanged(state, [&] { close_capacity_day(state, OrdinalDay{59}); });
    validate_capacity(state);
}
void outages_and_cancellation() {
    auto state = opening_capacity({source()}, 6);
    reserve_project(state, OrdinalDay{0}, training());
    reserve_project(state, OrdinalDay{0}, serving());
    update_effective_capacity(state, OrdinalDay{0}, "lease", MilliCUPerDay{40000});
    close_capacity_day(state, OrdinalDay{0});
    check(project(state, "serve").useful_tasks == 1000 && project(state, "train").completed_work == MilliCU{20000},
          "Outage must protect serving before assigning spare capacity to training");
    update_effective_capacity(state, OrdinalDay{1}, "lease", MilliCUPerDay{10000});
    close_capacity_day(state, OrdinalDay{1});
    check(project(state, "serve").useful_tasks == 1500 && project(state, "train").completed_work == MilliCU{20000},
          "Serving shortfall must floor whole tasks and pause training without invented work");
    update_effective_capacity(state, OrdinalDay{2}, "lease", MilliCUPerDay{100000});
    cancel_project(state, OrdinalDay{2}, "train");
    check(project(state, "train").status == ProjectStatus::cancelled &&
          project(state, "train").completed_work == MilliCU{20000} &&
          capacity_report(state).reserved == MilliCUPerDay{20000},
          "Cancellation must retain consumed work while releasing future allocations");
    close_capacity_day(state, OrdinalDay{2});
    check(project(state, "train").completed_work == MilliCU{20000}, "Cancelled work must never progress again");
    refuses_unchanged(state, [&] { cancel_project(state, OrdinalDay{3}, "train"); });
    refuses_unchanged(state, [&] { update_effective_capacity(state, OrdinalDay{3}, "lease", MilliCUPerDay{100001}); });
    auto corrupt = state;
    auto wrong_work = corrupt.days.front();
    auto edited_project = wrong_work.projects.front();
    edited_project.work = MilliCU{99999};
    wrong_work.projects.replace(0, std::move(edited_project));
    corrupt.days.replace(0, std::move(wrong_work));
    check(state.days.front().projects.front().work != MilliCU{99999},
          "Editing a candidate must preserve the shared settled day");
    bool rejected = false;
    try { validate_capacity(corrupt); } catch (const std::exception&) { rejected = true; }
    check(rejected, "Corrupt daily work must refuse restore");
}
void proportional_work_and_source_ownership() {
    auto state = opening_capacity({source()}, 6);
    reserve_project(state, OrdinalDay{0}, serving());
    auto large = training("large");
    large.staff = 2;
    auto small = training("small");
    small.requested = MilliCUPerDay{20000};
    small.staff = 2;
    reserve_project(state, OrdinalDay{0}, large);
    reserve_project(state, OrdinalDay{0}, small);
    update_effective_capacity(state, OrdinalDay{0}, "lease", MilliCUPerDay{40000});
    close_capacity_day(state, OrdinalDay{0});
    check(project(state, "large").completed_work == MilliCU{15000} &&
          project(state, "small").completed_work == MilliCU{5000},
          "Unprotected training must share curtailed capacity in proportion to reservations");
    auto duplicate = source();
    duplicate.id = "owned_alias";
    duplicate.kind = CapacitySourceKind::owned;
    bool rejected = false;
    try { (void)opening_capacity({source(), duplicate}, 6); } catch (const std::exception&) { rejected = true; }
    check(rejected, "The same physical capacity cannot be counted as leased and owned");
}
}
int main() {
    try {
        inference_and_finite_work();
        outages_and_cancellation();
        proportional_work_and_source_ownership();
        std::cout << checks << " capacity checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
