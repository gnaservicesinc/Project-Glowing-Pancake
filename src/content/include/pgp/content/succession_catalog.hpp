#pragma once
#include <pgp/core/succession.hpp>

namespace pgp::content {
[[nodiscard]] core::SuccessionCatalog load_succession_catalog(std::string_view);
[[nodiscard]] std::string canonical_succession_catalog(const core::SuccessionCatalog&);
}
