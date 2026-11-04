#pragma once

#include <pgp/core/market.hpp>

namespace pgp::content {
struct MarketOfferDefinition {
    core::MarketProduct product;
    core::CapacityProject serving;
};
struct StartingContractDefinition {
    core::MarketContract terms;
    bool start_immediately = false;
};
struct MarketSetup {
    std::vector<MarketOfferDefinition> offers;
    std::vector<core::MarketSegment> segments;
    std::vector<StartingContractDefinition> contracts;
};
// Shared scenario setup, used both for content validation and actual creation.
void apply_market_setup(const MarketSetup& setup, core::MarketState& market,
                        core::AccountsState& accounts, core::CapacityState& capacity,
                        core::OrdinalDay starts_on);
[[nodiscard]] std::string canonical_market_setup_bytes(const MarketSetup& setup,
                                                        core::OrdinalDay starts_on);
}
