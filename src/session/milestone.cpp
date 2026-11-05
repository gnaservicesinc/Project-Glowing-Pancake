#include <pgp/session/milestone.hpp>
#include <algorithm>
#include <stdexcept>
namespace pgp::session {
Milestone plan_milestone(const runtime::Checkpoint& checkpoint,std::string_view kind) {
    const auto& s=checkpoint.world.simulation;Milestone result;result.kind=kind;
    if (kind=="project" || kind=="evaluation") {
        if (s.capacity) for (const auto& work:s.capacity->projects)
            if (work.status==core::ProjectStatus::active && work.kind!=core::ProjectKind::serving && (kind=="project" || work.kind==core::ProjectKind::evaluation)) result.projects.push_back(work.id);
        if (result.projects.empty()) throw std::invalid_argument("Start a matching finite project before advancing to its completion.");
    } else if (kind=="site") {
        const auto consider=[&](core::OrdinalDay day){if (day>s.date && day<=s.end_date && (!result.deadline || day<*result.deadline)) result.deadline=day;};
        if (s.infrastructure) {
            const auto& owned=*s.infrastructure;
            for (const auto& site:owned.sites) if (!site.closed_on) {consider(site.built_on);consider(site.grid_on);}
            for (const auto& rack:owned.hardware) if (!rack.retired_on) consider(rack.delivered_on);
            for (const auto& hire:owned.hires) if (std::any_of(owned.sites.begin(),owned.sites.end(),[&](const auto& site){return site.id==hire.site_id && !site.closed_on;})) consider(hire.starts_on);
            for (const auto& appointment:owned.appointments) if (!appointment.cancelled_on) {consider(appointment.starts_on);consider(appointment.visit_on);consider(appointment.ends_before);}
            if (owned.initial_lease_ends_before) consider(*owned.initial_lease_ends_before);
        }
        if (!result.deadline) throw std::invalid_argument("There is no future construction, delivery, staffing, inspection or lease milestone.");
    } else if (kind=="deadline") {
        const auto consider=[&](core::OrdinalDay day){if (day>=s.date && day<=s.end_date && (!result.deadline || day<*result.deadline)) result.deadline=day;};
        if (s.accounts) {
            for (const auto& invoice:s.accounts->invoices) if (invoice.settled<invoice.amount) consider(invoice.due);
            for (const auto& debt:s.accounts->debts) if (debt.principal.value()>0) consider(debt.due);
        }
        if (s.market) for (const auto& contract:s.market->contracts) {
            if (contract.status==core::ContractStatus::pending) consider(contract.start_by);
            if (contract.status==core::ContractStatus::pending || contract.status==core::ContractStatus::active) consider(contract.ends_before);
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
        if (!result.deadline) throw std::invalid_argument("There is no known future deadline inside this scenario.");
        if (*result.deadline==s.date) throw std::invalid_argument("A real deadline is today. Review the obligation before advancing.");
    } else throw std::invalid_argument("Choose project, evaluation, site milestone or actual deadline.");
    return result;
}
bool milestone_reached(const runtime::Checkpoint& checkpoint,const Milestone& milestone) {
    const auto& s=checkpoint.world.simulation;
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
