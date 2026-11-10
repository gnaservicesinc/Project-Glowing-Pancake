#pragma once
#include <pgp/core/succession.hpp>
#include <type_traits>

namespace pgp::content::succession_codec {
template<class T> inline constexpr unsigned enum_count=std::is_same_v<T,core::FounderRole>?3:std::is_same_v<T,core::SuccessionStatus>?7:0;
template<class T,class F> void fields(T& r,F field) {
    using V=std::remove_cvref_t<T>;
    if constexpr(std::is_same_v<V,core::SuccessionCatalog>) {
        field("founder_name",r.founder_name);field("founder_board",r.founder_board);field("founder_birth_year",r.founder_birth_year);
        field("executive_retirement_age",r.executive_retirement_age);field("chair_retirement_age",r.chair_retirement_age);
        field("minimum_candidate_age",r.minimum_candidate_age);field("maximum_candidate_age",r.maximum_candidate_age);
        field("maximum_term_years",r.maximum_term_years);field("caretaker_term_years",r.caretaker_term_years);
        field("chair_seats",r.chair_seats);field("trust_seats",r.trust_seats);field("planning_notice_days",r.planning_notice);
        field("transition_days",r.transition_days);field("vote_window_days",r.vote_window);field("transition_work_milli_CU",r.transition_work);
        field("founder_salary_cents",r.founder_salary);field("chair_salary_cents",r.chair_salary);field("cash_floor_cents",r.cash_floor);
        field("personal_cash_cents",r.personal_cash);field("personal_liabilities_cents",r.personal_liabilities);field("tracks",r.tracks);
        field("departure",r.departure,true);
    } else if constexpr(std::is_same_v<V,core::DepartureRules>) {
        field("messages",r.messages);field("broken_effect_bp",r.broken_effect);
    } else if constexpr(std::is_same_v<V,core::DepartureMessage>) {
        field("id",r.id);field("title",r.title);field("copy",r.copy);field("fee_cents",r.fee);field("completed_effect_bp",r.completed_effect);
    } else if constexpr(std::is_same_v<V,core::DepartureRelease>) {
        field("plan",r.plan);field("posting_cursor",r.posting_cursor);field("message",r.message);field("day",r.day);
    } else if constexpr(std::is_same_v<V,core::LeadershipTrack>) {
        field("id",r.id);field("priorities",r.priorities);field("names",r.names);field("first_birth_year",r.first_birth_year);
        field("cohort_years",r.cohort_years);field("signing_cost_cents",r.signing_cost);field("monthly_salary_cents",r.monthly_salary);
    } else if constexpr(std::is_same_v<V,core::LeadershipTerm>) {
        field("id",r.id);field("plan",r.plan);field("person",r.person);field("name",r.name);field("birth_year",r.birth_year);
        field("starts_on",r.starts_on);field("due_on",r.due_on);field("ended_on",r.ended_on);field("caretaker",r.caretaker);field("portrait",r.portrait,true);
    } else if constexpr(std::is_same_v<V,core::PortraitRecipe>) {
        field("kit",r.kit);field("parts",r.parts);field("style",r.style,true);
    } else if constexpr(std::is_same_v<V,core::LeadershipControl>) {
        field("day",r.day);field("term",r.term);field("role",r.role);
    } else if constexpr(std::is_same_v<V,core::SuccessionPlan>) {
        field("id",r.id);field("incumbent",r.incumbent);field("candidate",r.candidate);field("project",r.project);
        field("proposed_on",r.proposed_on);field("vote_before",r.vote_before);field("handover_on",r.handover_on);
        field("retain_chair",r.retain_chair);field("allocation",r.allocation);field("status",r.status);
        field("decided_on",r.decided_on);field("finished_on",r.finished_on);field("player_vote",r.player_vote);
        field("reviewed_cash_cents",r.reviewed_cash);field("posting_cursor",r.posting_cursor);field("control_cursor",r.control_cursor);field("ballots",r.ballots);
    } else if constexpr(std::is_same_v<V,core::BoardBallot>) {
        field("member",r.member);field("seats",r.seats);field("support",r.support);
    } else if constexpr(std::is_same_v<V,core::SuccessionNotice>) {
        field("id",r.id);field("term",r.term);field("plan",r.plan);field("kind",r.kind);field("day",r.day);field("deadline",r.deadline);field("acknowledged",r.acknowledged);
    } else if constexpr(std::is_same_v<V,core::SuccessionState>) {
        field("founder_holder",r.founder_holder);field("personal_cash_cents",r.personal_cash);field("posting_cursor",r.posting_cursor);
        field("terms",r.terms);field("controls",r.controls);field("plans",r.plans);field("notices",r.notices);
        field("founder_name",r.founder_name,true);field("releases",r.releases,true);
    }
}
}
