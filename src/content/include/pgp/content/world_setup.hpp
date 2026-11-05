#pragma once
#include <pgp/core/business_offers.hpp>

namespace pgp::content {
struct RivalDefinition {
    std::string id, name;
    std::uint64_t seed = 0;
    core::Cents cash{0}, monthly_payroll{0}, monthly_lease{0};
    core::MilliCUPerDay capacity{0}, training_rate{0};
    std::uint32_t staff = 0;
    core::Days start_after{0}, warning_lead{0};
    std::uint32_t player_deployment_threshold = 0;
    std::string recipe_id, data_id;
    core::MilliCU training_work{0};
    auto operator<=>(const RivalDefinition&) const = default;
};
[[nodiscard]] std::vector<core::BusinessOffer> load_business_offer_catalog(std::string_view bytes);
[[nodiscard]] std::string canonical_business_offer_catalog(const std::vector<core::BusinessOffer>& offers);
[[nodiscard]] std::vector<RivalDefinition> load_rival_definitions(std::string_view bytes);
[[nodiscard]] std::string canonical_rival_definitions(const std::vector<RivalDefinition>& rivals);
}
