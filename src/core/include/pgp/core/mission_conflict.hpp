#pragma once
#include <pgp/core/calendar.hpp>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pgp::core {
struct SimulationState;

struct MissionConflictRules {
    int commitment_days=60;
    int employee_days=30;
    int concern_days=14;
    int notice_days=14;
    int repair_days=7;
    int trust_loss=450;
    int trust_ceiling=200;
    auto operator<=>(const MissionConflictRules&) const=default;
};

struct MissionConcern {
    std::string previous_mission,new_mission;
    OrdinalDay raised_on{0};
    int previous_affinity=0;
    std::optional<OrdinalDay> rivalry_on,heard_on,settled_on;
    std::string mediator;
    bool frenemy=false;
    auto operator<=>(const MissionConcern&) const=default;
};

struct MissionConflictState {
    std::string mission;
    OrdinalDay committed_on{0};
    std::map<std::string,MissionConcern,std::less<>> concerns;
    auto operator<=>(const MissionConflictState&) const=default;
};

struct ChangeCompanyMission {std::string mission;};
struct RespondToMissionConcern {std::string person;std::string mediator;};

void validate_mission_conflict_rules(const MissionConflictRules&);
void validate_mission_conflicts(const SimulationState&);
void prepare_mission_conflicts(SimulationState&);
void change_company_mission(SimulationState&,const ChangeCompanyMission&);
void respond_to_mission_concern(SimulationState&,const RespondToMissionConcern&);
[[nodiscard]] const MissionConflictRules* mission_conflict_rules(const SimulationState&);
[[nodiscard]] const MissionConcern* mission_concern(const SimulationState&,std::string_view person);
[[nodiscard]] bool is_personal_rival(const SimulationState&,std::string_view first,std::string_view second);
[[nodiscard]] std::vector<std::string> mission_pivot_concerns(const SimulationState&,std::string_view mission);
[[nodiscard]] std::string mission_response_blocker(const SimulationState&,const RespondToMissionConcern&);
[[nodiscard]] bool mission_mediator_available(const SimulationState&,std::string_view person);
[[nodiscard]] std::string_view committed_company_mission(const SimulationState&);
}
