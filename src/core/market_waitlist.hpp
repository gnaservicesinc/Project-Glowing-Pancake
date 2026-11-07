#pragma once

#include <pgp/core/market.hpp>

namespace pgp::core::market_detail {
void apply_waitlist_limits(const MarketState&,OrdinalDay,const std::vector<AllocationProduct>&,std::vector<AllocationSegment>&);
void settle_waitlists(const MarketState&,MarketDay&,const std::vector<AllocationSegment>&,const AllocationResult&);
void validate_waitlist_history(const MarketState&);
} // namespace pgp::core::market_detail
