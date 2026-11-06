#pragma once
#include "json_support.hpp"
#include <pgp/runtime/objectives.hpp>

namespace pgp::persistence::detail {
[[nodiscard]] Json objectives_json(const runtime::ObjectivesState&);
[[nodiscard]] runtime::ObjectivesState parse_objectives(const Json&);
}
