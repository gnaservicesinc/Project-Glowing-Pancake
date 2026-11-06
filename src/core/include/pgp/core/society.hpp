#pragma once

#include <pgp/core/market.hpp>

namespace pgp::core {
inline constexpr std::size_t society_regions = 8, society_components = 14, society_audiences = 6;
inline constexpr std::size_t society_month_limit = retained_campaign_months, society_campaign_limit = 4096, society_forum_limit = 256;
using WelfareComponents = std::array<std::uint32_t, society_components>;
using WelfareIndices = std::array<std::uint32_t, 5>;
using AudienceScores = std::array<std::uint32_t, society_audiences>;
[[nodiscard]] std::string_view welfare_component_name(std::size_t);
[[nodiscard]] std::string_view welfare_index_name(std::size_t);
[[nodiscard]] std::string_view welfare_index_key(std::size_t);
[[nodiscard]] std::string_view society_audience_name(std::size_t);
[[nodiscard]] WelfareIndices welfare_indices(const WelfareComponents&);

struct SocietyRegion {
    std::string id, name;
    std::uint32_t population_weight = 1, output_weight = 1;
    WelfareComponents initial{};
    BasisPoints adaptation{1000}, forum_interest{5000};
    Days implementation{30};
    auto operator<=>(const SocietyRegion&) const = default;
};
struct ForumTerms {
    std::string id, title;
    OrdinalDay first_agenda{0};
    std::uint32_t repeat_years = 4, quorum = 5;
    Days hearing{60}, term{1080};
    ProductChannel channel = ProductChannel::agent;
    std::uint32_t evidence_axis = 0;
    BasisPoints evidence_floor{6000}, support_threshold{5000}, endorsement_shift{500};
    Cents endorsement_fee{0};
    auto operator<=>(const ForumTerms&) const = default;
};
struct MediaTerms {
    Cents fee{0};
    Days duration{30};
    BasisPoints awareness{1000};
    std::uint32_t monthly_limit = 3;
    auto operator<=>(const MediaTerms&) const = default;
};
// Durable access and institutional capacity can improve from years of
// adequately delivered public services. The original regional profile
// remains the baseline for short scenarios without this program.
struct SocietyDevelopment {
    BasisPoints public_coverage{3500}, useful_coverage{2500}, evidence_floor{5500};
    BasisPoints maximum_gain{4400}, monthly_gain{40}, monthly_decay{15};
    auto operator<=>(const SocietyDevelopment&) const = default;
};
struct SocietyCatalog {
    std::string assumptions;
    std::vector<SocietyRegion> regions;
    std::vector<std::string> public_services, science, industry;
    std::uint32_t lag_months = 2;
    MediaTerms media;
    ForumTerms forum;
    std::optional<SocietyDevelopment> development{};
    auto operator<=>(const SocietyCatalog&) const = default;
};
struct RegionOutcomes {
    std::uint64_t demand = 0, delivered = 0, public_demand = 0, public_work = 0, science_work = 0, industrial_work = 0;
    std::uint64_t agent_work = 0, compute_milli = 0, promised = 0, missed = 0, player_work = 0;
    std::uint64_t player_public = 0, player_science = 0, player_promised = 0, player_missed = 0;
    BasisPoints concentration{0}, quality{0}, player_quality{0};
    auto operator<=>(const RegionOutcomes&) const = default;
};
struct SocietyMonth {
    OrdinalDay month_end{0};
    std::array<RegionOutcomes, society_regions> outcomes{};
    std::array<WelfareComponents, society_regions> components{};
    AudienceScores audiences{};
    std::optional<std::array<std::uint32_t,society_regions>> durable_gains{};
    auto operator<=>(const SocietyMonth&) const = default;
};
struct MediaCampaign {
    std::uint64_t id = 0, deployment = 0;
    std::string region, service, evidence;
    std::uint32_t audience = 0;
    OrdinalDay starts_on{0}, ends_before{0};
    Cents paid{0};
    BasisPoints awareness{0};
    auto operator<=>(const MediaCampaign&) const = default;
};
struct ForumEndorsement {
    OrdinalDay day{0};
    std::string region, evidence;
    bool support = true;
    Cents paid{0};
    auto operator<=>(const ForumEndorsement&) const = default;
};
struct ForumMember {
    std::string region;
    BasisPoints score{0};
    bool supports = false;
    std::optional<OrdinalDay> effective_from;
    auto operator<=>(const ForumMember&) const = default;
};
struct ForumRound {
    std::uint64_t id = 0;
    OrdinalDay opened_on{0}, ballot_on{0}, expires_before{0};
    std::vector<ForumEndorsement> endorsements;
    std::vector<ForumMember> members;
    auto operator<=>(const ForumRound&) const = default;
};
struct SocietyState {
    SocietyCatalog catalog;
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> prepared_on;
    std::vector<SocietyMonth> months;
    std::vector<MediaCampaign> campaigns;
    std::vector<ForumRound> rounds;
    auto operator<=>(const SocietyState&) const = default;
};
struct BuyMediaCampaign { std::string region, service, evidence; std::uint32_t audience = 0; };
struct EndorseForumStandard { std::uint64_t round = 0; std::string region, evidence; bool support = true; };
[[nodiscard]] SocietyState opening_society(SocietyCatalog, OrdinalDay);
void validate_society_catalog(const SocietyCatalog&);
void prepare_society(SocietyState&, OrdinalDay);
void close_society_month(SocietyState&, OrdinalDay, const std::array<RegionOutcomes, society_regions>&);
void validate_society(const SocietyState&, OrdinalDay);
[[nodiscard]] std::optional<OrdinalDay> next_society_boundary(const SocietyState&, OrdinalDay);
[[nodiscard]] std::size_t society_region_index(const SocietyCatalog&, std::string_view);
[[nodiscard]] bool forum_enforced(const SocietyState&, std::string_view region, OrdinalDay);
[[nodiscard]] MarketInfluences society_market_influences(const SocietyState&, const MarketState&, OrdinalDay,
    const std::function<bool(std::string_view)>& qualified);
[[nodiscard]] BasisPoints campaign_awareness(const SocietyState&, std::string_view region, std::string_view service, OrdinalDay,
    std::uint32_t audience = 2);
[[nodiscard]] WelfareIndices global_welfare(const SocietyState&);
[[nodiscard]] WelfareIndices minimum_welfare(const SocietyState&);
} // namespace pgp::core
