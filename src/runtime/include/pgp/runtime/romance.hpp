#pragma once
#include <pgp/core/romance.hpp>
namespace pgp::runtime {
struct WorldState;
struct CompiledProgram;
struct RomanceRequest {std::string person,action;};
struct RomanceQuote {std::string blocker,explanation;};
[[nodiscard]] const core::RomanceRules* romance_rules(const WorldState&);
void initialize_romance(WorldState&);
void prepare_romance(WorldState&);
void validate_romance(const WorldState&,bool deep=true);
[[nodiscard]] std::string romance_person_id(const WorldState&,std::string_view);
[[nodiscard]] const core::RomanceBond* romantic_partner(const WorldState&,std::string_view);
[[nodiscard]] const core::RomanceBond* latest_romance(const WorldState&,std::string_view,std::string_view);
[[nodiscard]] RomanceQuote romance_quote(const WorldState&,const RomanceRequest&);
[[nodiscard]] std::string pursue_romance(WorldState&,const RomanceRequest&);
void set_romantic_preferences(WorldState&,bool interested,const std::vector<std::string>& genders);
[[nodiscard]] bool romantic_identity_known(const WorldState&,std::string_view);
}
