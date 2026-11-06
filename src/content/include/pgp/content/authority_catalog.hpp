#pragma once
#include <pgp/core/authority.hpp>
namespace pgp::content {
[[nodiscard]] core::AuthorityCatalog load_authority_catalog(std::string_view);
[[nodiscard]] std::string canonical_authority_catalog(const core::AuthorityCatalog&);
}
