#include "campaign_facts.hpp"
#include <pgp/runtime/romance.hpp>

#include <algorithm>

namespace pgp::runtime::detail {
namespace {
template<class State, class Member, class Predicate>
std::int64_t count(const std::optional<State>& state, Member member, Predicate predicate) {
    return state ? std::ranges::count_if((*state).*member, predicate) : 0;
}

bool offer_eligible(const core::SimulationState& s, std::string_view offer_id) {
    if (!s.business_offers || !s.accounts || !s.capacity) return false;
    const auto& offers = s.business_offers->catalog;
    const auto offer = std::ranges::find(offers, offer_id, &core::BusinessOffer::id);
    if (offer == offers.end() || s.date < offer->available_from || s.date >= offer->available_before) return false;
    core::Cents reserved{0};
    for (const auto& entry : s.reservations) reserved = core::checked_unit_add(reserved, entry.reserved);
    return core::business_offer_eligible(*s.business_offers, *offer, *s.accounts, *s.capacity, s.market, s.date, reserved);
}
}

content::Scalar campaign_fact(const WorldState& world, std::string_view id) {
    using namespace core;
    const auto& s = world.simulation;
    const auto* founder=world.founding?&*world.founding:nullptr;
    if(id=="company.founder_children" || id=="company.founder_dependents" || id=="company.family_plans" || id=="company.family_applications") {
        if(!world.family)return INT64_C(0);const auto& family=*world.family;
        if(id=="company.family_plans")return static_cast<std::int64_t>(std::ranges::count_if(family.pending,[](const auto& row){return row.second.first=="founder" || row.second.second=="founder";}));
        if(id=="company.family_applications")return static_cast<std::int64_t>(std::ranges::count_if(family.members,[&](const auto& member){return member.application && member.application->company==world.company_id;}));
        std::set<std::string> children;for(const auto& edge:family.edges)if(edge.parent=="founder")if(const auto* child=family_member(world,edge.child);child && (id=="company.founder_children" || (!edge.ended_on && !adult_on(*child->profile.romantic,campaign_day(world)))))children.insert(edge.child);
        return static_cast<std::int64_t>(children.size());
    }
    if(id=="company.founder_dating" || id=="company.founder_engaged" || id=="company.founder_married") {
        const auto* bond=romantic_partner(world,"founder");
        return std::int64_t{bond && (id=="company.founder_married"?bond->married_on.has_value():id=="company.founder_engaged"?bond->engaged_on.has_value() && !bond->married_on:!bond->engaged_on)};
    }
    if(id=="company.industry_contacts" || id=="company.industry_close_friends" || id=="company.referral_contacts") {
        if(!social_rules(s))return INT64_C(0);
        const auto& people=*s.management->people;
        if(id=="company.referral_contacts")return static_cast<std::int64_t>(people.social->referrals.size());
        return static_cast<std::int64_t>(std::ranges::count_if(people.social->links,[&](const auto& bond){
            const auto other=bond.first=="founder"?bond.second:bond.second=="founder"?bond.first:std::string{};
            return bond.known && people.industry.contains(other) && (id=="company.industry_contacts" || bond.affinity>=social_rules(s)->thresholds[3]);}));
    }
    if(id=="company.founder_close_friends" || id=="company.known_close_relationships") {
        if(!social_rules(s))return INT64_C(0);
        return static_cast<std::int64_t>(std::ranges::count_if(s.management->people->social->links,[&](const auto& bond){
            return bond.known && bond.affinity>=social_rules(s)->thresholds[3] && (id=="company.known_close_relationships" || bond.first=="founder" || bond.second=="founder");}));
    }
    if(id=="company.founder_programming")return person_has_skill(s,"founder","programming");
    if(id=="company.has_programmer" || id=="company.has_architect") {
        if(!talent_rules(s))return false;
        return std::ranges::any_of(s.management->people->employees,[&](const auto& e){return !e.departed_on && e.ready_on<=s.date && person_has_skill(s,e.terms.profile.id,id=="company.has_programmer"?"programming":"architecture");});
    }
    if(id=="company.completed_courses")return talent_rules(s)?static_cast<std::int64_t>(std::ranges::count_if(s.management->people->talent->courses,[](const auto& c){return c.completed;})):INT64_C(0);
    if(id=="company.released_software")return talent_rules(s)?static_cast<std::int64_t>(std::ranges::count_if(s.management->people->talent->software,[](const auto& p){return p.released_on.has_value();})):INT64_C(0);
    if(id=="company.founder_genius") return founder && founder->leg=="genius";
    if(id=="company.founder_outgoing") return founder && founder->sociability=="outgoing";
    if(id=="company.founder_athletic") return founder && founder->activity=="athletic";
    if(id=="company.is_nonprofit") return founder && founder->legal_form=="nonprofit";
    if(id=="company.founder_age") return founder?static_cast<std::int64_t>(std::max(0,from_ordinal(s.date).year-1997)):INT64_C(0);
    if(id=="company.founder_gender") return founder?founder->gender:std::string("unrecorded");
    if(id=="company.founder_leg_up") return founder?founder->leg:std::string("unrecorded");
    if(id=="company.founder_family") return founder?founder->family:std::string("unrecorded");
    if(id.starts_with("company.childhood_")) {
        if(founder) if(const auto found=founder->choices.find(id.substr(std::string_view("company.childhood_").size()));found!=founder->choices.end()) return found->second;
        return std::string("unrecorded");
    }
    if (id == "company.small_supplier_eligible") return offer_eligible(s, "offer.supplier_small");
    if (id == "company.document_recovery_eligible") return offer_eligible(s, "offer.recovery_pilot");
    if (id == "company.regional_recovery_eligible") return offer_eligible(s, "offer.regional_recovery");
    if (id == "company.cooperative_burst_eligible") return offer_eligible(s, "offer.cooperative_burst");
    if (id == "company.independent_lab_eligible") return offer_eligible(s, "offer.independent_lab");
    if (id == "company.campaign_days" || id == "world.campaign_days")
        return std::max<std::int64_t>(0, checked_subtract(s.date.value(), world.campaign_start.value()));
    const auto& lab = s.lab;
    if (id == "company.data_licenses") return lab ? static_cast<std::int64_t>(lab->licenses.size()) : 0;
    if (id == "company.open_licenses" || id == "company.research_only_licenses")
        return count(lab, &LabState::licenses, [&](const DataLicense& license) {
            const auto offer = std::ranges::find(lab->catalog.data, license.offer_id, &DataOffer::id);
            return offer != lab->catalog.data.end() &&
                (id == "company.open_licenses" ? offer->open_weight_release : !offer->commercial_use);
        });
    if (id == "company.trained_models")
        return count(lab, &LabState::models, [](const auto& model) { return model.consumed_work.value() > 0; });
    if (id == "company.independent_evaluations")
        return count(lab, &LabState::evaluations, [&](const auto& evidence) {
            return evidence.completed && std::ranges::any_of(lab->catalog.evaluations, [&](const auto& plan) {
                return plan.id == evidence.plan_id && plan.independent;
            });
        });
    if (id == "company.published_research") return lab ? static_cast<std::int64_t>(lab->publications.size()) : 0;
    if (id == "company.adopted_research") return lab ? static_cast<std::int64_t>(lab->insights.size()) : 0;

    const auto& frontier = s.frontier;
    if (id == "company.frontier_systems") return frontier ? static_cast<std::int64_t>(frontier->systems.size()) : 0;
    if (id == "company.frontier_programs") return frontier ? static_cast<std::int64_t>(frontier->research_programs.size()) : 0;
    if (id == "company.frontier_assessments")
        return count(frontier, &FrontierState::trials, [](const auto& trial) {
            return trial.kind == FrontierTrialKind::assessment && trial.result.has_value();
        });
    if (id == "company.successful_refreshes" || id == "company.held_refreshes") {
        if (!frontier) return std::int64_t{0};
        std::int64_t total = 0;
        for (const auto& program : frontier->refresh_programs)
            total += std::ranges::count_if(program.runs, [&](const auto& run) {
                return run.finished_on && (id == "company.successful_refreshes" ? run.rollout_deployment != 0
                                                                              : run.rollout_deployment == 0);
            });
        return total;
    }
    if (id == "company.product_launches")
        return s.deployments ? static_cast<std::int64_t>(s.deployments->records.size()) : 0;
    if (id == "company.active_services" || id == "company.hosted_services")
        return count(s.market, &MarketState::products, [&](const auto& product) {
            return product.available &&
                   (id == "company.active_services" || customer_hosted(product.channel));
        });
    if (id == "company.active_contracts" || id == "company.missed_contracts")
        return count(s.market, &MarketState::contracts, [&](const auto& contract) {
            return contract.status == (id == "company.active_contracts" ? ContractStatus::active : ContractStatus::missed);
        });
    if (id == "company.price_changes") return s.market ? static_cast<std::int64_t>(s.market->prices.size()) : 0;
    if (id == "company.contract_shortfalls_today" || id == "company.served_tasks_today") {
        if (!s.market || s.market->days.empty()) return std::int64_t{0};
        const auto& day = s.market->days.back();
        if (day.day.value() + 1 != s.date.value()) return std::int64_t{0};
        if (id == "company.contract_shortfalls_today")
            return std::ranges::count_if(day.contracts, [](const auto& row) { return row.delivered_tasks < row.promised_tasks; });
        std::int64_t tasks = 0;
        for (const auto& row : day.contracts) tasks += static_cast<std::int64_t>(row.delivered_tasks);
        for (const auto& row : day.discretionary) tasks += static_cast<std::int64_t>(row.tasks);
        return tasks;
    }

    if (s.management && s.management->governance) {
        const auto& g = *s.management->governance;
        if (id == "company.executive_appointments")
            return std::ranges::count_if(g.appointments, [](const auto& row) { return !row.departed_on; });
        if (id == "company.vesting_grants")
            return std::ranges::count_if(g.grants, [](const auto& row) { return !row.departed && row.unvested.value() > 0; });
        if (id == "company.open_board_votes")
            return std::ranges::count_if(g.motions, [](const auto& row) { return row.status == BoardMotionStatus::pending; });
        if (id == "company.board_refusals")
            return std::ranges::count_if(g.motions, [](const auto& row) {
                return row.status == BoardMotionStatus::rejected || row.status == BoardMotionStatus::expired;
            });
        if (id == "company.executive_remedies")
            return std::ranges::count_if(g.disputes, [](const auto& row) {
                return row.status == CorporateDisputeStatus::resolved || row.status == CorporateDisputeStatus::waived ||
                       row.status == CorporateDisputeStatus::accelerated;
            });
        if (id == "company.capital_events")
            return std::ranges::count_if(g.motions, [](const auto& row) {
                return row.kind == BoardMotionKind::financing && row.executed;
            });
    }
    if (id == "company.handover_count")
        return s.succession ? static_cast<std::int64_t>(s.succession->terms.size()) - 1 : 0;
    if (id == "company.resolved_workouts")
        return count(s.liquidity, &LiquidityState::workouts, [](const auto& row) { return row.resolved_on.has_value(); });
    if (id == "company.acquisitions")
        return count(world.competition, &CompetitionState::acquisitions, [](const auto& row) { return row.completed_on.has_value(); });

    if (id == "company.site_orders" || id == "company.online_sites")
        return count(s.infrastructure, &InfrastructureState::sites, [&](const auto& row) {
            return !row.closed_on && (id == "company.site_orders" || row.commissioned_on.has_value());
        });
    if (id == "company.online_racks")
        return count(s.infrastructure, &InfrastructureState::hardware, [](const auto& row) {
            return row.commissioned_on.has_value() && !row.retired_on;
        });
    if (id == "company.open_site_incidents") return s.resilience ? static_cast<std::int64_t>(s.resilience->open_incidents.size()) : 0;
    if (id == "company.site_repairs")
        return count(s.resilience, &ResilienceState::incidents, [](const auto& row) { return row.repaired_on.has_value(); });
    if (id == "company.paid_site_claims")
        return count(s.resilience, &ResilienceState::incidents, [](const auto& row) { return row.paid_on.has_value(); });
    if (id == "company.ready_replicas")
        return count(s.resilience, &ResilienceState::replicas, [](const auto& row) { return row.ready_on && !row.stopped_on; });
    if (id == "company.wholesale_contracts")
        return count(world.competition, &CompetitionState::wholesale, [&](const auto& row) {
            return row.starts_on <= s.date && s.date < row.ends_before && !row.terminated_on;
        });
    if (id == "company.used_wholesale_contracts")
        return count(world.competition, &CompetitionState::wholesale, [](const auto& row) {
            return std::ranges::any_of(row.days, [](const auto& day) { return day.used.value() > 0; });
        });

    if (id == "company.public_testimonies") return world.authority ? static_cast<std::int64_t>(world.authority->testimonies.size()) : 0;
    if (id == "company.fulfilled_promises")
        return count(world.authority, &AuthorityState::promises, [](const auto& row) { return row.met_on.has_value(); });
    if (id == "company.public_cases" || id == "company.public_remedies")
        return count(world.authority, &AuthorityState::cases, [&](const auto& row) {
            return id == "company.public_cases" ? !row.resolved_on && !row.disclosed_on : row.resolved_on.has_value();
        });
    if (id == "company.approved_mandates")
        return count(world.authority, &AuthorityState::mandates, [&](const auto& row) {
            return s.date >= row.decision_on && s.date < row.ends_before && !row.withdrawn_on && !row.revoked_on &&
                   !row.reviews.empty() && row.reviews.front().approved;
        });
    if (id == "world.rival_entries" || id == "world.rival_closures")
        return count(world.competition, &CompetitionState::notices, [&](const auto& notice) {
            return notice.kind == (id == "world.rival_entries" ? "entry" : "closure");
        });
    if (world.society && !world.society->months.empty()) {
        const auto& month = world.society->months.back();
        if (id == "world.public_regions")
            return std::ranges::count_if(month.outcomes, [](const auto& row) { return row.player_public > 0; });
        if (id == "world.durable_regions" && month.durable_gains)
            return std::ranges::count_if(*month.durable_gains, [](auto gain) { return gain > 0; });
    }
    return std::int64_t{0};
}
}
