#include "workplace_json.hpp"
#include "headquarters_json.hpp"
#include "devices_json.hpp"
#include "access_json.hpp"
#include "departments_json.hpp"
namespace pgp::persistence::detail {
Json workplace_json(const core::WorkplaceState& w) {
    Json rooms=Json::array(),blueprints=Json::array(),builds=Json::array();
    for(const auto& room:w.rooms) rooms.push_back({{"id",room.id},{"offer",room.offer},{"ordered_on",room.ordered_on.value()},{"ready_on",room.ready_on.value()}});
    for(const auto& b:w.blueprints) blueprints.push_back({{"id",b.id},{"name",b.name},{"hardware_family",b.hardware_family},{"grid_watts",b.grid_watts},{"operators",b.operators},{"evaporative",b.evaporative}});
    for(const auto& b:w.builds) builds.push_back({{"blueprint",b.blueprint},{"site",b.site},{"ordered_on",b.ordered_on.value()}});
    Json j{{"starts_on",w.starts_on.value()},{"opening_office",w.opening_office},{"nonresearch_founder",w.nonresearch_founder},{"rooms",std::move(rooms)},{"blueprints",std::move(blueprints)},{"builds",std::move(builds)}};
    if(w.headquarters) j["headquarters"]=headquarters_json(*w.headquarters);
    if(w.devices) j["devices"]=devices_json(*w.devices);
    if(w.access) j["access"]=access_json(*w.access);
    if(w.departments) j["departments"]=departments_json(*w.departments);return j;
}
core::WorkplaceState parse_workplace(const Json& j) {
    fields(j,"workplace",{"starts_on","opening_office","nonresearch_founder","rooms","blueprints","builds"},{"devices","access","departments","headquarters"});
    core::WorkplaceState w;w.starts_on=core::OrdinalDay{i64(j.at("starts_on"),"starts_on")};w.opening_office=text(j.at("opening_office"),"opening_office",16);
    w.nonresearch_founder=boolean(j.at("nonresearch_founder"),"nonresearch_founder");
    if(j.contains("headquarters")) w.headquarters=parse_headquarters(j.at("headquarters"));
    if(j.contains("devices")) w.devices=parse_devices(j.at("devices"));
    if(j.contains("access")) w.access=parse_access(j.at("access"));
    if(j.contains("departments")) w.departments=parse_departments(j.at("departments"));
    array(j.at("rooms"),"rooms",24);for(const auto& row:j.at("rooms")) {
        fields(row,"room",{"id","offer","ordered_on","ready_on"});
        w.rooms.push_back({text(row.at("id"),"room id",48),text(row.at("offer"),"room offer",48),core::OrdinalDay{i64(row.at("ordered_on"),"ordered_on")},core::OrdinalDay{i64(row.at("ready_on"),"ready_on")}});
    }
    array(j.at("blueprints"),"blueprints",16);for(const auto& row:j.at("blueprints")) {
        fields(row,"blueprint",{"id","name","hardware_family","grid_watts","operators","evaporative"});
        w.blueprints.push_back({text(row.at("id"),"blueprint id",48),text(row.at("name"),"blueprint name",96),text(row.at("hardware_family"),"hardware family",128),
            u64(row.at("grid_watts"),"grid reservation",1000000),static_cast<std::uint32_t>(u64(row.at("operators"),"operators",32)),boolean(row.at("evaporative"),"evaporative")});
    }
    array(j.at("builds"),"blueprint builds",50);for(const auto& row:j.at("builds")) {
        fields(row,"blueprint build",{"blueprint","site","ordered_on"});
        w.builds.push_back({text(row.at("blueprint"),"blueprint id",48),text(row.at("site"),"site id",128),core::OrdinalDay{i64(row.at("ordered_on"),"ordered_on")}});
    }
    return w;
}
}
