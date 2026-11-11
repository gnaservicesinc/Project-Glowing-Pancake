#pragma once
#include <pgp/core/family_rules.hpp>
#include <nlohmann/json.hpp>
#include <climits>
namespace pgp::core {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FamilyLearningTrack,skill,role,name)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FamilyRules,interval_days,birth_days,adoption_days,foster_days,foster_term_days,foster_adoption_days,minimum_parent_gap,household_cooldown,maximum_children,married_days,npc_family_bp,birth_weight,adoption_weight,consent_bp,older_child_consent_bp,study_days,application_interval_days,application_bp,tracks)
}
namespace pgp::content::family_codec {
inline nlohmann::json encode(const core::FamilyRules& r){return r;}
inline core::FamilyRules decode(const nlohmann::json& j) {
    if(!j.is_object())throw std::invalid_argument("Family rules require an object.");
    for(const auto& [key,value]:j.items())if(key!="tracks" && (!value.is_number_integer() || value.get<std::int64_t>()<0 || value.get<std::uint64_t>()>static_cast<std::uint64_t>(INT_MAX)))throw std::invalid_argument("Family tuning requires bounded integer tokens.");
    const auto r=j.get<core::FamilyRules>();if(nlohmann::json(r)!=j)throw std::invalid_argument("Unsupported family rules.");core::validate_family_rules(r);return r;
}
}
