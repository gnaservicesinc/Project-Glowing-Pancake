#include "devices_json.hpp"
namespace pgp::persistence::detail {
Json devices_json(const core::DeviceInventory& inventory) {
    auto rows=Json::array();for(const auto& d:inventory.records) {
        auto services=Json::array();for(const auto& s:d.services) services.push_back({{"ordered_on",s.ordered_on.value()},{"ready_on",s.ready_on.value()}});
        rows.push_back({{"id",d.id},{"offer",d.offer},{"ordered_on",d.ordered_on.value()},{"ready_on",d.ready_on.value()},
            {"included",d.included},{"powered",d.powered},{"retired_on",d.retired_on?Json(d.retired_on->value()):Json(nullptr)},{"services",std::move(services)}});
    }return {{"records",std::move(rows)}};
}
core::DeviceInventory parse_devices(const Json& j) {
    fields(j,"devices",{"records"});array(j.at("records"),"owned devices",128);core::DeviceInventory result;
    for(const auto& row:j.at("records")) {
        fields(row,"device",{"id","offer","ordered_on","ready_on","included","powered","retired_on","services"});
        core::PersonalDevice d{u64(row.at("id"),"device id",128),text(row.at("offer"),"device model",16),
            core::OrdinalDay{i64(row.at("ordered_on"),"ordered_on")},core::OrdinalDay{i64(row.at("ready_on"),"ready_on")},
            boolean(row.at("included"),"included"),boolean(row.at("powered"),"powered"),{}, {}};
        if(!row.at("retired_on").is_null()) d.retired_on=core::OrdinalDay{i64(row.at("retired_on"),"retired_on")};
        array(row.at("services"),"device services",128);for(const auto& s:row.at("services")) {
            fields(s,"service",{"ordered_on","ready_on"});d.services.push_back({core::OrdinalDay{i64(s.at("ordered_on"),"ordered_on")},core::OrdinalDay{i64(s.at("ready_on"),"ready_on")}});
        }result.records.push_back(std::move(d));
    }return result;
}
}
