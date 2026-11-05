#pragma once

#include <pgp/core/market.hpp>
#include <pgp/core/deployment.hpp>

namespace pgp::content {
using MarketOfferDefinition = core::ServiceDefinition;
struct StartingContractDefinition {
    core::MarketContract terms;
    bool start_immediately = false;
};
struct MarketSetup {
    std::vector<MarketOfferDefinition> offers;
    std::vector<core::MarketSegment> segments;
    std::vector<StartingContractDefinition> contracts;
};
[[nodiscard]] std::vector<core::ServiceDefinition> service_definitions(const MarketSetup& setup);
// Shared scenario setup, used both for content validation and actual creation.
void apply_market_setup(const MarketSetup& setup, core::MarketState& market,
                        core::AccountsState& accounts, core::CapacityState& capacity,
                        core::OrdinalDay starts_on);
[[nodiscard]] std::string canonical_market_setup_bytes(const MarketSetup& setup,
                                                        core::OrdinalDay starts_on);
}
