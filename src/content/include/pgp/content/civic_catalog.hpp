#pragma once
#include <pgp/core/civic.hpp>

namespace pgp::content {
[[nodiscard]] core::CivicCatalog load_civic_catalog(std::string_view);
[[nodiscard]] std::string canonical_civic_catalog(const core::CivicCatalog&);
}
