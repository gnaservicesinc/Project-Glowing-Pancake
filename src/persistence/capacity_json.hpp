#pragma once

#include "json_support.hpp"
#include <pgp/core/capacity.hpp>

namespace pgp::persistence::detail {
[[nodiscard]] Json capacity_json(const core::CapacityState& state, bool packed = false);
[[nodiscard]] core::CapacityState parse_capacity(const Json& value, bool packed = false);
}
