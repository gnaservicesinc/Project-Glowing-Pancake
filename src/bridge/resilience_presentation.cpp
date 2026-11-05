#include "resilience_presentation.hpp"
#include "flow_presentation.hpp"
#include <algorithm>
#include <map>
namespace pgp::bridge {
namespace {
using namespace core;
QString text(std::string_view v) {return QString::fromUtf8(v.data(),static_cast<qsizetype>(v.size()));}
QString money(Cents value) {
    auto whole=QString::number(value.value()/100);
    for (auto position=whole.size()-3;position>0;position-=3) whole.insert(position,QLatin1Char(','));
    return QStringLiteral("$%1.%2").arg(whole).arg(value.value()%100,2,10,QLatin1Char('0'));
}
QString date(OrdinalDay value) {return text(iso_date(value));}
QString bp(BasisPoints value) {return QStringLiteral("%1.%2%").arg(value.value()/100).arg(value.value()%100,2,10,QLatin1Char('0'));}
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& entry){return entry.id==id;});
    if (row==rows.end()) throw std::invalid_argument("Select an available resilience record.");
    return *row;
}
QVariantMap option(std::string_view id,QString label,QString detail={}) {
    return {{QStringLiteral("id"),text(id)},{QStringLiteral("label"),std::move(label)},{QStringLiteral("detail"),std::move(detail)}};
}
QString protection_effect(const SiteProtectionOffer& offer) {
    switch (offer.kind) {
    case SiteProtectionKind::generator:
        return QStringLiteral("%1 kW emergency facility power for %2 days of a grid outage. Fuel costs %3 cents/kWh.")
            .arg(offer.emergency_watts/1000).arg(offer.emergency_days.value()).arg(static_cast<double>(offer.power_millicents_per_kwh)/1000.0,0,'f',3);
    case SiteProtectionKind::backup: return QStringLiteral("Offsite copies reduce the site's data recovery delay to %1 of its usual length.").arg(bp(offer.factor));
    case SiteProtectionKind::maintenance: return QStringLiteral("Preventive checks reduce the monthly fire probability to %1 of the published exposure.").arg(bp(offer.factor));
    case SiteProtectionKind::hardening: return QStringLiteral("Fire, storm, flood and earthquake repairs take %1 of the usual time and cost.").arg(bp(offer.factor));
    case SiteProtectionKind::cooling: return QStringLiteral("Cooling reserves reduce the capacity loss during heat events to %1 of the usual loss.").arg(bp(offer.factor));
    }
    return {};
}
QString insurance_terms(const SiteInsuranceOffer& offer) {
    QStringList hazards;
    for (const auto kind:offer.covered) hazards.push_back(readable(hazard_key(kind)));
    return QStringLiteral("%1 premium for %2 days. Coverage begins after %3 days.\n"
        "%4 deductible per incident · %5 reimbursement · %6 total policy limit.\n"
        "Claims settle %7 days after damage. Covered causes · %8.")
        .arg(money(offer.premium),QString::number(offer.term.value()),QString::number(offer.waiting.value()),
            money(offer.deductible),bp(offer.coverage),money(offer.limit),QString::number(offer.claim_delay.value()),hazards.join(QStringLiteral(", ")));
}
}
QString site_exposure(const core::SimulationState& s,std::string_view state) {
    if (!s.resilience) return {};
    const auto& r=*s.resilience;
    const auto found=std::find_if(r.catalog.exposures.begin(),r.catalog.exposures.end(),[&](const auto& row){return row.state_id==state;});
    if (found==r.catalog.exposures.end()) return {};
    const auto month=core::from_ordinal(s.date).month;
    QStringList values;
    for (std::size_t kind=0;kind<core::site_hazard_count;++kind)
        values.push_back(QStringLiteral("%1 %2").arg(readable(core::hazard_key(static_cast<core::SiteHazard>(kind))),
            bp(core::BasisPoints{found->monthly_probability[kind][month-1]})));
    return QStringLiteral("Monthly exposure for month %1 · %2\n%3").arg(month).arg(values.join(QStringLiteral(" · ")),text(r.catalog.assumptions));
}
ResilienceModels resilience_models(const core::SimulationState& s) {
    ResilienceModels result;
    if (!s.resilience) return result;
    const auto& r=*s.resilience;
    for (const auto& site:s.infrastructure->sites) if (!site.closed_on) result.sites.push_back(option(site.id,text(site.name)));
    for (const auto& offer:r.catalog.protections) {
        const auto detail=protection_effect(offer)+QStringLiteral("\n%1 now · %2/month after %3 days of installation.")
            .arg(money(offer.setup),money(offer.monthly),QString::number(offer.lead.value()));
        result.protections.push_back(option(offer.id,text(offer.name),detail));
    }
    for (const auto& offer:r.catalog.insurance) result.insurance.push_back(option(offer.id,text(offer.name),insurance_terms(offer)));
    for (const auto& row:r.protections) {
        const auto& offer=find(r.catalog.protections,row.offer_id);
        const auto& site=find(s.infrastructure->sites,row.site_id);
        result.installed.push_back({text(row.id),text(offer.name),text(site.name),protection_effect(offer),
            site.closed_on?QStringLiteral("Site closed"):s.date<row.active_from?QStringLiteral("Ready ")+date(row.active_from):QStringLiteral("Active")});
    }
    std::map<std::string,Cents,std::less<>> claims;
    for (const auto& incident:r.incidents) if (!incident.policy_id.empty()) {
        auto& total=claims.try_emplace(incident.policy_id,Cents{0}).first->second;
        total=checked_unit_add(total,incident.claim);
    }
    for (const auto& policy:r.policies) {
        const auto& offer=find(r.catalog.insurance,policy.offer_id);
        const auto total=claims.find(policy.id);
        const auto claimed=total==claims.end()?Cents{0}:total->second;
        result.policies.push_back({text(policy.id),text(find(s.infrastructure->sites,policy.site_id).name),text(offer.name),
            insurance_terms(offer)+QStringLiteral("\nEffective %1 through %2 (exclusive). Claims use %3 of the %4 policy limit.")
                .arg(date(policy.covers_from),date(policy.ends_before),money(claimed),money(offer.limit)),
            s.date>=policy.ends_before?QStringLiteral("Expired"):s.date<policy.covers_from?QStringLiteral("Waiting period"):QStringLiteral("Covered")});
    }
    const auto first=r.incidents.size()>32?r.incidents.size()-32:0;
    for (auto index=r.incidents.size();index>first;--index) {
        const auto& row=r.incidents[index-1];
        const auto restored=row.repaired_on?std::optional{advance_days(*row.repaired_on,row.restoration_days)}:std::nullopt;
        auto detail=QStringLiteral("Started %1 · earliest physical repair %2\n%3 capacity during damage · %4 repair bill · %5 days for site data recovery.")
            .arg(date(row.starts_on),date(row.repair_due),bp(row.remaining_capacity),money(row.repair_cost),QString::number(row.restoration_days.value()));
        if (restored) detail+=QStringLiteral("\nPhysical repair %1 · site recovery %2.").arg(date(*row.repaired_on),date(*restored));
        if (row.claim.value()>0) detail+=QStringLiteral("\n%1 insurance reimbursement %2.").arg(money(row.claim),row.paid_on?QStringLiteral("paid ")+date(*row.paid_on):QStringLiteral("due ")+date(row.claim_due));
        result.incidents.push_back({text(row.id),text(find(s.infrastructure->sites,row.site_id).name),readable(hazard_key(row.kind)),detail,
            restored && s.date>=*restored?QStringLiteral("Recovered"):row.repaired_on?QStringLiteral("Restoring data"):QStringLiteral("Physical disruption")});
    }
    if (s.market) for (const auto& service:s.market->products) {
        const auto& work=find(s.capacity->projects,service.capacity_project_id);
        if (work.status==ProjectStatus::active && !work.customer_inference)
            result.services.push_back(option(service.id,readable(service.id)));
    }
    for (const auto& source:s.capacity->sources) if (source.kind==CapacitySourceKind::owned)
        result.sources.push_back(option(source.id,readable(source.id)));
    for (const auto& row:r.replicas) {
        const auto& project=find(s.capacity->projects,row.sync_project);
        const auto detail=QStringLiteral("%1 CU/day reserved · %2 → %3\n"
            "Synchronization uses %4 CU/day, %5 staff and %6/month. Copied %7 of %8 CU for initial readiness.")
            .arg(cu(row.reservation.value()),text(row.primary_source),text(row.backup_source),
                cu(project.requested.value()),QString::number(project.staff),money(project.monthly_support_cost),
                cu(project.completed_work.value()),cu(r.catalog.replication.initial_copy.value()));
        result.replicas.push_back({text(row.id),readable(row.service_id),row.ready_on?QStringLiteral("Ready ")+date(*row.ready_on):QStringLiteral("Copying"),
            detail,row.stopped_on?QStringLiteral("Stopped"):QStringLiteral("Reserved"),!row.stopped_on});
    }
    return result;
}
QString resilience_review(const core::SimulationState& after,const core::CommandAction& action) {
    if (!after.resilience) return {};
    const auto& r=*after.resilience;
    if (const auto* request=std::get_if<core::BuySiteProtection>(&action)) {
        const auto& row=r.protections.back();const auto& offer=find(r.catalog.protections,request->offer_id);
        return QStringLiteral("Install %1 at %2\n%3 now · %4/month · ready %5.\n%6")
            .arg(text(offer.name),text(find(after.infrastructure->sites,request->site_id).name),money(offer.setup),money(offer.monthly),date(row.active_from),protection_effect(offer));
    }
    if (const auto* request=std::get_if<core::InsureSite>(&action)) {
        const auto& policy=r.policies.back();const auto& offer=find(r.catalog.insurance,request->offer_id);
        return insurance_terms(offer)+QStringLiteral("\nCoverage at %1 begins %2 and ends %3 (exclusive).")
            .arg(text(find(after.infrastructure->sites,request->site_id).name),date(policy.covers_from),date(policy.ends_before));
    }
    if (std::holds_alternative<core::ReplicateService>(action)) {
        const auto& row=r.replicas.back();const auto& terms=r.catalog.replication;
        return QStringLiteral("Reserve %1 CU/day on %2 for %3.\n%4 setup · %5/month. Synchronization needs %6 CU/day and %7 staff.\n"
            "The replica becomes ready after %8 CU of actual copying. It can then carry this same service during a primary-site outage.")
            .arg(cu(row.reservation.value()),text(row.backup_source),readable(row.service_id),money(terms.setup),money(terms.monthly),
                cu(terms.synchronization.value()),QString::number(terms.staff),cu(terms.initial_copy.value()));
    }
    if (std::holds_alternative<core::StopServiceReplica>(action))
        return QStringLiteral("Stop synchronization and release the standby reservation. The service continues on its current rack.");
    return {};
}
}
