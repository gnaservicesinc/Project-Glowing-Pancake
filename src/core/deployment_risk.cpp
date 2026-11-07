#include "deployment_risk_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>

namespace pgp::core {
namespace deployment_risk_detail {
[[noreturn]] void invalid(const char* message) {throw std::invalid_argument(message);}
std::map<std::string_view,Release,std::less<>> releases(const SimulationState& s) {
    std::map<std::string_view,Release,std::less<>> result;
    if(!s.deployments) return result;
    for(const auto& r:s.deployments->records) result.emplace(r.project_id,Release{&r,false});
    for(const auto& r:s.deployments->rollouts) result.emplace(r.candidate.project_id,Release{&r.candidate,true});
    return result;
}
const DeploymentRiskRules& rules(const SimulationState& s,std::string_view project) {
    const auto all=releases(s);
    const auto found=all.find(project);
    if(found==all.end()) invalid("An incident needs a recorded deployed release");
    const auto& catalog=s.deployments->catalog;
    const auto service=std::ranges::find(catalog,found->second.record->service_id,[](const auto& v){return v.product.id;});
    if(service==catalog.end() || !service->incident_risk) invalid("This release has no authored incident risk table");
    return *service->incident_risk;
}
std::map<std::string,std::uint64_t,std::less<>> previous_attempts(const SimulationState& s,OrdinalDay day) {
    std::map<std::string,std::uint64_t,std::less<>> result;
    if(!s.capacity) return result;
    const auto calendar=from_ordinal(day);
    const auto previous=from_ordinal(OrdinalDay{day.value()-1});
    const auto first=to_ordinal({previous.year,previous.month,1});
    if(calendar.day!=1) invalid("Deployment risk sampling requires a calendar-month boundary");
    const auto start=std::max(first,s.capacity->starts_on);
    for(auto date=start;date<day;date=advance_days(date,Days{1})) {
        const auto index=static_cast<std::size_t>(date.value()-s.capacity->starts_on.value());
        if(index>=s.capacity->days.size()) invalid("Risk exposure requires settled work from the prior month");
        for(const auto& work:s.capacity->days[index].projects)
            if(work.useful_tasks>0 || work.failed_tasks>0)
                result[work.project_id]+=work.useful_tasks+work.failed_tasks;
    }
    return result;
}
namespace {
std::size_t cell_for(const SimulationState& s,const Release& selected,const DeploymentRiskRules& r,
                     std::uint64_t attempts,std::uint64_t exposure_days,bool public_view) {
    const auto& release=*selected.record;
    const auto& model=named(s.lab->models,release.model_id);
    const auto& evidence=named(s.lab->evaluations,release.evidence_id);
    const auto& plan=named(s.lab->catalog.evaluations,evidence.plan_id);
    const bool mismatch=public_view ? (plan.coverage[r.domain].value()==0 || evidence.lower[r.domain]<r.control_threshold) :
        model.capability[r.domain]<r.control_threshold;
    const bool large=attempts>=r.large_daily_tasks*exposure_days;
    const auto autonomy=release.agent_controls?release.agent_controls->tools:AgentTools::draft;
    const bool supervised=release.human_review || (release.agent_controls && release.agent_controls->oversight==AgentOversight::each_action);
    for(std::size_t i=0;i<r.lookup.size();++i) {
        const auto& c=r.lookup[i];
        if(c.large==large && c.autonomy==autonomy && c.mismatch==mismatch &&
            c.supervised==supervised && c.staged==selected.staged) return i;
    }
    invalid("The authored incident lookup does not cover this release's controls and exposure");
}
}
DeploymentRiskSample draw_sample(const SimulationState& s,std::string_view project,OrdinalDay day,std::uint64_t attempts,std::uint64_t before) {
    const auto all=releases(s);
    const auto release=all.find(project);
    if(release==all.end()) invalid("Risk sample has no immutable release");
    const auto& r=rules(s,project);
    const auto prior=from_ordinal(OrdinalDay{day.value()-1});
    const auto span=static_cast<std::uint64_t>(days_in_month(prior.year,prior.month));
    const auto cell=cell_for(s,release->second,r,attempts,span,false);
    SplitMix64 stream{before};
    const auto draw=stream.bounded(10000);
    const auto now=from_ordinal(day);
    const auto offset=stream.bounded(days_in_month(now.year,now.month));
    return {std::string(project),day,advance_days(day,Days{offset}),attempts,before,stream.state(),
        static_cast<std::uint32_t>(cell),draw<static_cast<std::uint64_t>(r.lookup[cell].probability.value())};
}
std::string claim_id(std::string_view id) {return "deployment.claim/"+std::string(id);}
DeploymentResponsePreview response_terms(const SimulationState& s,const DeploymentIncident& incident,MilliCUPerDay rate,std::uint32_t staff) {
    if(rate.value()<=0 || rate.value()>1'000'000'000 || staff==0 || staff>1'000'000)
        invalid("Choose positive bounded response compute and staff");
    const auto& r=rules(s,incident.project_id);
    const auto& claim=named(s.accounts->invoices,claim_id(incident.id));
    const auto days=Days{r.response_staff_days.value()/staff+(r.response_staff_days.value()%staff!=0?1:0)};
    const auto work=std::max(r.response_work,integrate(rate,days));
    if(work.value()>1'000'000'000'000LL) invalid("Response work exceeds the finite investigation bound");
    const auto outstanding=checked_unit_subtract(claim.amount,claim.settled);
    return {r.response_fee,outstanding,checked_unit_add(r.response_fee,outstanding),work,
        Days{work.value()/rate.value()+(work.value()%rate.value()!=0?1:0)}};
}
std::vector<ProjectPause> expected_disruptions(const DeploymentRiskState& risk,std::string_view project) {
    std::vector<ProjectPause> result;
    for(const auto& i:risk.incidents) if(i.project_id==project) {
        if(!result.empty() && (!result.back().ends_before || i.discovered_on<=*result.back().ends_before)) {
            if(!i.recovered_on || !result.back().ends_before) result.back().ends_before.reset();
            else result.back().ends_before=std::max(*result.back().ends_before,*i.recovered_on);
        } else result.push_back({i.discovered_on,i.recovered_on});
    }
    return result;
}
} // namespace deployment_risk_detail
using namespace deployment_risk_detail;
void validate_deployment_risk_rules(const DeploymentRiskRules& r) {
    if(r.assumptions.empty() || r.assumptions.size()>1024 ||
        (r.exposure!="bounded" && r.exposure!="public" && r.exposure!="physical") ||
        (r.severity!="routine" && r.severity!="consequential" && r.severity!="critical") ||
        r.domain>=lab_domain_count || r.control_threshold.value()==0 || r.large_daily_tasks==0 ||
        r.large_daily_tasks>market_task_limit || r.elevated_from.value()==0 || r.high_from<=r.elevated_from ||
        r.lookup.size()!=48 || r.compensation.value()<=0 || r.compensation.value()>1'000'000'000 ||
        r.response_fee.value()<0 || r.response_fee.value()>1'000'000'000 || r.response_work.value()<=0 ||
        r.response_work.value()>1'000'000'000 || r.response_staff_days.value()<1 || r.response_staff_days.value()>365)
        invalid("Deployment risk requires a complete bounded table, fictional assumptions and paid recovery terms");
    std::set<std::tuple<bool,AgentTools,bool,bool,bool>> seen;
    for(const auto& c:r.lookup) {
        (void)agent_tools_key(c.autonomy);
        if(!seen.emplace(c.large,c.autonomy,c.mismatch,c.supervised,c.staged).second)
            invalid("Deployment risk lookup repeats a factor combination");
    }
}
void initialize_deployment_risk(SimulationState& s) {
    if(!s.deployments || s.deployments->risk || !std::ranges::any_of(s.deployments->catalog,[](const auto& r){return r.incident_risk.has_value();})) return;
    DeploymentRiskState risk;
    risk.starts_on=s.date;
    risk.stream_origin=s.random[static_cast<std::size_t>(RandomStream::risk)];
    s.deployments->risk=std::move(risk);
}
void sample_deployment_risk(SimulationState& s) {
    if(!s.deployments || !s.deployments->risk || s.date>=s.end_date || from_ordinal(s.date).day!=1) return;
    auto& risk=*s.deployments->risk;
    if(risk.sampled_on==s.date) return;
    const auto totals=previous_attempts(s,s.date);
    auto& stream=s.random[static_cast<std::size_t>(RandomStream::risk)];
    for(const auto& [id,release]:releases(s)) {
        const auto service=std::ranges::find(s.deployments->catalog,release.record->service_id,[](const auto& v){return v.product.id;});
        const auto attempts=totals.find(id);
        if(service==s.deployments->catalog.end() || !service->incident_risk || attempts==totals.end() || attempts->second==0) continue;
        if(risk.samples.size()>=deployment_risk_sample_limit) invalid("The bounded deployment sampling history is full");
        auto sample=draw_sample(s,id,s.date,attempts->second,stream);
        stream=sample.stream_after;
        risk.samples.push_back(std::move(sample));
    }
    risk.sampled_on=s.date;
}
void prepare_deployment_risk(SimulationState& s) {
    if(!s.deployments || !s.deployments->risk || s.date>=s.end_date) return;
    auto& risk=*s.deployments->risk;
    if(risk.prepared_on==s.date) return;
    sample_deployment_risk(s);
    const auto begin=std::ranges::lower_bound(risk.samples,OrdinalDay{s.date.value()-31},{},&DeploymentRiskSample::sampled_on);
    for(auto sample=begin;sample!=risk.samples.end();++sample) if(sample->incident && sample->event_on==s.date) {
        if(risk.incidents.size()>=deployment_incident_limit || risk.alerts.size()>=deployment_incident_alert_limit)
            invalid("The bounded deployment incident history is full");
        const auto index=static_cast<std::uint64_t>(sample-risk.samples.begin());
        const auto id="deployment.incident."+std::to_string(index+1);
        const auto& r=rules(s,sample->project_id);
        issue_invoice(*s.accounts,s.date,{claim_id(id),InvoiceKind::payable,advance_days(s.date,Days{14}),r.compensation,Cents{0},
            "Compensation for deployment incident "+id});
        auto& work=*std::ranges::find(s.capacity->projects,sample->project_id,&CapacityProject::id);
        if(work.disruptions.empty() || work.disruptions.back().ends_before) work.disruptions.push_back({s.date,{}});
        risk.open_incidents.push_back(risk.incidents.size());
        risk.incidents.push_back({id,sample->project_id,index,s.date,{},{}});
        risk.alerts.push_back({"discovered/"+id,id,s.date,false,false});
        s.paused=true;
    }
    for(const auto index:risk.open_incidents) {
        auto& incident=risk.incidents.at(static_cast<std::size_t>(index));
        if(!incident.last_response) continue;
        const auto& response=risk.responses.at(static_cast<std::size_t>(*incident.last_response));
        const auto& work=named(s.capacity->projects,response.project_id);
        const auto& claim=named(s.accounts->invoices,claim_id(incident.id));
        if(work.status!=ProjectStatus::completed || !work.stopped_on || *work.stopped_on>=s.date || claim.settled!=claim.amount) continue;
        incident.recovered_on=s.date;
        if(risk.alerts.size()>=deployment_incident_alert_limit) invalid("The bounded incident alert history is full");
        risk.alerts.push_back({"recovered/"+incident.id,incident.id,s.date,true,false});
    }
    std::erase_if(risk.open_incidents,[&](auto index){return risk.incidents.at(static_cast<std::size_t>(index)).recovered_on.has_value();});
    for(auto& project:s.capacity->projects) if(!project.disruptions.empty() && !project.disruptions.back().ends_before &&
        std::ranges::none_of(risk.open_incidents,[&](auto index){return risk.incidents.at(static_cast<std::size_t>(index)).project_id==project.id;}))
        project.disruptions.back().ends_before=s.date;
    risk.prepared_on=s.date;
    while(risk.alert_cursor<risk.alerts.size() && (risk.alerts[risk.alert_cursor].recovery || risk.alerts[risk.alert_cursor].acknowledged)) ++risk.alert_cursor;
}
DeploymentRiskReport deployment_risk_report(const SimulationState& s,const DeploymentRecord& release,bool staged) {
    const auto& r=rules(s,release.project_id);
    auto calendar=from_ordinal(s.date);
    // Forecast only settled days so far in this calendar month; no future draw.
    std::uint64_t attempts=0,days=0;
    const auto first=std::max(to_ordinal({calendar.year,calendar.month,1}),s.capacity->starts_on);
    for(auto day=first;day<s.date;day=advance_days(day,Days{1})) {
        const auto index=static_cast<std::size_t>(day.value()-s.capacity->starts_on.value());
        if(index>=s.capacity->days.size()) break;
        ++days;
        for(const auto& work:s.capacity->days[index].projects) if(work.project_id==release.project_id) attempts+=work.useful_tasks+work.failed_tasks;
    }
    const auto cell=cell_for(s,{&release,staged},r,attempts,std::max<std::uint64_t>(1,days),true);
    const auto chance=r.lookup[cell].probability;
    const auto band=chance>=r.high_from?"High":chance>=r.elevated_from?"Elevated":"Limited";
    const auto& c=r.lookup[cell];
    auto reasons=std::string("Fictional operational risk. ")+r.assumptions+"\n"+
        (c.large?"Larger":"Smaller")+" settled deployment scale; "+std::string(agent_tools_key(c.autonomy))+" tool permissions; "+
        (c.supervised?"individual approval or paid human review":"batch or unsupervised operation")+
        (staged?"; limited staged exposure":"; production exposure")+".\n"+
        (c.mismatch?"The measured lower bound does not establish the authored control margin.":"Independent evidence meets the authored control margin.")+
        " Incident containment stops only the implicated release. Compensation and paid investigation remain due after replacement.";
    return {band,r.exposure,r.severity,std::move(reasons),staged};
}
DeploymentResponsePreview preview_deployment_response(const SimulationState& s,const RespondDeploymentIncident& request) {
    if(!s.deployments || !s.deployments->risk || s.deployments->risk->responses.size()>=deployment_response_limit)
        invalid("No deployment incident response is available");
    const auto& risk=*s.deployments->risk;
    const auto& incident=named(risk.incidents,request.id);
    if(incident.discovered_on>s.date || incident.recovered_on) invalid("Choose an unresolved discovered incident");
    if(incident.last_response) {
        const auto& work=named(s.capacity->projects,risk.responses.at(static_cast<std::size_t>(*incident.last_response)).project_id);
        if(work.status!=ProjectStatus::cancelled) invalid("This incident already has a committed investigation; resume or complete it");
    }
    return response_terms(s,incident,request.daily_compute,request.staff);
}
void respond_deployment_incident(SimulationState& s,const RespondDeploymentIncident& request,Cents protected_cash) {
    const auto quote=preview_deployment_response(s,request);
    if(quote.cash_required>checked_unit_subtract(s.cash,protected_cash)) invalid("Incident response would spend protected or unavailable cash");
    auto& risk=*s.deployments->risk;
    auto& incident=*std::ranges::find(risk.incidents,request.id,&DeploymentIncident::id);
    CapacityProject work;
    work.id="deployment.response/"+std::to_string(risk.responses.size()+1);
    work.kind=ProjectKind::incident_response;
    work.source_id=request.source_id;
    work.starts_on=s.date;
    work.requested=request.daily_compute;
    work.staff=request.staff;
    work.total_work=quote.work;
    work.cause="Investigate and remediate "+incident.id;
    reserve_project(*s.capacity,s.date,work);
    if(quote.fee.value()>0) record_cash(*s.accounts,s.date,Cents{-quote.fee.value()},Account::expenses,work.cause,work.id);
    if(quote.compensation_due.value()>0) settle_invoice(*s.accounts,s.date,claim_id(incident.id),quote.compensation_due,protected_cash,"response.claim/"+work.id);
    incident.last_response=risk.responses.size();
    risk.responses.push_back({incident.id,work.id,s.date});
    s.cash=accounts_report(*s.accounts).cash;
}
const DeploymentIncidentAlert* required_deployment_incident(const SimulationState& s) {
    if(!s.deployments || !s.deployments->risk) return nullptr;
    const auto& risk=*s.deployments->risk;
    return risk.alert_cursor<risk.alerts.size()?&risk.alerts[risk.alert_cursor]:nullptr;
}
std::string deployment_incident_description(const SimulationState& s,const DeploymentIncidentAlert& alert) {
    const auto& incident=named(s.deployments->risk->incidents,alert.incident_id);
    const auto all=releases(s);
    const auto& release=*all.at(incident.project_id).record;
    const auto& claim=named(s.accounts->invoices,claim_id(incident.id));
    const auto amount=[](Cents v){return "$"+std::to_string(v.value()/100)+"."+(v.value()%100<10?"0":"")+std::to_string(v.value()%100);};
    auto text=release.service_id+" · "+incident.project_id+" · "+incident.id+"\n";
    if(alert.recovery) return text+"Paid investigation and compensation completed. Containment ended on "+iso_date(alert.day)+
        ". Earlier costs, customer credits and release history remain recorded.";
    text+="A deployment incident was discovered on "+iso_date(incident.discovered_on)+". Containment stops the implicated release; signed promises retain ordinary service credits. Other releases keep their own resources.\n"+
        amount(claim.amount)+" compensation, due "+iso_date(claim.due)+"; "+amount(checked_unit_subtract(claim.amount,claim.settled))+" remains unpaid.\n";
    if(incident.recovered_on) text+="Recovery completed on "+iso_date(*incident.recovered_on)+".";
    else if(incident.last_response) {
        const auto& response=s.deployments->risk->responses.at(static_cast<std::size_t>(*incident.last_response));
        const auto& work=named(s.capacity->projects,response.project_id);
        text+="Investigation "+work.id+": "+std::to_string(work.completed_work.value())+" / "+std::to_string(work.total_work.value())+" milli-CU recorded.";
    } else text+="Review a paid investigation in Office Services. Acknowledging this notice does not repair the release.";
    return text;
}
void acknowledge_deployment_incident(SimulationState& s,std::string_view id) {
    if(!s.deployments || !s.deployments->risk) invalid("No deployment incident notice is available");
    auto& risk=*s.deployments->risk;
    auto found=std::ranges::find(risk.alerts,id,&DeploymentIncidentAlert::id);
    if(found==risk.alerts.end()) invalid("Choose a recorded deployment incident notice");
    found->acknowledged=true;
    while(risk.alert_cursor<risk.alerts.size() && (risk.alerts[risk.alert_cursor].recovery || risk.alerts[risk.alert_cursor].acknowledged)) ++risk.alert_cursor;
}
} // namespace pgp::core
