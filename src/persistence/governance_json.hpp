#pragma once
#include "json_support.hpp"
#include <pgp/core/governance.hpp>
namespace pgp::persistence::detail {
Json governance_json(const core::GovernanceState&);
core::GovernanceState parse_governance(const Json&);
}
