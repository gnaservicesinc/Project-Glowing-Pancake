#include "infrastructure_internal.hpp"
#include <set>
namespace pgp::core {
using namespace infrastructure_detail;
void validate_infrastructure_catalog(const InfrastructureCatalog& c) {
    const auto text=[](std::string_view v,std::size_t max) {return !v.empty() && v.size()<=max && std::none_of(v.begin(),v.end(),[](unsigned char ch){return ch<32 || ch==127;});};
    const auto money=[](Cents v,bool positive=false) {return v.value()>=(positive?1:0) && v.value()<=1'000'000'000'000LL;};
    const auto lead=[](Days v,bool positive=false) {return v.value()>=(positive?1:0) && v.value()<=3650;};
    if (!identifier(c.map_asset_id) || c.map_sha256.size()!=64 || !std::all_of(c.map_sha256.begin(),c.map_sha256.end(),[](unsigned char ch){return (ch>='0' && ch<='9') || (ch>='a' && ch<='f');}) ||
        !text(c.assumptions,4096) || c.states.size()!=51 || c.hardware.empty() ||
        c.hardware.size()>infrastructure_hardware_family_limit || c.travel.empty() || c.travel.size()>100 ||
        c.building_life.value()==0 || c.building_life.value()>36524 || !lead(c.initial_lease_notice,true) ||
        !money(c.onsite_inspection,true) || !money(c.remote_inspection,true) || !money(c.delegated_inspection,true) ||
        !lead(c.onsite_booking_lead,true) || !lead(c.remote_booking_lead,true) || !lead(c.delegated_booking_lead,true) ||
        c.minimum_site_watts<1000 || c.maximum_site_watts>1'000'000 || c.maximum_site_watts<c.minimum_site_watts || c.minimum_site_watts%1000!=0 || c.maximum_site_watts%1000!=0 ||
        c.maximum_sites==0 || c.maximum_sites>50 || c.maximum_hardware_per_site==0 || c.maximum_hardware_per_site>4 || c.maximum_operators_per_site==0 || c.maximum_operators_per_site>32) invalid("Invalid bounded domestic infrastructure catalog or map identity");
    const std::set<std::string> state_ids={"US-AL","US-AK","US-AZ","US-AR","US-CA","US-CO","US-CT","US-DE","US-DC","US-FL","US-GA","US-HI","US-ID","US-IL","US-IN","US-IA","US-KS","US-KY","US-LA","US-ME","US-MD","US-MA","US-MI","US-MN","US-MS","US-MO","US-MT","US-NE","US-NV","US-NH","US-NJ","US-NM","US-NY","US-NC","US-ND","US-OH","US-OK","US-OR","US-PA","US-RI","US-SC","US-SD","US-TN","US-TX","US-UT","US-VT","US-VA","US-WA","US-WV","US-WI","US-WY"};
    std::set<std::string> ids,zones;
    for (const auto& state:c.states) {
        if (!state_ids.contains(state.id) || !ids.insert(state.id).second || !text(state.name,96) || !identifier(state.travel_zone) ||
            !money(state.construction_base,true) || !money(state.construction_per_kw,true) || !money(state.monthly_property) || !money(state.monthly_overhead) ||
            !money(state.operator_signing) || !money(state.operator_monthly,true) || !money(state.incentive_cap) || state.construction_incentive.value()>9000 ||
            !lead(state.build_lead,true) || !lead(state.grid_lead,true) || !lead(state.operator_hire_lead,true) ||
            state.grid_watts<c.minimum_site_watts || state.grid_watts>50'000'000 || state.grid_watts%1000!=0 || state.water_limit_ml_per_day==0 || state.water_limit_ml_per_day>1'000'000'000'000ULL ||
            state.water_ml_per_kwh==0 || state.water_ml_per_kwh>100'000 || state.air_pue_milli<1000 || state.air_pue_milli>3000 || state.water_pue_milli<1000 || state.water_pue_milli>3000 ||
            state.grid_grams_per_kwh>10'000 || state.electricity_millicents_per_kwh==0 || state.electricity_millicents_per_kwh>1'000'000 || state.water_cents_per_1000_liters>100'000 || state.connectivity_class>16)
            invalid("Invalid authored state profile, physical bound or construction/operator term");
        zones.insert(state.travel_zone);
    }
    if (zones.size()>10) invalid("Too many authored travel zones");
    ids.clear();for (const auto& h:c.hardware) {
        if (!identifier(h.id) || !ids.insert(h.id).second || !text(h.name,96) || !identifier(h.interconnect) || !money(h.purchase_cost,true) || !money(h.monthly_maintenance) ||
            h.capacity.value()==0 || h.capacity.value()>1'000'000'000'000LL || h.memory_mib==0 || h.memory_mib>1'048'576'000 || h.active_it_watts==0 || h.active_it_watts>1'000'000 || h.idle_it_watts>h.active_it_watts ||
            h.operators==0 || h.operators>c.maximum_operators_per_site || h.connectivity_class>16 || !lead(h.delivery_lead,true) || h.useful_life.value()==0 || h.useful_life.value()>36524 || h.requires_research.size()>16)
            invalid("Invalid hardware CU reference, compatibility, paid term or bounded power");
        std::set<std::string> research;for (const auto& id:h.requires_research) if (!identifier(id) || !research.insert(id).second) invalid("Hardware integration prerequisites must be unique typed research IDs");
    }
    std::set<std::pair<std::string,std::string>> routes;
    for (const auto& route:c.travel) if (!zones.contains(route.from) || !zones.contains(route.to) || !routes.emplace(route.from,route.to).second || route.one_way.value()>7 || !money(route.fare)) invalid("Invalid or duplicate authored domestic travel route");
    for (const auto& from:zones) for (const auto& to:zones) if (!routes.contains({from,to})) invalid("Every domestic zone pair needs an explicit travel time and fare");
}
}
