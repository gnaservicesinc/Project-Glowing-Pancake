#pragma once
#include <pgp/core/family.hpp>
namespace pgp::runtime {
struct WorldState;
struct CompiledProgram;
struct FamilyRequest {
    std::string action,band="baby",name,child,track;
    bool with_partner=false;
};
struct FamilyQuote {std::string blocker,explanation;std::optional<core::OrdinalDay> due_on;};
[[nodiscard]] const core::FamilyRules* family_rules(const WorldState&);
void initialize_family(WorldState&);
void prepare_family(WorldState&);
void validate_family(const WorldState&,bool deep=true);
[[nodiscard]] std::string family_person_id(const WorldState&,std::string_view);
[[nodiscard]] const core::FamilyMember* family_member(const WorldState&,std::string_view);
[[nodiscard]] std::map<std::string,std::set<std::string>> family_ancestors(const WorldState&);
[[nodiscard]] bool family_related(const WorldState&,std::string_view,std::string_view);
[[nodiscard]] FamilyQuote family_quote(const WorldState&,const FamilyRequest&);
[[nodiscard]] std::string manage_family(WorldState&,const FamilyRequest&);
[[nodiscard]] int family_age(core::OrdinalDay birthday,core::OrdinalDay day);
}
