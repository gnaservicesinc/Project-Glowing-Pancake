#pragma once

#include <pgp/content/market_setup.hpp>

namespace pgp::content {
// Engineering scenario policy inputs; executing them still uses the same
// revision-checked commands as a human player. This is not a model recipe.
struct BusinessPlan {
    core::CapacityProject training;
    core::Cents data_license{0};
    core::OrdinalDay expansion_on{0};
    MarketSetup expansion;
    core::Cents rescue_proceeds{0};
    core::BasisPoints rescue_shares{0};
};
[[nodiscard]] std::string canonical_business_plan_bytes(const BusinessPlan& plan,
                                                        core::OrdinalDay starts_on);
}
