#pragma once
#include <pgp/core/people.hpp>
#include <nlohmann/json.hpp>
#include <limits>
#include "talent_codec.hpp"
#include "social_codec.hpp"
namespace pgp::content::people_codec {
using Json=nlohmann::json;
inline void fields(const Json& j,std::initializer_list<const char*> names) {
    if(!j.is_object() || j.size()!=names.size()) throw std::invalid_argument("Invalid recruitment fields");
    for(const auto* name:names) if(!j.contains(name)) throw std::invalid_argument("Missing recruitment field");
}
inline std::int64_t integer(const Json& j) {
    if(!j.is_number_integer() || (j.is_number_unsigned() && j.get<std::uint64_t>()>static_cast<std::uint64_t>(INT64_MAX))) throw std::invalid_argument("Recruitment values require integer tokens");
    return j.get<std::int64_t>();
}
inline std::uint32_t count(const Json& j) {
    const auto n=integer(j);if(n<0 || n>1000) throw std::invalid_argument("Recruitment count out of bounds");return static_cast<std::uint32_t>(n);
}
inline Json encode(const core::PeopleRules& r) {
    Json j{{"first_names",r.first_names},{"last_names",r.last_names},{"maximum_hq",r.maximum_hq},{"candidates",r.candidates},
        {"recruitment_days",r.recruitment_interval.value()},{"onboarding_days",r.onboarding.value()},
        {"monthly_salary_cents",r.monthly_salary.value()},{"signing_cost_cents",r.signing_cost.value()},
        {"base_skill",r.base_skill},{"skill_jitter",r.skill_jitter},{"pay_skill_weight",r.pay_skill_weight},
        {"prestige_skill_weight",r.prestige_skill_weight},{"mission_skill_weight",r.mission_skill_weight},
        {"maximum_evaluation_credit_bp",r.maximum_evaluation_credit.value()}};
    if(r.recruitment) {const auto& n=*r.recruitment;j["recruitment"]={{"response_days",n.response.value()},{"validity_days",n.validity.value()},{"maximum_rounds",n.maximum_rounds},
        {"minimum_ask_bp",n.minimum_ask},{"maximum_ask_bp",n.maximum_ask},{"minimum_acceptance_bp",n.minimum_acceptance},{"maximum_acceptance_bp",n.maximum_acceptance},
        {"minimum_offer_bp",n.minimum_offer},{"maximum_offer_bp",n.maximum_offer},{"maximum_signing_multiple",n.maximum_signing_multiple}};}
    if(!r.trait_groups.empty()) j["trait_groups"]=r.trait_groups;
    if(r.dismissal) j["dismissal"]={{"severance_months",r.dismissal->severance_months}};
    if(!r.founder_audiences.empty()) {
        auto& rows=j["founder_audiences"]=Json::array();
        for(const auto& row:r.founder_audiences)
            rows.push_back({{"founder",row.founder},{"audience",row.audience},{"salary_bp",row.salary_bp},{"mood",row.mood}});
    }
    if(r.mood) {
        const auto& m=*r.mood;
        j["mood"]={{"pay",m.pay},{"company_prestige",m.company_prestige},{"ceo_prestige",m.ceo_prestige},
            {"role",m.role},{"coworkers",m.coworkers},{"amenity_cap",m.amenity_cap},
            {"temperaments",m.temperaments},{"amenities",m.amenities}};
    }
    if(r.social)j["social"]=social_codec::encode(*r.social);
    if(r.talent)j["talent"]=talent_codec::encode(*r.talent);
    return j;
}
inline core::PeopleRules decode(Json j) {
    std::optional<core::SocialRules> social;
    if(j.contains("social")){social=social_codec::decode(j.at("social"));j.erase("social");}
    std::optional<core::TalentRules> talent;
    if(j.contains("talent")){talent=talent_codec::decode(j.at("talent"));j.erase("talent");}
    std::vector<core::FounderAudienceRule> audiences;
    if(j.contains("founder_audiences")) {
        const auto& rows=j.at("founder_audiences");
        if(!rows.is_array() || rows.empty() || rows.size()>9) throw std::invalid_argument("Use one to nine founder audience rules");
        for(const auto& row:rows) {
            fields(row,{"founder","audience","salary_bp","mood"});
            const auto salary=integer(row.at("salary_bp")),mood=integer(row.at("mood"));
            if(salary < -500 || salary>500 || mood < -5 || mood>5) throw std::invalid_argument("Founder audience reaction out of bounds");
            audiences.push_back({row.at("founder").get<std::string>(),row.at("audience").get<std::string>(),static_cast<int>(salary),static_cast<int>(mood)});
        }
        j.erase("founder_audiences");
    }
    std::optional<core::DismissalRules> dismissal;
    if(j.contains("dismissal")) {
        fields(j.at("dismissal"),{"severance_months"});
        dismissal=core::DismissalRules{count(j.at("dismissal").at("severance_months"))};
        j.erase("dismissal");
    }
    std::optional<core::WorkplaceMoodRules> mood;
    if(j.contains("mood")) {
        const auto& m=j.at("mood");
        fields(m,{"pay","company_prestige","ceo_prestige","role","coworkers","amenity_cap","temperaments","amenities"});
        mood=core::WorkplaceMoodRules{count(m.at("pay")),count(m.at("company_prestige")),count(m.at("ceo_prestige")),
            count(m.at("role")),count(m.at("coworkers")),count(m.at("amenity_cap")),{}, {}};
        const auto& temperaments=m.at("temperaments");
        const auto& amenities=m.at("amenities");
        if(!temperaments.is_object() || temperaments.size()>16 || !amenities.is_object() || amenities.size()>7)
            throw std::invalid_argument("Mood needs bounded temperament and amenity maps");
        for(const auto& [name,points]:temperaments.items()) {
            const auto value=integer(points);
            if(value < -15 || value>15) throw std::invalid_argument("Temperament mood points out of bounds");
            mood->temperaments.emplace(name,static_cast<int>(value));
        }
        for(const auto& [kind,points]:amenities.items()) mood->amenities.emplace(kind,count(points));
        j.erase("mood");
    }
    std::vector<std::vector<std::string>> traits;
    if(j.contains("trait_groups")) {
        const auto& groups=j.at("trait_groups");
        if(!groups.is_array() || groups.size()>12) throw std::invalid_argument("Use at most twelve trait groups");
        for(const auto& group:groups) {
            if(!group.is_array() || group.empty() || group.size()>16) throw std::invalid_argument("Use one to sixteen traits per group");
            std::vector<std::string> values;for(const auto& v:group) {if(!v.is_string()) throw std::invalid_argument("Traits need names");values.push_back(v.get<std::string>());}traits.push_back(std::move(values));
        }
        j.erase("trait_groups");
    }
    std::optional<core::RecruitmentRules> recruitment;
    if(j.contains("recruitment")) {
        const auto& n=j.at("recruitment");fields(n,{"response_days","validity_days","maximum_rounds","minimum_ask_bp","maximum_ask_bp","minimum_acceptance_bp","maximum_acceptance_bp","minimum_offer_bp","maximum_offer_bp","maximum_signing_multiple"});
        const auto ratio=[&](const char* key){const auto v=integer(n.at(key));if(v<0 || v>30000) throw std::invalid_argument("Recruiting ratio out of bounds");return static_cast<std::uint32_t>(v);};
        recruitment=core::RecruitmentRules{core::Days{integer(n.at("response_days"))},core::Days{integer(n.at("validity_days"))},count(n.at("maximum_rounds")),
            ratio("minimum_ask_bp"),ratio("maximum_ask_bp"),ratio("minimum_acceptance_bp"),ratio("maximum_acceptance_bp"),ratio("minimum_offer_bp"),ratio("maximum_offer_bp"),count(n.at("maximum_signing_multiple"))};
        j.erase("recruitment");
    }
    fields(j,{"first_names","last_names","maximum_hq","candidates","recruitment_days","onboarding_days","monthly_salary_cents","signing_cost_cents","base_skill","skill_jitter","pay_skill_weight","prestige_skill_weight","mission_skill_weight","maximum_evaluation_credit_bp"});
    core::PeopleRules r;r.talent=std::move(talent);r.social=std::move(social);r.recruitment=std::move(recruitment);r.trait_groups=std::move(traits);r.mood=std::move(mood);r.dismissal=dismissal;r.founder_audiences=std::move(audiences);
    for(const char* name:{"first_names","last_names"}) {
        const auto& list=j.at(name);
        if(!list.is_array() || list.size()>512) throw std::invalid_argument("Name pool exceeds its limit");
        auto& target=std::string_view{name}=="first_names"?r.first_names:r.last_names;
        for(const auto& entry:list) {if(!entry.is_string()) throw std::invalid_argument("Names must be text");target.push_back(entry.get<std::string>());}
    }
    r.maximum_hq=count(j.at("maximum_hq"));r.candidates=count(j.at("candidates"));
    r.recruitment_interval=core::Days{integer(j.at("recruitment_days"))};r.onboarding=core::Days{integer(j.at("onboarding_days"))};
    r.monthly_salary=core::Cents{integer(j.at("monthly_salary_cents"))};r.signing_cost=core::Cents{integer(j.at("signing_cost_cents"))};
    r.base_skill=count(j.at("base_skill"));r.skill_jitter=count(j.at("skill_jitter"));r.pay_skill_weight=count(j.at("pay_skill_weight"));
    r.prestige_skill_weight=count(j.at("prestige_skill_weight"));r.mission_skill_weight=count(j.at("mission_skill_weight"));
    r.maximum_evaluation_credit=core::BasisPoints{integer(j.at("maximum_evaluation_credit_bp"))};
    core::validate_people_rules(r);return r;
}
}
