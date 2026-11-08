#pragma once
#include <pgp/core/units.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace pgp::core {
struct SimulationState;
struct LiaisonOfficeOffer {
    std::string id,name,region;Cents setup{0},monthly{0};Days lead{0};std::uint32_t people=0;
    auto operator<=>(const LiaisonOfficeOffer&) const = default;
};
struct AccessRules {
    std::uint32_t productive_staff=12,publications=3;Cents earned_revenue{25000000};
    std::vector<LiaisonOfficeOffer> offices;
    auto operator<=>(const AccessRules&) const = default;
};
struct NationalAccess {
    OrdinalDay day{0};std::string cause;
    std::uint64_t staff_additions=0,staff_departures=0,postings=0,publications=0;
    auto operator<=>(const NationalAccess&) const = default;
};
struct LiaisonOffice {
    std::uint64_t id=0;std::string offer;OrdinalDay ordered_on{0},ready_on{0};std::optional<OrdinalDay> closed_on;
    auto operator<=>(const LiaisonOffice&) const = default;
};
struct AccessState {
    bool connected_background=false;std::optional<NationalAccess> national;std::vector<LiaisonOffice> offices;
    std::optional<OrdinalDay> first_overseas_opened{};
    auto operator<=>(const AccessState&) const = default;
};
struct OpenLiaisonOffice {std::string offer;};
struct CloseLiaisonOffice {std::uint64_t id;};
void validate_access_rules(const AccessRules&);
void initialize_access(SimulationState&,bool connected_background);
bool prepare_access(SimulationState&);
bool national_access(const SimulationState&);
bool world_access(const SimulationState&);
std::string regional_contact_block(const SimulationState&,std::string_view region);
bool liaison_office_open(const SimulationState&,const LiaisonOffice&,OrdinalDay);
void open_liaison_office(SimulationState&,std::string_view offer,Cents protected_cash);
void close_liaison_office(SimulationState&,std::uint64_t);
void validate_access(const SimulationState&,bool deep=true);
}
