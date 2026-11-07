#include "deployment_risk_internal.hpp"
#include <set>
#include <tuple>

namespace pgp::core {
namespace {
using namespace deployment_risk_detail;
OrdinalDay month_start(OrdinalDay day) {
    const auto date=from_ordinal(day);
    return to_ordinal({date.year,date.month,1});
}
OrdinalDay next_month(OrdinalDay first) {
    const auto date=from_ordinal(first);
    return advance_days(first,Days{days_in_month(date.year,date.month)});
}
void validate_samples(const SimulationState& state,const DeploymentRiskState& risk) {
    const auto records=releases(state);
    auto stream=risk.stream_origin;
    std::size_t count=0;
    std::optional<OrdinalDay> last;
    if(risk.prepared_on) {
        auto day=month_start(risk.starts_on);
        if(day<risk.starts_on) day=next_month(day);
        for(;day<=*risk.prepared_on;day=next_month(day)) {
            last=day;
            for(const auto& [project,attempts]:previous_attempts(state,day)) {
                const auto record=records.find(project);
                const auto* terms=record==records.end()?nullptr:risk_rules(state,record->second);
                if(!terms) continue;
                const auto expected=draw_sample(state,record->second,*terms,day,attempts,stream);
                if(count>=risk.samples.size() || risk.samples[count++]!=expected)
                    invalid("Saved deployment draw differs from settled exposure or its owning risk stream");
                stream=expected.stream_after;
            }
        }
    }
    if(count!=risk.samples.size() || last!=risk.sampled_on || state.random[static_cast<std::size_t>(RandomStream::risk)]!=stream)
        invalid("Deployment sampling skipped a monthly boundary or lost its risk stream");
}
struct PostingEvidence {
    std::map<std::string_view,const Posting*,std::less<>> by_key;
    std::map<std::string_view,OrdinalDay,std::less<>> paid_on;
};
PostingEvidence posting_evidence(const AccountsState& accounts) {
    PostingEvidence result;
    for(const auto& posting:accounts.postings) {
        if(posting.key.starts_with("invoice/deployment.claim/") || posting.key.starts_with("deployment.response/"))
            result.by_key.emplace(posting.key,&posting);
        if(posting.document_id.starts_with("deployment.claim/") && posting.debit==Account::payables &&
            posting.credit==Account::cash) result.paid_on.insert_or_assign(posting.document_id,posting.day);
    }
    return result;
}
} // namespace

void validate_deployment_risk(const SimulationState& state,bool deep) {
    if(!deep) return;
    const bool authored=state.deployments && std::ranges::any_of(state.deployments->catalog,[](const auto& row) {
        return row.incident_risk.has_value();
    });
    const auto* risk=state.deployments && state.deployments->risk?&*state.deployments->risk:nullptr;
    if(authored!=(risk!=nullptr)) invalid("Deployment risk state differs from the authored service catalog");
    if(!risk) {
        if(state.capacity && std::ranges::any_of(state.capacity->projects,[](const auto& row) {
            return row.kind==ProjectKind::incident_response || !row.disruptions.empty();
        })) invalid("Incident work and holds require their owning deployment risk state");
        return;
    }
    if(!state.accounts || !state.capacity || risk->starts_on!=state.capacity->starts_on ||
        risk->samples.size()>deployment_risk_sample_limit || risk->incidents.size()>deployment_incident_limit ||
        risk->responses.size()>deployment_response_limit || risk->alerts.size()>deployment_incident_alert_limit ||
        (risk->prepared_on && (*risk->prepared_on<risk->starts_on || *risk->prepared_on>state.date || *risk->prepared_on>=state.end_date)) ||
        (!risk->prepared_on && (state.date!=risk->starts_on || state.phase!=DayPhase::opening)) ||
        (risk->prepared_on && state.phase!=DayPhase::finished && state.date<state.end_date &&
            *risk->prepared_on<state.date && (state.phase!=DayPhase::opening || advance_days(*risk->prepared_on,Days{1})!=state.date)))
        invalid("Deployment risk history is missing its prepared morning or owning resources");
    validate_samples(state,*risk);

    std::map<std::string_view,const DeploymentIncident*,std::less<>> incidents;
    std::vector<std::pair<OrdinalDay,std::size_t>> due;
    for(std::size_t i=0;i<risk->samples.size();++i) {
        const auto& sample=risk->samples[i];
        if(sample.incident && risk->prepared_on && sample.event_on<=*risk->prepared_on) due.emplace_back(sample.event_on,i);
    }
    std::sort(due.begin(),due.end());
    if(due.size()!=risk->incidents.size()) invalid("Scheduled deployment outcomes do not match discovered incidents");
    const auto postings=posting_evidence(*state.accounts);
    std::size_t posting_count=0;
    std::set<std::string> claims;
    for(std::size_t i=0;i<due.size();++i) {
        const auto [date,sample]=due[i];
        const auto& incident=risk->incidents[i];
        if(incident.id!="deployment.incident."+std::to_string(sample+1) || incident.sample_index!=sample ||
            incident.discovered_on!=date || incident.project_id!=risk->samples[sample].project_id ||
            !incidents.emplace(incident.id,&incident).second)
            invalid("A discovered deployment incident lost its sampled identity or date");
        const auto id=claim_id(incident.id);
        claims.insert(id);
        const auto& claim=named(state.accounts->invoices,id);
        const auto& terms=rules(state,incident.project_id);
        const auto posting=postings.by_key.find("invoice/"+id);
        if(claim.kind!=InvoiceKind::payable || claim.amount!=terms.compensation ||
            claim.due!=advance_days(date,Days{14}) || posting==postings.by_key.end() ||
            posting->second->day!=date || posting->second->document_id!=id)
            invalid("Incident compensation differs from its authored and posted terms");
        ++posting_count;
    }
    for(const auto& claim:state.accounts->invoices)
        if(claim.id.starts_with("deployment.claim/") && !claims.contains(claim.id))
            invalid("An incident claim has no discovered deployment outcome");

    std::map<std::string_view,const CapacityProject*,std::less<>> projects;
    for(const auto& work:state.capacity->projects) projects.emplace(work.id,&work);
    std::map<std::string_view,std::size_t,std::less<>> latest;
    std::set<std::string_view> response_work;
    std::optional<OrdinalDay> last_response_day;
    for(std::size_t i=0;i<risk->responses.size();++i) {
        const auto& response=risk->responses[i];
        const auto owner=incidents.find(response.incident_id);
        const auto project=projects.find(response.project_id);
        if(owner==incidents.end() || project==projects.end() || !response_work.insert(response.project_id).second ||
            response.project_id!="deployment.response/"+std::to_string(i+1) ||
            (last_response_day && response.started_on<*last_response_day) || response.started_on>state.date)
            invalid("Incident investigation has no unique discovered owner or work");
        const auto& incident=*owner->second;
        const auto& work=*project->second;
        const auto quote=response_terms(state,incident,work.requested,work.staff);
        if(work.kind!=ProjectKind::incident_response || work.starts_on!=response.started_on ||
            work.starts_on<incident.discovered_on || work.total_work!=quote.work || work.customer_inference ||
            work.minimum_memory_mib!=0 || work.interconnect!="general" ||
            work.cause!="Investigate and remediate "+incident.id)
            invalid("Incident response differs from its committed staff, compute or date");
        const auto paid=postings.paid_on.find(claim_id(incident.id));
        const auto& claim=named(state.accounts->invoices,claim_id(incident.id));
        if(claim.settled!=claim.amount || paid==postings.paid_on.end() || paid->second>work.starts_on)
            invalid("Investigation work began before compensation was paid");
        if(const auto previous=latest.find(incident.id);previous!=latest.end()) {
            const auto& prior=*projects.at(risk->responses[previous->second].project_id);
            if(prior.status!=ProjectStatus::cancelled || !prior.stopped_on || *prior.stopped_on>work.starts_on)
                invalid("Incident response attempts overlap or bypass a completed investigation");
        }
        latest[incident.id]=i;
        const auto fee=postings.by_key.find(response.project_id);
        if(quote.fee.value()>0) {
            if(fee==postings.by_key.end() || fee->second->day!=work.starts_on || fee->second->amount!=quote.fee ||
                fee->second->debit!=Account::expenses || fee->second->credit!=Account::cash)
                invalid("Incident response is missing its paid investigation fee");
            ++posting_count;
        }
        last_response_day=response.started_on;
    }
    if(posting_count!=postings.by_key.size()) invalid("Deployment incident postings require their owning claim or investigation");

    std::vector<std::uint64_t> open;
    std::vector<DeploymentIncidentAlert> alerts;
    for(std::size_t i=0;i<risk->incidents.size();++i) {
        const auto& incident=risk->incidents[i];
        std::optional<std::uint64_t> response_index;
        std::optional<OrdinalDay> recovered;
        if(const auto response=latest.find(incident.id);response!=latest.end()) {
            response_index=response->second;
            const auto& work=*projects.at(risk->responses[response->second].project_id);
            if(work.status==ProjectStatus::completed && work.stopped_on && risk->prepared_on && *work.stopped_on<*risk->prepared_on)
                recovered=advance_days(*work.stopped_on,Days{1});
        }
        if(incident.last_response!=response_index || incident.recovered_on!=recovered)
            invalid("Incident recovery differs from actual response work and compensation");
        alerts.push_back({"discovered/"+incident.id,incident.id,incident.discovered_on,false,false});
        if(recovered) alerts.push_back({"recovered/"+incident.id,incident.id,*recovered,true,false});
        else open.push_back(i);
    }
    // Discoveries precede recoveries on the same morning. Within each group
    // the owner follows numeric sample/discovery order, as in preparation.
    std::stable_sort(alerts.begin(),alerts.end(),[](const auto& a,const auto& b) {
        return std::tie(a.day,a.recovery)<std::tie(b.day,b.recovery);
    });
    if(risk->alerts.size()!=alerts.size() || risk->open_incidents!=open)
        invalid("Incident recovery and alert cursors disagree with discovered outcomes");
    std::size_t cursor=alerts.size();
    for(std::size_t i=0;i<alerts.size();++i) {
        const auto& actual=risk->alerts[i];
        if(actual.recovery && actual.acknowledged) invalid("Recovery bulletins require no acknowledgment");
        alerts[i].acknowledged=actual.acknowledged;
        if(alerts[i]!=actual) invalid("Incident alert differs from the dated owning outcome");
        if(!actual.recovery && !actual.acknowledged) cursor=std::min(cursor,i);
    }
    if(risk->alert_cursor!=cursor || (cursor<alerts.size() && !state.paused))
        invalid("An unreviewed deployment incident must hold time");
    std::set<std::string_view> affected;
    for(const auto& incident:risk->incidents) affected.insert(incident.project_id);
    for(const auto& [id,work]:projects) {
        if((work->kind==ProjectKind::incident_response)!=response_work.contains(id) ||
            (!affected.contains(id) && !work->disruptions.empty()) ||
            (affected.contains(id) && work->disruptions!=expected_disruptions(*risk,id)))
            invalid("Serving containment and response projects must match actual deployment incidents");
    }
}
} // namespace pgp::core
