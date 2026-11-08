#pragma once
#include "json_support.hpp"
#include <pgp/core/counties.hpp>
namespace pgp::persistence::detail {
Json counties_json(const core::CountyState&);
core::CountyState parse_counties(const Json&);
}
