#include "social_json.hpp"
namespace pgp::persistence::detail {
Json social_json(const core::SocialState& s) {
    Json links=Json::array();for(const auto& r:s.links)links.push_back({{"first",r.first},{"second",r.second},{"affinity",r.affinity},{"encounters",r.encounters},
        {"met_on",r.met_on.value()},{"last_contact",r.last_contact.value()},{"last_invited",r.last_invited?Json(r.last_invited->value()):Json(nullptr)},{"memory",r.memory},{"known",r.known}});
    return {{"stream",s.stream},{"next_meeting",s.next_meeting.value()},{"last_outing",s.last_outing?Json(s.last_outing->value()):Json(nullptr)},
        {"founder_traits",s.founder_traits},{"links",links},{"known_traits",s.known_traits}};
}
core::SocialState parse_social(const Json& j) {
    fields(j,"social",{"stream","next_meeting","last_outing","founder_traits","links","known_traits"});core::SocialState s;
    s.stream=u64(j.at("stream"),"social stream");s.next_meeting=core::OrdinalDay{i64(j.at("next_meeting"),"next meeting")};
    if(!j.at("last_outing").is_null())s.last_outing=core::OrdinalDay{i64(j.at("last_outing"),"last outing")};
    array(j.at("founder_traits"),"founder traits",16);for(const auto& t:j.at("founder_traits"))s.founder_traits.push_back(text(t,"trait",80));
    array(j.at("links"),"relationships",8192);for(const auto& r:j.at("links")) {
        fields(r,"relationship",{"first","second","affinity","encounters","met_on","last_contact","last_invited","memory","known"});
        s.links.push_back({text(r.at("first"),"first person",64),text(r.at("second"),"second person",64),static_cast<int>(u64(r.at("affinity"),"affinity",1000)),
            static_cast<int>(u64(r.at("encounters"),"encounters",100000)),core::OrdinalDay{i64(r.at("met_on"),"met")},core::OrdinalDay{i64(r.at("last_contact"),"last contact")},
            r.at("last_invited").is_null()?std::nullopt:std::optional{core::OrdinalDay{i64(r.at("last_invited"),"last invited")}},text(r.at("memory"),"memory",160),boolean(r.at("known"),"known")});
    }
    const auto& known=j.at("known_traits");if(!known.is_object() || known.size()>256)invalid("known traits","Too many people");
    for(const auto& [id,traits]:known.items()) {array(traits,"known traits",16);text(Json(id),"person",64);for(const auto& t:traits)s.known_traits[id].push_back(text(t,"trait",80));}
    return s;
}
}
