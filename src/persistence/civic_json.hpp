#pragma once

#include "json_support.hpp"
#include <pgp/core/civic.hpp>

namespace pgp::persistence::detail {
[[nodiscard]] Json civic_json(const core::CivicState&);
[[nodiscard]] core::CivicState parse_civic(const Json&);
}
