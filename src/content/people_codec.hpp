#pragma once
#include <pgp/core/people.hpp>
#include <nlohmann/json.hpp>
#include <limits>
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
    return {{"first_names",r.first_names},{"last_names",r.last_names},{"maximum_hq",r.maximum_hq},{"candidates",r.candidates},
        {"recruitment_days",r.recruitment_interval.value()},{"onboarding_days",r.onboarding.value()},
        {"monthly_salary_cents",r.monthly_salary.value()},{"signing_cost_cents",r.signing_cost.value()},
        {"base_skill",r.base_skill},{"skill_jitter",r.skill_jitter},{"pay_skill_weight",r.pay_skill_weight},
        {"prestige_skill_weight",r.prestige_skill_weight},{"mission_skill_weight",r.mission_skill_weight},
        {"maximum_evaluation_credit_bp",r.maximum_evaluation_credit.value()}};
}
inline core::PeopleRules decode(const Json& j) {
    fields(j,{"first_names","last_names","maximum_hq","candidates","recruitment_days","onboarding_days","monthly_salary_cents","signing_cost_cents","base_skill","skill_jitter","pay_skill_weight","prestige_skill_weight","mission_skill_weight","maximum_evaluation_credit_bp"});
    core::PeopleRules r;
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
