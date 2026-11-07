#pragma once

#include <pgp/core/market.hpp>

namespace pgp::core::market_detail {
[[nodiscard]] bool local_hardware_fits(const CapacityProject&,const MarketSegment&);
[[nodiscard]] bool copy_offer(std::string_view id);
[[nodiscard]] const WeightInstallation* retained_installation(const MarketState&,const CapacityState&,
    const MarketProduct&,const MarketSegment&,const MarketInfluence*);
void record_weight_installations(MarketState&,const MarketDay&);
void validate_weight_installations(const MarketState&);
} // namespace pgp::core::market_detail
