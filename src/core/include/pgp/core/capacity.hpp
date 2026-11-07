#pragma once

#include <pgp/core/paged_history.hpp>
#include <pgp/core/day_rows.hpp>

#include <pgp/core/calendar.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pgp::core {
enum class CapacitySourceKind { leased, owned, imported };
struct CapacitySource {
    std::string id;
    std::string physical_id;
    std::string owner;
    std::string provider;
    CapacitySourceKind kind = CapacitySourceKind::leased;
    MilliCUPerDay nominal{0};
    MilliCUPerDay effective{0};
    std::uint64_t memory_mib = 0;
    std::string interconnect = "general";
    bool energy_included = true;
    // Absent only for legacy sources present since the campaign started.
    std::optional<OrdinalDay> available_from{};
    MilliCUPerDay wholesale_reserved{0};
    MilliCUPerDay replica_reserved{0};
    auto operator<=>(const CapacitySource&) const = default;
};
enum class ProjectKind { training, research, evaluation, serving, replication, data_generation, incident_response };
enum class ProjectStatus { active, completed, cancelled, paused };
struct ProjectPause {
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> ends_before;
    auto operator<=>(const ProjectPause&) const = default;
};
struct CapacityProject {
    std::string id;
    std::string source_id;
    ProjectKind kind = ProjectKind::training;
    ProjectStatus status = ProjectStatus::active;
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> stopped_on;
    MilliCUPerDay requested{0};
    std::uint32_t staff = 0;
    std::uint64_t minimum_memory_mib = 0;
    std::string interconnect = "general";
    // Finite non-serving goal; serving instead runs until cancellation.
    MilliCU total_work{0};
    MilliCU completed_work{0};
    std::uint64_t tasks_per_day = 0;
    MilliCU inference_milli_CU_per_task{0};
    std::uint64_t useful_tasks = 0;
    Cents monthly_support_cost{0};
    std::string cause;
    bool customer_inference = false;
    std::vector<ProjectPause> pauses{};
    // Incident containment keeps reservations and records zero work, while
    // preserving ordinary cancellation and release history.
    std::vector<ProjectPause> disruptions{};
    auto operator<=>(const CapacityProject&) const = default;
};
struct SourceDayUsage {
    std::string source_id;
    MilliCUPerDay effective{0};
    MilliCU used{0};
    MilliCUPerDay wholesale_available{0};
    MilliCU wholesale_used{0};
    MilliCUPerDay replica_available{0};
    auto operator<=>(const SourceDayUsage&) const = default;
};
struct ProjectDayWork {
    std::string project_id;
    MilliCU work{0};
    std::uint64_t useful_tasks = 0;
    std::uint32_t assigned_staff = 0;
    std::uint64_t protected_tasks = 0;
    std::uint64_t failed_tasks = 0;
    auto operator<=>(const ProjectDayWork&) const = default;
};
struct CapacityDay {
    OrdinalDay day{0};
    DayRows<SourceDayUsage> sources;
    DayRows<ProjectDayWork> projects;
    void share_unchanged(const CapacityDay& previous) {
        sources.reuse_equal(previous.sources);
        projects.reuse_equal(previous.projects);
    }
    auto operator<=>(const CapacityDay&) const = default;
};
inline constexpr std::size_t capacity_source_limit = 256;
inline constexpr std::size_t capacity_staff_change_limit = 8192;
inline constexpr std::size_t capacity_project_limit = 4096;
inline constexpr std::size_t capacity_day_limit = retained_campaign_days;
inline constexpr std::size_t capacity_work_record_limit = capacity_day_limit * 16;
struct StaffAddition {
    std::string id;
    OrdinalDay day{0};
    std::uint32_t people=0;
    auto operator<=>(const StaffAddition&) const = default;
};
struct CapacityState {
    std::string owner = "company.player";
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> closed_through;
    std::uint32_t available_staff = 0;
    std::vector<CapacitySource> sources;
    std::vector<CapacityProject> projects;
    PagedHistory<CapacityDay, 8> days;
    std::vector<StaffAddition> staff_additions{};
    std::vector<StaffAddition> staff_departures{};
    auto operator<=>(const CapacityState&) const = default;
};
struct CapacityReport {
    MilliCUPerDay nominal{0};
    MilliCUPerDay effective{0};
    MilliCUPerDay reserved{0};
    MilliCUPerDay protected_serving{0};
    std::uint32_t available_staff = 0;
    std::uint32_t assigned_staff = 0;
    MilliCU completed_work{0};
    std::uint64_t useful_tasks = 0;
    auto operator<=>(const CapacityReport&) const = default;
};
[[nodiscard]] MilliCU required_inference(std::uint64_t tasks, MilliCU cost_per_task);
[[nodiscard]] std::uint64_t maximum_tasks(MilliCU available, MilliCU cost_per_task);
[[nodiscard]] MilliCUPerDay company_capacity(const CapacitySource& source);
[[nodiscard]] CapacityState opening_capacity(std::vector<CapacitySource> sources,
                                             std::uint32_t staff, OrdinalDay starts_on = OrdinalDay{0},
                                             std::string owner = "company.player");
[[nodiscard]] CapacityReport capacity_report(const CapacityState& state);
[[nodiscard]] bool project_disrupted(const CapacityProject&,OrdinalDay);
void validate_capacity(const CapacityState& state);
void reserve_project(CapacityState& state, OrdinalDay day, CapacityProject project);
void add_staff_capacity(CapacityState& state, OrdinalDay day, std::string id, std::uint32_t people);
void remove_staff_capacity(CapacityState& state,OrdinalDay day,std::string id,std::uint32_t people);
[[nodiscard]] std::uint32_t opening_staff_count(const CapacityState& state);
[[nodiscard]] std::uint32_t available_staff_on(const CapacityState& state, OrdinalDay day);
void add_capacity_source(CapacityState& state, OrdinalDay day, CapacitySource source);
void cancel_project(CapacityState& state, OrdinalDay day, std::string_view project_id);
void pause_project(CapacityState& state,OrdinalDay day,std::string_view project_id);
void resume_project(CapacityState& state,OrdinalDay day,std::string_view project_id);
void update_effective_capacity(CapacityState& state, OrdinalDay day,
                               std::string_view source_id, MilliCUPerDay effective);
struct WholesaleUse {
    std::string source_id;
    MilliCU work{0};
};
void close_capacity_day(CapacityState& state, OrdinalDay day, bool validate_io = true,
                        std::span<const WholesaleUse> wholesale = {});
// A common daily plan: contract tasks precede discretionary serving, then
// finite projects. Omitted serving rows do no work; unused compute can train.
struct ServingRequest {
    std::string project_id;
    std::uint64_t protected_tasks = 0;
    std::uint64_t additional_tasks = 0;
    std::uint64_t failed_tasks = 0;
};
[[nodiscard]] CapacityDay preview_capacity_day(const CapacityState& state, OrdinalDay day,
                                               const std::vector<ServingRequest>& serving,bool validate_input=true);
void close_capacity_day(CapacityState& state, OrdinalDay day,
                        const std::vector<ServingRequest>& serving, bool validate_io = true,
                        std::span<const WholesaleUse> wholesale = {});
} // namespace pgp::core
