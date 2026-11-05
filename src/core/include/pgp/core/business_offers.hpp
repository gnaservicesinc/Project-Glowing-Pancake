#pragma once
#include <pgp/core/market.hpp>

namespace pgp::core {
enum class BusinessOfferKind { compute_lease, recovery_pilot, enterprise_contract };
struct BusinessOffer {
    std::string id, name;
    BusinessOfferKind kind = BusinessOfferKind::compute_lease;
    OrdinalDay available_from{0}, available_before{0};
    Days term{0};
    Cents setup_cost{0}, monthly_amount{0};
    MilliCUPerDay capacity{0};
    std::string segment_id, product_id;
    std::uint64_t tasks_per_day = 0;
    bool renewable = false;
    auto operator<=>(const BusinessOffer&) const = default;
};
struct AcceptedBusinessOffer {
    std::string offer_id;
    OrdinalDay accepted_on{0}, expires_before{0};
    auto operator<=>(const AcceptedBusinessOffer&) const = default;
};
struct LeaseRenewal {
    std::string offer_id;
    OrdinalDay renewed_on{0},expires_before{0};
    auto operator<=>(const LeaseRenewal&) const = default;
};
struct RenewComputeLease { std::string id; };
struct BusinessOffersState {
    std::vector<BusinessOffer> catalog;
    std::vector<AcceptedBusinessOffer> accepted;
    std::vector<LeaseRenewal> renewals{};
    auto operator<=>(const BusinessOffersState&) const = default;
};
void validate_business_offer_catalog(const std::vector<BusinessOffer>& catalog);
void validate_business_offers(const BusinessOffersState& state, const AccountsState& accounts,
                              const CapacityState& capacity, const std::optional<MarketState>& market, bool deep = true);
[[nodiscard]] bool business_offer_eligible(const BusinessOffersState& state, const BusinessOffer& offer,
                                          const AccountsState& accounts, const CapacityState& capacity,
                                          const std::optional<MarketState>& market, OrdinalDay day, Cents protected_cash);
void accept_business_offer(BusinessOffersState& state, AccountsState& accounts, CapacityState& capacity,
                           std::optional<MarketState>& market, OrdinalDay day, std::string_view id, Cents protected_cash);
[[nodiscard]] OrdinalDay lease_expiry(const BusinessOffersState& state,std::string_view id);
[[nodiscard]] Cents lease_commitment_ceiling(const BusinessOffer& offer);
void renew_compute_lease(BusinessOffersState& state,AccountsState& accounts,CapacityState& capacity,OrdinalDay day,std::string_view id,Cents protected_cash);
void expire_business_offers(const BusinessOffersState& state, CapacityState& capacity, OrdinalDay day);
}
