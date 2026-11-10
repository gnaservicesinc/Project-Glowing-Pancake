#pragma once
#include <pgp/core/headquarters.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace pgp::content::headquarters_codec {
using Json=nlohmann::json;
[[noreturn]] inline void invalid() {throw std::invalid_argument("Headquarters catalog has unsupported or invalid typed fields.");}
template<class T,class F> void visit(T& r,F&& f) {
    using V=std::remove_cvref_t<T>;
    if constexpr(std::is_same_v<V,core::HQRoom>) {f("x",r.x);f("y",r.y);f("width",r.width);f("depth",r.depth);}
    else if constexpr(std::is_same_v<V,core::HQMarket>) {
        f("id",r.id);f("state",r.state);f("city",r.city);f("agent",r.agent);f("landlord",r.landlord);f("street",r.street);
        f("rent_cents_per_square_metre",r.rent_per_cell);f("land_cents_per_square_metre",r.land_per_cell);f("monthly_land_tax_cents_per_square_metre",r.monthly_land_tax_per_cell);
        f("deposit_months",r.deposit_months);f("minimum_lease_days",r.minimum_lease_days);f("early_exit_months",r.early_exit_months);
    } else if constexpr(std::is_same_v<V,core::HQPlan>) {f("id",r.id);f("name",r.name);f("width",r.width);f("depth",r.depth);f("rooms",r.rooms);}
    else if constexpr(std::is_same_v<V,core::HQHomePlan>) {f("family",r.family);f("plan",r.plan);f("room_names",r.room_names);}
    else if constexpr(std::is_same_v<V,core::HQEquipmentOffer>) {
        f("id",r.id);f("name",r.name);f("supplier",r.supplier);f("kind",r.kind);f("price_cents",r.price);f("delivery_days",r.delivery);f("width",r.width);f("depth",r.depth);f("compute_milli_cu_per_day",r.compute);f("watts",r.watts);f("memory_mib",r.memory_mib);
    } else if constexpr(std::is_same_v<V,core::HQContractor>) {
        f("id",r.id);f("name",r.name);f("cost_cents_per_square_metre",r.cost_per_cell);f("base_days",r.base_days);f("square_metres_per_day",r.cells_per_day);
        f("delay_bp",r.delay_bp);f("overrun_bp",r.overrun_bp);f("maximum_delay_days",r.maximum_delay_days);f("maximum_overrun_bp",r.maximum_overrun_bp);
    } else if constexpr(std::is_same_v<V,core::HeadquartersRules>) {
        f("home_width",r.home_width);f("home_depth",r.home_depth);f("maximum_edge",r.maximum_edge);f("maximum_items",r.maximum_items);f("maximum_properties",r.maximum_properties);f("building_tax_annual_bp",r.building_tax_annual_bp);
        f("markets",r.markets);f("plans",r.plans);f("equipment",r.equipment);f("contractors",r.contractors);
        f("homes",r.homes,true);
    }
}
template<class T> T read(const Json& j) {
    if constexpr(std::is_same_v<T,std::string>) {if(!j.is_string()) invalid();auto v=j.get<std::string>();if(v.size()>512 || v.find('\0')!=v.npos) invalid();return v;}
    else if constexpr(std::is_integral_v<T>) {
        if(!j.is_number_integer()) invalid();
        if constexpr(std::is_unsigned_v<T>) {if(j<0 || j>std::numeric_limits<T>::max()) invalid();}
        else if(j<std::numeric_limits<T>::min() || j>std::numeric_limits<T>::max()) invalid();
        return j.get<T>();
    } else if constexpr(requires(T v){v.value();}) {return T{read<std::int64_t>(j)};}
    else if constexpr(requires(T v){v.push_back(typename T::value_type{});}) {
        if(!j.is_array() || j.size()>128) invalid();T rows;for(const auto& value:j) rows.push_back(read<typename T::value_type>(value));return rows;
    } else {
        if(!j.is_object()) invalid();T result;std::size_t count=0;
        visit(result,[&](const char* name,auto& value,bool optional=false){if(optional && !j.contains(name)) return;if(!j.contains(name)) invalid();value=read<std::remove_cvref_t<decltype(value)>>(j.at(name));++count;});
        if(j.size()!=count) invalid();return result;
    }
}
template<class T> Json write(const T& value) {
    if constexpr(std::is_same_v<T,std::string> || std::is_integral_v<T>) return value;
    else if constexpr(requires{value.value();}) return value.value();
    else if constexpr(requires{value.begin();value.end();}) {auto j=Json::array();for(const auto& row:value) j.push_back(write(row));return j;}
    else {auto j=Json::object();visit(value,[&](const char* name,const auto& field,bool optional=false){if constexpr(requires{field.empty();}) {if(optional && field.empty())return;}j[name]=write(field);});return j;}
}
inline core::HeadquartersRules decode(const Json& j) {auto r=read<core::HeadquartersRules>(j);core::validate_headquarters_rules(r);return r;}
inline Json encode(const core::HeadquartersRules& r) {return write(r);}
}
