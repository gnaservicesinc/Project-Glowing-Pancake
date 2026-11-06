#pragma once
#include <pgp/core/society.hpp>
namespace pgp::content {
[[nodiscard]] core::SocietyCatalog load_society_catalog(std::string_view);
[[nodiscard]] std::string canonical_society_catalog(const core::SocietyCatalog&);
}
