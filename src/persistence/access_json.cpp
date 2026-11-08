#include "access_json.hpp"
namespace pgp::persistence::detail {
Json access_json(const core::AccessState& a) {
    auto offices=Json::array();for(const auto& o:a.offices) offices.push_back({{"id",o.id},{"offer",o.offer},{"ordered_on",o.ordered_on.value()},{"ready_on",o.ready_on.value()},{"closed_on",o.closed_on?Json(o.closed_on->value()):Json(nullptr)}});
    return {{"connected_background",a.connected_background},{"national",a.national?Json{{"day",a.national->day.value()},{"cause",a.national->cause},{"staff_additions",a.national->staff_additions},{"staff_departures",a.national->staff_departures},{"postings",a.national->postings},{"publications",a.national->publications}}:Json(nullptr)},{"offices",std::move(offices)},{"first_overseas_opened",a.first_overseas_opened?Json(a.first_overseas_opened->value()):Json(nullptr)}};
}
core::AccessState parse_access(const Json& j) {
    fields(j,"access",{"connected_background","national","offices","first_overseas_opened"});core::AccessState a;a.connected_background=boolean(j.at("connected_background"),"connected background");
    if(!j.at("national").is_null()) {const auto& n=j.at("national");fields(n,"national access",{"day","cause","staff_additions","staff_departures","postings","publications"});a.national=core::NationalAccess{core::OrdinalDay{i64(n.at("day"),"national access day")},text(n.at("cause"),"national access cause",32),u64(n.at("staff_additions"),"staff additions",8192),u64(n.at("staff_departures"),"staff departures",8192),u64(n.at("postings"),"revenue postings",1000000),u64(n.at("publications"),"research publications",4096)};}
    if(!j.at("first_overseas_opened").is_null()) a.first_overseas_opened=core::OrdinalDay{i64(j.at("first_overseas_opened"),"first overseas opening")};
    array(j.at("offices"),"overseas offices",64);for(const auto& o:j.at("offices")) {
        fields(o,"overseas office",{"id","offer","ordered_on","ready_on","closed_on"});core::LiaisonOffice office{u64(o.at("id"),"office id",64),text(o.at("offer"),"office offer",48),core::OrdinalDay{i64(o.at("ordered_on"),"ordered_on")},core::OrdinalDay{i64(o.at("ready_on"),"ready_on")},{}};
        if(!o.at("closed_on").is_null()) office.closed_on=core::OrdinalDay{i64(o.at("closed_on"),"closed_on")};a.offices.push_back(std::move(office));
    }return a;
}
}
