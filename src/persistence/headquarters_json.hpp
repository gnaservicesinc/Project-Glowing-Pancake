#pragma once
#include "json_support.hpp"
#include <pgp/core/headquarters.hpp>
namespace pgp::persistence::detail {
Json headquarters_json(const core::HeadquartersState&);
core::HeadquartersState parse_headquarters(const Json&);
}
