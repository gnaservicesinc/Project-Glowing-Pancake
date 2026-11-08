#pragma once
#include "json_support.hpp"
#include <pgp/core/workplace.hpp>
namespace pgp::persistence::detail {
Json workplace_json(const core::WorkplaceState&);
core::WorkplaceState parse_workplace(const Json&);
}
