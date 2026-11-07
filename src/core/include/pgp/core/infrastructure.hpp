#pragma once
#include <pgp/core/accounts.hpp>
#include <pgp/core/capacity.hpp>

namespace pgp::core {
struct SimulationState;
enum class CoolingMode { air, evaporative };
enum class AppointmentMode { onsite, remote, delegated };
struct StateSiteProfile {
    std::string id,name,travel_zone;
    Cents construction_base{0},construction_per_kw{0},monthly_property{0},monthly_overhead{0};
    Cents operator_signing{0},operator_monthly{0},incentive_cap{0};
    BasisPoints construction_incentive{0};
    Days build_lead{0},grid_lead{0},operator_hire_lead{0};
    std::uint64_t grid_watts=0,water_limit_ml_per_day=0,water_ml_per_kwh=0;
    std::uint32_t air_pue_milli=1000,water_pue_milli=1000,grid_grams_per_kwh=0;
    std::uint32_t electricity_millicents_per_kwh=0,water_cents_per_1000_liters=0,connectivity_class=0;
    auto operator<=>(const StateSiteProfile&) const = default;
};
struct HardwareFamily {
    std::string id,name,interconnect;
    Cents purchase_cost{0},monthly_maintenance{0};
    MilliCUPerDay capacity{0};
    std::uint64_t memory_mib=0,active_it_watts=0,idle_it_watts=0;
    std::uint32_t operators=1,connectivity_class=0;
    Days delivery_lead{0},useful_life{0};
    BasisPoints resale_fraction{0};
    std::vector<std::string> requires_research;
    auto operator<=>(const HardwareFamily&) const = default;
};
inline constexpr std::size_t infrastructure_hardware_family_limit = 18;
struct TravelRoute {
    std::string from,to;
    Days one_way{0};Cents fare{0};
    auto operator<=>(const TravelRoute&) const = default;
};
struct InfrastructureCatalog {
    std::string map_asset_id,map_sha256,assumptions;
    std::vector<StateSiteProfile> states;
    std::vector<HardwareFamily> hardware;
    std::vector<TravelRoute> travel;
    Days building_life{10950},initial_lease_notice{30};
    BasisPoints building_resale{5000};
    Cents onsite_inspection{10000},remote_inspection{25000},delegated_inspection{50000};
    Days onsite_booking_lead{1},remote_booking_lead{7},delegated_booking_lead{3};
    std::uint64_t minimum_site_watts=1000,maximum_site_watts=1000000;
    std::uint32_t maximum_sites=50,maximum_hardware_per_site=4,maximum_operators_per_site=32;
    auto operator<=>(const InfrastructureCatalog&) const = default;
};
struct SitePowerLimit {
    OrdinalDay day{0};std::uint64_t watts=0;
    auto operator<=>(const SitePowerLimit&) const = default;
};
struct SiteMeterMonth {
    OrdinalDay month{0};
    std::uint64_t energy_wh=0,water_ml=0,emissions_grams=0;
    Cents electricity{0},water{0},operating{0};
    auto operator<=>(const SiteMeterMonth&) const = default;
};
struct OwnedSite {
    std::string id,name,state_id;
    CoolingMode cooling=CoolingMode::air;
    OrdinalDay ordered_on{0},built_on{0},grid_on{0};
    std::optional<OrdinalDay> commissioned_on,closed_on;
    std::uint64_t grid_watts=0;
    Cents paid{0},depreciated{0},resale{0};
    std::vector<SitePowerLimit> power;
    std::vector<SiteMeterMonth> meters;
    std::uint64_t electricity_remainder=0,water_charge_remainder=0,water_remainder=0,emissions_remainder=0;
    std::uint64_t last_energy_wh=0,last_water_ml=0,last_emissions_grams=0;
    Cents last_utility_cost{0};
    auto operator<=>(const OwnedSite&) const = default;
};
struct HardwareOrder {
    std::string id,site_id,family_id,source_id;
    OrdinalDay ordered_on{0},delivered_on{0};
    std::optional<OrdinalDay> commissioned_on,retired_on;
    Cents paid{0},depreciated{0},resale{0};
    std::uint64_t energy_remainder=0;
    auto operator<=>(const HardwareOrder&) const = default;
};
struct SiteOperatorHire {
    std::string id,site_id;
    std::uint32_t people=0;
    OrdinalDay ordered_on{0},starts_on{0};
    auto operator<=>(const SiteOperatorHire&) const = default;
};
struct SiteInspection {
    OrdinalDay day{0};bool built=false,connected=false;
    std::uint32_t delivered_bundles=0,operators=0;
    MilliCUPerDay effective{0};
    std::uint64_t observed_energy_wh=0;
    auto operator<=>(const SiteInspection&) const = default;
};
struct SiteAppointment {
    std::string id,site_id;
    AppointmentMode mode=AppointmentMode::remote;
    OrdinalDay booked_on{0},starts_on{0},visit_on{0},ends_before{0};
    Cents paid{0};
    std::optional<OrdinalDay> cancelled_on;
    std::optional<SiteInspection> inspection;
    auto operator<=>(const SiteAppointment&) const = default;
};
struct InfrastructureNotice {
    std::string id,kind,subject;
    OrdinalDay day{0};
    auto operator<=>(const InfrastructureNotice&) const = default;
};
struct InfrastructureState {
    InfrastructureCatalog catalog;
    std::string executive_base;
    std::optional<OrdinalDay> closed_through,initial_lease_noticed_on,initial_lease_ends_before;
    std::vector<OwnedSite> sites;
    std::vector<HardwareOrder> hardware;
    std::vector<SiteOperatorHire> hires;
    std::vector<SiteAppointment> appointments;
    std::vector<InfrastructureNotice> notices;
    auto operator<=>(const InfrastructureState&) const = default;
};
struct BuildOwnedSite {std::string id,name,state_id;std::uint64_t grid_watts=0;CoolingMode cooling=CoolingMode::air;};
struct OrderOwnedHardware {std::string id,site_id,family_id;};
struct HireSiteOperators {std::string id,site_id;std::uint32_t people=0;};
struct SetSitePowerLimit {std::string site_id;std::uint64_t watts=0;};
struct RetireOwnedHardware {std::string id;};
struct CloseOwnedSite {std::string id;};
struct ScheduleSiteAppointment {std::string id,site_id;OrdinalDay visit_on{0};AppointmentMode mode=AppointmentMode::remote;};
struct CancelSiteAppointment {std::string id;};
struct NoticeInitialLeaseTermination {};
struct SiteBuildPreview {Cents price{0},incentive{0};OrdinalDay built_on{0},grid_on{0};std::uint64_t facility_watts=0,it_watts=0;};
struct SiteHardwarePower {std::string order_id;MilliCUPerDay effective{0};bool powered=false;};
struct SitePowerPreview {std::uint64_t facility_watts=0,it_watts=0;std::uint32_t operators=0;std::vector<SiteHardwarePower> hardware;};
struct HardwareReplacementTerms {
    OrdinalDay eligible_on{0};
    Cents purchase{0},monthly_maintenance{0};
};
struct ExecutiveLocation {std::string base,state_id;bool traveling=false;std::optional<std::string> appointment;};
void validate_infrastructure_catalog(const InfrastructureCatalog&);
void validate_infrastructure(const SimulationState&,bool deep=true);
[[nodiscard]] SiteBuildPreview preview_site_build(const SimulationState&,const BuildOwnedSite&);
[[nodiscard]] SitePowerPreview preview_site_power(const SimulationState&,std::string_view site_id,OrdinalDay);
[[nodiscard]] Cents wholesale_electricity(const SimulationState&,std::string_view source_id,MilliCU work);
[[nodiscard]] SiteAppointment preview_site_appointment(const SimulationState&,const ScheduleSiteAppointment&);
[[nodiscard]] ExecutiveLocation executive_location(const SimulationState&);
[[nodiscard]] CashProjection project_company_cash(const SimulationState&,OrdinalDay,Cents protected_cash=Cents{0});
[[nodiscard]] CashOutlook project_company_cash_outlook(const SimulationState&,OrdinalDay near_through,
    Cents protected_cash,std::span<const ProjectedCashDay>);
[[nodiscard]] Cents infrastructure_monthly_fixed_cost(const SimulationState&,OrdinalDay);
void build_owned_site(SimulationState&,const BuildOwnedSite&,Cents protected_cash);
void order_owned_hardware(SimulationState&,const OrderOwnedHardware&,Cents protected_cash);
[[nodiscard]] HardwareReplacementTerms hardware_replacement_terms(const SimulationState&,std::string_view incumbent,std::string_view family);
void order_hardware_replacement(SimulationState&,std::string_view incumbent,std::string_view family,std::string id,Cents protected_cash);
[[nodiscard]] std::string hardware_retirement_wait(const SimulationState&,std::string_view incumbent,std::string_view replacement);
void hire_site_operators(SimulationState&,const HireSiteOperators&,Cents protected_cash);
void set_site_power_limit(SimulationState&,const SetSitePowerLimit&);
void retire_owned_hardware(SimulationState&,std::string_view);
void close_owned_site(SimulationState&,std::string_view);
void schedule_site_appointment(SimulationState&,const ScheduleSiteAppointment&,Cents protected_cash);
void cancel_site_appointment(SimulationState&,std::string_view);
void notice_initial_lease_termination(SimulationState&);
void prepare_infrastructure(SimulationState&);
void close_infrastructure_day(SimulationState&);
}
