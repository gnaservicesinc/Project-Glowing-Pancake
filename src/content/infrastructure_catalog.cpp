#include <pgp/content/infrastructure_catalog.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace pgp::content {
namespace {
using Json=nlohmann::json;
[[noreturn]] void invalid() {throw std::invalid_argument("Infrastructure catalog has missing, unsupported or invalid typed fields");}
template<class T,class F> void visit(T& row,F&& f) {
    using V=std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<V,core::StateSiteProfile>) {
        f("id",row.id);f("name",row.name);f("travel_zone",row.travel_zone);
        f("construction_base_cents",row.construction_base);f("construction_per_kw_cents",row.construction_per_kw);f("monthly_property_cents",row.monthly_property);f("monthly_overhead_cents",row.monthly_overhead);
        f("operator_signing_cents",row.operator_signing);f("operator_monthly_cents",row.operator_monthly);f("incentive_cap_cents",row.incentive_cap);f("construction_incentive_bp",row.construction_incentive);
        f("build_lead_days",row.build_lead);f("grid_lead_days",row.grid_lead);f("operator_hire_lead_days",row.operator_hire_lead);f("grid_watts",row.grid_watts);f("water_limit_ml_per_day",row.water_limit_ml_per_day);f("water_ml_per_kwh",row.water_ml_per_kwh);
        f("air_pue_milli",row.air_pue_milli);f("water_pue_milli",row.water_pue_milli);f("grid_grams_per_kwh",row.grid_grams_per_kwh);f("electricity_millicents_per_kwh",row.electricity_millicents_per_kwh);f("water_cents_per_1000_liters",row.water_cents_per_1000_liters);f("connectivity_class",row.connectivity_class);
    } else if constexpr (std::is_same_v<V,core::HardwareFamily>) {
        f("id",row.id);f("name",row.name);f("interconnect",row.interconnect);f("purchase_cost_cents",row.purchase_cost);f("monthly_maintenance_cents",row.monthly_maintenance);f("capacity_milli_CU_per_day",row.capacity);
        f("memory_mib",row.memory_mib);f("active_it_watts",row.active_it_watts);f("idle_it_watts",row.idle_it_watts);f("operators",row.operators);f("connectivity_class",row.connectivity_class);f("delivery_lead_days",row.delivery_lead);f("useful_life_days",row.useful_life);f("resale_bp_of_book",row.resale_fraction);f("requires_research",row.requires_research);
    } else if constexpr (std::is_same_v<V,core::TravelRoute>) {
        f("from",row.from);f("to",row.to);f("one_way_days",row.one_way);f("fare_cents",row.fare);
    } else if constexpr (std::is_same_v<V,core::InfrastructureCatalog>) {
        f("map_asset_id",row.map_asset_id);f("map_sha256",row.map_sha256);f("assumptions",row.assumptions);f("states",row.states);f("hardware",row.hardware);f("travel",row.travel);
        f("building_life_days",row.building_life);f("initial_lease_notice_days",row.initial_lease_notice);f("building_resale_bp_of_book",row.building_resale);f("onsite_inspection_cents",row.onsite_inspection);f("remote_inspection_cents",row.remote_inspection);f("delegated_inspection_cents",row.delegated_inspection);
        f("onsite_booking_lead_days",row.onsite_booking_lead);f("remote_booking_lead_days",row.remote_booking_lead);f("delegated_booking_lead_days",row.delegated_booking_lead);f("minimum_site_watts",row.minimum_site_watts);f("maximum_site_watts",row.maximum_site_watts);f("maximum_sites",row.maximum_sites);f("maximum_hardware_per_site",row.maximum_hardware_per_site);f("maximum_operators_per_site",row.maximum_operators_per_site);
    }
}
template<class T> T decode(const Json& j) {
    if constexpr (std::is_same_v<T,std::string>) {
        if (!j.is_string()) invalid();auto s=j.get<std::string>();if (s.empty() || s.size()>4096 || s.find('\0')!=std::string::npos) invalid();return s;
    } else if constexpr (std::is_integral_v<T> || requires(T v) {v.value();}) {
        if (!j.is_number_integer() || (j.is_number_unsigned() && j.get<std::uint64_t>()>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))) invalid();
        const auto value=j.get<std::int64_t>();if constexpr (std::is_integral_v<T>) {if (!std::in_range<T>(value)) invalid();return static_cast<T>(value);}else return T{value};
    } else if constexpr (requires(T v) {v.push_back(typename T::value_type{});}) {
        using Row=typename T::value_type;constexpr std::size_t maximum=std::is_same_v<Row,core::StateSiteProfile>?51:(std::is_same_v<Row,core::TravelRoute>?100:16);
        if (!j.is_array() || j.size()>maximum) invalid();T rows;for (const auto& row:j) rows.push_back(decode<Row>(row));return rows;
    } else {
        if (!j.is_object()) invalid();T result;std::size_t fields=0;
        visit(result,[&](const char* name,auto& field){if (!j.contains(name)) invalid();field=decode<std::remove_cvref_t<decltype(field)>>(j.at(name));++fields;});
        if (j.size()!=fields) invalid();return result;
    }
}
template<class T> Json encode(const T& value) {
    if constexpr (std::is_same_v<T,std::string> || std::is_integral_v<T>) return value;
    else if constexpr (requires {value.value();}) return value.value();
    else if constexpr (requires {value.begin();value.end();}) {auto j=Json::array();for (const auto& row:value) j.push_back(encode(row));return j;}
    else {auto j=Json::object();visit(value,[&](const char* name,const auto& field){j[name]=encode(field);});return j;}
}
}
core::InfrastructureCatalog load_infrastructure_catalog(std::string_view bytes) {
    if (bytes.size()>131072) invalid();std::vector<std::set<std::string>> keys;std::size_t tokens=0;
    const auto j=Json::parse(bytes.begin(),bytes.end(),[&](int depth,Json::parse_event_t event,Json& value) {
        if (depth>16 || ++tokens>32768) invalid();
        if (event==Json::parse_event_t::object_start) keys.emplace_back();
        else if (event==Json::parse_event_t::object_end) keys.pop_back();
        else if (event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second) invalid();
        else if (event==Json::parse_event_t::value && value.is_number_float()) invalid();return true;
    });
    auto catalog=decode<core::InfrastructureCatalog>(j);core::validate_infrastructure_catalog(catalog);return catalog;
}
std::string canonical_infrastructure_catalog(const core::InfrastructureCatalog& c) {core::validate_infrastructure_catalog(c);return encode(c).dump();}
}
