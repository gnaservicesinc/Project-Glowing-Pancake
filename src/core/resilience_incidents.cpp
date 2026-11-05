#include "resilience_internal.hpp"
namespace pgp::core {
using namespace resilience_detail;
namespace resilience_detail {
std::string incident_description(const SimulationState& s,const SiteIncident& row) {
    const auto amount=[](Cents value) {
        return "$"+std::to_string(value.value()/100)+"."+(value.value()%100<10?"0":"")+std::to_string(value.value()%100);
    };
    auto cause=site(s,row.site_id).name+" experienced "+std::string(hazard_key(row.kind))+
        ". Physical work is disrupted through at least "+iso_date(row.repair_due)+
        ". Repairs cost "+amount(row.repair_cost)+
        " in 2014 dollars. Site data recovery takes "+std::to_string(row.restoration_days.value())+" further days.";
    if (protection(s,row.site_id,SiteProtectionKind::hardening,row.starts_on) && row.kind!=SiteHazard::grid && row.kind!=SiteHazard::heat)
        cause+=" Installed preparedness reduced repair time and cost.";
    if (protection(s,row.site_id,SiteProtectionKind::backup,row.starts_on))
        cause+=" Installed offsite copies shortened the data restoration stage.";
    if (row.kind==SiteHazard::grid && protection(s,row.site_id,SiteProtectionKind::generator,row.starts_on))
        cause+=" Emergency generation has finite power and fuel endurance.";
    if (row.kind==SiteHazard::heat && protection(s,row.site_id,SiteProtectionKind::cooling,row.starts_on))
        cause+=" Supplemental cooling reduced the heat-related capacity loss.";
    if (row.claim.value()>0) cause+=" Insurance schedules "+amount(row.claim)+" for "+iso_date(row.claim_due)+".";
    return cause;
}
SiteHazardSample draw_sample(const SimulationState& s,std::string_view site_id,OrdinalDay day,std::uint64_t before) {
    const auto& owned=site(s,site_id);
    const auto& exposures=risks(s).catalog.exposures;
    const auto exposure=std::find_if(exposures.begin(),exposures.end(),[&](const auto& row){return row.state_id==owned.state_id;});
    if (exposure==exposures.end()) invalid("The site's state has no seasonal exposure");
    const auto month=from_ordinal(day);
    const auto days=std::min<std::uint64_t>(days_in_month(month.year,month.month),static_cast<std::uint64_t>(s.end_date.value()-day.value()));
    SplitMix64 stream{before};
    const auto draw=stream.bounded(10000);
    const auto offset=stream.bounded(days);
    std::uint64_t cumulative=0;
    std::uint32_t outcome=0;
    for (std::size_t h=0;h<site_hazard_count;++h) {
        auto chance=static_cast<std::uint64_t>(exposure->monthly_probability[h][month.month-1]);
        if (h==static_cast<std::size_t>(SiteHazard::fire)) if (const auto* maintenance=protection(s,site_id,SiteProtectionKind::maintenance,day))
            chance=mul_div_floor(chance,static_cast<std::uint64_t>(maintenance->factor.value()),10000).quotient;
        cumulative+=chance;
        if (outcome==0 && draw<cumulative) outcome=static_cast<std::uint32_t>(h+1);
    }
    return {std::string(site_id),day,advance_days(day,Days{offset}),before,stream.state(),outcome};
}
SiteIncident incident_terms(const SimulationState& s,const SiteHazardSample& sample,std::size_t index) {
    if (sample.outcome==0 || sample.outcome>site_hazard_count) invalid("A quiet draw cannot cause physical damage");
    const auto kind=static_cast<SiteHazard>(sample.outcome-1);
    const auto& rule=hazard(risks(s).catalog,kind);
    auto physical=rule.physical_days;
    auto restoration=rule.restoration_days;
    auto remaining=rule.remaining_capacity;
    auto loss=rule.repair_cost;
    if (kind!=SiteHazard::grid && kind!=SiteHazard::heat) {
        if (const auto* hardening=protection(s,sample.site_id,SiteProtectionKind::hardening,sample.event_on)) {
            const auto scaled=mul_div_floor(static_cast<std::uint64_t>(physical.value()),static_cast<std::uint64_t>(hardening->factor.value()),10000);
            physical=Days{std::max<std::uint64_t>(1,scaled.quotient+(scaled.remainder>0?1:0))};
            loss=Cents{mul_div_floor(static_cast<std::uint64_t>(loss.value()),static_cast<std::uint64_t>(hardening->factor.value()),10000).quotient};
        }
    }
    if (kind==SiteHazard::heat) if (const auto* cooling=protection(s,sample.site_id,SiteProtectionKind::cooling,sample.event_on)) {
        const auto reduction=mul_div_floor(static_cast<std::uint64_t>(10000-remaining.value()),static_cast<std::uint64_t>(cooling->factor.value()),10000);
        remaining=BasisPoints{10000-reduction.quotient};
    }
    if (const auto* backup=protection(s,sample.site_id,SiteProtectionKind::backup,sample.event_on)) {
        const auto scaled=mul_div_floor(static_cast<std::uint64_t>(restoration.value()),static_cast<std::uint64_t>(backup->factor.value()),10000);
        restoration=Days{scaled.quotient+(scaled.remainder>0?1:0)};
    }
    SiteIncident result{"incident."+std::to_string(index+1),sample.site_id,index,kind,sample.event_on,
        advance_days(sample.event_on,physical),{},restoration,remaining,loss,"",Cents{0},sample.event_on,{}};
    for (const auto& policy:risks(s).policies) {
        if (policy.site_id!=sample.site_id || sample.event_on<policy.covers_from || sample.event_on>=policy.ends_before) continue;
        const auto& terms=find(risks(s).catalog.insurance,policy.offer_id);
        if (std::find(terms.covered.begin(),terms.covered.end(),kind)==terms.covered.end()) continue;
        Cents prior{0};
        for (const auto& incident:risks(s).incidents) if (incident.policy_id==policy.id &&
            std::pair{incident.starts_on,incident.sample_index}<std::pair{sample.event_on,static_cast<std::uint64_t>(index)})
            prior=checked_unit_add(prior,incident.claim);
        const auto eligible=std::max<std::int64_t>(0,checked_subtract(loss.value(),terms.deductible.value()));
        result.policy_id=policy.id;
        result.claim=std::min(checked_unit_subtract(terms.limit,prior),
            Cents{mul_div_floor(static_cast<std::uint64_t>(eligible),static_cast<std::uint64_t>(terms.coverage.value()),10000).quotient});
        result.claim_due=advance_days(sample.event_on,terms.claim_delay);
        break;
    }
    return result;
}
}
void sample_site_hazards(SimulationState& s) {
    if (!s.resilience || s.date>=s.end_date || from_ordinal(s.date).day!=1) return;
    auto& r=risks(s);
    std::vector<const OwnedSite*> sites;
    for (const auto& owned:s.infrastructure->sites) if (owned.commissioned_on && *owned.commissioned_on<s.date &&
        before_end(s.date,owned.closed_on) && std::any_of(s.infrastructure->hardware.begin(),s.infrastructure->hardware.end(),[&](const auto& row) {
            return row.site_id==owned.id && row.commissioned_on && *row.commissioned_on<s.date && before_end(s.date,row.retired_on);
        })) sites.push_back(&owned);
    std::sort(sites.begin(),sites.end(),[](const auto* a,const auto* b){return a->id<b->id;});
    auto& stream=s.random[static_cast<std::size_t>(RandomStream::hazards)];
    for (const auto* owned:sites) {
        if (r.samples.size()>=resilience_sample_limit) invalid("The century site sampling record is full");
        auto sample=draw_sample(s,owned->id,s.date,stream);
        stream=sample.stream_after;r.samples.push_back(std::move(sample));
    }
}
void prepare_site_resilience(SimulationState& s) {
    if (!s.resilience) return;
    auto& r=risks(s);
    for (const auto& owned:s.infrastructure->sites) if (owned.closed_on) {
        for (const auto& p:r.protections) if (p.site_id==owned.id) {
            const auto term=std::find_if(s.accounts->recurring.begin(),s.accounts->recurring.end(),[&](const auto& row){return row.id=="resilience/"+p.id;});
            if (term!=s.accounts->recurring.end() && !term->ends_before)
                end_recurring(*s.accounts,term->id,std::max(term->starts_on,*owned.closed_on));
        }
    }
    // At most this month's and the previous month's scheduled samples can
    // become due. Older history stays untouched in ordinary daily preparation.
    const auto lower=std::max<std::int64_t>(0,s.date.value()-31);
    const auto begin=std::lower_bound(r.samples.begin(),r.samples.end(),lower,
        [](const auto& row,std::int64_t day){return row.sampled_on.value()<day;});
    for (auto sample=begin;sample!=r.samples.end() && r.prepared_on!=s.date;++sample) if (sample->outcome!=0 && sample->event_on==s.date &&
        before_end(s.date,site(s,sample->site_id).closed_on)) {
        const auto index=static_cast<std::size_t>(sample-r.samples.begin());
        const auto id="incident."+std::to_string(index+1);
        if (std::any_of(r.incidents.begin(),r.incidents.end(),[&](const auto& row){return row.id==id;})) continue;
        if (r.incidents.size()>=resilience_incident_limit) invalid("The bounded site incident record is full");
        auto incident=incident_terms(s,*sample,index);
        if (incident.repair_cost.value()>0)
            issue_invoice(*s.accounts,s.date,{repair_invoice(id),InvoiceKind::payable,incident.repair_due,incident.repair_cost,Cents{0},"Site incident repair"});
        alert(s,id,incident.site_id,s.date,incident_description(s,incident),true);
        r.open_incidents.push_back(r.incidents.size());
        r.incidents.push_back(std::move(incident));
    }
    r.prepared_on=s.date;
    for (const auto index:r.open_incidents) {
        auto& incident=r.incidents.at(static_cast<std::size_t>(index));
        if (!incident.repaired_on && s.date>=incident.repair_due) {
            const bool settled=incident.repair_cost.value()==0 ||
                find(s.accounts->invoices,repair_invoice(incident.id)).settled==incident.repair_cost;
            if (settled) incident.repaired_on=s.date;
        }
        if (incident.repaired_on && recovered_on(incident)<=s.date && !incident.recovery_reported) {
            alert(s,"restored/"+incident.id,incident.site_id,s.date,site(s,incident.site_id).name+" completed physical repairs and site data recovery.");
            incident.recovery_reported=true;
        }
        if (incident.claim.value()>0 && !incident.paid_on && s.date>=incident.claim_due) {
            if (r.insurer_cash<incident.claim) invalid("Reserved insurance funds disappeared before settlement");
            record_cash(*s.accounts,s.date,incident.claim,Account::expenses,"Site insurance reimbursement","resilience.claim/"+incident.id);
            s.cash=checked_unit_add(s.cash,incident.claim);r.insurer_cash=checked_unit_subtract(r.insurer_cash,incident.claim);
            incident.paid_on=s.date;
            alert(s,"insured/"+incident.id,incident.site_id,s.date,
                site(s,incident.site_id).name+" received the agreed insurance reimbursement. Physical recovery follows the repair schedule.");
        }
    }
    std::erase_if(r.open_incidents,[&](auto index) {
        const auto& row=r.incidents[static_cast<std::size_t>(index)];
        return row.recovery_reported && (row.claim.value()==0 || row.paid_on);
    });
    while (r.alert_cursor<r.alerts.size() && (!r.alerts[r.alert_cursor].required || r.alerts[r.alert_cursor].acknowledged)) ++r.alert_cursor;
}
}
