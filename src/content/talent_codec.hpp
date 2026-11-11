#pragma once
#include <pgp/core/talent.hpp>
#include <nlohmann/json.hpp>
namespace pgp::core {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SkillDefinition,id,name,description,prerequisite,course_days,tuition_cents,engineering,research,evaluation,teachable)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SoftwareBrief,id,name,description,advanced,work_days,setup_cents,monthly_revenue_cents,features)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TalentRules,skills,roles,software,market_bonus_max_bp,ai_architecture_floor,ai_programming_floor,ai_reliability_floor,ai_workflow_floor,ai_compute_milli,ai_work_percent,extra_skills,design_bonus,focus_bonus,focus_penalty)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SoftwareDesign,brief,name,aesthetic,navigation,focus,workers,previous,ai_source)
}
namespace pgp::content::talent_codec {
inline nlohmann::json encode(const core::TalentRules& r) {
    nlohmann::json j=r;
    if(r.employee_study_share_bp) {j["employee_study_share_bp"]=r.employee_study_share_bp;j["employee_study_interval_days"]=r.employee_study_interval_days;}
    return j;
}
inline core::TalentRules decode(const nlohmann::json& j) {
    if(!j.is_object() || !j.at("skills").is_array() || j.at("skills").size()>96 || !j.at("software").is_array() || j.at("software").size()>12)throw std::invalid_argument("Talent catalogue is oversized.");
    auto fields=j;fields.erase("employee_study_share_bp");fields.erase("employee_study_interval_days");
    auto result=fields.get<core::TalentRules>();
    result.employee_study_share_bp=j.value("employee_study_share_bp",0);
    result.employee_study_interval_days=j.value("employee_study_interval_days",30);
    if(encode(result)!=j)throw std::invalid_argument("Unsupported talent catalogue fields.");
    core::validate_talent_rules(result);return result;
}
}
