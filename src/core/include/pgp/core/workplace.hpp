#pragma once
#include <pgp/core/units.hpp>
#include <optional>
#include <string>
#include <vector>
namespace pgp::core {
struct SimulationState;
struct HeadquartersRoomOffer {
    std::string id,name,place,description;
    Cents construction{0},monthly{0};Days lead{0};std::uint32_t seats=0;
    auto operator<=>(const HeadquartersRoomOffer&) const = default;
};
struct WorkplaceRules {
    std::uint32_t home_seats=2,shared_seats=8,leased_seats=16,maximum_rooms=12,maximum_blueprints=8;
    std::vector<HeadquartersRoomOffer> rooms;
    auto operator<=>(const WorkplaceRules&) const = default;
};
struct HeadquartersRoom {
    std::string id,offer;OrdinalDay ordered_on{0},ready_on{0};
    auto operator<=>(const HeadquartersRoom&) const = default;
};
struct FacilityBlueprint {
    std::string id,name,hardware_family;
    std::uint64_t grid_watts=0;std::uint32_t operators=0;bool evaporative=false;
    auto operator<=>(const FacilityBlueprint&) const = default;
};
struct BlueprintBuild {
    std::string blueprint,site;OrdinalDay ordered_on{0};
    auto operator<=>(const BlueprintBuild&) const = default;
};
struct WorkplaceState {
    OrdinalDay starts_on{0};std::string opening_office;bool nonresearch_founder=false;
    std::vector<HeadquartersRoom> rooms;
    std::vector<FacilityBlueprint> blueprints;
    std::vector<BlueprintBuild> builds;
    auto operator<=>(const WorkplaceState&) const = default;
};
struct ExpandHeadquarters {std::string id,offer;};
struct CreateFacilityBlueprint {FacilityBlueprint blueprint;};
struct BuildFromBlueprint {std::string blueprint,id,name,state;};
struct BlueprintQuote {
    Cents construction{0},hardware{0},recruitment{0},total{0},monthly{0};
    OrdinalDay built_on{0},grid_on{0},delivery_on{0},operators_on{0};
};
void validate_workplace_rules(const WorkplaceRules&);
void initialize_workplace(SimulationState&,std::string office,bool nonresearch_founder);
std::uint32_t headquarters_seats(const SimulationState&);
std::uint32_t headquarters_seats_used(const SimulationState&);
void expand_headquarters(SimulationState&,const ExpandHeadquarters&,Cents protected_cash);
void create_facility_blueprint(SimulationState&,const FacilityBlueprint&);
BlueprintQuote preview_blueprint_build(const SimulationState&,const BuildFromBlueprint&);
void build_from_blueprint(SimulationState&,const BuildFromBlueprint&,Cents protected_cash);
void validate_workplace(const SimulationState&,bool deep=true);
}
