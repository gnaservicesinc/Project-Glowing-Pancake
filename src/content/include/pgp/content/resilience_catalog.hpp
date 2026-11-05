#pragma once
#include <pgp/core/resilience.hpp>
namespace pgp::content {
[[nodiscard]] core::ResilienceCatalog load_resilience_catalog(std::string_view);
[[nodiscard]] std::string canonical_resilience_catalog(const core::ResilienceCatalog&);
}
