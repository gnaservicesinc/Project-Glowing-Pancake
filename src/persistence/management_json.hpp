#pragma once
#include "json_support.hpp"
#include <pgp/core/management.hpp>
namespace pgp::persistence::detail {
Json management_json(const core::ManagementState& state);
core::ManagementState parse_management(const Json& value);
}
