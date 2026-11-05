#pragma once
#include <pgp/core/governance.hpp>
namespace pgp::content {
[[nodiscard]] core::GovernanceCatalog load_governance_catalog(std::string_view);
[[nodiscard]] std::string canonical_governance_catalog(const core::GovernanceCatalog&);
}
