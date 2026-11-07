#pragma once

#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <stdexcept>

namespace pgp::core::frontier_detail {
[[noreturn]] inline void invalid(const char* message) { throw std::invalid_argument(message); }

template<class Rows> const auto* named(const Rows& rows, std::string_view id) {
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const auto& entry) { return entry.id == id; });
    return row == rows.end() ? nullptr : &*row;
}
template<class Rows> const auto& require_named(const Rows& rows, std::string_view id) {
    const auto* row = named(rows, id);
    if (!row) invalid("Choose an existing frontier definition or source record.");
    return *row;
}
[[nodiscard]] bool text_valid(std::string_view, std::size_t maximum = 120);
[[nodiscard]] bool condition_met(const FutureConditions& actual, const FutureConditions& minimum);
[[nodiscard]] FrontierScores model_scores(const FrontierCatalog&, const FuturePath&, const LabCatalog&, const LabModel&,std::span<const GeneratedDataset> generated={},std::span<const DataAcquisition> acquired={});
[[nodiscard]] bool meets_agi(const FrontierScores&);
[[nodiscard]] bool meets_asi(const FrontierScores&);
[[nodiscard]] const FrontierSystem& system(const FrontierState&, std::uint64_t);
[[nodiscard]] const DeploymentRecord& deployment(const SimulationState&, std::uint64_t);
[[nodiscard]] bool serves_interval(const CapacityProject&, OrdinalDay first, OrdinalDay last);
[[nodiscard]] FrontierResult observe_trial(const SimulationState&, const FrontierTrial&, OrdinalDay last);
[[nodiscard]] std::string allocation_block(const CapacityState&, const LabAllocation&);
void validate_policies(const SimulationState&);
}
