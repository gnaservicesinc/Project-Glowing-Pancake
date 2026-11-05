#pragma once
#include "json_support.hpp"
#include <pgp/core/resilience.hpp>
namespace pgp::persistence::detail {
[[nodiscard]] Json resilience_json(const core::ResilienceState&);
[[nodiscard]] core::ResilienceState parse_resilience(const Json&);
}
