#include "social_json.hpp"
#include <pgp/core/people.hpp>
namespace pgp::persistence::detail {
namespace {
Json date(std::optional<core::OrdinalDay> day){return day?Json(day->value()):Json(nullptr);}
std::optional<core::OrdinalDay> optional_date(const Json& day){return day.is_null()?std::nullopt:std::optional{core::OrdinalDay{i64(day,"mission concern date")}};}

Json mission_json(const core::MissionConflictState& state) {
    Json concerns=Json::object();
    for(const auto& [person,c]:state.concerns)
        concerns[person]={{"previous",c.previous_mission},{"next",c.new_mission},{"raised",c.raised_on.value()},{"affinity",c.previous_affinity},
            {"rivalry",date(c.rivalry_on)},{"heard",date(c.heard_on)},{"settled",date(c.settled_on)},{"mediator",c.mediator},{"frenemy",c.frenemy}};
    return {{"mission",state.mission},{"since",state.committed_on.value()},{"concerns",std::move(concerns)}};
}

core::MissionConflictState parse_mission(const Json& j) {
    fields(j,"mission",{"mission","since","concerns"});
    core::MissionConflictState state{text(j.at("mission"),"mission",24),core::OrdinalDay{i64(j.at("since"),"commitment date")},{}};
    const auto& concerns=j.at("concerns");
    if(!concerns.is_object() || concerns.size()>256)invalid("concerns","Too many mission concerns");
    for(const auto& [person,c]:concerns.items()) {
        fields(c,"mission concern",{"previous","next","raised","affinity","rivalry","heard","settled","mediator","frenemy"});
        state.concerns.emplace(text(Json(person),"employee",64),core::MissionConcern{text(c.at("previous"),"previous mission",24),text(c.at("next"),"new mission",24),
            core::OrdinalDay{i64(c.at("raised"),"raised on")},static_cast<int>(u64(c.at("affinity"),"previous affinity",1000)),optional_date(c.at("rivalry")),
            optional_date(c.at("heard")),optional_date(c.at("settled")),text(c.at("mediator"),"mediator",64,true),boolean(c.at("frenemy"),"frenemy")});
    }
    return state;
}
}
Json social_json(const core::SocialState& s) {
    Json links=Json::array();for(const auto& r:s.links)links.push_back({{"first",r.first},{"second",r.second},{"affinity",r.affinity},{"encounters",r.encounters},
        {"met_on",r.met_on.value()},{"last_contact",r.last_contact.value()},{"last_invited",r.last_invited?Json(r.last_invited->value()):Json(nullptr)},{"memory",r.memory},{"known",r.known}});
    Json result={{"industry_stream",s.industry_stream},{"next_industry_meeting",s.next_industry_meeting.value()},{"referrals",Json::object()}, {"stream",s.stream},{"next_meeting",s.next_meeting.value()},{"last_outing",s.last_outing?Json(s.last_outing->value()):Json(nullptr)},
        {"founder_traits",s.founder_traits},{"links",links},{"known_traits",s.known_traits}};
    for(const auto& [id,day]:s.referrals)result["referrals"][id]=day.value();
    if(!s.hostile_peers.empty())result["hostile_peers"]=s.hostile_peers;
    if(s.mission_conflict)result["mission_conflict"]=mission_json(*s.mission_conflict);
    return result;
}
core::SocialState parse_social(const Json& j) {
    fields(j,"social",{"industry_stream","next_industry_meeting","referrals","stream","next_meeting","last_outing","founder_traits","links","known_traits"},{"mission_conflict","hostile_peers"});core::SocialState s;
    if(j.contains("mission_conflict"))s.mission_conflict=parse_mission(j.at("mission_conflict"));
    if(j.contains("hostile_peers")){array(j.at("hostile_peers"),"hostile peers",64);for(const auto& id:j.at("hostile_peers"))s.hostile_peers.push_back(text(id,"hostile peer",160));}
    s.industry_stream=u64(j.at("industry_stream"),"industry stream");s.next_industry_meeting=core::OrdinalDay{i64(j.at("next_industry_meeting"),"next industry meeting")};
    const auto& referrals=j.at("referrals");if(!referrals.is_object() || referrals.size()>8192)invalid("referrals","Too many referrals");
    for(const auto& [id,day]:referrals.items())s.referrals.emplace(text(Json(id),"referrer",160),core::OrdinalDay{i64(day,"referred on")});
    s.stream=u64(j.at("stream"),"social stream");s.next_meeting=core::OrdinalDay{i64(j.at("next_meeting"),"next meeting")};
    if(!j.at("last_outing").is_null())s.last_outing=core::OrdinalDay{i64(j.at("last_outing"),"last outing")};
    array(j.at("founder_traits"),"founder traits",core::person_trait_limit);for(const auto& t:j.at("founder_traits"))s.founder_traits.push_back(text(t,"trait",80));
    array(j.at("links"),"relationships",8192);for(const auto& r:j.at("links")) {
        fields(r,"relationship",{"first","second","affinity","encounters","met_on","last_contact","last_invited","memory","known"});
        s.links.push_back({text(r.at("first"),"first person",160),text(r.at("second"),"second person",160),static_cast<int>(u64(r.at("affinity"),"affinity",1000)),
            static_cast<int>(u64(r.at("encounters"),"encounters",100000)),core::OrdinalDay{i64(r.at("met_on"),"met")},core::OrdinalDay{i64(r.at("last_contact"),"last contact")},
            r.at("last_invited").is_null()?std::nullopt:std::optional{core::OrdinalDay{i64(r.at("last_invited"),"last invited")}},text(r.at("memory"),"memory",160),boolean(r.at("known"),"known")});
    }
    const auto& known=j.at("known_traits");if(!known.is_object() || known.size()>8192)invalid("known traits","Too many people");
    for(const auto& [id,traits]:known.items()) {array(traits,"known traits",core::person_trait_limit);text(Json(id),"person",160);for(const auto& t:traits)s.known_traits[id].push_back(text(t,"trait",80));}
    return s;
}
}
