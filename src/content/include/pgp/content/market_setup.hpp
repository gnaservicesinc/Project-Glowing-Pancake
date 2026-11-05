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
struct RegionProfile {
    std::string id;
    std::string name;
    std::string description;
};
struct WholesalePlan {
    std::string id;
    std::string name;
    core::MilliCUPerDay capacity{0};
    std::uint64_t minimum_memory_mib = 0;
    std::string interconnect;
    core::Days term{0};
    core::Cents daily_fee{0};
    bool energy_included = true;
    core::BasisPoints availability{10000};
};
struct RegionalMarkets {
    std::vector<RegionProfile> regions;
    MarketSetup products;
    std::vector<WholesalePlan> wholesale;
};
[[nodiscard]] std::vector<core::ServiceDefinition> service_definitions(const MarketSetup& setup);
// Shared scenario setup, used both for content validation and actual creation.
void apply_market_setup(const MarketSetup& setup, core::MarketState& market,
                        core::AccountsState& accounts, core::CapacityState& capacity,
                        core::OrdinalDay starts_on);
[[nodiscard]] std::string canonical_market_setup_bytes(const MarketSetup& setup,
                                                        core::OrdinalDay starts_on);
[[nodiscard]] std::string canonical_regional_market_bytes(const RegionalMarkets& catalog);
}
