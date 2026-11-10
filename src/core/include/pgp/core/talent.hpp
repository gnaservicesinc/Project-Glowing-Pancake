#pragma once
#include <pgp/core/calendar.hpp>
#include <pgp/core/units.hpp>
#include <map>
#include <optional>
#include <string_view>
#include <string>
#include <vector>
#include <variant>
namespace pgp::core {
struct SimulationState;
enum class ProductChannel;
struct PersonProfile;
struct SkillDefinition {
    std::string id,name,description,prerequisite;
    int course_days=30;
    std::int64_t tuition_cents=0;
    int engineering=0,research=0,evaluation=0;
    bool teachable=true;
    auto operator<=>(const SkillDefinition&) const = default;
};
struct SoftwareBrief {
    std::string id,name,description;
    bool advanced=false;
    int work_days=30;
    std::int64_t setup_cents=0,monthly_revenue_cents=0;
    std::vector<std::string> features;
    auto operator<=>(const SoftwareBrief&) const = default;
};
struct TalentRules {
    std::vector<SkillDefinition> skills;
    std::map<std::string,std::string> roles;
    std::vector<SoftwareBrief> software;
    int market_bonus_max_bp=1500;
    int ai_architecture_floor=8500,ai_programming_floor=9500,ai_reliability_floor=9000,ai_workflow_floor=8500;
    int ai_compute_milli=1000,ai_work_percent=75;
    int extra_skills=2,design_bonus=10,focus_bonus=20,focus_penalty=10;
    auto operator<=>(const TalentRules&) const = default;
};
struct SkillCourse {
    std::uint32_t id=0;
    std::string person,skill,payer;
    OrdinalDay started_on{0},completes_on{0};
    bool completed=false;
    auto operator<=>(const SkillCourse&) const = default;
};
struct SoftwareDesign {
    std::string brief,name,aesthetic="calm",navigation="sidebar",focus="balanced";
    std::vector<std::string> workers;
    std::uint32_t previous=0;
    std::string ai_source;
    auto operator<=>(const SoftwareDesign&) const = default;
};
struct SoftwareProject {
    std::uint32_t id=0;
    SoftwareDesign design;
    std::string project;
    OrdinalDay started_on{0};
    std::optional<OrdinalDay> released_on;
    int usability=0,reliability=0,capability=0;
    std::string blocker;
    auto operator<=>(const SoftwareProject&) const = default;
};
struct TalentState {
    std::vector<std::string> founder_skills;
    bool founder_genius=false;
    std::vector<SkillCourse> courses;
    std::vector<SoftwareProject> software;
    auto operator<=>(const TalentState&) const = default;
};
struct EnrollSkill {std::string person,skill,payer;};
struct StartSoftware {SoftwareDesign design;};
struct ReleaseSoftware {std::uint32_t id=0;};
struct ResumeSoftware {std::uint32_t id=0;};
using TalentAction=std::variant<EnrollSkill,StartSoftware,ReleaseSoftware,ResumeSoftware>;
struct ManageTalent {TalentAction action;};
struct SoftwareQuote {
    int usability=0,reliability=0,capability=0,staff_days=0,ai_level=0;
    Cents fee{0},monthly_revenue{0};
    std::string blocker;
};
void validate_talent_rules(const TalentRules&);
const TalentRules* talent_rules(const SimulationState&);
bool has_skill(const PersonProfile&,std::string_view);
bool person_has_skill(const SimulationState&,std::string_view,std::string_view);
std::vector<std::string> person_skills(const SimulationState&,std::string_view);
std::string software_worker_blocker(const SimulationState&,std::string_view,std::string_view except={});
SoftwareQuote software_quote(const SimulationState&,const SoftwareDesign&,std::string_view except={});
void manage_talent(SimulationState&,const TalentAction&,Cents protected_cash);
void prepare_talent(SimulationState&);
void reconcile_software(SimulationState&);
void validate_talent(const SimulationState&);
int software_ai_level(const SimulationState&);
BasisPoints software_market_bonus(const SimulationState&,ProductChannel,std::optional<OrdinalDay> day={});
Cents personal_tuition(const SimulationState&,OrdinalDay before);
}
