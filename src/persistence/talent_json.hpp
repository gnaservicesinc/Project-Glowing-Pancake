#pragma once
#include "json_support.hpp"
#include <pgp/core/talent.hpp>
namespace pgp::persistence::detail {
Json talent_json(const core::TalentState&);
core::TalentState parse_talent(const Json&);
}
