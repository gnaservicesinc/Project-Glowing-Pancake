#pragma once
#include <pgp/core/headquarters.hpp>
#include <map>

namespace pgp::core {
inline constexpr std::size_t personal_property_limit=8, personal_item_limit=64;
inline constexpr std::size_t personal_event_limit=8192, personal_event_guest_limit=32;
inline constexpr std::size_t family_visit_limit=8192;
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
struct PersonalEventOffer {
    std::string id,name,description,venue;
    Cents booking{0},per_guest{0};
    std::uint32_t guests=8,mood=0;
    Days notice{2},duration{7};
    BasisPoints cancellation_refund{7500};
    auto operator<=>(const PersonalEventOffer&) const = default;
};
struct PersonalEventRules {
    std::vector<PersonalEventOffer> offers;
    Days interval{7},maximum_notice{90};
    auto operator<=>(const PersonalEventRules&) const = default;
};
struct FamilyVisitRules {
    Days notice{1}, maximum_stay{7};
    auto operator<=>(const FamilyVisitRules&) const = default;
};
struct PersonalLifeCatalog {
    std::vector<PersonalPropertyOffer> properties;
    std::vector<PersonalDecorOffer> decor;
    std::optional<PersonalEventRules> events{};
    std::optional<FamilyVisitRules> family_visits{};
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
struct PersonalEventGuest {
    std::string person,reply;
    bool attended=false;
    auto operator<=>(const PersonalEventGuest&) const = default;
};
struct PersonalEvent {
    std::uint32_t id=0,property=0,premises=0;
    std::string offer,outcome="scheduled";
    OrdinalDay booked_on{0},day{0};
    Cents paid{0},refund{0};
    std::optional<OrdinalDay> resolved_on;
    std::vector<PersonalEventGuest> guests;
    auto operator<=>(const PersonalEvent&) const = default;
};
struct FamilyVisit {
    std::uint32_t premises=0;
    Days stay{1};
    OrdinalDay booked_on{0}, departs_on{0}, arrives_on{0}, leaves_on{0}, returns_on{0};
    Cents fare{0};
    std::optional<OrdinalDay> cancelled_on{};
    auto operator<=>(const FamilyVisit&) const = default;
};
struct PersonalLifeState {
    std::vector<PersonalProperty> properties;
    std::vector<PersonalDecor> decor;
    std::vector<PersonalBill> bills;
    Cents spent{0},unpaid{0};
    std::vector<PersonalEvent> events{};
    std::vector<std::uint32_t> pending_events{};
    std::vector<FamilyVisit> family_visits{};
    auto operator<=>(const PersonalLifeState&) const = default;
};
struct BuyPersonalProperty {std::string offer;};
struct SellPersonalProperty {std::uint32_t property=0;};
struct BuyPersonalDecor {std::string offer;};
struct PlacePersonalDecor {std::uint32_t item=0,property=0;int x=0,y=0;bool rotated=false;};
struct StorePersonalDecor {std::uint32_t item=0;};
struct SellPersonalDecor {std::uint32_t item=0;};
struct PayPersonalUpkeep {};
struct BookPersonalEvent {std::string offer;std::uint32_t property=0;OrdinalDay day{0};std::vector<std::string> guests;};
struct CancelPersonalEvent {std::uint32_t event=0;};
struct BookFamilyVisit {Days stay{1};};
struct CancelFamilyVisit {};
using PersonalLifeAction=std::variant<BuyPersonalProperty,SellPersonalProperty,BuyPersonalDecor,PlacePersonalDecor,StorePersonalDecor,SellPersonalDecor,PayPersonalUpkeep,BookPersonalEvent,CancelPersonalEvent,BookFamilyVisit,CancelFamilyVisit>;
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
void validate_personal_event_rules(const PersonalEventRules&);
void validate_personal_events(const SimulationState&);
void prepare_personal_events(SimulationState&);
void book_personal_event(SimulationState&,const BookPersonalEvent&);
void cancel_personal_event(SimulationState&,std::uint32_t);
Cents personal_event_quote(const SimulationState&,const BookPersonalEvent&);
std::map<std::string,int> personal_event_moods(const SimulationState&);
[[nodiscard]] FamilyVisit family_visit_quote(const SimulationState&,Days stay);
[[nodiscard]] const FamilyVisit* pending_family_visit(const SimulationState&);
[[nodiscard]] const FamilyVisit* family_visit_on(const SimulationState&,OrdinalDay);
[[nodiscard]] std::string_view family_home_state(const SimulationState&);
void validate_family_visits(const SimulationState&);
}
