#pragma once
#include <pgp/core/people.hpp>
#include <pgp/core/paged_history.hpp>
#include <set>
namespace pgp::core {
inline constexpr std::size_t maximum_family_people=1024,maximum_family_edges=8192,maximum_family_plans=4096;
struct FamilyApplication {
    std::string company,person;
    OrdinalDay day{0};
    auto operator<=>(const FamilyApplication&) const=default;
};
struct FamilyMember {
    PersonProfile profile;
    std::string track,extra_skill;
    OrdinalDay joined_on{0},learning_since{0},next_application{0};
    std::optional<OrdinalDay> qualified_on;
    std::optional<FamilyApplication> application;
    bool known=false,seeks_work=true;
    auto operator<=>(const FamilyMember&) const=default;
};
struct FamilyEdge {
    std::string parent,child,kind;
    OrdinalDay started_on{0};
    std::optional<OrdinalDay> ended_on;
    bool known=false;
    std::uint32_t marriage=0;
    auto operator<=>(const FamilyEdge&) const=default;
};
struct FamilyPlan {
    std::uint32_t id=0;
    std::string first,second,kind,band,name,child,status="pending";
    OrdinalDay requested_on{0},due_on{0};
    std::optional<OrdinalDay> resolved_on;
    bool known=false;
    auto operator<=>(const FamilyPlan&) const=default;
};
struct FamilyReply {
    OrdinalDay day{0};
    std::string action,subject,message;
    bool accepted=false;
    std::uint32_t plan=0;
    auto operator<=>(const FamilyReply&) const=default;
};
struct FamilyState {
    std::uint64_t stream=0;
    std::uint32_t next_plan=1;
    OrdinalDay next_review{0};
    std::optional<OrdinalDay> next_due,last_action;
    PagedHistory<FamilyMember,16> members;
    PagedHistory<FamilyEdge,16> edges;
    std::map<std::uint32_t,FamilyPlan> pending;
    PagedHistory<FamilyPlan,16> history;
    PagedHistory<FamilyReply,16> replies;
    std::map<std::string,std::string> aliases;
    std::set<std::uint32_t> marriages;
    auto operator<=>(const FamilyState&) const=default;
};
}
