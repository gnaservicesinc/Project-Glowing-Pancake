#pragma once

#include <pgp/core/liquidity.hpp>
#include <pgp/core/objectives.hpp>

namespace pgp::runtime {

struct WorldState;
struct CompiledProgram;
struct ObjectiveSelectionEvent {
    core::OrdinalDay day{0};
    core::ObjectiveSelection selection;
    auto operator<=>(const ObjectiveSelectionEvent&) const = default;
};
struct ObjectiveBreach {
    std::optional<core::OrdinalDay> first;
    std::uint32_t failed_checks = 0;
    std::optional<core::OrdinalDay> acknowledged_on;
    auto operator<=>(const ObjectiveBreach&) const = default;
};
struct AsiRaceResult {
    core::OrdinalDay day{0};
    std::vector<std::string> winners;
    auto operator<=>(const AsiRaceResult&) const = default;
};
struct LegacyValuation {
    std::string institution;
    core::Cents realized_assets{0}, payables{0}, debt{0}, audited_profit{0}, company_equity{0};
    core::Cents founder_equity{0}, personal_cash{0}, personal_liabilities{0}, legacy_wealth{0};
    core::Cents personal_assets{0};
    auto operator<=>(const LegacyValuation&) const = default;
};
struct SurvivalEvidence {
    bool operating = false, serviced_liabilities = false, active_team = false;
    std::uint64_t paid_tasks = 0;
    core::MilliCU paid_wholesale{0}, funded_research{0};
    auto operator<=>(const SurvivalEvidence&) const = default;
};
enum class CampaignEndCause { deadline, asi_race, insolvency, institutional_control, final_order, catastrophe, postmortem, retirement };
struct CampaignEnd {
    core::OrdinalDay day{0};
    CampaignEndCause cause = CampaignEndCause::deadline;
    std::string source;
    std::vector<LegacyValuation> profit;
    std::optional<SurvivalEvidence> survival;
    core::LiquidityExposure obligations;
    std::uint64_t posting_cursor = 0;
    auto operator<=>(const CampaignEnd&) const = default;
};
enum class ObjectiveMode { scored, sandbox, observer, freeplay, retrospective };
struct CareerResearch {
    std::string topic,project;
    std::optional<core::OrdinalDay> published_on;
    auto operator<=>(const CareerResearch&) const = default;
};
struct ResearchAppointment {
    std::string employer;
    core::OrdinalDay accepted_on{0};
    std::vector<CareerResearch> projects;
    auto operator<=>(const ResearchAppointment&) const = default;
};
inline constexpr std::size_t career_appointment_limit=21, career_project_limit=128;
struct ObjectivesState {
    core::OrdinalDay deadline{0};
    std::vector<ObjectiveSelectionEvent> selections;
    std::vector<core::ObjectiveMonth> months;
    std::array<ObjectiveBreach,core::objective_kinds.size()> breaches{};
    std::optional<AsiRaceResult> race;
    std::optional<CampaignEnd> end;
    ObjectiveMode mode = ObjectiveMode::scored;
    // In observer mode the institution and its private morning stay frozen.
    std::optional<core::OrdinalDay> observer_day;
    bool observer_paused = true;
    std::optional<std::vector<ResearchAppointment>> career{};
    auto operator<=>(const ObjectivesState&) const = default;
};
inline constexpr std::size_t objective_selection_limit = 128;

enum class ObjectiveResult { pending, won, lost, unavailable, unfinished };
[[nodiscard]] std::string_view campaign_end_key(CampaignEndCause) noexcept;
[[nodiscard]] std::string_view objective_mode_key(ObjectiveMode) noexcept;
[[nodiscard]] bool objective_is_custom(const ObjectivesState&) noexcept;
[[nodiscard]] bool objective_time_stopped(const WorldState&) noexcept;
[[nodiscard]] bool observing_world(const WorldState&) noexcept;
[[nodiscard]] core::OrdinalDay campaign_day(const WorldState&) noexcept;
[[nodiscard]] core::OrdinalDay campaign_calendar_limit(const WorldState&);
[[nodiscard]] bool campaign_paused(const WorldState&) noexcept;
[[nodiscard]] bool has_asi_objective(const WorldState&) noexcept;
[[nodiscard]] std::optional<core::ObjectiveKind> required_objective_review(const WorldState&);
[[nodiscard]] std::optional<core::ObjectiveKind> objective_postmortem_candidate(const WorldState&);
[[nodiscard]] std::optional<core::OrdinalDay> next_objective_boundary(const WorldState&);
[[nodiscard]] ObjectiveResult objective_result(const WorldState&,core::ObjectiveKind);
[[nodiscard]] std::string objective_breach_explanation(const WorldState&,core::ObjectiveKind);

void initialize_objectives(WorldState&,const core::ObjectiveCharter&,std::optional<core::ObjectiveSelection> = {});
void settle_objectives(WorldState&,std::optional<core::OrdinalDay>,const CompiledProgram&);
void validate_objectives(const WorldState&,const CompiledProgram&,bool deep);
void select_objectives(WorldState&,const core::ObjectiveSelection&);
void acknowledge_objective(WorldState&,core::ObjectiveKind);
void open_campaign_postmortem(WorldState&);
[[nodiscard]] std::optional<std::uint64_t> completed_founder_departure(const WorldState&,core::OrdinalDay);
void finish_founder_run(WorldState&);
void continue_campaign(WorldState&);

} // namespace pgp::runtime
