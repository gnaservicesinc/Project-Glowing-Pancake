#pragma once
#include <pgp/core/simulation.hpp>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <tuple>
#include <utility>
namespace pgp::core::infrastructure_detail {
inline constexpr std::size_t maximum_hardware_orders=200,maximum_hires=400,maximum_appointments=512,maximum_notices=4096,maximum_months_per_site=6000;
[[noreturn]] inline void invalid(const char* message) {throw std::invalid_argument(message);}
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    if (it==rows.end()) throw std::invalid_argument("The infrastructure record is unavailable: "+std::string(id));return *it;
}
inline const InfrastructureState& infrastructure(const SimulationState& s) {
    if (!s.infrastructure || !s.accounts || !s.capacity) invalid("This scenario has no owned infrastructure rules");return *s.infrastructure;
}
inline InfrastructureState& infrastructure(SimulationState& s) {return const_cast<InfrastructureState&>(infrastructure(std::as_const(s)));}
inline const OwnedSite& site(const SimulationState& s,std::string_view id) {return find(infrastructure(s).sites,id);}
inline OwnedSite& site(SimulationState& s,std::string_view id) {return const_cast<OwnedSite&>(site(std::as_const(s),id));}
inline const StateSiteProfile& profile(const SimulationState& s,const OwnedSite& owned) {return find(infrastructure(s).catalog.states,owned.state_id);}
inline const HardwareFamily& family(const SimulationState& s,const HardwareOrder& order) {return find(infrastructure(s).catalog.hardware,order.family_id);}
inline OrdinalDay first_of_month(OrdinalDay day) {const auto c=from_ordinal(day);return to_ordinal({c.year,c.month,1});}
inline bool month_end(OrdinalDay day) {const auto c=from_ordinal(day);return c.day==days_in_month(c.year,c.month);}
inline bool before_end(OrdinalDay day,const std::optional<OrdinalDay>& end) {return !end || day<*end;}
inline bool identifier(std::string_view text) {
    return !text.empty() && text.size()<=64 && std::all_of(text.begin(),text.end(),[](unsigned char c){return (c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='.' || c=='_' || c=='-';});
}
void add_notice(SimulationState&,std::string id,std::string kind,std::string subject,OrdinalDay);
void pay_asset(SimulationState&,Cents,Cents protected_cash,std::string key,std::string cause);
void pay_expense(SimulationState&,Cents,Cents protected_cash,std::string key,std::string cause);
[[nodiscard]] Cents prorated_day(Cents monthly,OrdinalDay starts,OrdinalDay day,std::optional<OrdinalDay> ends={});
[[nodiscard]] Cents depreciation_through(Cents cost,OrdinalDay starts,OrdinalDay through,Days life);
[[nodiscard]] std::uint32_t operators_on(const SimulationState&,const OwnedSite&,OrdinalDay);
[[nodiscard]] std::uint32_t pue(const StateSiteProfile&,CoolingMode);
[[nodiscard]] std::uint64_t facility_limit(const StateSiteProfile&,const OwnedSite&,std::uint64_t budget);
[[nodiscard]] SitePowerPreview power_plan(const SimulationState&,const OwnedSite&,OrdinalDay,bool commissioning=false);
[[nodiscard]] Cents fixed_cost_day(const SimulationState&,const OwnedSite&,OrdinalDay);
[[nodiscard]] Cents monthly_fixed_cost(const SimulationState&,const OwnedSite&,OrdinalDay);
[[nodiscard]] std::vector<MeteredProjection> forecast_terms(const SimulationState&);
struct MeterSample {std::uint64_t wh=0,water=0,emissions=0;Cents electricity{0},water_charge{0};};
[[nodiscard]] MeterSample meter_site(const SimulationState&,const OwnedSite&,OrdinalDay,const CapacityDay&,std::vector<HardwareOrder>&,OwnedSite&);
void prepare_site_appointments(SimulationState&);
void finish_site_inspections(SimulationState&);
}
