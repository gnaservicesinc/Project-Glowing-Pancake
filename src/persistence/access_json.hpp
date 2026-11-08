#pragma once
#include "json_support.hpp"
#include <pgp/core/access.hpp>
namespace pgp::persistence::detail {
Json access_json(const core::AccessState&);
core::AccessState parse_access(const Json&);
}
