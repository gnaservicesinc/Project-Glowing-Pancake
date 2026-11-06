#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pgp::runtime {
struct CampaignIdentity {
    std::string company_name;
    std::string founder_name;
    std::string founder_pronouns;
    auto operator<=>(const CampaignIdentity&) const = default;
};
void validate_campaign_identity(const CampaignIdentity&);
[[nodiscard]] std::uint64_t parse_world_seed(std::string_view);
[[nodiscard]] CampaignIdentity generate_campaign_identity(std::uint64_t local_seed);
} // namespace pgp::runtime
