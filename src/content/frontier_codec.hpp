#pragma once

#include <pgp/core/frontier.hpp>
#include <type_traits>

namespace pgp::content::frontier_codec {
template<class T> inline constexpr std::size_t enum_count =
    std::is_same_v<T,core::FrontierTechnology> ? 4 : std::is_same_v<T,core::FrontierTrialKind> ? 3 : 0;

template<class T, class F> void fields(T& row, F field) {
    using V = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<V,core::FrontierCatalog>) {
        field("name",row.name);
        field("assumptions",row.assumptions);
        field("branches_after_day",row.branches_after);
        field("asi_route",row.asi_route);
        field("paths",row.paths);
        field("gates",row.gates);
        field("corpora",row.corpora);
        field("recipes",row.recipes);
        field("panels",row.panels);
        field("experiments",row.experiments);
        field("minimum_field_tasks_per_day",row.minimum_field_tasks_per_day);
        field("integration_loss_bp",row.integration_loss);
        field("refresh_research",row.refresh_research);
        field("refresh_review_age_days",row.refresh_review_age);
    } else if constexpr (std::is_same_v<V,core::FuturePath>) {
        field("id",row.id);
        field("name",row.name);
        field("description",row.description);
        field("minimum_bp",row.minimum);
        field("maximum_bp",row.maximum);
        field("ceiling_bp",row.ceiling);
    } else if constexpr (std::is_same_v<V,core::FrontierGate>) {
        field("kind",row.kind);
        field("target",row.target);
        field("minimum_bp",row.minimum);
    } else if constexpr (std::is_same_v<V,core::FrontierCorpus>) {
        field("id",row.id);
        field("coverage_bp",row.coverage);
    } else if constexpr (std::is_same_v<V,core::FrontierRecipeProfile>) {
        field("id",row.id);
        field("baseline_bp",row.baseline);
        field("gain_bp",row.gain);
        field("ceiling_bp",row.ceiling);
        field("data_scale",row.data_scale);
    } else if constexpr (std::is_same_v<V,core::FrontierWorkTerms>) {
        field("fee_cents",row.fee);
        field("work_milli_CU",row.work);
        field("minimum_days",row.minimum_days);
        field("minimum_staff",row.minimum_staff);
    } else if constexpr (std::is_same_v<V,core::FrontierPanel>) {
        field("id",row.id);
        field("name",row.name);
        field("reviewer",row.reviewer);
        field("holdout",row.holdout);
        field("terms",row.terms);
        field("margin_bp",row.margin);
    } else if constexpr (std::is_same_v<V,core::FrontierExperiment>) {
        field("id",row.id);
        field("name",row.name);
        field("research",row.research);
        field("reviewer",row.reviewer);
        field("verification",row.verification);
        field("pilot",row.pilot);
        field("replication",row.replication);
        field("baseline_bp",row.baseline);
        field("possible_gain_bp",row.possible_gain);
        field("required_gain_bp",row.required_gain);
        field("margin_bp",row.margin);
    } else if constexpr (std::is_same_v<V,core::FrontierSystem>) {
        field("id",row.id);
        field("name",row.name);
        field("registered_on",row.registered_on);
        field("deployments",row.deployments);
    } else if constexpr (std::is_same_v<V,core::FrontierResult>) {
        field("observed_on",row.observed_on);
        field("lower_bp",row.lower);
        field("field_tasks",row.field_tasks);
        field("gain_bp",row.gain);
        field("valid",row.valid);
    } else if constexpr (std::is_same_v<V,core::FrontierTrial>) {
        field("id",row.id);
        field("system",row.system);
        field("kind",row.kind);
        field("protocol",row.protocol);
        field("project",row.project);
        field("starts_on",row.starts_on);
        field("result",row.result);
    } else if constexpr (std::is_same_v<V,core::LabAllocation>) {
        field("source",row.source_id);
        field("compute_milli_CU_per_day",row.daily_compute);
        field("staff",row.staff);
    } else if constexpr (std::is_same_v<V,core::FrontierBudget>) {
        field("allocation",row.allocation);
        field("limit_cents",row.limit);
        field("reserve_cents",row.reserve);
        field("interval_days",row.interval);
        field("maximum_runs",row.runs);
    } else if constexpr (std::is_same_v<V,core::FrontierResearchPlan>) {
        field("topics",row.topics);
        field("budget",row.budget);
        field("standing",row.standing);
    } else if constexpr (std::is_same_v<V,core::FrontierResearchDecision>) {
        field("day",row.day);
        field("project",row.project);
        field("paid_cents",row.paid);
    } else if constexpr (std::is_same_v<V,core::FrontierResearchProgram>) {
        field("id",row.id);
        field("plan",row.plan);
        field("authorized_on",row.authorized_on);
        field("cancelled_on",row.cancelled_on);
        field("decisions",row.decisions);
        field("waiting",row.waiting);
    } else if constexpr (std::is_same_v<V,core::FrontierRefreshPlan>) {
        field("service",row.service);
        field("recipe",row.recipe);
        field("evaluation",row.evaluation);
        field("data",row.data);
        field("budget",row.budget);
        field("work_milli_CU",row.work);
        field("improvement_bp",row.improvement);
    } else if constexpr (std::is_same_v<V,core::FrontierRefreshRun>) {
        field("previous_deployment",row.previous_deployment);
        field("rollout_deployment",row.rollout_deployment);
        field("starts_on",row.starts_on);
        field("evaluated_on",row.evaluated_on);
        field("finished_on",row.finished_on);
        field("outcome",row.outcome);
    } else if constexpr (std::is_same_v<V,core::FrontierRefreshProgram>) {
        field("id",row.id);
        field("plan",row.plan);
        field("authorized_on",row.authorized_on);
        field("cancelled_on",row.cancelled_on);
        field("runs",row.runs);
        field("waiting",row.waiting);
    } else if constexpr (std::is_same_v<V,core::FrontierState>) {
        field("catalog",row.catalog);
        field("seed",row.seed);
        field("path",row.path);
        field("conditions_bp",row.conditions);
        field("systems",row.systems);
        field("trials",row.trials);
        field("research_programs",row.research_programs);
        field("refresh_programs",row.refresh_programs);
        field("policies_reviewed_on",row.policies_reviewed_on);
    }
}
}
