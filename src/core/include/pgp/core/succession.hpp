#pragma once
#include <pgp/core/lab.hpp>
#include <array>
#include <pgp/core/portrait.hpp>
#include <pgp/core/governance.hpp>
#include <pgp/core/personal_life.hpp>

namespace pgp::core {
struct SimulationState;
inline constexpr std::size_t succession_plan_limit=128, succession_term_limit=256, succession_notice_limit=512;
enum class FounderRole { executive,chair,trust };
enum class SuccessionStatus { proposed,approved,rejected,expired,cancelled,activated,obsolete };
struct DepartureMessage {
    std::string id,title,copy;
    Cents fee{0};
    std::array<std::int32_t,6> completed_effect{};
    std::optional<std::string> claim{};
    auto operator<=>(const DepartureMessage&) const = default;
};
struct DepartureReason {
    std::string id,title,copy;
    auto operator<=>(const DepartureReason&) const = default;
};
struct DeparturePressRules {
    std::vector<DepartureReason> reasons;
    std::array<std::int32_t,6> challenged_effect{};
    auto operator<=>(const DeparturePressRules&) const = default;
};
struct DepartureRules {
    std::vector<DepartureMessage> messages;
    std::array<std::int32_t,6> broken_effect{};
    std::optional<DeparturePressRules> press{};
    auto operator<=>(const DepartureRules&) const = default;
};
struct DepartureEvidence {
    Cents cash{0},profit{0},debt{0};
    std::uint32_t publications=0;
    std::uint32_t completed_contracts=0,missed_starts=0;
    std::optional<std::string> public_dispute{};
    auto operator<=>(const DepartureEvidence&) const = default;
};
struct DepartureRelease {
    std::uint64_t plan=0,posting_cursor=0;
    std::string message;
    OrdinalDay day{0};
    std::string reason{};
    std::optional<DepartureEvidence> evidence{};
    auto operator<=>(const DepartureRelease&) const = default;
};
struct LeadershipTrack {
    std::string id,priorities;
    std::vector<std::string> names;
    int first_birth_year=1990;
    unsigned cohort_years=20;
    Cents signing_cost{0},monthly_salary{0};
    auto operator<=>(const LeadershipTrack&) const = default;
};
struct OutsideLeadershipCandidate {
    std::string id,name,background,priorities;
    int birth_year=0;
    Cents signing_cost{0},monthly_salary{0};
    std::array<std::int32_t,6> appointment_effect{};
    auto operator<=>(const OutsideLeadershipCandidate&) const = default;
};
struct AsiLeadershipRules {
    std::string priorities;
    Cents signing_cost{0},monthly_operations{0};
    unsigned maximum_term_years=5;
    std::uint32_t control_scrutiny_below=8500;
    std::array<std::int32_t,6> appointment_effect{},weak_control_effect{};
    auto operator<=>(const AsiLeadershipRules&) const = default;
};
struct AsiLeadershipEvidence {
    std::uint64_t system=0;
    OrdinalDay qualified_on{0};
    std::uint32_t control=0;
    auto operator<=>(const AsiLeadershipEvidence&) const = default;
};
struct SuccessionCatalog {
    std::string founder_name,founder_board;
    int founder_birth_year=1980;
    unsigned executive_retirement_age=70,chair_retirement_age=80;
    unsigned minimum_candidate_age=35,maximum_candidate_age=60,maximum_term_years=20,caretaker_term_years=5;
    std::uint32_t chair_seats=1,trust_seats=1;
    Days planning_notice{365},transition_days{90},vote_window{14};
    MilliCU transition_work{900000};
    Cents founder_salary{0},chair_salary{0},cash_floor{0},personal_cash{0},personal_liabilities{0};
    std::vector<LeadershipTrack> tracks;
    std::optional<DepartureRules> departure;
    std::optional<PersonalLifeCatalog> personal_life{};
    std::optional<AsiLeadershipRules> asi{};
    std::optional<std::vector<OutsideLeadershipCandidate>> outside_candidates{};
    auto operator<=>(const SuccessionCatalog&) const = default;
};
struct LeadershipCandidate {
    std::string id,name,priorities;
    int birth_year=0;
    Cents signing_cost{0},monthly_salary{0};
    std::optional<AsiLeadershipEvidence> asi{};
    auto operator<=>(const LeadershipCandidate&) const = default;
};
struct LeadershipTerm {
    std::uint64_t id=0,plan=0;
    std::string person,name;
    int birth_year=0;
    OrdinalDay starts_on{0},due_on{0};
    std::optional<OrdinalDay> ended_on;
    bool caretaker=false;
    std::optional<PortraitRecipe> portrait{};
    std::optional<AsiLeadershipEvidence> asi{};
    auto operator<=>(const LeadershipTerm&) const = default;
};
struct LeadershipControl {
    OrdinalDay day{0};
    std::uint64_t term=0;
    FounderRole role=FounderRole::executive;
    auto operator<=>(const LeadershipControl&) const = default;
};
struct SuccessionPlan {
    std::uint64_t id=0,incumbent=0;
    std::string candidate,project;
    OrdinalDay proposed_on{0},vote_before{0},handover_on{0};
    bool retain_chair=false;
    LabAllocation allocation;
    SuccessionStatus status=SuccessionStatus::proposed;
    std::optional<OrdinalDay> decided_on,finished_on;
    std::optional<bool> player_vote;
    Cents reviewed_cash{0};
    std::uint64_t posting_cursor=0,control_cursor=0;
    std::optional<std::uint64_t> charter_cursor{};
    std::vector<BoardBallot> ballots;
    std::optional<AsiLeadershipEvidence> asi{};
    auto operator<=>(const SuccessionPlan&) const = default;
};
struct SuccessionNotice {
    std::uint64_t id=0,term=0,plan=0;
    std::string kind;
    OrdinalDay day{0},deadline{0};
    bool acknowledged=false;
    auto operator<=>(const SuccessionNotice&) const = default;
};
struct SuccessionState {
    std::string founder_holder;
    Cents personal_cash{0};
    std::uint64_t posting_cursor=0;
    std::vector<LeadershipTerm> terms;
    std::vector<LeadershipControl> controls;
    std::vector<SuccessionPlan> plans;
    std::vector<SuccessionNotice> notices;
    std::optional<std::string> founder_name{};
    std::optional<std::vector<DepartureRelease>> releases;
    std::optional<PersonalLifeState> personal_life{};
    auto operator<=>(const SuccessionState&) const = default;
};
struct ProposeSuccession { std::string candidate;OrdinalDay handover_on{0};bool retain_chair=false;LabAllocation allocation; };
struct VoteSuccession { std::uint64_t id=0;bool support=false; };
struct CancelSuccession { std::uint64_t id=0; };
struct RetireFounderChair {};
struct AnnounceFounderDeparture { std::uint64_t plan=0;std::string message;std::string reason{}; };
struct AcknowledgeSuccession { std::uint64_t id=0; };

void validate_succession_catalog(const SuccessionCatalog&);
void validate_succession_opening(const SuccessionCatalog&,OrdinalDay start,OrdinalDay end);
void initialize_succession(SimulationState&, std::optional<std::string> founder_name = {});
void prepare_succession(SimulationState&);
void validate_succession(const SimulationState&,bool deep=true);
[[nodiscard]] const SuccessionCatalog& succession_catalog(const SimulationState&);
[[nodiscard]] const OutsideLeadershipCandidate* outside_leadership_candidate(const SuccessionCatalog&,std::string_view);
[[nodiscard]] std::vector<LeadershipCandidate> leadership_candidates(const SuccessionCatalog&,OrdinalDay);
[[nodiscard]] LeadershipCandidate leadership_candidate(const SuccessionCatalog&,std::string_view);
[[nodiscard]] std::vector<LeadershipCandidate> leadership_candidates(const SimulationState&,OrdinalDay);
[[nodiscard]] LeadershipCandidate leadership_candidate(const SimulationState&,std::string_view,std::optional<OrdinalDay> as_of={});
[[nodiscard]] FounderRole founder_role(const SuccessionState&);
[[nodiscard]] const SuccessionNotice* required_succession_notice(const SimulationState&);
[[nodiscard]] std::optional<OrdinalDay> next_succession_boundary(const SimulationState&);
[[nodiscard]] std::vector<BoardMember> succession_board(const SimulationState&,std::vector<BoardMember>,std::uint64_t cursor);
void propose_succession(SimulationState&,const ProposeSuccession&);
void vote_succession(SimulationState&,const VoteSuccession&,Cents protected_cash);
void cancel_succession(SimulationState&,std::uint64_t);
void retire_founder_chair(SimulationState&);
void announce_founder_departure(SimulationState&,const AnnounceFounderDeparture&,Cents protected_cash);
void validate_founder_departure(const SimulationState&);
[[nodiscard]] const DepartureMessage& departure_message(const SuccessionCatalog&,std::string_view);
[[nodiscard]] std::string departure_reference(std::uint64_t);
[[nodiscard]] DepartureRelease preview_founder_departure(const SimulationState&,const AnnounceFounderDeparture&);
[[nodiscard]] std::string departure_assessment(const SimulationState&,const DepartureRelease&);
[[nodiscard]] const DepartureReason& departure_reason(const SuccessionCatalog&,std::string_view);
void acknowledge_succession(SimulationState&,std::uint64_t);
}
