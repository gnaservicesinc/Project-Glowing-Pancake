#pragma once

#include "json_support.hpp"
#include <pgp/core/civic.hpp>
#include <pgp/core/society.hpp>
#include <pgp/core/authority.hpp>
#include <pgp/core/frontier.hpp>

namespace pgp::persistence::detail {
[[nodiscard]] Json civic_json(const core::CivicState&);
[[nodiscard]] core::CivicState parse_civic(const Json&);
[[nodiscard]] Json society_json(const core::SocietyState&);
[[nodiscard]] core::SocietyState parse_society(const Json&);
[[nodiscard]] Json authority_json(const core::AuthorityState&);
[[nodiscard]] core::AuthorityState parse_authority(const Json&);
[[nodiscard]] Json frontier_json(const core::FrontierState&);
[[nodiscard]] core::FrontierState parse_frontier(const Json&);
}
