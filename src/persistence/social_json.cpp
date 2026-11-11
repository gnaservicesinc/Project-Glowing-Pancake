#include "social_json.hpp"
namespace pgp::persistence::detail {
Json social_json(const core::SocialState& s) {
    Json links=Json::array();for(const auto& r:s.links)links.push_back({{"first",r.first},{"second",r.second},{"affinity",r.affinity},{"encounters",r.encounters},
        {"met_on",r.met_on.value()},{"last_contact",r.last_contact.value()},{"last_invited",r.last_invited?Json(r.last_invited->value()):Json(nullptr)},{"memory",r.memory},{"known",r.known}});
    Json result={{"industry_stream",s.industry_stream},{"next_industry_meeting",s.next_industry_meeting.value()},{"referrals",Json::object()}, {"stream",s.stream},{"next_meeting",s.next_meeting.value()},{"last_outing",s.last_outing?Json(s.last_outing->value()):Json(nullptr)},
        {"founder_traits",s.founder_traits},{"links",links},{"known_traits",s.known_traits}};
    for(const auto& [id,day]:s.referrals)result["referrals"][id]=day.value();
    return result;
}
core::SocialState parse_social(const Json& j) {
    fields(j,"social",{"industry_stream","next_industry_meeting","referrals","stream","next_meeting","last_outing","founder_traits","links","known_traits"});core::SocialState s;
    s.industry_stream=u64(j.at("industry_stream"),"industry stream");s.next_industry_meeting=core::OrdinalDay{i64(j.at("next_industry_meeting"),"next industry meeting")};
    const auto& referrals=j.at("referrals");if(!referrals.is_object() || referrals.size()>8192)invalid("referrals","Too many referrals");
    for(const auto& [id,day]:referrals.items())s.referrals.emplace(text(Json(id),"referrer",160),core::OrdinalDay{i64(day,"referred on")});
    s.stream=u64(j.at("stream"),"social stream");s.next_meeting=core::OrdinalDay{i64(j.at("next_meeting"),"next meeting")};
    if(!j.at("last_outing").is_null())s.last_outing=core::OrdinalDay{i64(j.at("last_outing"),"last outing")};
    array(j.at("founder_traits"),"founder traits",16);for(const auto& t:j.at("founder_traits"))s.founder_traits.push_back(text(t,"trait",80));
    array(j.at("links"),"relationships",8192);for(const auto& r:j.at("links")) {
        fields(r,"relationship",{"first","second","affinity","encounters","met_on","last_contact","last_invited","memory","known"});
        s.links.push_back({text(r.at("first"),"first person",160),text(r.at("second"),"second person",160),static_cast<int>(u64(r.at("affinity"),"affinity",1000)),
            static_cast<int>(u64(r.at("encounters"),"encounters",100000)),core::OrdinalDay{i64(r.at("met_on"),"met")},core::OrdinalDay{i64(r.at("last_contact"),"last contact")},
            r.at("last_invited").is_null()?std::nullopt:std::optional{core::OrdinalDay{i64(r.at("last_invited"),"last invited")}},text(r.at("memory"),"memory",160),boolean(r.at("known"),"known")});
    }
    const auto& known=j.at("known_traits");if(!known.is_object() || known.size()>8192)invalid("known traits","Too many people");
    for(const auto& [id,traits]:known.items()) {array(traits,"known traits",16);text(Json(id),"person",160);for(const auto& t:traits)s.known_traits[id].push_back(text(t,"trait",80));}
    return s;
}
}
