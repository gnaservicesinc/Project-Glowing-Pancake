#pragma once
#include <pgp/core/lab.hpp>

namespace pgp::content {
struct Collection;
struct ScenarioDefinition;
// Shared strict integer-only vocabulary for authored lab data and saves.
[[nodiscard]] core::LabCatalog load_lab_catalog(std::string_view bytes);
[[nodiscard]] std::string canonical_lab_catalog(const core::LabCatalog& catalog);
[[nodiscard]] core::LabCatalog scenario_lab_catalog(const Collection& collection, const ScenarioDefinition& scenario);
}
