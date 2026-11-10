#include <pgp/session/milestone.hpp>
#include <pgp/runtime/authority.hpp>
#include <algorithm>
#include <stdexcept>
namespace pgp::session {
Milestone plan_milestone(const runtime::Checkpoint& checkpoint,std::string_view kind) {
    const auto& s=checkpoint.world.simulation;Milestone result;result.kind=kind;
    if(kind=="objectives") {
        result.deadline=runtime::next_objective_boundary(checkpoint.world);
        if(!result.deadline) throw std::invalid_argument("There is no later scored objective check in this campaign.");
    } else if(runtime::observing_world(checkpoint.world)) {
        if(kind=="society" && checkpoint.world.society)
            result.deadline=core::next_society_boundary(*checkpoint.world.society,runtime::campaign_day(checkpoint.world));
        if(!result.deadline) throw std::invalid_argument("Observer mode can advance to a regional world date.");
    } else if (kind=="project" || kind=="evaluation") {
        if (s.capacity) for (const auto& work:s.capacity->projects)
            if (work.status==core::ProjectStatus::active && work.kind!=core::ProjectKind::serving && work.kind!=core::ProjectKind::replication &&
                (kind=="project" || work.kind==core::ProjectKind::evaluation)) result.projects.push_back(work.id);
        if (result.projects.empty()) throw std::invalid_argument("Start a matching finite project before advancing to its completion.");
    } else if (kind=="gathering") {
        if(s.succession && s.succession->personal_life) {
            const auto& owned=*s.succession->personal_life;
            for(const auto id:owned.pending_events) {
                const auto day=owned.events[id-1].day;
                if(day>s.date && (!result.deadline || day<*result.deadline))result.deadline=day;
            }
        }
        if(!result.deadline)throw std::invalid_argument("There is no upcoming gathering.");
    } else if(kind=="family_visit") {
        if(const auto* trip=core::pending_family_visit(s))
            for(const auto day:{trip->departs_on,trip->arrives_on,trip->leaves_on,trip->returns_on})
                if(day>s.date) {result.deadline=day;break;}
        if(!result.deadline) throw std::invalid_argument("There is no upcoming family visit milestone.");
    } else if (kind=="leadership") {
        result.deadline=core::next_succession_boundary(s);
        if(!result.deadline) throw std::invalid_argument("There is no later leadership boundary within this campaign.");
    } else if (kind=="site") {
        const auto consider=[&](core::OrdinalDay day){if (day>s.date && day<=s.end_date && (!result.deadline || day<*result.deadline)) result.deadline=day;};
        if (s.infrastructure) {
            const auto& owned=*s.infrastructure;
            for (const auto& site:owned.sites) if (!site.closed_on) {consider(site.built_on);consider(site.grid_on);}
            for (const auto& rack:owned.hardware) if (!rack.retired_on) consider(rack.delivered_on);
            for (const auto& hire:owned.hires) if (std::any_of(owned.sites.begin(),owned.sites.end(),[&](const auto& site){return site.id==hire.site_id && !site.closed_on;})) consider(hire.starts_on);
            for (const auto& appointment:owned.appointments) if (!appointment.cancelled_on) {consider(appointment.starts_on);consider(appointment.visit_on);consider(appointment.ends_before);}
            if (owned.initial_lease_ends_before) consider(*owned.initial_lease_ends_before);
            if (s.resilience) {
                for (const auto& protection:s.resilience->protections) consider(protection.active_from);
                for (const auto& incident:s.resilience->incidents) {
                    consider(incident.repair_due);
                    if (incident.repaired_on) consider(core::advance_days(*incident.repaired_on,incident.restoration_days));
                    if (incident.claim.value()>0 && !incident.paid_on) consider(incident.claim_due);
                }
            }
        }
        if (!result.deadline) throw std::invalid_argument("There is no future construction, delivery, staffing, inspection or lease milestone.");
    } else if (kind=="civic") {
        if (s.civic && s.civic->next_boundary && *s.civic->next_boundary > s.date && *s.civic->next_boundary < s.end_date)
            result.deadline = s.civic->next_boundary;
        if (!result.deadline) throw std::invalid_argument("There is no later civic event within the campaign.");
    } else if (kind=="society") {
        if(checkpoint.world.society) result.deadline=core::next_society_boundary(*checkpoint.world.society,s.date);
        if(result.deadline && *result.deadline>=s.end_date) result.deadline.reset();
        if(!result.deadline) throw std::invalid_argument("There is no later regional month or forum date within this campaign.");
    } else if (kind=="institutions") {
        result.deadline=runtime::next_authority_boundary(checkpoint.world);
        if(!result.deadline) throw std::invalid_argument("There is no later institutional response or review date in this campaign.");
    } else if (kind=="deadline") {
        const auto consider=[&](core::OrdinalDay day){if (day>=s.date && day<=s.end_date && (!result.deadline || day<*result.deadline)) result.deadline=day;};
        if (s.accounts) {
            for (const auto& invoice:s.accounts->invoices) if (invoice.settled<invoice.amount) consider(invoice.due);
            for (const auto& debt:s.accounts->debts) if (debt.principal.value()>0) consider(debt.due);
        }
        if (s.market) {
            for (const auto& contract:s.market->contracts) {
                if (contract.status==core::ContractStatus::pending) consider(contract.start_by);
                if (contract.status==core::ContractStatus::pending || contract.status==core::ContractStatus::active) consider(contract.ends_before);
            }
            for (const auto& product:s.market->products)
                if (product.retirement && product.retirement->retires_on>s.date) consider(product.retirement->retires_on);
            for (const auto& term:s.market->compatibility) if (term.ends_before>s.date) consider(term.ends_before);
            for (const auto& trial:s.market->trials) if (!trial.stopped_on) consider(trial.ends_before);
        }
        if (s.liquidity) if (const auto deadline=core::liquidity_report(*s.liquidity).deadline) consider(*deadline);
        if (s.management) {
            if (s.management->governance) {
                const auto& g=*s.management->governance;const auto& c=*s.management->catalog.governance;
                for (const auto& m:g.motions) if (m.status==core::BoardMotionStatus::pending) consider(m.deadline);
                for (const auto& d:g.disputes) if (d.status==core::CorporateDisputeStatus::concern || d.status==core::CorporateDisputeStatus::formal) {
                    consider(d.deadline);if (d.status==core::CorporateDisputeStatus::concern) consider(core::advance_days(d.opened_on,c.concern_period));
                }
                for (const auto& a:g.appointments) if (!a.departed_on) {
                    const auto it=std::find_if(c.executives.begin(),c.executives.end(),[&](const auto& e){return e.id==a.candidate;});
                    if (it!=c.executives.end() && !it->publication_research.empty() && std::none_of(s.lab->publications.begin(),s.lab->publications.end(),[&](const auto& p){return p.research_id==it->publication_research;})) consider(core::advance_days(a.hired_on,it->promise_term));
                }
            }
            for (const auto& setback:s.management->catalog.setbacks) {
                if (setback.announced_on<=s.date && s.date<setback.starts_on) consider(setback.starts_on);
                if (setback.starts_on<=s.date && s.date<setback.ends_before) consider(setback.ends_before);
            }
        }
        if (checkpoint.world.competition) {
            for (const auto& notice:checkpoint.world.competition->notices) if (notice.response_due && notice.kind!="offer") consider(*notice.response_due);
            for (const auto& offer:checkpoint.world.competition->offers) {
                if (offer.status==runtime::CompetitiveOfferStatus::pending) consider(offer.respond_by);
                else if (offer.status==runtime::CompetitiveOfferStatus::active && offer.due_on) consider(*offer.due_on);
            }
        }
        if (s.business_offers) for (const auto& accepted:s.business_offers->accepted) {
            const auto offer=std::find_if(s.business_offers->catalog.begin(),s.business_offers->catalog.end(),[&](const auto& o){return o.id==accepted.offer_id;});
            if (offer!=s.business_offers->catalog.end() && offer->kind==core::BusinessOfferKind::compute_lease) consider(core::lease_expiry(*s.business_offers,offer->id));
        }
        if(const auto next=core::next_succession_boundary(s)) consider(*next);
        if(const auto next=runtime::next_objective_boundary(checkpoint.world)) consider(*next);
        if (!result.deadline) throw std::invalid_argument("There is no known future deadline inside this scenario.");
        if (*result.deadline==s.date) throw std::invalid_argument("A real deadline is today. Review the obligation before advancing.");
    } else throw std::invalid_argument("Choose a project, evaluation, site, civic event or actual deadline.");
    return result;
}
bool milestone_reached(const runtime::Checkpoint& checkpoint,const Milestone& milestone) {
    const auto& s=checkpoint.world.simulation;
    if(runtime::objective_time_stopped(checkpoint.world) || runtime::required_objective_review(checkpoint.world)) return true;
    if(runtime::observing_world(checkpoint.world))
        return !milestone.deadline || runtime::campaign_day(checkpoint.world)>=*milestone.deadline;
    if (s.phase==core::DayPhase::finished || s.phase==core::DayPhase::closed) return true;
    if (milestone.deadline) return s.date>=*milestone.deadline;
    if (!s.capacity) return true;
    for (const auto& id:milestone.projects) {
        const auto found=std::find_if(s.capacity->projects.begin(),s.capacity->projects.end(),[&](const auto& work){return work.id==id;});
        if (found==s.capacity->projects.end() || found->status!=core::ProjectStatus::active) return true;
    }
    return false;
}
}
