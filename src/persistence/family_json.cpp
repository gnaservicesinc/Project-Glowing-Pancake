#include "family_json.hpp"
#include "people_json.hpp"
namespace pgp::persistence::detail {
namespace {
Json date(std::optional<core::OrdinalDay> d){return d?Json(d->value()):Json(nullptr);}
core::OrdinalDay day(const Json& j){return core::OrdinalDay{i64(j,"family date")};}
std::optional<core::OrdinalDay> optional_day(const Json& j){return j.is_null()?std::nullopt:std::optional{day(j)};}
Json plan_json(const core::FamilyPlan& p){return {{"id",p.id},{"first",p.first},{"second",p.second},{"kind",p.kind},{"band",p.band},{"name",p.name},{"child",p.child},{"status",p.status},{"requested_on",p.requested_on.value()},{"due_on",p.due_on.value()},{"resolved_on",date(p.resolved_on)},{"known",p.known}};}
core::FamilyPlan parse_plan(const Json& j) {
    fields(j,"family plan",{"id","first","second","kind","band","name","child","status","requested_on","due_on","resolved_on","known"});core::FamilyPlan p;
    p.id=static_cast<std::uint32_t>(u64(j.at("id"),"plan",core::maximum_family_plans));p.first=text(j.at("first"),"parent",160);p.second=text(j.at("second"),"other parent",160,true);p.kind=text(j.at("kind"),"plan kind",32);p.band=text(j.at("band"),"age group",16);p.name=text(j.at("name"),"name",128,true);p.child=text(j.at("child"),"child",160,true);p.status=text(j.at("status"),"status",16);p.requested_on=day(j.at("requested_on"));p.due_on=day(j.at("due_on"));p.resolved_on=optional_day(j.at("resolved_on"));p.known=boolean(j.at("known"),"known");return p;
}
}
Json family_json(const core::FamilyState& s) {
    Json members=Json::array(),edges=Json::array(),pending=Json::array(),history=Json::array(),replies=Json::array();
    for(const auto& m:s.members)members.push_back({{"profile",person_profile_json(m.profile)},{"track",m.track},{"extra_skill",m.extra_skill},{"joined_on",m.joined_on.value()},{"learning_since",m.learning_since.value()},{"next_application",m.next_application.value()},{"qualified_on",date(m.qualified_on)},{"known",m.known},{"seeks_work",m.seeks_work},{"application",m.application?Json{{"company",m.application->company},{"person",m.application->person},{"day",m.application->day.value()}}:Json(nullptr)}});
    for(const auto& e:s.edges)edges.push_back({{"parent",e.parent},{"child",e.child},{"kind",e.kind},{"started_on",e.started_on.value()},{"ended_on",date(e.ended_on)},{"known",e.known},{"marriage",e.marriage}});
    for(const auto& [id,p]:s.pending){(void)id;pending.push_back(plan_json(p));}for(const auto& p:s.history)history.push_back(plan_json(p));
    for(const auto& r:s.replies)replies.push_back({{"day",r.day.value()},{"action",r.action},{"subject",r.subject},{"message",r.message},{"accepted",r.accepted},{"plan",r.plan}});
    return {{"stream",s.stream},{"next_plan",s.next_plan},{"next_review",s.next_review.value()},{"next_due",date(s.next_due)},{"last_action",date(s.last_action)},{"members",std::move(members)},{"edges",std::move(edges)},{"pending",std::move(pending)},{"history",std::move(history)},{"replies",std::move(replies)},{"aliases",s.aliases},{"marriages",s.marriages}};
}
core::FamilyState parse_family(const Json& j) {
    fields(j,"family",{"stream","next_plan","next_review","next_due","last_action","members","edges","pending","history","replies","aliases","marriages"});core::FamilyState s;
    s.stream=u64(j.at("stream"),"family stream");s.next_plan=static_cast<std::uint32_t>(u64(j.at("next_plan"),"next plan",core::maximum_family_plans+1));s.next_review=day(j.at("next_review"));s.next_due=optional_day(j.at("next_due"));s.last_action=optional_day(j.at("last_action"));
    array(j.at("members"),"family members",core::maximum_family_people);for(const auto& row:j.at("members")){
        fields(row,"family member",{"profile","track","extra_skill","joined_on","learning_since","next_application","qualified_on","known","seeks_work","application"});core::FamilyMember m;
        m.profile=parse_person_profile(row.at("profile"));m.track=text(row.at("track"),"learning path",64);m.extra_skill=text(row.at("extra_skill"),"hobby",64,true);m.joined_on=day(row.at("joined_on"));m.learning_since=day(row.at("learning_since"));m.next_application=day(row.at("next_application"));m.qualified_on=optional_day(row.at("qualified_on"));m.known=boolean(row.at("known"),"known");m.seeks_work=boolean(row.at("seeks_work"),"seeks work");
        if(const auto& a=row.at("application");!a.is_null()){fields(a,"family application",{"company","person","day"});m.application=core::FamilyApplication{text(a.at("company"),"company",64),text(a.at("person"),"applicant",64),day(a.at("day"))};}s.members.push_back(std::move(m));
    }
    array(j.at("edges"),"family links",core::maximum_family_edges);for(const auto& row:j.at("edges")){fields(row,"family link",{"parent","child","kind","started_on","ended_on","known","marriage"});s.edges.push_back({text(row.at("parent"),"parent",160),text(row.at("child"),"child",160),text(row.at("kind"),"kinship",24),day(row.at("started_on")),optional_day(row.at("ended_on")),boolean(row.at("known"),"known"),static_cast<std::uint32_t>(u64(row.at("marriage"),"step marriage",core::romance_history_limit))});}
    array(j.at("pending"),"pending family plans",128);for(const auto& row:j.at("pending")){auto p=parse_plan(row);if(!s.pending.emplace(p.id,p).second)invalid("family","Duplicate pending plan");}
    array(j.at("history"),"family plan history",core::maximum_family_plans);for(const auto& row:j.at("history"))s.history.push_back(parse_plan(row));
    array(j.at("replies"),"family conversations",core::maximum_family_plans);for(const auto& row:j.at("replies")){fields(row,"family conversation",{"day","action","subject","message","accepted","plan"});s.replies.push_back({day(row.at("day")),text(row.at("action"),"action",32),text(row.at("subject"),"subject",160,true),text(row.at("message"),"message",512),boolean(row.at("accepted"),"accepted"),static_cast<std::uint32_t>(u64(row.at("plan"),"plan",core::maximum_family_plans))});}
    const auto& aliases=j.at("aliases");if(!aliases.is_object() || aliases.size()>core::maximum_family_people)invalid("family","Too many identity aliases");for(const auto& [key,value]:aliases.items())s.aliases.emplace(text(Json(key),"employment identity",160),text(value,"family identity",160));
    array(j.at("marriages"),"known marriage index",4096);for(const auto& id:j.at("marriages"))if(!s.marriages.insert(static_cast<std::uint32_t>(u64(id,"marriage",core::romance_history_limit))).second)invalid("family","Duplicate marriage index");return s;
}
}
