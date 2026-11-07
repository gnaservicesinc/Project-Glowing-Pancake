#include "deployment_internal.hpp"
#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <map>
#include <stdexcept>

namespace pgp::core {
namespace {
using deployment_detail::qualified_project;
using deployment_detail::service;
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
const CapacityProject& workload(const CapacityState& capacity,std::string_view id) {
    const auto row=std::ranges::find(capacity.projects,id,&CapacityProject::id);
    if (row==capacity.projects.end()) invalid("Service rollout has no serving workload");
    return *row;
}
const MarketProduct& product(const MarketState& market,std::string_view id) {
    const auto row=std::ranges::find(market.products,id,&MarketProduct::id);
    if (row==market.products.end()) invalid("Select a service already in production");
    return *row;
}
std::uint64_t live_release(const DeploymentState& deployments,const MarketProduct& product) {
    const auto it=std::ranges::find(deployments.records,product.capacity_project_id,&DeploymentRecord::project_id);
    if (it==deployments.records.end()) invalid("Production service has no qualified release");
    return static_cast<std::uint64_t>(it-deployments.records.begin())+1;
}
bool same_release(const DeploymentRecord& row,const DeployModel& other) {
    return row.service_id==other.service_id && row.model_id==other.model_id && row.evidence_id==other.evidence_id &&
           row.human_review==other.human_review && (other.region_id.empty() || row.region_id==other.region_id);
}
std::string trial_id(std::size_t index) { return "serving.trial/"+std::to_string(index+1); }
void require_service_state(const SimulationState& state) {
    if (!state.deployments || !state.lab || !state.market || !state.capacity || !state.accounts)
        invalid("This company has no qualified service catalog");
}
void require_no_replica(const SimulationState& state,std::string_view id) {
    if (state.resilience && std::ranges::any_of(state.resilience->replicas,[&](const auto& row) {
        return row.service_id==id && !row.stopped_on;
    })) invalid("Stop the geographic replica before changing its production release");
}
void require_civic_launch(const SimulationState& state,const DeployModel& request) {
    if (const auto blocked=civic_launch_block(state.civic?&*state.civic:nullptr,state,request,state.date); !blocked.empty())
        throw std::invalid_argument(blocked);
}
CapacityProject trial_profile(CapacityProject candidate,const CapacityProject& incumbent,BasisPoints share) {
    // The customer budget is expressed in inference work per task. A shared
    // on-premise quote needs the same hardware footprint for either version.
    if (candidate.customer_inference!=incumbent.customer_inference ||
        (candidate.customer_inference && (candidate.minimum_memory_mib!=incumbent.minimum_memory_mib ||
         candidate.interconnect!=incumbent.interconnect || candidate.inference_milli_CU_per_task!=incumbent.inference_milli_CU_per_task)))
        invalid("A staged local release must fit the current buyer hardware and compute terms");
    const auto limit=std::min(candidate.tasks_per_day,mul_div_floor(incumbent.tasks_per_day,
        static_cast<std::uint64_t>(share.value()),10000).quotient);
    if (limit==0) invalid("This traffic share is too small for one task in the serving profile");
    const auto support=mul_div_floor(static_cast<std::uint64_t>(candidate.monthly_support_cost.value()),
        limit,candidate.tasks_per_day);
    candidate.monthly_support_cost=Cents{support.quotient+(support.remainder>0?1U:0U)};
    candidate.tasks_per_day=limit;
    if (!candidate.customer_inference)
        candidate.requested=MilliCUPerDay{required_inference(limit,candidate.inference_milli_CU_per_task).value()};
    return candidate;
}
void stop_trial(SimulationState& state,std::size_t index) {
    auto& trial=state.market->trials.at(index);
    if (trial.stopped_on) return;
    const auto& project=workload(*state.capacity,trial.project_id);
    const auto support=project.monthly_support_cost;
    cancel_project(*state.capacity,state.date,trial.project_id);
    if (support.value()>0 && state.date<trial.ends_before)
        end_recurring(*state.accounts,"support/"+trial.project_id,state.date);
    trial.stopped_on=state.date;
}
}

const ServiceRollout* pending_rollout(const DeploymentState& state,std::string_view service_id) {
    const auto found=std::find_if(state.rollouts.rbegin(),state.rollouts.rend(),[&](const auto& row) {
        return row.candidate.service_id==service_id && !row.resolved_on;
    });
    return found==state.rollouts.rend()?nullptr:&*found;
}
const DeploymentRecord* trial_release(const DeploymentState& state,const MarketTrial& trial) {
    const auto found=std::find_if(state.rollouts.begin(),state.rollouts.end(),[&](const auto& row) {
        return row.candidate.project_id==trial.project_id;
    });
    return found==state.rollouts.end()?nullptr:&found->candidate;
}
DeployModel release_request(const DeploymentRecord& row,const CapacityState& capacity) {
    return {row.service_id,row.model_id,row.evidence_id,workload(capacity,row.project_id).source_id,row.human_review,row.region_id};
}
std::uint64_t rollback_release(const DeploymentState& state,const MarketState& market,std::string_view id) {
    const auto found=std::ranges::find(market.products,id,&MarketProduct::id);
    if (found==market.products.end() || pending_rollout(state,id)) return 0;
    const auto current=live_release(state,*found);
    const auto& active=state.records[static_cast<std::size_t>(current-1)];
    for (auto i=current-1;i>0;--i) {
        const auto& row=state.records[static_cast<std::size_t>(i-1)];
        if (row.service_id==id && (row.model_id!=active.model_id || row.evidence_id!=active.evidence_id ||
            row.human_review!=active.human_review)) return i;
    }
    return 0;
}
CapacityProject preview_rollout(const SimulationState& state,const StageServiceRollout& request) {
    require_service_state(state);
    const auto& selection=request.candidate;
    const auto& current=product(*state.market,selection.service_id);
    const auto& incumbent=workload(*state.capacity,current.capacity_project_id);
    if (!current.available || incumbent.status!=ProjectStatus::active)
        invalid("A staged rollout requires an operating production service");
    const auto& deployments=*state.deployments;
    if (deployments.rollouts.size()>=service_trial_limit || deployments.records.size()>=lab_record_limit)
        invalid("The company has filled its service release history");
    if (pending_rollout(deployments,selection.service_id))
        invalid("Resolve the current staged release before starting another trial");
    if (request.share.value()==0 || request.share.value()>service_trial_share_limit ||
        request.term.value()==0 || request.term.value()>service_trial_days_limit ||
        advance_days(state.date,request.term)>state.end_date)
        invalid("Choose up to 50% of discretionary traffic for up to 30 days within the campaign");
    if (!selection.region_id.empty() && selection.region_id!=current.region_id)
        invalid("A staged release keeps the service's current territory");
    const auto prior=live_release(deployments,current);
    if (same_release(deployments.records[static_cast<std::size_t>(prior-1)],selection))
        invalid("Choose a different checkpoint, report or review setting for the candidate");
    require_no_replica(state,selection.service_id);
    require_civic_launch(state,selection);
    return trial_profile(qualified_project(service(deployments,selection.service_id),*state.lab,*state.capacity,
        selection,state.date,trial_id(deployments.rollouts.size())),incumbent,request.share);
}
void stage_service_rollout(SimulationState& state,const StageServiceRollout& request) {
    auto work=preview_rollout(state,request);
    const auto& selection=request.candidate;
    const auto& current=product(*state.market,selection.service_id);
    const auto prior=live_release(*state.deployments,current);
    const auto ends_before=advance_days(state.date,request.term);
    reserve_project(*state.capacity,state.date,work);
    if (work.monthly_support_cost.value()>0)
        add_recurring(*state.accounts,{"support/"+work.id,work.monthly_support_cost,state.date,ends_before,"Staged service support: "+work.id});
    state.market->trials.push_back({selection.service_id,work.id,state.date,ends_before,request.share});
    state.deployments->rollouts.push_back({{selection.service_id,selection.model_id,selection.evidence_id,work.id,
        selection.human_review,state.date,current.region_id},prior});
}
void finish_service_rollout(SimulationState& state,const FinishServiceRollout& request) {
    require_service_state(state);
    auto& deployments=*state.deployments;
    const auto* pending=pending_rollout(deployments,request.service_id);
    if (!pending) invalid("This service has no pending staged release");
    const auto index=static_cast<std::size_t>(pending-deployments.rollouts.data());
    auto& row=deployments.rollouts[index];
    const auto launch=release_request(row.candidate,*state.capacity);
    if (request.promote) {
        if (workload(*state.capacity,row.candidate.project_id).useful_tasks==0)
            invalid("The candidate needs settled customer work before promotion");
        require_no_replica(state,request.service_id);
        require_civic_launch(state,launch);
    }
    stop_trial(state,index);
    row.resolved_on=state.date;
    if (request.promote) {
        deploy_model(deployments,*state.lab,*state.accounts,*state.capacity,*state.market,state.date,launch);
        row.promoted_release=deployments.records.size();
    }
}
void rollback_service(SimulationState& state,const RollbackService& request) {
    require_service_state(state);
    auto& deployments=*state.deployments;
    const auto prior=rollback_release(deployments,*state.market,request.service_id);
    if (prior==0) invalid("Resolve any pending trial and choose a service with a previous release");
    const auto launch=release_request(deployments.records[static_cast<std::size_t>(prior-1)],*state.capacity);
    require_no_replica(state,request.service_id);
    require_civic_launch(state,launch);
    deploy_model(deployments,*state.lab,*state.accounts,*state.capacity,*state.market,state.date,launch);
    deployments.records.back().restored_release=prior;
}
void expire_service_rollouts(SimulationState& state) {
    if (!state.market || !state.deployments) return;
    for (std::size_t i=0;i<state.market->trials.size();++i) {
        const auto& route=state.market->trials[i];
        if (!route.stopped_on && state.date==route.ends_before) stop_trial(state,i);
    }
}
void discard_service_rollouts(SimulationState& state) {
    if (!state.deployments) return;
    for (std::size_t i=0;i<state.deployments->rollouts.size();++i) {
        auto& row=state.deployments->rollouts[i];
        if (row.resolved_on) continue;
        stop_trial(state,i);
        row.resolved_on=state.date;
    }
}
void validate_service_rollouts(const DeploymentState& state,const LabState& lab,const CapacityState& capacity,const MarketState& market) {
    if (state.rollouts.size()!=market.trials.size()) invalid("Qualified rollout history differs from its traffic schedule");
    const auto today=market.closed_through?advance_days(*market.closed_through,Days{1}):market.starts_on;
    std::map<std::string_view,OrdinalDay> resolved;
    for (std::size_t i=0;i<state.rollouts.size();++i) {
        const auto& row=state.rollouts[i];
        const auto& trial=market.trials[i];
        const auto& candidate=row.candidate;
        if (row.incumbent_release==0 || row.incumbent_release>state.records.size() ||
            candidate.project_id!=trial.project_id || candidate.project_id!=trial_id(i) ||
            candidate.service_id!=trial.product_id || candidate.deployed_on!=trial.starts_on ||
            candidate.restored_release!=0 ||
            (!trial.stopped_on && trial.ends_before<=today) ||
            (row.resolved_on && (*row.resolved_on<trial.starts_on || *row.resolved_on>today || !trial.stopped_on ||
             *trial.stopped_on!=std::min(*row.resolved_on,trial.ends_before))) ||
            (!row.resolved_on && (row.promoted_release>0 || (trial.stopped_on && *trial.stopped_on!=trial.ends_before))))
            invalid("Service rollout has inconsistent release or decision dates");
        if (const auto previous=resolved.find(trial.product_id); previous!=resolved.end() && previous->second>trial.starts_on)
            invalid("A later service trial began before the earlier decision");
        resolved.insert_or_assign(trial.product_id,row.resolved_on.value_or(advance_days(today,Days{1})));
        const auto& incumbent=state.records[static_cast<std::size_t>(row.incumbent_release-1)];
        const auto& old=workload(capacity,incumbent.project_id);
        const auto& staged=workload(capacity,candidate.project_id);
        const auto launch=release_request(candidate,capacity);
        if (incumbent.service_id!=trial.product_id || incumbent.deployed_on>trial.starts_on ||
            incumbent.region_id!=candidate.region_id || same_release(incumbent,launch) ||
            (old.stopped_on && (!row.resolved_on || *old.stopped_on<*row.resolved_on)) ||
            (!row.resolved_on && live_release(state,product(market,trial.product_id))!=row.incumbent_release))
            invalid("A staged release changed or lost its incumbent");
        const auto expected=trial_profile(qualified_project(service(state,candidate.service_id),lab,capacity,
            launch,trial.starts_on,trial.project_id),old,trial.share);
        auto comparable=staged;
        comparable.status=ProjectStatus::active;
        comparable.stopped_on.reset();
        comparable.useful_tasks=0;
        comparable.completed_work=MilliCU{0};
        if (expected!=comparable) invalid("Service trial differs from its qualified resource profile");
        if (row.promoted_release>0) {
            if (row.promoted_release<=row.incumbent_release || row.promoted_release>state.records.size() || staged.useful_tasks==0)
                invalid("Promoted service has no settled trial or new release");
            const auto& release=state.records[static_cast<std::size_t>(row.promoted_release-1)];
            if (release.deployed_on!=*row.resolved_on || release.restored_release!=0 || !same_release(release,launch) ||
                workload(capacity,release.project_id).source_id!=launch.source_id)
                invalid("Promotion did not use the measured staged release");
        }
    }
}
} // namespace pgp::core
