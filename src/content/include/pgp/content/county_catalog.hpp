#pragma once
#include <pgp/core/counties.hpp>
#include <string_view>
namespace pgp::content {
core::CountyCatalog load_county_catalog(std::string_view);
std::string canonical_county_catalog(const core::CountyCatalog&);
}
