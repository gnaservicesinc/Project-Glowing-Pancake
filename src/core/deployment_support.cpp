#include "deployment_internal.hpp"
#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <stdexcept>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
template<class Rows> const auto& named(const Rows& rows,std::string_view id) {
    const auto row=std::ranges::find(rows,id,&Rows::value_type::id);
    if (row==rows.end()) invalid("Service support refers to a missing release, workload or report");
    return *row;
}
const MarketProduct& live_service(const SimulationState& state,std::string_view id) {
    if (!state.deployments || !state.lab || !state.market || !state.capacity || !state.accounts)
        invalid("This company has no qualified services");
    const auto& product=named(state.market->products,id);
    if (!product.available || named(state.capacity->projects,product.capacity_project_id).status!=ProjectStatus::active)
        invalid("Choose an operating production service");
    if (pending_rollout(*state.deployments,id)) invalid("Promote or discard the staged release before publishing new terms");
    return product;
}
OrdinalDay support_end(const SimulationState& state,Days days) {
    if (days.value()<service_term_min_days || days.value()>service_term_max_days)
        invalid("Choose a service period from 30 to 1,096 days");
    const auto end=advance_days(state.date,days);
    if (end>state.end_date) invalid("Choose a service period within this campaign");
    return end;
}
CompatibilityProfile profile(const ServiceDefinition& service,const LabState& lab,std::string_view evidence,
                             bool review,const CapacityProject& work,std::string_view audience,std::optional<AgentControls> controls) {
    CompatibilityProfile result;
    const auto& report=named(lab.evaluations,evidence);
    for (std::size_t i=0;i<lab_domain_count;++i) if (service.required[i].value()>0) {
        const auto credit=review?service.review_credit[i].value():0;
        result.quality[i]=BasisPoints{std::min<std::int64_t>(10000,report.lower[i].value()+credit)};
    }
    result.human_review=review;
    result.tasks_per_day=work.tasks_per_day;
    result.weight_audience=audience;
    result.agent_controls=controls;
    if (work.customer_inference) {
        result.customer_memory_mib=work.minimum_memory_mib;
        result.customer_interconnect=work.interconnect;
        result.customer_compute_per_task=work.inference_milli_CU_per_task;
    }
    return result;
}
const char* incompatibility(const CompatibilityProfile& promise,const CompatibilityProfile& candidate) {
    if (!candidate.weight_audience.empty() && candidate.weight_audience!=promise.weight_audience)
        return "retain the promised publication audience";
    if (promise.human_review && !candidate.human_review) return "retain paid human review";
    if (promise.agent_controls && promise.agent_controls!=candidate.agent_controls)
        return "keep the promised tool permissions and oversight";
    if (candidate.tasks_per_day<promise.tasks_per_day) return "keep the promised daily task capacity";
    for (std::size_t i=0;i<lab_domain_count;++i)
        if (candidate.quality[i]<promise.quality[i]) return "meet the promised measured quality floor";
    if (candidate.customer_memory_mib>promise.customer_memory_mib ||
        candidate.customer_interconnect!=promise.customer_interconnect ||
        candidate.customer_compute_per_task>promise.customer_compute_per_task)
        return "stay within the promised buyer memory, interconnect and compute requirements";
    return nullptr;
}
auto release_for(const DeploymentState& deployments,std::string_view project) {
    const auto release=std::ranges::find(deployments.records,project,&DeploymentRecord::project_id);
    if (release==deployments.records.end()) invalid("Compatibility commitment has no qualified production release");
    return release;
}
CompatibilityProfile profile(const DeploymentState& deployments,const LabState& lab,const CapacityState& capacity,
                             const DeploymentRecord& release) {
    return profile(deployment_detail::service(deployments,release.service_id),lab,release.evidence_id,
                   release.human_review,named(capacity.projects,release.project_id),release.weight_audience,release.agent_controls);
}
// Reconcile adjacent production bindings, including any changes made on the
// same morning. A cancelled project is a gap unless another binding starts then.
void verify_coverage(const DeploymentState& deployments,const LabState& lab,const CapacityState& capacity,
                     std::vector<DeploymentRecord>::const_iterator release,OrdinalDay starts,OrdinalDay end,
                     const CompatibilityProfile* promise) {
    const std::string_view id=release->service_id;
    auto covered=starts;
    for (;release!=deployments.records.end();++release) {
        if (release->service_id!=id) continue;
        const auto& work=named(capacity.projects,release->project_id);
        if (release->deployed_on>covered || (work.stopped_on && *work.stopped_on<covered))
            invalid("A supported service lost its production binding during the published period");
        if (promise && incompatibility(*promise,profile(deployments,lab,capacity,*release)))
            invalid("A saved release broke its published compatibility commitment");
        if (!work.stopped_on || *work.stopped_on>=end) return;
        covered=*work.stopped_on;
    }
    invalid("A supported service stopped before its published date");
}
}

ServiceCompatibility preview_compatibility(const SimulationState& state,const CommitServiceCompatibility& request) {
    const auto& product=live_service(state,request.service_id);
    const auto end=support_end(state,request.term);
    if (state.market->compatibility.size()>=service_compatibility_limit)
        invalid("Compatibility commitment history is full");
    if (market_compatibility(*state.market,product.id,state.date))
        invalid("This service already has an active compatibility commitment");
    if (product.retirement && end>product.retirement->retires_on)
        invalid("The compatibility commitment would run past the published retirement date");
    return {product.id,product.capacity_project_id,state.date,end};
}
CompatibilityProfile compatibility_profile(const SimulationState& state,const ServiceCompatibility& term) {
    return profile(*state.deployments,*state.lab,*state.capacity,*release_for(*state.deployments,term.project_id));
}
void commit_service_compatibility(SimulationState& state,const CommitServiceCompatibility& request) {
    auto term=preview_compatibility(state,request);
    state.market->compatibility.push_back(std::move(term));
}
ProductRetirement preview_retirement(const SimulationState& state,const AnnounceServiceRetirement& request) {
    const auto& product=live_service(state,request.service_id);
    if (product.retirement) invalid("This service already has a published retirement date");
    if (state.resilience && std::ranges::any_of(state.resilience->replicas,[&](const auto& row) {
        return row.service_id==product.id && !row.stopped_on;
    })) invalid("Stop the geographic replica before announcing this service's retirement");
    const auto end=support_end(state,request.notice);
    auto earliest=state.date;
    if (const auto* term=market_compatibility(*state.market,product.id,state.date)) earliest=term->ends_before;
    for (const auto& contract:state.market->contracts) if (contract.product_id==product.id &&
        (contract.status==ContractStatus::pending || contract.status==ContractStatus::active))
        earliest=std::max(earliest,contract.ends_before);
    if (end<earliest) throw std::invalid_argument("Existing signed or published terms set the earliest retirement date to "+iso_date(earliest)+".");
    return {state.date,end};
}
void announce_service_retirement(SimulationState& state,const AnnounceServiceRetirement& request) {
    const auto notice=preview_retirement(state,request);
    auto row=std::ranges::find(state.market->products,request.service_id,&MarketProduct::id);
    const auto& work=named(state.capacity->projects,row->capacity_project_id);
    if (work.monthly_support_cost.value()>0)
        schedule_recurring_end(*state.accounts,state.date,"support/"+work.id,notice.retires_on);
    row->retirement=notice;
}
std::size_t retire_due_services(SimulationState& state) {
    if (!state.market) return 0;
    std::size_t retired=0;
    for (auto& product:state.market->products) {
        if (!product.available || !product.retirement || product.retirement->retires_on!=state.date) continue;
        if (pending_rollout(*state.deployments,product.id)) finish_service_rollout(state,{product.id,false});
        cancel_project(*state.capacity,state.date,product.capacity_project_id);
        product.available=false;
        ++retired;
    }
    return retired;
}

void deployment_detail::require_compatible(const DeploymentState& state,const LabState& lab,const CapacityState& capacity,
    const MarketState& market,const DeployModel& request,const CapacityProject& work,OrdinalDay day) {
    const auto* term=market_compatibility(market,request.service_id,day);
    if (!term) return;
    const auto promise=profile(state,lab,capacity,*release_for(state,term->project_id));
    const auto candidate=profile(service(state,request.service_id),lab,request.evidence_id,request.human_review,work,request.weight_audience,request.agent_controls);
    if (const auto* conflict=incompatibility(promise,candidate))
        throw std::invalid_argument("The compatibility commitment requires this release to "+std::string(conflict)+
            " until "+iso_date(term->ends_before)+".");
}
void validate_service_support(const DeploymentState& state,const LabState& lab,const CapacityState& capacity,const MarketState& market) {
    const auto today=market.closed_through?advance_days(*market.closed_through,Days{1}):market.starts_on;
    for (const auto& term:market.compatibility) {
        const auto release=release_for(state,term.project_id);
        if (release->service_id!=term.product_id || release->deployed_on>term.starts_on)
            invalid("Compatibility commitment must name its qualified production release");
        const auto promise=profile(state,lab,capacity,*release);
        verify_coverage(state,lab,capacity,release,term.starts_on,std::min(advance_days(today,Days{1}),term.ends_before),&promise);
        for (const auto& trial:market.trials)
            if (trial.product_id==term.product_id && trial.starts_on<term.starts_on &&
                trial.stopped_on.value_or(trial.ends_before)>term.starts_on)
                invalid("Compatibility commitment began before the previous service trial stopped");
    }
    for (const auto& product:market.products) if (const auto& notice=product.retirement; notice) {
        const auto& current=named(capacity.projects,product.capacity_project_id);
        const bool retired=today>=notice->retires_on;
        if (product.available==retired || (retired && current.stopped_on!=notice->retires_on))
            invalid("Service retirement differs from its advertised date or released workload");
        const auto opening=std::ranges::find_if(state.records,[&](const auto& row) {
            if (row.service_id!=product.id || row.deployed_on>notice->announced_on) return false;
            const auto& work=named(capacity.projects,row.project_id);
            return !work.stopped_on || *work.stopped_on>=notice->announced_on;
        });
        if (opening==state.records.end()) invalid("Service retirement notice was issued without a production release");
        verify_coverage(state,lab,capacity,opening,notice->announced_on,
                        std::min(advance_days(today,Days{1}),notice->retires_on),nullptr);
        for (const auto& trial:market.trials) if (trial.product_id==product.id) {
            const auto until=trial.stopped_on.value_or(trial.ends_before);
            if ((trial.starts_on<notice->announced_on && until>notice->announced_on) ||
                (until>notice->announced_on && trial.ends_before>notice->retires_on))
                invalid("A service trial overlapped publication of notice or extended past retirement");
        }
    }
}
} // namespace pgp::core
