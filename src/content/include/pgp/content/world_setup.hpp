#pragma once
#include <pgp/core/business_offers.hpp>
#include <array>

namespace pgp::content {
struct HeavyweightPolicy {
    core::Days review_interval{30},warning_lead{90},delivery_lead{30},cooldown{180};
    core::BasisPoints entry_pressure{6000},exit_pressure{2500};
    std::array<std::uint32_t,4> weights{4000,2000,3000,1000};
    core::Cents revenue_at_risk{50000000};
    std::uint32_t capability_target=4,financed_target=2;
    std::string specialist_id,lease_offer_id,publication_node,pilot_product;
    core::Cents publication_fee{5000000},pilot_fee{3000000};
    core::Days offer_response{30},offer_term{90};
    std::uint64_t pilot_tasks=20000;
    auto operator<=>(const HeavyweightPolicy&) const = default;
};
[[nodiscard]] HeavyweightPolicy load_heavyweight_policy(std::string_view bytes);
[[nodiscard]] std::string canonical_heavyweight_policy(const HeavyweightPolicy& policy);
struct RivalPlanning {
    std::string group,region,leader;
    std::uint32_t slot=1;
    core::Cents reserve_floor{0};
    core::Cents price_floor{50},price_ceiling{150};
    std::uint32_t runway_months=2;
    bool publish_results=true;
    // data, research, training, evaluation, deploy, price, hire, finance, expand, publish
    std::array<std::uint32_t,10> priorities{};
    std::vector<std::string> research,recipes,services;
    auto operator<=>(const RivalPlanning&) const = default;
};
struct CompetitionSetup {
    std::uint32_t domestic=6,international=6,heavyweights=3;
    core::Cents catchup_cash{0};
    core::BasisPoints catchup_acquired{1500};
    core::Days publication_delay{30};
    core::BasisPoints publication_credit{2500};
    std::uint32_t estimate_error=20;
    core::Days review_interval{7};
    auto operator<=>(const CompetitionSetup&) const = default;
};
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
    std::optional<RivalPlanning> planning{};
    std::optional<HeavyweightPolicy> response{};
    auto operator<=>(const RivalDefinition&) const = default;
};
[[nodiscard]] std::vector<core::BusinessOffer> load_business_offer_catalog(std::string_view bytes);
[[nodiscard]] std::string canonical_business_offer_catalog(const std::vector<core::BusinessOffer>& offers);
[[nodiscard]] std::vector<RivalDefinition> load_rival_definitions(std::string_view bytes);
[[nodiscard]] std::string canonical_rival_definitions(const std::vector<RivalDefinition>& rivals);
[[nodiscard]] CompetitionSetup load_competition_setup(std::string_view bytes);
[[nodiscard]] std::string canonical_competition_setup(const CompetitionSetup& setup);
[[nodiscard]] bool selected_rival(const RivalDefinition& definition,const CompetitionSetup& setup);
}
