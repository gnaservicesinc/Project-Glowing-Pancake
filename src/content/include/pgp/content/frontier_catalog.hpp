#pragma once
#include <pgp/core/frontier.hpp>

namespace pgp::content {
[[nodiscard]] core::FrontierCatalog load_frontier_catalog(std::string_view);
[[nodiscard]] std::string canonical_frontier_catalog(const core::FrontierCatalog&);
}
