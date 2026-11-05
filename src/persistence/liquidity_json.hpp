#pragma once

#include "json_support.hpp"
#include <pgp/core/liquidity.hpp>

namespace pgp::persistence::detail {
[[nodiscard]] Json liquidity_json(const core::LiquidityState& state);
[[nodiscard]] core::LiquidityState parse_liquidity(const Json& value);
}
