#pragma once
#include <pgp/core/lab.hpp>

namespace pgp::core {
[[nodiscard]] bool research_data_fits(const ResearchDataRequirement&, const DataOffer&);
[[nodiscard]] std::string research_data_blocker(const LabState&, const CapacityState&, OrdinalDay,
                                                const LabResearch&, const std::vector<std::string>&);
}
