#pragma once
#include "people_codec.hpp"
#include <pgp/core/access.hpp>
namespace pgp::content::access_codec {
inline core::AccessRules decode(const nlohmann::json& j) {
    people_codec::fields(j,{"productive_staff","earned_revenue_cents","publications","offices"});
    core::AccessRules r;r.productive_staff=people_codec::count(j.at("productive_staff"));r.publications=people_codec::count(j.at("publications"));r.earned_revenue=core::Cents{people_codec::integer(j.at("earned_revenue_cents"))};
    if(!j.at("offices").is_array() || j.at("offices").size()>7) throw std::invalid_argument("Overseas office catalog exceeds its bound");
    for(const auto& o:j.at("offices")) {
        people_codec::fields(o,{"id","name","region","setup_cents","monthly_cents","lead_days","people"});
        r.offices.push_back({o.at("id").get<std::string>(),o.at("name").get<std::string>(),o.at("region").get<std::string>(),
            core::Cents{people_codec::integer(o.at("setup_cents"))},core::Cents{people_codec::integer(o.at("monthly_cents"))},core::Days{people_codec::integer(o.at("lead_days"))},people_codec::count(o.at("people"))});
    }core::validate_access_rules(r);return r;
}
inline nlohmann::json encode(const core::AccessRules& r) {
    auto offices=nlohmann::json::array();for(const auto& o:r.offices) offices.push_back({{"id",o.id},{"name",o.name},{"region",o.region},{"setup_cents",o.setup.value()},{"monthly_cents",o.monthly.value()},{"lead_days",o.lead.value()},{"people",o.people}});
    return {{"productive_staff",r.productive_staff},{"earned_revenue_cents",r.earned_revenue.value()},{"publications",r.publications},{"offices",std::move(offices)}};
}
}
