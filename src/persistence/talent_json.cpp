#include "talent_json.hpp"
#include "../content/talent_codec.hpp"
namespace pgp::persistence::detail {
Json talent_json(const core::TalentState& t) {
    Json courses=Json::array(),software=Json::array();
    for(const auto& c:t.courses)courses.push_back({{"id",c.id},{"person",c.person},{"skill",c.skill},{"payer",c.payer},{"started_on",c.started_on.value()},{"completes_on",c.completes_on.value()},{"completed",c.completed}});
    for(const auto& p:t.software)software.push_back({{"id",p.id},{"design",p.design},{"project",p.project},{"started_on",p.started_on.value()},
        {"released_on",p.released_on?Json(p.released_on->value()):Json(nullptr)},{"usability",p.usability},{"reliability",p.reliability},{"capability",p.capability},{"blocker",p.blocker}});
    return {{"founder_skills",t.founder_skills},{"founder_genius",t.founder_genius},{"courses",courses},{"software",software}};
}
core::TalentState parse_talent(const Json& j) {
    fields(j,"talent",{"founder_skills","founder_genius","courses","software"});core::TalentState t;
    array(j.at("founder_skills"),"founder skills",96);for(const auto& id:j.at("founder_skills"))t.founder_skills.push_back(text(id,"skill",64));
    t.founder_genius=boolean(j.at("founder_genius"),"genius");
    array(j.at("courses"),"courses",2048);for(const auto& c:j.at("courses")) {
        fields(c,"course",{"id","person","skill","payer","started_on","completes_on","completed"});
        t.courses.push_back({static_cast<std::uint32_t>(u64(c.at("id"),"course",2048)),text(c.at("person"),"person",64),text(c.at("skill"),"skill",64),text(c.at("payer"),"payer",16),
            core::OrdinalDay{i64(c.at("started_on"),"started")},core::OrdinalDay{i64(c.at("completes_on"),"ends")},boolean(c.at("completed"),"completed")});
    }
    array(j.at("software"),"software",256);for(const auto& p:j.at("software")) {
        fields(p,"software",{"id","design","project","started_on","released_on","usability","reliability","capability","blocker"});
        const auto& d=p.at("design");fields(d,"design",{"brief","name","aesthetic","navigation","focus","workers","previous","ai_source"});
        core::SoftwareDesign design;text(d.at("brief"),"brief",64);text(d.at("name"),"name",80);
        array(d.at("workers"),"workers",8);for(const auto& id:d.at("workers"))text(id,"worker",64);
        text(d.at("ai_source"),"AI source",100,true);
        design=d.get<core::SoftwareDesign>();
        if(design.previous>256)invalid("design","Refinement parent out of bounds");
        t.software.push_back({static_cast<std::uint32_t>(u64(p.at("id"),"software",256)),std::move(design),text(p.at("project"),"project",64),core::OrdinalDay{i64(p.at("started_on"),"started")},
            p.at("released_on").is_null()?std::nullopt:std::optional{core::OrdinalDay{i64(p.at("released_on"),"released")}},
            static_cast<int>(u64(p.at("usability"),"usability",100)),static_cast<int>(u64(p.at("reliability"),"reliability",100)),static_cast<int>(u64(p.at("capability"),"capability",100)),text(p.at("blocker"),"blocker",512,true)});
    }
    return t;
}
}
