#pragma once

#include <pgp/core/society.hpp>
#include <span>

namespace pgp::core {

enum class ObjectiveKind { monopoly, domination, utopia, profit, asi, survival };
inline constexpr std::array objective_kinds{ObjectiveKind::monopoly, ObjectiveKind::domination,
    ObjectiveKind::utopia, ObjectiveKind::profit, ObjectiveKind::asi, ObjectiveKind::survival};
inline constexpr std::size_t monopoly_months = 60, utopia_months = 120;
inline constexpr std::uint32_t monopoly_global_floor = 8500, monopoly_region_floor = 6000;
inline constexpr std::uint32_t authority_region_floor = 6000, authority_coverage_floor = 8000;
inline constexpr WelfareIndices utopia_global_floors{9000,8500,8000,8000,7500};
inline constexpr std::uint32_t utopia_region_floor = 7000;

struct ObjectiveSelection {
    ObjectiveKind primary = ObjectiveKind::survival;
    std::vector<ObjectiveKind> secondary;
    auto operator<=>(const ObjectiveSelection&) const = default;
};

// Victory thresholds and windows are shared rules. Only opening selections
// and the disclosed, symmetric valuation assumptions are authored here.
struct ObjectiveCharter {
    ObjectiveSelection opening;
    BasisPoints receivables_realizable{8000}, physical_assets_realizable{6000};
    std::uint32_t profit_multiple = 3;
    bool retrospective = false;
    auto operator<=>(const ObjectiveCharter&) const = default;
};
// These requests require the world transaction: several sources belong to
// rival companies and public institutions.
struct SelectCampaignObjectives { ObjectiveSelection selection; };
struct AcknowledgeObjectiveBreach { ObjectiveKind objective; };
struct EndWithPostmortem {};
struct FinishFounderRun {};
struct ContinueAfterEnding {};
struct AcceptResearchAppointment { std::string employer; };
struct DirectCareerResearch { std::string topic; };
struct PublishCareerResearch { std::string topic; };

struct DeliveredWork {
    std::uint64_t controlled = 0, total = 0;
    auto operator<=>(const DeliveredWork&) const = default;
};
using RegionalDeliveredWork = std::array<DeliveredWork,society_regions>;

struct ObjectiveMonth {
    OrdinalDay month_end{0};
    RegionalDeliveredWork delivery{};
    std::uint32_t population_coverage = 0, output_coverage = 0;
    WelfareIndices global_welfare{};
    std::array<WelfareIndices,society_regions> regional_welfare{};
    auto operator<=>(const ObjectiveMonth&) const = default;
};

[[nodiscard]] std::string_view objective_key(ObjectiveKind) noexcept;
[[nodiscard]] std::string_view objective_name(ObjectiveKind) noexcept;
[[nodiscard]] std::optional<ObjectiveKind> objective_kind(std::string_view) noexcept;
void validate_objective_selection(const ObjectiveSelection&,bool asi_available);
void validate_objective_charter(const ObjectiveCharter&);
[[nodiscard]] OrdinalDay objective_window_start(OrdinalDay deadline,ObjectiveKind);
[[nodiscard]] bool objective_month_end(OrdinalDay);
[[nodiscard]] bool domination_met(const ObjectiveMonth&) noexcept;
[[nodiscard]] bool utopia_met(const ObjectiveMonth&) noexcept;
[[nodiscard]] RegionalDeliveredWork aggregate_delivery(std::span<const ObjectiveMonth>,OrdinalDay first);
[[nodiscard]] DeliveredWork global_delivery(const RegionalDeliveredWork&);
// Zero observed and zero possible future tasks fails the denominator test.
// The bound gives all future work to the player and none to rivals.
[[nodiscard]] bool monopoly_still_possible(const RegionalDeliveredWork&,
    const std::array<std::uint64_t,society_regions>& future_upper_bound = {});

} // namespace pgp::core
