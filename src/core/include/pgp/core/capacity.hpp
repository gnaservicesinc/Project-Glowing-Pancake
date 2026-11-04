#pragma once

#include <pgp/core/calendar.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pgp::core {
enum class CapacitySourceKind { leased, owned };
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
    auto operator<=>(const CapacitySource&) const = default;
};
enum class ProjectKind { training, research, evaluation, serving };
enum class ProjectStatus { active, completed, cancelled };
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
    auto operator<=>(const CapacityProject&) const = default;
};
struct SourceDayUsage {
    std::string source_id;
    MilliCUPerDay effective{0};
    MilliCU used{0};
    auto operator<=>(const SourceDayUsage&) const = default;
};
struct ProjectDayWork {
    std::string project_id;
    MilliCU work{0};
    std::uint64_t useful_tasks = 0;
    std::uint32_t assigned_staff = 0;
    auto operator<=>(const ProjectDayWork&) const = default;
};
struct CapacityDay {
    OrdinalDay day{0};
    std::vector<SourceDayUsage> sources;
    std::vector<ProjectDayWork> projects;
    auto operator<=>(const CapacityDay&) const = default;
};
inline constexpr std::size_t capacity_source_limit = 256;
inline constexpr std::size_t capacity_project_limit = 4096;
inline constexpr std::size_t capacity_day_limit = 40960;
inline constexpr std::size_t capacity_work_record_limit = 262144;
struct CapacityState {
    std::string owner = "company.player";
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> closed_through;
    std::uint32_t available_staff = 0;
    std::vector<CapacitySource> sources;
    std::vector<CapacityProject> projects;
    std::vector<CapacityDay> days;
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
[[nodiscard]] CapacityState opening_capacity(std::vector<CapacitySource> sources,
                                             std::uint32_t staff, OrdinalDay starts_on = OrdinalDay{0},
                                             std::string owner = "company.player");
[[nodiscard]] CapacityReport capacity_report(const CapacityState& state);
void validate_capacity(const CapacityState& state);
void reserve_project(CapacityState& state, OrdinalDay day, CapacityProject project);
void cancel_project(CapacityState& state, OrdinalDay day, std::string_view project_id);
void update_effective_capacity(CapacityState& state, OrdinalDay day,
                               std::string_view source_id, MilliCUPerDay effective);
void close_capacity_day(CapacityState& state, OrdinalDay day);
} // namespace pgp::core
