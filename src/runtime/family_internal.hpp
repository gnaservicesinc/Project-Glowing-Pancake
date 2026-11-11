#pragma once
#include "romance_internal.hpp"
#include <pgp/runtime/family.hpp>
namespace pgp::runtime::family_detail {
using namespace core;
[[nodiscard]] CivilDate anniversary(OrdinalDay,int years);
[[nodiscard]] FamilyMember make_member(const WorldState&,std::uint32_t,OrdinalDay,std::string_view,bool,bool,std::string_view surname={});
[[nodiscard]] std::string blocker(const WorldState&,const FamilyRequest&,const romance_detail::People&);
[[nodiscard]] int dependents(const WorldState&,std::string_view);
[[nodiscard]] bool has_parent(const WorldState&,std::string_view,std::string_view);
void add_edge(WorldState&,std::string,std::string,std::string,OrdinalDay,bool,std::uint32_t marriage=0);
void remember_due(FamilyState&,OrdinalDay);
[[nodiscard]] bool apply_member(WorldState&,FamilyMember&,std::string_view,SplitMix64&);
void qualify_and_apply(WorldState&,SplitMix64&);
void reconcile_steps(WorldState&,bool force=false);
[[nodiscard]] FamilyPlan& plan(WorldState&,std::string,std::string,const FamilyRequest&,bool known);
void resolve_plans(WorldState&,SplitMix64&);
}
