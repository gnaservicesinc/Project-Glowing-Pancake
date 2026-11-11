#pragma once

#include <pgp/core/accounts.hpp>

namespace pgp::core {
struct SimulationState;
struct LabAllocation;
struct ResearchCooperation;

enum class FundingSupportKind { research_grant, cloud_credit };
inline constexpr std::size_t funding_offer_limit=8;
inline constexpr std::size_t funding_use_limit=4096;

struct FundingSupportOffer {
    std::string id,name;
    FundingSupportKind kind=FundingSupportKind::research_grant;
    std::vector<std::string> targets;
    Cents limit{0};
    BasisPoints coverage{0};
    Days opens_after{0},claim_days{0},use_days{0};
    auto operator<=>(const FundingSupportOffer&) const = default;
};

struct FundingAward {
    std::string offer_id;
    OrdinalDay accepted_on{0};
    Cents used{0};
    auto operator<=>(const FundingAward&) const = default;
};

struct FundingUse {
    std::string offer_id,target;
    OrdinalDay day{0};
    Cents amount{0};
    auto operator<=>(const FundingUse&) const = default;
};

struct FundingSupportState {
    std::vector<FundingAward> awards;
    PagedHistory<FundingUse,32> uses;
    auto operator<=>(const FundingSupportState&) const = default;
};

struct FundingSupportStatus {
    OrdinalDay opens_on{0},claim_before{0};
    std::optional<OrdinalDay> active_on,expires_before;
    Cents unused{0};
    bool claimable=false;
};

void validate_funding_offers(const std::vector<FundingSupportOffer>&);
void validate_funding_support(const SimulationState&,bool deep);
[[nodiscard]] FundingSupportStatus funding_support_status(const SimulationState&,const FundingSupportOffer&);
void accept_funding_support(SimulationState&,const FundingSupportOffer&);
[[nodiscard]] Cents research_cash_fee(const SimulationState&,std::string_view research);
Cents start_funded_research(SimulationState&,std::string_view research,const LabAllocation&,Cents protected_cash,const ResearchCooperation* cooperation=nullptr);
[[nodiscard]] std::vector<RecurringCredit> redeem_cloud_credits(SimulationState&);
}
