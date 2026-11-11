#include <pgp/core/family_rules.hpp>
#include <set>
#include <stdexcept>
namespace pgp::core {
void validate_family_rules(const FamilyRules& r) {
    if(r.interval_days<7 || r.interval_days>90 || r.birth_days<240 || r.birth_days>365 || r.adoption_days<30 || r.adoption_days>730 ||
       r.foster_days<7 || r.foster_days>180 || r.foster_term_days<180 || r.foster_term_days>1095 || r.foster_adoption_days<90 || r.foster_adoption_days>=r.foster_term_days ||
       r.minimum_parent_gap<18 || r.minimum_parent_gap>30 || r.household_cooldown<180 || r.household_cooldown>1095 || r.maximum_children<1 || r.maximum_children>8 ||
       r.married_days<30 || r.married_days>730 || r.birth_weight<0 || r.adoption_weight<0 || r.birth_weight+r.adoption_weight>100 || r.study_days<180 || r.study_days>1095 ||
       r.application_interval_days<30 || r.application_interval_days>365 || r.tracks.empty() || r.tracks.size()>8)throw std::invalid_argument("Family rules need bounded care, waiting periods and learning tracks.");
    for(const auto chance:{r.npc_family_bp,r.consent_bp,r.older_child_consent_bp,r.application_bp})if(chance<0 || chance>10000)throw std::invalid_argument("Family chances must be basis points.");
    std::set<std::string> skills;
    for(const auto& track:r.tracks)if(track.skill.empty() || track.skill.size()>64 || track.role.empty() || track.role.size()>64 || track.name.empty() || track.name.size()>128 || !skills.insert(track.skill).second)
        throw std::invalid_argument("Family learning tracks need unique bounded skills, roles and names.");
}
}
