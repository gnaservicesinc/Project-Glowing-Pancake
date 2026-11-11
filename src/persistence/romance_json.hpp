#pragma once
#include "json_support.hpp"
#include <pgp/core/romance.hpp>
namespace pgp::persistence::detail {
Json romantic_profile_json(const core::RomanticProfile&);
core::RomanticProfile parse_romantic_profile(const Json&);
Json romance_json(const core::RomanceState&);
core::RomanceState parse_romance(const Json&);
}
