#pragma once
#include "json_support.hpp"
#include <pgp/core/people.hpp>
namespace pgp::persistence::detail {
Json person_profile_json(const core::PersonProfile&);
core::PersonProfile parse_person_profile(const Json&);
Json people_json(const core::PeopleState&);
core::PeopleState parse_people(const Json&);
}
