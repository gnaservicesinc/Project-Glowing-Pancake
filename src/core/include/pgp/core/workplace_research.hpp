#pragma once

#include <pgp/core/lab.hpp>

namespace pgp::core {
struct SimulationState;
struct ResearchWorker {
    std::string id, name, unavailable;
    std::uint32_t skill = 0;
};

[[nodiscard]] bool workplace_research_enabled(const SimulationState &);
[[nodiscard]] std::vector<ResearchWorker> research_workers(const SimulationState &,
                                                           std::string_view except_project = {});
[[nodiscard]] std::vector<std::string> research_equipment(const SimulationState &, std::string_view topic);
[[nodiscard]] std::string research_workplace_blocker(const SimulationState &, std::string_view topic,
                                                     const LabAllocation &, std::string_view except_project = {});
void require_workplace_research(const SimulationState &, std::string_view topic, const LabAllocation &);
void require_workplace_research_resume(const SimulationState &, std::string_view project);
void reconcile_workplace_research(SimulationState &);
void validate_workplace_research(const SimulationState &);
} // namespace pgp::core
