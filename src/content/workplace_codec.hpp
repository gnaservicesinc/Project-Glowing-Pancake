#pragma once
#include "people_codec.hpp"
#include "devices_codec.hpp"
#include "access_codec.hpp"
#include <pgp/core/workplace.hpp>
namespace pgp::content::workplace_codec {
inline core::WorkplaceRules decode(nlohmann::json j) {
    std::optional<core::AccessRules> access;
    if(j.contains("access")) {access=access_codec::decode(j.at("access"));j.erase("access");}
    std::optional<core::DeviceRules> devices;
    if(j.contains("devices")) {devices=devices_codec::decode(j.at("devices"));j.erase("devices");}
    people_codec::fields(j,{"home_seats","shared_seats","leased_seats","maximum_rooms","maximum_blueprints","rooms"});
    core::WorkplaceRules r;r.devices=std::move(devices);r.access=std::move(access);r.home_seats=people_codec::count(j.at("home_seats"));r.shared_seats=people_codec::count(j.at("shared_seats"));
    r.leased_seats=people_codec::count(j.at("leased_seats"));r.maximum_rooms=people_codec::count(j.at("maximum_rooms"));r.maximum_blueprints=people_codec::count(j.at("maximum_blueprints"));
    if(!j.at("rooms").is_array() || j.at("rooms").size()>8) throw std::invalid_argument("Room catalog exceeds its limit");
    for(const auto& row:j.at("rooms")) {
        people_codec::fields(row,{"id","name","place","description","construction_cents","monthly_cents","lead_days","seats"});
        r.rooms.push_back({row.at("id").get<std::string>(),row.at("name").get<std::string>(),row.at("place").get<std::string>(),row.at("description").get<std::string>(),
            core::Cents{people_codec::integer(row.at("construction_cents"))},core::Cents{people_codec::integer(row.at("monthly_cents"))},
            core::Days{people_codec::integer(row.at("lead_days"))},people_codec::count(row.at("seats"))});
    }
    core::validate_workplace_rules(r);return r;
}
inline nlohmann::json encode(const core::WorkplaceRules& r) {
    nlohmann::json rooms=nlohmann::json::array();
    for(const auto& room:r.rooms) rooms.push_back({{"id",room.id},{"name",room.name},{"place",room.place},{"description",room.description},
        {"construction_cents",room.construction.value()},{"monthly_cents",room.monthly.value()},{"lead_days",room.lead.value()},{"seats",room.seats}});
    nlohmann::json j{{"home_seats",r.home_seats},{"shared_seats",r.shared_seats},{"leased_seats",r.leased_seats},{"maximum_rooms",r.maximum_rooms},{"maximum_blueprints",r.maximum_blueprints},{"rooms",std::move(rooms)}};
    if(r.devices) j["devices"]=devices_codec::encode(*r.devices);
    if(r.access) j["access"]=access_codec::encode(*r.access);return j;
}
}
