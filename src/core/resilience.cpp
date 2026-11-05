#include "resilience_internal.hpp"
namespace pgp::core {
using namespace resilience_detail;
namespace resilience_detail {
const CapacitySource& capacity_source(const SimulationState& s,std::string_view id) {return find(s.capacity->sources,id);}
std::string site_for_source(const SimulationState& s,std::string_view id) {
    for (const auto& row:s.infrastructure->hardware) if (row.source_id==id && row.commissioned_on) return row.site_id;
    invalid("Resilience requires commissioned owned hardware");
}
void spend(SimulationState& s,Cents cost,Cents protected_cash,std::string key,std::string cause) {
    if (cost>checked_unit_subtract(s.cash,protected_cash)) invalid("Unreserved cash cannot fund this resilience commitment");
    if (cost.value()>0) record_cash(*s.accounts,s.date,Cents{-cost.value()},Account::expenses,std::move(cause),std::move(key));
    s.cash=checked_unit_subtract(s.cash,cost);
}
void alert(SimulationState& s,std::string id,std::string_view site_id,OrdinalDay day,std::string cause,bool required) {
    auto& rows=risks(s).alerts;
    if (rows.size()>=resilience_alert_limit) invalid("The bounded site alert record is full");
    const auto& hardware=s.infrastructure->hardware;
    const auto source=std::find_if(hardware.begin(),hardware.end(),[&](const auto& row){
        return row.site_id==site_id && row.commissioned_on && *row.commissioned_on<=day;
    });
    if (source==hardware.end()) invalid("A site outcome needs an actual commissioned source");
    rows.push_back({std::move(id),source->source_id,std::move(cause),day,required,false});
    if (required) s.paused=true;
}
MilliCUPerDay free_capacity(const SimulationState& s,const CapacitySource& source) {
    auto available=company_capacity(source);
    for (const auto& project:s.capacity->projects) if (project.status==ProjectStatus::active && project.source_id==source.id)
        available=MilliCUPerDay{std::max<std::int64_t>(0,checked_subtract(available.value(),project.requested.value()))};
    return available;
}
}
const SiteAlert* required_site_alert(const SimulationState& s) {
    if (s.resilience) for (auto index=s.resilience->alert_cursor;index<s.resilience->alerts.size();++index) {
        const auto& row=s.resilience->alerts[index];
        if (row.required && !row.acknowledged) return &row;
    }
    return nullptr;
}
void buy_site_protection(SimulationState& s,const BuySiteProtection& request,Cents protected_cash) {
    auto& r=risks(s);
    const auto& owned=site(s,request.site_id);
    const auto& offer=find(r.catalog.protections,request.offer_id);
    if (owned.closed_on || r.protections.size()>=resilience_protection_limit ||
        advance_days(s.date,offer.lead)>=s.end_date ||
        std::any_of(r.protections.begin(),r.protections.end(),[&](const auto& row){
            return row.site_id==owned.id && protection_terms(r,row).kind==offer.kind;
        })) invalid("The site already has this protection or cannot complete installation");
    const auto id="protection."+std::to_string(r.protections.size()+1);
    spend(s,offer.setup,protected_cash,"resilience.install/"+id,"Site resilience installation");
    const auto active=advance_days(s.date,offer.lead);
    add_recurring(*s.accounts,{"resilience/"+id,offer.monthly,active,{},"Site resilience upkeep"});
    r.protections.push_back({id,owned.id,offer.id,s.date,active});
}
Cents insurer_available(const SimulationState& s) {
    const auto& r=risks(s);
    auto available=r.insurer_cash;
    for (const auto& policy:r.policies) {
        const auto& terms=find(r.catalog.insurance,policy.offer_id);
        Cents paid{0},outstanding{0};
        for (const auto& incident:r.incidents) if (incident.policy_id==policy.id) {
            if (incident.paid_on) paid=checked_unit_add(paid,incident.claim);
            else outstanding=checked_unit_add(outstanding,incident.claim);
        }
        const auto reserved=s.date<policy.ends_before?checked_unit_subtract(terms.limit,paid):outstanding;
        available=checked_unit_subtract(available,reserved);
    }
    return available;
}
void insure_site(SimulationState& s,const InsureSite& request,Cents protected_cash) {
    auto& r=risks(s);
    const auto& owned=site(s,request.site_id);
    const auto& terms=find(r.catalog.insurance,request.offer_id);
    if (owned.closed_on || r.policies.size()>=resilience_policy_limit || advance_days(s.date,terms.waiting)>=s.end_date ||
        std::any_of(r.policies.begin(),r.policies.end(),[&](const auto& row){return row.site_id==owned.id && s.date<row.ends_before;}))
        invalid("Choose an open site without an overlapping insurance term");
    if (checked_unit_add(insurer_available(s),terms.premium)<terms.limit)
        invalid("The insurer's finite cash pool cannot reserve this full policy limit");
    const auto id="policy."+std::to_string(r.policies.size()+1);
    spend(s,terms.premium,protected_cash,"resilience.premium/"+id,"Site insurance premium");
    r.insurer_cash=checked_unit_add(r.insurer_cash,terms.premium);
    r.policies.push_back({id,owned.id,terms.id,s.date,advance_days(s.date,terms.waiting),advance_days(s.date,terms.term)});
}
SiteDisruption site_disruption(const SimulationState& s,std::string_view site_id,OrdinalDay day) {
    SiteDisruption result;
    if (!s.resilience) return result;
    const auto apply=[&](const SiteIncident& row) {
        if (row.site_id!=site_id || row.starts_on>day || (row.repaired_on && day>=recovered_on(row))) return;
        auto remaining=row.remaining_capacity;
        if (row.repaired_on && day>=*row.repaired_on) remaining=BasisPoints{0};
        if (row.kind==SiteHazard::grid && (!row.repaired_on || day<*row.repaired_on)) {
            const auto* generator=protection(s,site_id,SiteProtectionKind::generator,row.starts_on);
            const auto watts=generator && day<advance_days(row.starts_on,generator->emergency_days)?generator->emergency_watts:0;
            result.emergency_watts=result.emergency_watts?std::min(*result.emergency_watts,watts):watts;
            if (watts>0) {
                result.power_millicents_per_kwh=generator->power_millicents_per_kwh;
                result.power_grams_per_kwh=generator->power_grams_per_kwh;
            }
        } else result.remaining=std::min(result.remaining,remaining);
    };
    if (day==s.date) {
        for (const auto index:s.resilience->open_incidents) apply(s.resilience->incidents.at(static_cast<std::size_t>(index)));
    } else {
        for (const auto& row:s.resilience->incidents) apply(row);
    }
    return result;
}
}
