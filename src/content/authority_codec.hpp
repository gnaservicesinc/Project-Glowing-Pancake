#pragma once
#include <pgp/core/authority.hpp>
#include <type_traits>

namespace pgp::content::authority_codec {
template<class T> inline constexpr bool is_enum=std::is_same_v<T,core::AuthorityKind> ||
    std::is_same_v<T,core::PublicPromiseKind> || std::is_same_v<T,core::PublicCaseKind>;
template<class T,class F> void fields(T& r,F f) {
    using V=std::remove_cvref_t<T>;
    if constexpr(std::is_same_v<V,core::AuthorityCatalog>) {
        f("assumptions",r.assumptions);f("regions",r.regions);f("offers",r.offers);f("promise_window_days",r.promise_window);
        f("concern_period_days",r.concern_period);f("response_window_days",r.response_window);f("extension_days",r.extension);
        f("sanctions_days",r.sanctions);f("identity_memory_days",r.identity_memory);f("remedy_fee_cents",r.remedy_fee);
        f("reapplication_delay_days",r.reapplication_delay);f("directive_spacing_days",r.directive_spacing);
        f("remedy_compute_milli_CU_per_day",r.remedy_compute);f("remedy_staff",r.remedy_staff);f("promise_tasks",r.promise_tasks);
        f("minimum_operation_bp",r.minimum_operation);f("procurement_floor_bp",r.procurement_floor);
        f("initial_resistance_bp",r.initial_resistance);f("resistance_growth_bp",r.resistance_growth);f("resistance_decay_bp",r.resistance_decay);
        f("remedy_relief_bp",r.remedy_relief);f("resistance_threshold_bp",r.resistance_threshold);f("identity_effects_bp",r.identity_effects);
    } else if constexpr(std::is_same_v<V,core::AuthorityRegion>) {
        f("id",r.id);f("issuer",r.issuer);f("consent_bp",r.consent);
    } else if constexpr(std::is_same_v<V,core::AuthorityOffer>) {
        f("id",r.id);f("name",r.name);f("rights",r.rights);f("kind",r.kind);f("setup_cents",r.setup);f("monthly_cents",r.monthly);
        f("compute_milli_CU_per_day",r.compute);f("staff",r.staff);f("lead_days",r.lead);f("term_days",r.term);
        f("public_work",r.public_work);f("points_bp",r.points);f("evidence_floor_bp",r.evidence_floor);f("consent_floor_bp",r.consent_floor);
    } else if constexpr(std::is_same_v<V,core::PublicTestimony>) {
        f("id",r.id);f("report",r.report);f("day",r.day);
    } else if constexpr(std::is_same_v<V,core::PublicPromise>) {
        f("id",r.id);f("kind",r.kind);f("region",r.region);f("subject",r.subject);f("made_on",r.made_on);f("deployment",r.deployment);
        f("due_on",r.due_on);f("target",r.target);f("progress",r.progress);f("met_on",r.met_on);f("closed_on",r.closed_on);
    } else if constexpr(std::is_same_v<V,core::InstitutionalReview>) {
        f("day",r.day);f("work",r.work);f("required_work",r.required_work);f("public_work",r.public_work);
        f("required_public",r.required_public);f("operation_bp",r.operation);f("consent_bp",r.consent);f("score_bp",r.score);f("evidence_bp",r.evidence);f("approved",r.approved);
    } else if constexpr(std::is_same_v<V,core::ProcurementDirective>) {
        f("day",r.day);f("enabled",r.enabled);
    } else if constexpr(std::is_same_v<V,core::AuthorityMandate>) {
        f("id",r.id);f("testimony",r.testimony);f("promise",r.promise);f("forum_round",r.forum_round);
        f("deployment",r.deployment);
        f("offer",r.offer);f("region",r.region);f("service",r.service);f("project",r.project);f("source",r.source);f("site",r.site);
        f("applied_on",r.applied_on);f("decision_on",r.decision_on);f("ends_before",r.ends_before);
        f("withdrawn_on",r.withdrawn_on);f("staff_released_on",r.staff_released_on);f("revoked_on",r.revoked_on);f("reviews",r.reviews);f("directives",r.directives);
    } else if constexpr(std::is_same_v<V,core::PublicCase>) {
        f("id",r.id);f("subject",r.subject);f("testimony",r.testimony);f("kind",r.kind);f("region",r.region);f("opened_on",r.opened_on);
        f("deadline",r.deadline);f("negotiated_on",r.negotiated_on);f("resolved_on",r.resolved_on);f("disclosed_on",r.disclosed_on);
        f("cited_on",r.cited_on);
        f("remedy_project",r.remedy_project);f("acknowledged",r.acknowledged);
    } else if constexpr(std::is_same_v<V,core::ExecutiveNegotiation>) {
        f("dispute",r.dispute);f("project",r.project);f("day",r.day);
    } else if constexpr(std::is_same_v<V,core::AuthorityMonth>) {
        f("month_end",r.month_end);f("soft_bp",r.soft);f("hard_bp",r.hard);f("resistance_bp",r.resistance);
        f("population_coverage_bp",r.population_coverage);f("output_coverage_bp",r.output_coverage);
    } else if constexpr(std::is_same_v<V,core::AuthorityState>) {
        f("catalog",r.catalog);f("starts_on",r.starts_on);f("prepared_on",r.prepared_on);f("testimonies",r.testimonies);
        f("promises",r.promises);f("mandates",r.mandates);f("cases",r.cases);f("executive_negotiations",r.executive_negotiations);f("months",r.months);
    }
}
}
