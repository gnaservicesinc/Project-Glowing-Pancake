#pragma once
#include "people_codec.hpp"
#include <pgp/core/devices.hpp>
namespace pgp::content::devices_codec {
inline core::DeviceRules decode(const nlohmann::json& j) {
    people_codec::fields(j,{"offers"});const auto& offers=j.at("offers");
    if(!offers.is_array() || offers.size()>4) throw std::invalid_argument("Invalid device offers");
    core::DeviceRules r;
    for(const auto& row:offers) {
        people_codec::fields(row,{"id","name","purchase_cents","service_cents","delivery_days","service_days","portable","included_offices","apps"});
        if(!row.at("portable").is_boolean()) throw std::invalid_argument("Device portability requires a boolean");
        core::DeviceOffer d{row.at("id").get<std::string>(),row.at("name").get<std::string>(),
            core::Cents{people_codec::integer(row.at("purchase_cents"))},core::Cents{people_codec::integer(row.at("service_cents"))},
            core::Days{people_codec::integer(row.at("delivery_days"))},core::Days{people_codec::integer(row.at("service_days"))},row.at("portable").get<bool>(),{}, {}};
        for(const auto* key:{"included_offices","apps"}) {
            const auto& list=row.at(key);if(!list.is_array() || list.size()>16) throw std::invalid_argument("Invalid device list");
            auto& to=std::string_view{key}=="apps"?d.apps:d.included_offices;
            for(const auto& value:list) to.push_back(value.get<std::string>());
        }
        r.offers.push_back(std::move(d));
    }
    core::validate_device_rules(r);return r;
}
inline nlohmann::json encode(const core::DeviceRules& r) {
    auto offers=nlohmann::json::array();
    for(const auto& d:r.offers) offers.push_back({{"id",d.id},{"name",d.name},{"purchase_cents",d.purchase.value()},{"service_cents",d.service.value()},
        {"delivery_days",d.delivery.value()},{"service_days",d.service_lead.value()},{"portable",d.portable},{"included_offices",d.included_offices},{"apps",d.apps}});
    return {{"offers",std::move(offers)}};
}
}
