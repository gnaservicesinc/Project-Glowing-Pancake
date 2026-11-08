#pragma once
#include "json_support.hpp"
#include <pgp/core/people.hpp>
namespace pgp::persistence::detail {
Json people_json(const core::PeopleState&);
core::PeopleState parse_people(const Json&);
}
