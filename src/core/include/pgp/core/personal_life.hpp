#pragma once
#include <pgp/core/headquarters.hpp>

namespace pgp::core {
inline constexpr std::size_t personal_property_limit=8, personal_item_limit=64;
inline constexpr std::size_t personal_bill_limit=personal_property_limit*(retained_campaign_months+1);
struct PersonalPropertyOffer {
    std::string id,name,agent,description;
    HQPlan plan;
    Cents price{0},monthly_upkeep{0};
    BasisPoints resale{9000};
    auto operator<=>(const PersonalPropertyOffer&) const = default;
};
struct PersonalDecorOffer {
    std::string id,name,supplier,kind;
    Cents price{0};
    Days delivery{0};
    int width=1,depth=1;
    BasisPoints resale{5000};
    auto operator<=>(const PersonalDecorOffer&) const = default;
};
struct PersonalLifeCatalog {
    std::vector<PersonalPropertyOffer> properties;
    std::vector<PersonalDecorOffer> decor;
    auto operator<=>(const PersonalLifeCatalog&) const = default;
};
struct PersonalProperty {
    std::uint32_t id=0;
    std::string offer;
    OrdinalDay purchased_on{0},next_upkeep{0};
    std::optional<OrdinalDay> sold_on;
    auto operator<=>(const PersonalProperty&) const = default;
};
struct PersonalDecor {
    std::uint32_t id=0;
    std::string offer;
    OrdinalDay purchased_on{0},delivered_on{0};
    std::uint32_t property=0;
    int x=0,y=0;
    bool rotated=false;
    std::optional<OrdinalDay> sold_on{};
    auto operator<=>(const PersonalDecor&) const = default;
};
struct PersonalBill {
    std::uint32_t id=0,property=0;
    OrdinalDay charged_on{0};
    Cents amount{0};
    std::optional<OrdinalDay> paid_on;
    auto operator<=>(const PersonalBill&) const = default;
};
struct PersonalLifeState {
    std::vector<PersonalProperty> properties;
    std::vector<PersonalDecor> decor;
    std::vector<PersonalBill> bills;
    Cents spent{0},unpaid{0};
    auto operator<=>(const PersonalLifeState&) const = default;
};
struct BuyPersonalProperty {std::string offer;};
struct SellPersonalProperty {std::uint32_t property=0;};
struct BuyPersonalDecor {std::string offer;};
struct PlacePersonalDecor {std::uint32_t item=0,property=0;int x=0,y=0;bool rotated=false;};
struct StorePersonalDecor {std::uint32_t item=0;};
struct SellPersonalDecor {std::uint32_t item=0;};
struct PayPersonalUpkeep {};
using PersonalLifeAction=std::variant<BuyPersonalProperty,SellPersonalProperty,BuyPersonalDecor,PlacePersonalDecor,StorePersonalDecor,SellPersonalDecor,PayPersonalUpkeep>;
struct ManagePersonalLife {PersonalLifeAction action;};
struct PersonalValuation {Cents spent{0},assets{0},liabilities{0};};
void validate_personal_life_catalog(const PersonalLifeCatalog&);
void validate_personal_life(const SimulationState&,bool deep=true);
void prepare_personal_life(SimulationState&);
void manage_personal_life(SimulationState&,const PersonalLifeAction&);
const PersonalLifeCatalog& personal_life_catalog(const SimulationState&);
Cents personal_resale(Cents,BasisPoints);
PersonalValuation personal_valuation(const SimulationState&,OrdinalDay before);
std::string personal_placement_blocker(const SimulationState&,const PlacePersonalDecor&);
}
