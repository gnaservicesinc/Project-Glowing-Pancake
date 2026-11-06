#pragma once

#include "json_support.hpp"
#include <pgp/core/civic.hpp>
#include <pgp/core/society.hpp>

namespace pgp::persistence::detail {
[[nodiscard]] Json civic_json(const core::CivicState&);
[[nodiscard]] core::CivicState parse_civic(const Json&);
[[nodiscard]] Json society_json(const core::SocietyState&);
[[nodiscard]] core::SocietyState parse_society(const Json&);
}
