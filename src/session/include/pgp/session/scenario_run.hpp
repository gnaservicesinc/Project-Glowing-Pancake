#pragma once

#include <pgp/persistence/save.hpp>

namespace pgp::session {
enum class BusinessStrategy { baseline, expansion, rescue };
enum class BusinessStop { finished, warning, closed, decision_required, requested_boundary };
struct BusinessRunOptions {
    // Stop on this prepared morning before applying that day's policy/work.
    std::optional<core::OrdinalDay> stop_on;
    // Exercise one exact in-memory save/load before that morning's policy.
    std::optional<core::OrdinalDay> reload_on;
    // Explicit engineering policy: acknowledge the warning, then allow the
    // disclosed workout deadline to run. Default drivers stop for the human.
    bool continue_workouts = false;
};
struct BusinessMonthReport {
    core::OrdinalDay through{0};
    core::Cents cash{0};
    core::Cents revenue{0};
    core::Cents expenses{0};
    core::Cents payables{0};
    core::Cents debt{0};
    std::uint64_t delivered_tasks = 0;
    std::uint64_t outside_tasks = 0;
    auto operator<=>(const BusinessMonthReport&) const = default;
};
struct BusinessRunResult {
    runtime::Checkpoint checkpoint;
    std::vector<BusinessMonthReport> months;
    BusinessStop stop = BusinessStop::requested_boundary;
    std::optional<core::OrdinalDay> reloaded_on;
};
// Three-year engineering scenario driver. Its plan comes from the compiled
// scenario's business_plan; every policy action uses the authoritative commands.
// Resumption reconstructs policy progress from committed IDs/history, never a
// separate unsaved cursor. No files, wall clock, external service or RNG forecast.
[[nodiscard]] BusinessRunResult run_business_scenario(
    const runtime::CompiledProgram& program, std::string_view scenario_id,
    std::string_view headquarters, BusinessStrategy strategy,
    BusinessRunOptions options = {}, std::optional<runtime::Checkpoint> resume = {});
[[nodiscard]] std::string_view to_string(BusinessStrategy strategy) noexcept;
[[nodiscard]] std::string_view to_string(BusinessStop stop) noexcept;
}
