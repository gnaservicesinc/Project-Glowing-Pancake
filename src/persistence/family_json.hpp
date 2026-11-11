#pragma once
#include "json_support.hpp"
#include <pgp/core/family.hpp>
namespace pgp::persistence::detail {
Json family_json(const core::FamilyState&);
core::FamilyState parse_family(const Json&);
}
