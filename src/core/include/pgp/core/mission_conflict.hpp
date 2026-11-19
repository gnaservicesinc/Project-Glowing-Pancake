#pragma once
#include <pgp/core/calendar.hpp>
#include <pgp/core/units.hpp>
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

struct MissionReversalScandal {
    std::string previous_mission,new_mission;
    OrdinalDay triggered_on{0};
    int employee_morale_hit=0;
    int donor_customer_reaction_bp=0;
    int investor_reaction_bp=0;
    std::string response;
    std::optional<OrdinalDay> responded_on;
    Cents response_cost{0};
    auto operator<=>(const MissionReversalScandal&) const=default;
};

struct MissionConflictState {
    std::string mission;
    OrdinalDay committed_on{0};
    std::map<std::string,MissionConcern,std::less<>> concerns;
    bool inherited_purpose=false;
    std::optional<MissionReversalScandal> scandal{};
    auto operator<=>(const MissionConflictState&) const=default;
};

struct NonprofitReaction {
    int goodwill = 0;
    BasisPoints awareness{0};
    std::uint32_t salary_discount_bp = 0;
};
[[nodiscard]] NonprofitReaction nonprofit_reaction(const SimulationState&, OrdinalDay);

struct ChangeCompanyMission {std::string mission;};
struct RespondToMissionConcern {std::string person;std::string mediator;};
struct RespondToMissionScandal {std::string choice;};

void validate_mission_conflict_rules(const MissionConflictRules&);
void validate_mission_conflicts(const SimulationState&);
void prepare_mission_conflicts(SimulationState&);
void change_company_mission(SimulationState&,const ChangeCompanyMission&);
void respond_to_mission_concern(SimulationState&,const RespondToMissionConcern&);
void respond_to_mission_scandal(SimulationState&,const RespondToMissionScandal&,Cents protected_cash);
[[nodiscard]] const MissionConflictRules* mission_conflict_rules(const SimulationState&);
[[nodiscard]] const MissionConcern* mission_concern(const SimulationState&,std::string_view person);
[[nodiscard]] const MissionReversalScandal* mission_scandal(const SimulationState&);
[[nodiscard]] bool is_personal_rival(const SimulationState&,std::string_view first,std::string_view second);
[[nodiscard]] std::vector<std::string> mission_pivot_concerns(const SimulationState&,std::string_view mission);
[[nodiscard]] std::string mission_response_blocker(const SimulationState&,const RespondToMissionConcern&);
[[nodiscard]] bool mission_mediator_available(const SimulationState&,std::string_view person);
[[nodiscard]] std::string_view committed_company_mission(const SimulationState&);
}
