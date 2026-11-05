#pragma once
#include <pgp/core/management.hpp>
namespace pgp::content {
[[nodiscard]] core::ManagementCatalog load_management_catalog(std::string_view bytes);
[[nodiscard]] std::string canonical_management_catalog(const core::ManagementCatalog& catalog);
}
