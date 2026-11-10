#pragma once
#include <pgp/core/units.hpp>
#include <pgp/core/calendar.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pgp::core {
struct SimulationState;
struct HQRoom {
    int x=0,y=0,width=0,depth=0;
    auto operator<=>(const HQRoom&) const = default;
};
struct HQMarket {
    std::string id,state,city,agent,landlord,street;
    Cents rent_per_cell{0},land_per_cell{0},monthly_land_tax_per_cell{0};
    std::uint32_t deposit_months=0,minimum_lease_days=0,early_exit_months=0;
    auto operator<=>(const HQMarket&) const = default;
};
struct HQPlan {
    std::string id,name;
    int width=0,depth=0;
    std::vector<HQRoom> rooms;
    auto operator<=>(const HQPlan&) const = default;
};
struct HQEquipmentOffer {
    std::string id,name,supplier,kind;
    Cents price{0};
    Days delivery{0};
    int width=0,depth=0;
    MilliCUPerDay compute{0};
    std::uint32_t watts=0;
    std::uint64_t memory_mib=0;
    auto operator<=>(const HQEquipmentOffer&) const = default;
};
struct HQContractor {
    std::string id,name;
    Cents cost_per_cell{0};
    std::uint32_t base_days=0,cells_per_day=0,delay_bp=0,overrun_bp=0,maximum_delay_days=0,maximum_overrun_bp=0;
    auto operator<=>(const HQContractor&) const = default;
};
struct HeadquartersRules {
    int home_width=3,home_depth=3,maximum_edge=40;
    std::uint32_t maximum_items=128,maximum_properties=16,building_tax_annual_bp=100;
    std::vector<HQMarket> markets;
    std::vector<HQPlan> plans;
    std::vector<HQEquipmentOffer> equipment;
    std::vector<HQContractor> contractors;
    auto operator<=>(const HeadquartersRules&) const = default;
};
struct HQPremises {
    std::uint32_t id=0;
    std::string kind,market,plan,lessor,address;
    int width=0,depth=0;
    std::vector<HQRoom> rooms;
    Cents rent{0},deposit{0};
    OrdinalDay entered_on{0};
    std::optional<OrdinalDay> left_on;
    std::uint32_t construction=0;
    auto operator<=>(const HQPremises&) const = default;
};
struct HQEquipment {
    std::uint32_t id=0;
    std::string offer,person;
    OrdinalDay ordered_on{0},delivered_on{0};
    Cents paid{0};
    bool inherited=false,powered=true,rotated=false;
    std::uint32_t premises=0,mount=0;
    int x=0,y=0;
    auto operator<=>(const HQEquipment&) const = default;
};
struct HQLand {
    std::uint32_t id=0;
    std::string market,plan;
    OrdinalDay purchased_on{0};
    Cents paid{0};
    auto operator<=>(const HQLand&) const = default;
};
struct HQConstruction {
    std::uint32_t id=0,land=0;
    std::string contractor;
    int width=0,depth=0;
    std::vector<HQRoom> rooms;
    OrdinalDay signed_on{0},promised_on{0},finish_on{0};
    Cents quote{0},prepaid{0},actual{0};
    bool delay_announced=false;
    std::optional<OrdinalDay> completed_on;
    auto operator<=>(const HQConstruction&) const = default;
};
struct HQNotice {
    OrdinalDay day{0};std::string text;
    auto operator<=>(const HQNotice&) const = default;
};
struct HeadquartersState {
    std::uint64_t stream=0,energy_wh=0,electricity_remainder=0;
    Cents electricity_cost{0};
    std::optional<OrdinalDay> metered_through;
    std::vector<HQPremises> premises;
    std::vector<HQEquipment> items;
    std::vector<HQLand> land;
    std::vector<HQConstruction> construction;
    std::vector<HQNotice> notices;
    auto operator<=>(const HeadquartersState&) const = default;
};
struct BuyHQEquipment {std::string offer;};
struct PlaceHQEquipment {std::uint32_t item=0,mount=0;int x=0,y=0;bool rotated=false;};
struct StoreHQEquipment {std::uint32_t item=0;};
struct AssignHQDesk {std::uint32_t desk=0;std::string person;};
struct PowerHQEquipment {std::uint32_t item=0;bool powered=true;};
struct AssignHQLaptop {std::uint32_t item=0;std::string person;};
struct LeaseHQ {std::string market,plan;};
struct BuyHQLand {std::string market,plan;};
struct BuildHQ {std::uint32_t land=0;std::string contractor;int width=0,depth=0;std::vector<HQRoom> rooms;};
struct MoveToOwnedHQ {std::uint32_t construction=0;};
using HeadquartersAction=std::variant<BuyHQEquipment,PlaceHQEquipment,StoreHQEquipment,AssignHQDesk,PowerHQEquipment,LeaseHQ,BuyHQLand,BuildHQ,MoveToOwnedHQ,AssignHQLaptop>;
struct ManageHeadquarters {HeadquartersAction action;};
struct HQQuote {
    Cents price{0},maximum{0},monthly{0},exit_fee{0},refund{0};
    OrdinalDay ready_on{0};
};
void validate_headquarters_rules(const HeadquartersRules&);
void validate_hq_plan(int width,int depth,const std::vector<HQRoom>&,int maximum_edge);
void initialize_headquarters(SimulationState&,std::uint64_t seed);
void prepare_headquarters(SimulationState&);
void meter_headquarters_day(SimulationState&);
void validate_headquarters(const SimulationState&,bool deep=true);
void manage_headquarters(SimulationState&,const HeadquartersAction&,Cents protected_cash);
bool headquarters_purchase(const HeadquartersAction&);
const HQPremises& current_headquarters(const SimulationState&);
HQQuote preview_hq_lease(const SimulationState&,const LeaseHQ&);
HQQuote preview_hq_move(const SimulationState&,const MoveToOwnedHQ&);
HQQuote preview_hq_land(const SimulationState&,const BuyHQLand&);
HQQuote preview_hq_build(const SimulationState&,const BuildHQ&);
bool hq_computer_available(const SimulationState&);
std::uint32_t furnished_hq_desks(const SimulationState&);
std::uint32_t assigned_hq_desks(const SimulationState&);
[[nodiscard]] std::uint32_t assigned_hq_desk(const SimulationState&,std::string_view person);
[[nodiscard]] bool hq_workstation_ready(const SimulationState&,std::string_view person);
[[nodiscard]] bool hq_laptop_ready(const SimulationState&,std::string_view person);
[[nodiscard]] bool hq_desk_device(std::string_view kind);
[[nodiscard]] bool hq_portable(std::string_view kind);
[[nodiscard]] std::string hq_placement_blocker(const SimulationState&,const PlaceHQEquipment&);
[[nodiscard]] bool hq_has_lab_equipment(const SimulationState&,std::span<const std::string> equipment);
}
