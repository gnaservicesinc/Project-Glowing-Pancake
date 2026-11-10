#include "people_json.hpp"
namespace pgp::persistence::detail {
namespace {
Json profile(const core::PersonProfile& p) {
    return {{"id",p.id},{"name",p.name},{"gender",p.gender},{"pronouns",p.pronouns},{"education",p.education},{"role",p.role},{"bio",p.bio},{"preferred_mission",p.preferred_mission},
        {"birth_year",p.birth_year},{"experience",p.experience},{"research",p.research},{"engineering",p.engineering},{"evaluation",p.evaluation},{"mission_fit",p.mission_fit},{"applied_on",p.applied_on.value()},
        {"traits",p.traits},{"portrait",{{"kit",p.portrait.kit},{"parts",p.portrait.parts},{"style",p.portrait.style}}}};
}
core::PersonProfile parse_profile(const Json& j) {
    fields(j,"person",{"id","name","gender","pronouns","education","role","bio","preferred_mission","birth_year","experience","research","engineering","evaluation","mission_fit","applied_on","portrait","traits"});
    const auto year=i64(j.at("birth_year"),"birth_year");if(year<1800 || year>3000) invalid("person","Birth year out of bounds");
    const auto count=[&](const char* name){return static_cast<std::uint32_t>(u64(j.at(name),name,100));};
    core::PersonProfile result{text(j.at("id"),"id",64),text(j.at("name"),"name",128),text(j.at("gender"),"gender",32),text(j.at("pronouns"),"pronouns",64),
        text(j.at("education"),"education",128,true),text(j.at("role"),"role",64),text(j.at("bio"),"bio",512,true),text(j.at("preferred_mission"),"preferred_mission",32),static_cast<int>(year),count("experience"),count("research"),count("engineering"),count("evaluation"),count("mission_fit"),core::OrdinalDay{i64(j.at("applied_on"),"applied_on")}};
    array(j.at("traits"),"traits",16);
    for(const auto& trait:j.at("traits")) result.traits.push_back(text(trait,"trait",80));
    if(j.contains("portrait")) {
        const auto& recipe=j.at("portrait");fields(recipe,"portrait",{"kit","parts","style"});
        result.portrait.kit=static_cast<std::uint16_t>(u64(recipe.at("kit"),"portrait kit",65535));
        array(recipe.at("parts"),"portrait layers",core::portrait_part_counts.size());
        if(recipe.at("parts").size()!=core::portrait_part_counts.size()) invalid("portrait","Missing layer IDs");
        for(std::size_t i=0;i<core::portrait_part_counts.size();++i)
            result.portrait.parts[i]=static_cast<std::uint8_t>(u64(recipe.at("parts")[i],"portrait layer",255));
        array(recipe.at("style"),"portrait styling",core::portrait_style_counts.size());
        if(recipe.at("style").size()!=result.portrait.style.size()) invalid("portrait","Missing style IDs");
        for(std::size_t i=0;i<result.portrait.style.size();++i) result.portrait.style[i]=static_cast<std::uint8_t>(u64(recipe.at("style")[i],"portrait style",255));
        core::validate_portrait(result.portrait);
    }
    return result;
}
Json recruitment(const core::Recruitment& n) {
    Json offers=Json::array();for(const auto& o:n.offers) offers.push_back({{"salary",o.salary.value()},{"signing",o.signing.value()},{"counter_salary",o.counter_salary.value()},{"counter_signing",o.counter_signing.value()},
        {"submitted_on",o.submitted_on.value()},{"respond_on",o.respond_on.value()},{"expires_on",o.expires_on.value()},{"reply",o.reply}});
    const auto date=[](auto value){return value?Json(value->value()):Json(nullptr);};
    return {{"ask_salary",n.ask_salary.value()},{"ask_signing",n.ask_signing.value()},{"minimum_salary",n.minimum_salary.value()},{"minimum_signing",n.minimum_signing.value()},
        {"offers",std::move(offers)},{"agreed_on",date(n.agreed_on)},{"withdrawn_on",date(n.withdrawn_on)},{"hired_on",date(n.hired_on)}};
}
core::Recruitment parse_recruitment(const Json& j) {
    fields(j,"recruitment",{"ask_salary","ask_signing","minimum_salary","minimum_signing","offers","agreed_on","withdrawn_on","hired_on"});
    core::Recruitment n;
    n.ask_salary=core::Cents{i64(j.at("ask_salary"),"ask_salary")};n.ask_signing=core::Cents{i64(j.at("ask_signing"),"ask_signing")};
    n.minimum_salary=core::Cents{i64(j.at("minimum_salary"),"minimum_salary")};n.minimum_signing=core::Cents{i64(j.at("minimum_signing"),"minimum_signing")};
    const auto date=[&](const char* key)->std::optional<core::OrdinalDay>{return j.at(key).is_null()?std::nullopt:std::optional{core::OrdinalDay{i64(j.at(key),key)}};};
    n.agreed_on=date("agreed_on");n.withdrawn_on=date("withdrawn_on");n.hired_on=date("hired_on");
    array(j.at("offers"),"employment offers",6);for(const auto& o:j.at("offers")) {
        fields(o,"employment offer",{"salary","signing","counter_salary","counter_signing","submitted_on","respond_on","expires_on","reply"});
        n.offers.push_back({core::Cents{i64(o.at("salary"),"salary")},core::Cents{i64(o.at("signing"),"signing")},core::Cents{i64(o.at("counter_salary"),"counter_salary")},core::Cents{i64(o.at("counter_signing"),"counter_signing")},
            core::OrdinalDay{i64(o.at("submitted_on"),"submitted_on")},core::OrdinalDay{i64(o.at("respond_on"),"respond_on")},core::OrdinalDay{i64(o.at("expires_on"),"expires_on")},text(o.at("reply"),"reply",16)});
    }return n;
}
Json applicant(const core::Applicant& a) {return {{"profile",profile(a.profile)},{"salary",a.salary.value()},{"signing",a.signing.value()},{"evaluation_credit",a.evaluation_credit.value()},{"recruitment",a.recruitment?recruitment(*a.recruitment):Json(nullptr)}};}
core::Applicant parse_applicant(const Json& j) {
    fields(j,"applicant",{"profile","salary","signing","evaluation_credit","recruitment"});
    return {parse_profile(j.at("profile")),core::Cents{i64(j.at("salary"),"salary")},core::Cents{i64(j.at("signing"),"signing")},core::BasisPoints{i64(j.at("evaluation_credit"),"evaluation_credit")},
        j.at("recruitment").is_null()?std::nullopt:std::optional{parse_recruitment(j.at("recruitment"))}};
}
}
Json people_json(const core::PeopleState& p) {
    Json applicants=Json::array(),employees=Json::array(),known=Json::object(),contacts=Json::array();
    for(const auto& [id,person]:p.known_people) known[id]=profile(person);
    for(const auto& a:p.applicants) applicants.push_back(applicant(a));
    for(const auto& e:p.employees) employees.push_back({{"terms",applicant(e.terms)},{"joined_on",e.joined_on.value()},{"ready_on",e.ready_on.value()},
        {"opening",e.opening},{"departed_on",e.departed_on?Json(e.departed_on->value()):Json(nullptr)},
        {"dismissal_severance",e.dismissal_severance?Json(e.dismissal_severance->value()):Json(nullptr)}});
    for(const auto& contact:p.contacts) {
        Json messages=Json::array(),funding=Json::array();
        for(const auto& m:contact.messages) messages.push_back({{"day",m.day.value()},{"topic",m.topic},{"reply",m.reply},{"mood",m.mood}});
        for(const auto& discussion:contact.funding) funding.push_back({{"id",discussion.id},{"reputation",discussion.reputation},{"publications",discussion.publications},{"evaluations",discussion.evaluations},
            {"requested_on",discussion.requested_on.value()},{"expires_on",discussion.expires_on.value()},{"offers",discussion.offers},
            {"status",discussion.status},{"selected",discussion.selected},{"motion",discussion.motion},
            {"responded_on",discussion.responded_on?Json(discussion.responded_on->value()):Json(nullptr)}});
        contacts.push_back({{"person",contact.person},{"exchanged_on",contact.exchanged_on.value()},{"messages",std::move(messages)},
            {"funding",std::move(funding)},
            {"opening",contact.opening?Json{{"source",contact.opening->source},{"outcome",contact.opening->outcome}}:Json(nullptr)}});
    }
    return {{"stream",p.stream},{"next_id",p.next_id},{"starts_on",p.starts_on.value()},{"next_recruitment",p.next_recruitment.value()},
        {"offered_pay",p.offered_pay.value()},{"mission",p.mission},{"founding_prestige",p.founding_prestige},{"applicants",std::move(applicants)},{"employees",std::move(employees)},{"known_people",std::move(known)},{"contacts",std::move(contacts)}};
}
core::PeopleState parse_people(const Json& j) {
    fields(j,"people",{"stream","next_id","starts_on","next_recruitment","offered_pay","mission","founding_prestige","applicants","employees","known_people","contacts"});
    core::PeopleState p;p.stream=u64(j.at("stream"),"stream");p.next_id=u64(j.at("next_id"),"next_id");
    p.starts_on=core::OrdinalDay{i64(j.at("starts_on"),"starts_on")};p.next_recruitment=core::OrdinalDay{i64(j.at("next_recruitment"),"next_recruitment")};
    p.offered_pay=core::SalaryRate{i64(j.at("offered_pay"),"offered_pay")};p.mission=text(j.at("mission"),"mission",32);p.founding_prestige=static_cast<std::uint32_t>(u64(j.at("founding_prestige"),"prestige",10));
    array(j.at("applicants"),"applicants",12);for(const auto& a:j.at("applicants")) p.applicants.push_back(parse_applicant(a));
    if(!j.at("known_people").is_object() || j.at("known_people").size()>512) invalid("known_people","Authored identity count out of bounds");
    for(const auto& [id,row]:j.at("known_people").items()) p.known_people.emplace(text(Json(id),"offer id",128),parse_profile(row));
    array(j.at("employees"),"employees",256);for(const auto& e:j.at("employees")) {
        fields(e,"employee",{"terms","joined_on","ready_on","opening","departed_on","dismissal_severance"});
        p.employees.push_back({parse_applicant(e.at("terms")),core::OrdinalDay{i64(e.at("joined_on"),"joined_on")},core::OrdinalDay{i64(e.at("ready_on"),"ready_on")},boolean(e.at("opening"),"opening"),
            e.at("departed_on").is_null()?std::nullopt:std::optional{core::OrdinalDay{i64(e.at("departed_on"),"departed_on")}},
            e.at("dismissal_severance").is_null()?std::nullopt:std::optional{core::Cents{i64(e.at("dismissal_severance"),"dismissal_severance")}}});
    }
    array(j.at("contacts"),"contacts",256+core::opening_contact_limit);
    for(const auto& contact:j.at("contacts")) {
        fields(contact,"contact",{"person","exchanged_on","messages","opening","funding"});
        core::PeopleContact entry{text(contact.at("person"),"person",72),core::OrdinalDay{i64(contact.at("exchanged_on"),"exchanged_on")},{}};
        if(const auto& opening=contact.at("opening"); !opening.is_null()) {
            fields(opening,"opening conversation",{"source","outcome"});
            entry.opening=core::OpeningConversation{text(opening.at("source"),"source",24),text(opening.at("outcome"),"outcome",24)};
        }
        array(contact.at("messages"),"messages",32);
        for(const auto& m:contact.at("messages")) {
            fields(m,"message",{"day","topic","reply","mood"});
            const auto mood=i64(m.at("mood"),"mood");
            if(mood < -1 || mood > 100) invalid("message","Mood is outside the conversation range");
            entry.messages.push_back({core::OrdinalDay{i64(m.at("day"),"day")},text(m.at("topic"),"topic",24),
                text(m.at("reply"),"reply",24),static_cast<int>(mood)});
        }
        array(contact.at("funding"),"funding discussions",core::contact_funding_limit);
        for(const auto& d:contact.at("funding")) {
            fields(d,"funding discussion",{"id","reputation","publications","evaluations","requested_on","expires_on","offers","status","selected","motion","responded_on"});
            core::ContactFundingDiscussion discussion;
            discussion.id=static_cast<std::uint32_t>(u64(d.at("id"),"discussion id",core::contact_funding_limit));
            discussion.reputation=static_cast<std::uint32_t>(u64(d.at("reputation"),"reputation",100));
            discussion.publications=static_cast<std::uint32_t>(u64(d.at("publications"),"publications",4096));
            discussion.evaluations=static_cast<std::uint32_t>(u64(d.at("evaluations"),"evaluations",4096));
            discussion.requested_on=core::OrdinalDay{i64(d.at("requested_on"),"requested_on")};
            discussion.expires_on=core::OrdinalDay{i64(d.at("expires_on"),"expires_on")};
            array(d.at("offers"),"funding offers",8);
            for(const auto& offer:d.at("offers")) discussion.offers.push_back(text(offer,"offer",64));
            discussion.status=text(d.at("status"),"status",24);discussion.selected=text(d.at("selected"),"selected",64,true);
            discussion.motion=text(d.at("motion"),"motion",128,true);
            if(!d.at("responded_on").is_null()) discussion.responded_on=core::OrdinalDay{i64(d.at("responded_on"),"responded_on")};
            entry.funding.push_back(std::move(discussion));
        }
        p.contacts.push_back(std::move(entry));
    }
    return p;
}
}
