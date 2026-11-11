#include "romance_json.hpp"
namespace pgp::persistence::detail {
namespace {
Json date(std::optional<core::OrdinalDay> d){return d?Json(d->value()):Json(nullptr);}
std::optional<core::OrdinalDay> optional_day(const Json& j){return j.is_null()?std::nullopt:std::optional{core::OrdinalDay{i64(j,"romance date")}};}
Json bond_json(const core::RomanceBond& b) {
    return {{"id",b.id},{"first",b.first},{"second",b.second},{"started_on",b.started_on.value()},{"last_time_together",b.last_time_together.value()},
        {"engaged_on",date(b.engaged_on)},{"married_on",date(b.married_on)},{"ended_on",date(b.ended_on)},{"strained_on",date(b.strained_on)},{"last_proposal",date(b.last_proposal)},
        {"affection",b.affection},{"dates",b.dates},{"known",b.known},{"memory",b.memory},{"end_reason",b.end_reason}};
}
core::RomanceBond parse_bond(const Json& j) {
    fields(j,"romantic relationship",{"id","first","second","started_on","last_time_together","engaged_on","married_on","ended_on","strained_on","last_proposal","affection","dates","known","memory","end_reason"});
    core::RomanceBond b;b.id=static_cast<std::uint32_t>(u64(j.at("id"),"relationship id",core::romance_history_limit));
    b.first=text(j.at("first"),"first person",160);b.second=text(j.at("second"),"second person",160);
    b.started_on=core::OrdinalDay{i64(j.at("started_on"),"started on")};b.last_time_together=core::OrdinalDay{i64(j.at("last_time_together"),"time together")};
    b.engaged_on=optional_day(j.at("engaged_on"));b.married_on=optional_day(j.at("married_on"));b.ended_on=optional_day(j.at("ended_on"));b.strained_on=optional_day(j.at("strained_on"));b.last_proposal=optional_day(j.at("last_proposal"));
    b.affection=static_cast<int>(u64(j.at("affection"),"affection",1000));b.dates=static_cast<int>(u64(j.at("dates"),"dates",10000));b.known=boolean(j.at("known"),"known");b.memory=text(j.at("memory"),"memory",256);b.end_reason=text(j.at("end_reason"),"end reason",256,true);return b;
}
}
Json romantic_profile_json(const core::RomanticProfile& p) {
    return {{"born_on",p.born_on.value()},{"interested",p.preferences.interested},{"public_orientation",p.preferences.public_orientation},{"genders",p.preferences.genders},{"minimum_friendship",p.preferences.minimum_friendship}};
}
core::RomanticProfile parse_romantic_profile(const Json& j) {
    fields(j,"romantic identity",{"born_on","interested","public_orientation","genders","minimum_friendship"});core::RomanticProfile p;
    p.born_on=core::OrdinalDay{i64(j.at("born_on"),"birth date")};p.preferences.interested=boolean(j.at("interested"),"interested");p.preferences.public_orientation=boolean(j.at("public_orientation"),"public orientation");
    array(j.at("genders"),"romantic preferences",3);for(const auto& gender:j.at("genders"))p.preferences.genders.push_back(text(gender,"gender",32));
    p.preferences.minimum_friendship=static_cast<int>(u64(j.at("minimum_friendship"),"minimum friendship",1000));return p;
}
Json romance_json(const core::RomanceState& s) {
    Json active=Json::array(),history=Json::array(),replies=Json::array();for(const auto& [id,b]:s.active){(void)id;active.push_back(bond_json(b));}for(const auto& b:s.history)history.push_back(bond_json(b));
    for(const auto& r:s.replies)replies.push_back({{"person",r.person},{"action",r.action},{"response",r.response},{"day",r.day.value()},{"relationship",r.relationship},{"accepted",r.accepted}});
    return {{"founder",romantic_profile_json(s.founder)},{"stream",s.stream},{"next_id",s.next_id},{"next_review",s.next_review.value()},{"last_action",date(s.last_action)},{"known_identities",s.known_identities},{"active",std::move(active)},{"history",std::move(history)},{"replies",std::move(replies)}};
}
core::RomanceState parse_romance(const Json& j) {
    fields(j,"romance",{"founder","stream","next_id","next_review","last_action","known_identities","active","history","replies"});core::RomanceState s;
    s.founder=parse_romantic_profile(j.at("founder"));s.stream=u64(j.at("stream"),"romance stream");s.next_id=static_cast<std::uint32_t>(u64(j.at("next_id"),"next relationship",core::romance_history_limit+1));s.next_review=core::OrdinalDay{i64(j.at("next_review"),"next review")};s.last_action=optional_day(j.at("last_action"));
    array(j.at("known_identities"),"shared identities",8192);for(const auto& id:j.at("known_identities"))if(!s.known_identities.insert(text(id,"shared identity",160)).second)invalid("romance","Duplicate shared identity");
    array(j.at("active"),"active relationships",4096);for(const auto& row:j.at("active")){auto b=parse_bond(row);if(!s.active.emplace(b.id,b).second)invalid("romance","Duplicate active relationship");}
    array(j.at("history"),"romance history",core::romance_history_limit);for(const auto& row:j.at("history"))s.history.push_back(parse_bond(row));
    array(j.at("replies"),"romantic replies",core::romance_history_limit);for(const auto& row:j.at("replies")){
        fields(row,"romantic reply",{"person","action","response","day","relationship","accepted"});s.replies.push_back({text(row.at("person"),"person",160),text(row.at("action"),"action",24),text(row.at("response"),"response",256),core::OrdinalDay{i64(row.at("day"),"reply date")},static_cast<std::uint32_t>(u64(row.at("relationship"),"relationship",core::romance_history_limit)),boolean(row.at("accepted"),"accepted")});
    }return s;
}
}
