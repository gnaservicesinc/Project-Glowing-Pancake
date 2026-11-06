#pragma once

#include "json_support.hpp"
#include <pgp/core/market.hpp>

namespace pgp::persistence::detail {
[[nodiscard]] Json market_json(const core::MarketState& state, bool packed = false);
[[nodiscard]] core::MarketState parse_market(const Json& value, bool packed = false);
}
