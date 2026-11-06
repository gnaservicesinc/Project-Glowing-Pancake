#include <pgp/runtime/campaign_identity.hpp>
#include <pgp/core/random.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <stdexcept>

namespace pgp::runtime {
namespace {
void validate_text(std::string_view value, std::size_t maximum, bool required) {
    if (value.size() > maximum || (required && value.find_first_not_of(' ') == value.npos) ||
        std::ranges::any_of(value, [](unsigned char character) { return character < 32 || character == 127; }))
        throw std::invalid_argument("Enter a company and founder name within the supported length and without control characters");
    try { (void)nlohmann::json(value).dump(); }
    catch (const nlohmann::json::type_error&) { throw std::invalid_argument("Campaign names must contain valid Unicode text"); }
}
}
void validate_campaign_identity(const CampaignIdentity& identity) {
    validate_text(identity.company_name, 128, true);
    validate_text(identity.founder_name, 128, true);
    validate_text(identity.founder_pronouns, 64, false);
}
std::uint64_t parse_world_seed(std::string_view text) {
    std::uint64_t seed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), seed);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::invalid_argument("World seed must be a whole number from 0 through 18446744073709551615");
    return seed;
}
CampaignIdentity generate_campaign_identity(std::uint64_t local_seed) {
    static constexpr std::array prefixes{"Cedar", "Lighthouse", "Quiet River", "Blue Orchard", "Juniper", "Fieldstone", "Silver Fern", "Daybreak"};
    static constexpr std::array companies{"Systems", "Research", "Works", "Labs", "Computing", "Cooperative", "Technologies", "Instruments"};
    static constexpr std::array first_names{"Alex", "Rowan", "Jules", "Sam", "River", "Morgan", "Casey", "Avery"};
    static constexpr std::array last_names{"Vale", "Moss", "Reed", "Hale", "Finch", "North", "Lane", "Ash"};
    core::SplitMix64 local{local_seed};
    const auto company_prefix = local.bounded(prefixes.size());
    const auto company_suffix = local.bounded(companies.size());
    const auto first = local.bounded(first_names.size());
    const auto last = local.bounded(last_names.size());
    return {std::string(prefixes[company_prefix]) + ' ' + companies[company_suffix],
        std::string(first_names[first]) + ' ' + last_names[last], "they/them"};
}
} // namespace pgp::runtime
