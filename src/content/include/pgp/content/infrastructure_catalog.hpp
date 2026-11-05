#pragma once
#include <pgp/core/infrastructure.hpp>
#include <string_view>
namespace pgp::content {
[[nodiscard]] core::InfrastructureCatalog load_infrastructure_catalog(std::string_view);
[[nodiscard]] std::string canonical_infrastructure_catalog(const core::InfrastructureCatalog&);
}
