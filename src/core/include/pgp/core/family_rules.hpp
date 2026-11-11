#pragma once
#include <string>
#include <vector>
namespace pgp::core {
struct FamilyLearningTrack {
    std::string skill,role,name;
    auto operator<=>(const FamilyLearningTrack&) const=default;
};
struct FamilyRules {
    int interval_days=30,birth_days=280,adoption_days=90,foster_days=30,foster_term_days=365;
    int foster_adoption_days=180,minimum_parent_gap=18,household_cooldown=365,maximum_children=4,married_days=90;
    int npc_family_bp=800,birth_weight=60,adoption_weight=25,consent_bp=8500,older_child_consent_bp=8500;
    int study_days=365,application_interval_days=90,application_bp=2500;
    std::vector<FamilyLearningTrack> tracks;
    auto operator<=>(const FamilyRules&) const=default;
};
void validate_family_rules(const FamilyRules&);
}
