#include "frontier_internal.hpp"

#include <set>

namespace pgp::core {
namespace {
using namespace frontier_detail;

void check_budget(const SimulationState& state, const FrontierBudget& budget, Days minimum_interval, OrdinalDay day) {
    const auto& source = require_named(state.capacity->sources,budget.allocation.source_id);
    if ((source.available_from && *source.available_from>day) || budget.allocation.daily_compute.value()<=0 ||
        budget.allocation.daily_compute.value()>1'000'000'000 || budget.allocation.staff==0 || budget.allocation.staff>128 ||
        budget.limit.value()<0 || budget.limit.value()>1'000'000'000'000 || budget.reserve.value()<0 ||
        budget.reserve.value()>1'000'000'000'000 || budget.interval<minimum_interval || budget.interval.value()>1096 ||
        budget.runs==0 || budget.runs>frontier_policy_run_limit)
        invalid("Authorize a finite cash limit, reserve, work allocation and review interval.");
}
void check_research_plan(const SimulationState& state, const FrontierResearchPlan& plan, OrdinalDay day) {
    check_budget(state,plan.budget,Days{plan.standing?7:1},day);
    if (plan.topics.empty() || plan.topics.size()>frontier_queue_limit ||
        std::set<std::string>(plan.topics.begin(),plan.topics.end()).size()!=plan.topics.size() ||
        (plan.standing ? plan.topics.size()!=1 : plan.budget.runs!=plan.topics.size()))
        invalid("Queue distinct topics in order or authorize one repeatable standing track.");
    for (const auto& id : plan.topics) (void)require_named(state.lab->catalog.research,id);
    if (plan.standing) {
        const auto& topic=require_named(state.lab->catalog.research,plan.topics.front());
        if (topic.repeat_limit<2 || plan.budget.runs>topic.repeat_limit)
            invalid("Standing research must fit the topic's authored experiment limit.");
    }
}
std::uint64_t latest_deployment(const SimulationState& state, std::string_view service, OrdinalDay day) {
    const auto& records = state.deployments->records;
    const auto at = std::find_if(records.rbegin(),records.rend(),[&](const auto& row){
        return row.service_id==service && row.deployed_on<=day;
    });
    return at==records.rend() ? 0 : static_cast<std::uint64_t>(records.rend()-at);
}
void check_refresh_plan(const SimulationState& state, const FrontierRefreshPlan& plan, OrdinalDay day) {
    check_budget(state,plan.budget,Days{30},day);
    const auto& service = require_named(state.market->products,plan.service);
    const auto& recipe = require_named(state.lab->catalog.recipes,plan.recipe);
    const auto& test = require_named(state.lab->catalog.evaluations,plan.evaluation);
    if (service.channel==ProductChannel::open_weights || !test.independent ||
        plan.improvement.value()<1 || plan.improvement.value()>1000 || plan.work<recipe.minimum_work ||
        plan.work>recipe.maximum_work || plan.data.empty() || plan.data.size()>lab_catalog_limit ||
        !std::is_sorted(plan.data.begin(),plan.data.end()) ||
        std::adjacent_find(plan.data.begin(),plan.data.end())!=plan.data.end())
        invalid("Refresh needs licensed commercial data, independent evaluation, a finite candidate and private deployment rights.");
    for (const auto& id : plan.data) {
        const auto& data=require_named(state.lab->catalog.data,id);
        if (!data.commercial_use || std::ranges::none_of(state.lab->licenses,[&](const auto& license){
            return license.offer_id==id && license.acquired_on<=day;
        })) invalid("Refresh cannot buy new rights or use data outside an actual commercial license.");
    }
    if (!latest_deployment(state,plan.service,day)) invalid("Select a deployed service to authorize mature refresh.");
}
std::string_view topic_for(const FrontierResearchPlan& plan, std::size_t index) {
    return plan.topics.at(plan.standing?0:index);
}
std::uint32_t completed_experiments(const SimulationState& state, std::string_view topic, OrdinalDay day) {
    std::uint32_t count=0;
    for (const auto& run : state.lab->research) if (run.research_id==topic && run.completed) {
        const auto& work=require_named(state.capacity->projects,run.project_id);
        if (work.stopped_on && *work.stopped_on<day) ++count;
    }
    return count;
}
bool prerequisites(const SimulationState& state, const std::vector<std::string>& all, const std::vector<std::string>& any={}) {
    const auto known=[&](const auto& id){return completed_experiments(state,id,state.date)>0;};
    return std::ranges::all_of(all,known) && (any.empty() || std::ranges::any_of(any,known));
}
bool within_budget(const SimulationState& state, const FrontierBudget& budget, Cents paid, Cents fee, Cents protected_cash) {
    return fee<=checked_unit_subtract(budget.limit,paid) &&
        checked_unit_add(fee,budget.reserve)<=checked_unit_subtract(state.cash,protected_cash);
}
Cents research_spent(const FrontierResearchProgram& program) {
    Cents result{0};
    for (const auto& row : program.decisions) result=checked_unit_add(result,row.paid);
    return result;
}
Cents refresh_spent(const FrontierRefreshProgram& program, const EvaluationPlan& plan) {
    Cents result{0};
    for (const auto& run : program.runs) if (run.evaluated_on) result=checked_unit_add(result,plan.cash);
    return result;
}
bool fresh_method(const SimulationState& state, OrdinalDay day) {
    const auto& catalog=*state.frontier;
    for (const auto& run : state.lab->research) if (run.research_id==catalog.catalog.refresh_research && run.completed) {
        const auto& work=require_named(state.capacity->projects,run.project_id);
        if (work.stopped_on && *work.stopped_on<day &&
            advance_days(*work.stopped_on,catalog.catalog.refresh_review_age)>=day) return true;
    }
    return false;
}
bool improvement(const SimulationState& state, const DeploymentRecord& incumbent, const EvaluationEvidence& candidate, BasisPoints gain) {
    const auto& original=require_named(state.lab->evaluations,incumbent.evidence_id);
    const auto& old_plan=require_named(state.lab->catalog.evaluations,original.plan_id);
    const auto& new_plan=require_named(state.lab->catalog.evaluations,candidate.plan_id);
    bool better=false;
    for (std::size_t axis=0; axis<lab_domain_count; ++axis) {
        if (old_plan.coverage[axis].value()==0) continue;
        if (new_plan.coverage[axis]<old_plan.coverage[axis] || candidate.lower[axis]<original.lower[axis]) return false;
        if (candidate.lower[axis].value()-original.lower[axis].value()>=gain.value()) better=true;
    }
    return better;
}
void stop_refresh(SimulationState& state, FrontierRefreshProgram& program, std::string reason) {
    if (program.runs.empty() || program.runs.back().finished_on) return;
    auto& run=program.runs.back();
    const auto id=frontier_refresh_id(program.id,program.runs.size());
    const auto& model=require_named(state.lab->models,id);
    const auto project=run.evaluated_on?require_named(state.lab->evaluations,id).project_id:model.project_id;
    if (require_named(state.capacity->projects,project).status==ProjectStatus::active)
        cancel_project(*state.capacity,state.date,project);
    run.finished_on=state.date;
    run.outcome=std::move(reason);
    program.waiting.clear();
}
bool run_research(SimulationState& state, std::size_t index, Cents protected_cash) {
    auto& program=state.frontier->research_programs[index];
    if (!frontier_research_active(program) || state.date<=program.authorized_on) return false;
    const auto& plan=program.plan;
    if (!program.decisions.empty()) {
        const auto& previous=program.decisions.back();
        if (state.date<advance_days(previous.day,plan.budget.interval) ||
            (!previous.project.empty() && require_named(state.capacity->projects,previous.project).status==ProjectStatus::active))
            return false;
    }
    const auto& node=require_named(state.lab->catalog.research,topic_for(plan,program.decisions.size()));
    if (completed_experiments(state,node.id,state.date)>=node.repeat_limit) {
        program.decisions.push_back({state.date,{},Cents{0}});
        program.waiting="The topic already completed its authorized experiments.";
        return false;
    }
    if (std::ranges::any_of(state.lab->research,[&](const auto& run){
        return run.research_id==node.id && require_named(state.capacity->projects,run.project_id).status==ProjectStatus::active;
    })) {
        program.waiting="This topic already has work in progress.";
        return false;
    }
    if (auto blocked=frontier_technology_block(state,FrontierTechnology::research,node.id,state.date); !blocked.empty()) {
        program.waiting=std::move(blocked);
        return false;
    }
    if (!prerequisites(state,node.all_of,node.any_of)) {
        program.waiting="Waiting for the selected topic's current prerequisites.";
        return false;
    }
    if (!within_budget(state,plan.budget,research_spent(program),node.cash,protected_cash)) {
        program.waiting="Research would exceed the approved total or protected cash floor.";
        return false;
    }
    if (auto blocked=allocation_block(*state.capacity,plan.budget.allocation); !blocked.empty()) {
        program.waiting=std::move(blocked);
        return false;
    }
    auto candidate=state;
    try {
        start_research(*candidate.lab,*candidate.accounts,*candidate.capacity,state.date,node.id,plan.budget.allocation,protected_cash);
        candidate.cash=accounts_report(*candidate.accounts).cash;
        auto& accepted=candidate.frontier->research_programs[index];
        accepted.decisions.push_back({state.date,candidate.lab->research.back().project_id,node.cash});
        accepted.waiting.clear();
    } catch (const std::invalid_argument& error) {
        program.waiting=error.what();
        return false;
    }
    state=std::move(candidate);
    return true;
}
bool run_refresh(SimulationState& state, std::size_t index, Cents protected_cash, const FrontierLaunchGuard& guard) {
    auto& program=state.frontier->refresh_programs[index];
    if (!frontier_refresh_active(program) || state.date<=program.authorized_on) return false;
    const auto& plan=program.plan;
    const auto current=latest_deployment(state,plan.service,state.date);
    const auto& incumbent=deployment(state,current);
    const auto& incumbent_work=require_named(state.capacity->projects,incumbent.project_id);
    const auto& test=require_named(state.lab->catalog.evaluations,plan.evaluation);
    const bool active=!program.runs.empty() && !program.runs.back().finished_on;
    const bool live=serves_interval(incumbent_work,state.date,state.date) &&
        require_named(state.market->products,plan.service).available;
    if (active && (!live || current!=program.runs.back().previous_deployment)) {
        stop_refresh(state,program,"The original deployment changed or stopped during review.");
        return true;
    }
    if (!live) { program.waiting="Waiting for the named service to operate."; return false; }
    if (!active) {
        if (state.date<advance_days(incumbent.deployed_on,plan.budget.interval) ||
            (!program.runs.empty() && state.date<advance_days(*program.runs.back().finished_on,plan.budget.interval)))
            return false;
        if (!fresh_method(state,state.date)) {
            program.waiting="Complete a fresh deployment methods review through the Lab or its standing research track.";
            return false;
        }
        if (auto blocked=frontier_technology_block(state,FrontierTechnology::recipe,plan.recipe,state.date); !blocked.empty()) {
            program.waiting=std::move(blocked);
            return false;
        }
        const auto id=frontier_refresh_id(program.id,program.runs.size()+1);
        const TrainingRequest request{id,plan.recipe,"",plan.data,plan.budget.allocation.source_id,
            plan.budget.allocation.daily_compute,plan.budget.allocation.staff,plan.work};
        const auto forecast=preview_training(*state.lab,request);
        if (!forecast.licensed || !forecast.prerequisites_met) {
            program.waiting="Candidate data or research is unavailable under these terms.";
            return false;
        }
        if (!within_budget(state,plan.budget,refresh_spent(program,test),test.cash,protected_cash)) {
            program.waiting="Protect the cash required for the independent evaluation before training.";
            return false;
        }
        if (auto blocked=allocation_block(*state.capacity,plan.budget.allocation); !blocked.empty()) {
            program.waiting=std::move(blocked);
            return false;
        }
        auto candidate=state;
        try {
            start_training(*candidate.lab,*candidate.capacity,state.date,request);
            auto& accepted=candidate.frontier->refresh_programs[index];
            accepted.runs.push_back({current,0,state.date,{},{},{}});
            accepted.waiting.clear();
        } catch (const std::invalid_argument& error) {
            program.waiting=error.what();
            return false;
        }
        state=std::move(candidate);
        return true;
    }

    const auto& run=program.runs.back();
    const auto id=frontier_refresh_id(program.id,program.runs.size());
    const auto& model=require_named(state.lab->models,id);
    const auto& training=require_named(state.capacity->projects,model.project_id);
    if (training.status==ProjectStatus::active) return false;
    if (training.status==ProjectStatus::cancelled) {
        stop_refresh(state,program,"Candidate training was cancelled.");
        return false;
    }
    if (!run.evaluated_on) {
        if (!prerequisites(state,test.requires_research) ||
            !within_budget(state,plan.budget,refresh_spent(program,test),test.cash,protected_cash)) {
            program.waiting="The independent evaluation needs its prerequisites and remaining authorized funds.";
            return false;
        }
        if (auto blocked=allocation_block(*state.capacity,plan.budget.allocation); !blocked.empty()) {
            program.waiting=std::move(blocked);
            return false;
        }
        auto candidate=state;
        try {
            start_evaluation(*candidate.lab,*candidate.accounts,*candidate.capacity,state.date,id,id,plan.evaluation,
                plan.budget.allocation,protected_cash);
            candidate.cash=accounts_report(*candidate.accounts).cash;
            auto& accepted=candidate.frontier->refresh_programs[index];
            accepted.runs.back().evaluated_on=state.date;
            accepted.waiting.clear();
        } catch (const std::invalid_argument& error) {
            program.waiting=error.what();
            return false;
        }
        state=std::move(candidate);
        return true;
    }
    const auto& evidence=require_named(state.lab->evaluations,id);
    const auto status=require_named(state.capacity->projects,evidence.project_id).status;
    if (status==ProjectStatus::active) return false;
    if (!evidence.completed || !improvement(state,incumbent,evidence,plan.improvement)) {
        stop_refresh(state,program,evidence.completed?"Independent evidence did not improve the existing service without a regression.":"Independent evaluation was cancelled.");
        return false;
    }
    if (!fresh_method(state,state.date)) {
        program.waiting="The method review expired before rollout. Complete a fresh review.";
        return false;
    }
    const DeployModel launch{plan.service,id,id,incumbent_work.source_id,incumbent.human_review,incumbent.region_id};
    if (auto blocked=civic_launch_block(state.civic?&*state.civic:nullptr,state,launch,state.date); !blocked.empty()) {
        stop_refresh(state,program,std::move(blocked));
        return false;
    }
    if (guard) if (auto blocked=guard(state,launch); !blocked.empty()) {
        stop_refresh(state,program,std::move(blocked));
        return false;
    }
    auto candidate=state;
    try {
        if (state.resilience && std::ranges::any_of(state.resilience->replicas,[&](const auto& replica){
            return replica.service_id==plan.service && !replica.stopped_on;
        })) invalid("A protected service replica needs an explicit change before replacement.");
        deploy_model(*candidate.deployments,*candidate.lab,*candidate.accounts,*candidate.capacity,*candidate.market,state.date,launch);
        auto& accepted=candidate.frontier->refresh_programs[index];
        auto& completed=accepted.runs.back();
        completed.rollout_deployment=candidate.deployments->records.size();
        completed.finished_on=state.date;
        completed.outcome="Independent evidence qualified the replacement within the existing service terms.";
        accepted.waiting.clear();
    } catch (const std::invalid_argument& error) {
        stop_refresh(state,program,error.what());
        return false;
    }
    state=std::move(candidate);
    return true;
}
}

bool frontier_research_active(const FrontierResearchProgram& program) {
    return !program.cancelled_on && program.decisions.size()<program.plan.budget.runs;
}
bool frontier_refresh_active(const FrontierRefreshProgram& program) {
    return !program.cancelled_on && (program.runs.size()<program.plan.budget.runs ||
        (!program.runs.empty() && !program.runs.back().finished_on));
}
std::string frontier_refresh_id(std::uint64_t program,std::size_t attempt) {
    return std::string(frontier_refresh_prefix)+std::to_string(program)+"."+std::to_string(attempt);
}
void authorize_frontier_research(SimulationState& state, FrontierResearchPlan plan) {
    if (!state.frontier) invalid("This campaign has no frontier research policies.");
    auto& programs=state.frontier->research_programs;
    if (programs.size()>=frontier_policy_limit ||
        static_cast<std::size_t>(std::ranges::count_if(programs,frontier_research_active))>=frontier_active_policy_limit)
        invalid("The bounded research program register is full.");
    check_research_plan(state,plan,state.date);
    programs.push_back({programs.size()+1,std::move(plan),state.date,{},{},{}});
}
void authorize_frontier_refresh(SimulationState& state, FrontierRefreshPlan plan) {
    if (!state.frontier) invalid("This campaign has no frontier refresh policies.");
    auto& programs=state.frontier->refresh_programs;
    if (programs.size()>=frontier_policy_limit ||
        static_cast<std::size_t>(std::ranges::count_if(programs,frontier_refresh_active))>=frontier_active_policy_limit ||
        std::ranges::any_of(programs,[&](const auto& program){return program.plan.service==plan.service && frontier_refresh_active(program);}))
        invalid("Use one bounded active refresh policy per service.");
    std::sort(plan.data.begin(),plan.data.end());
    check_refresh_plan(state,plan,state.date);
    programs.push_back({programs.size()+1,std::move(plan),state.date,{},{},{}});
}
void cancel_frontier_policy(SimulationState& state, const CancelFrontierPolicy& request) {
    if (!state.frontier || request.id==0) invalid("Choose an existing authorized policy.");
    if (request.refresh) {
        auto& programs=state.frontier->refresh_programs;
        if (request.id>programs.size() || !frontier_refresh_active(programs[request.id-1]))
            invalid("Choose an active refresh policy.");
        auto& program=programs[request.id-1];
        stop_refresh(state,program,"Refresh policy cancelled before rollout.");
        program.cancelled_on=state.date;
    } else {
        auto& programs=state.frontier->research_programs;
        if (request.id>programs.size() || !frontier_research_active(programs[request.id-1]))
            invalid("Choose an active research program.");
        programs[request.id-1].cancelled_on=state.date;
    }
}
bool review_frontier_policies(SimulationState& state,Cents protected_cash,const FrontierLaunchGuard& guard) {
    if (!state.frontier || state.phase!=DayPhase::work_ready || required_management_notice(state) ||
        required_governance_notice(state) || required_site_alert(state) ||
        (state.liquidity && liquidity_report(*state.liquidity).pending_warning) ||
        state.frontier->policies_reviewed_on==state.date ||
        (state.frontier->research_programs.empty() && state.frontier->refresh_programs.empty())) return false;
    state.frontier->policies_reviewed_on=state.date;
    bool changed=false;
    for (std::size_t i=0; i<state.frontier->research_programs.size(); ++i) changed=run_research(state,i,protected_cash)||changed;
    for (std::size_t i=0; i<state.frontier->refresh_programs.size(); ++i) changed=run_refresh(state,i,protected_cash,guard)||changed;
    if (changed && state.liquidity) {
        refresh_liquidity(*state.liquidity,*state.accounts);
        if (liquidity_report(*state.liquidity).pending_warning) state.paused=true;
    }
    return changed;
}

namespace frontier_detail {
void validate_policies(const SimulationState& state) {
    const auto& future=*state.frontier;
    if (future.research_programs.size()>frontier_policy_limit || future.refresh_programs.size()>frontier_policy_limit ||
        (future.policies_reviewed_on && (*future.policies_reviewed_on<state.lab->starts_on || *future.policies_reviewed_on>state.date)) ||
        static_cast<std::size_t>(std::ranges::count_if(future.research_programs,frontier_research_active))>frontier_active_policy_limit ||
        static_cast<std::size_t>(std::ranges::count_if(future.refresh_programs,frontier_refresh_active))>frontier_active_policy_limit)
        invalid("Saved frontier automation exceeds its authorized program bounds.");
    const auto in_campaign=[&](OrdinalDay day){return day>=state.lab->starts_on && day<=state.date;};
    const auto paid=[&](std::string_view key,OrdinalDay day,Cents amount) {
        return amount.value()==0 || std::ranges::any_of(state.accounts->postings,[&](const auto& row){
            return row.key==key && row.day==day && row.amount==amount && row.debit==Account::expenses && row.credit==Account::cash;
        });
    };
    const auto allocated=[&](const CapacityProject& project,const LabAllocation& allocation) {
        return project.source_id==allocation.source_id && project.requested==allocation.daily_compute && project.staff==allocation.staff;
    };
    std::set<std::string_view> research_projects;
    for (std::size_t i=0; i<future.research_programs.size(); ++i) {
        const auto& program=future.research_programs[i];
        check_research_plan(state,program.plan,program.authorized_on);
        if (program.id!=i+1 || !in_campaign(program.authorized_on) ||
            (program.cancelled_on && (!in_campaign(*program.cancelled_on) || *program.cancelled_on<program.authorized_on)) ||
            program.decisions.size()>program.plan.budget.runs || research_spent(program)>program.plan.budget.limit || program.waiting.size()>512)
            invalid("Saved research program has invalid dates, costs or queue size.");
        auto earliest=advance_days(program.authorized_on,Days{1});
        for (std::size_t j=0; j<program.decisions.size(); ++j) {
            const auto& decision=program.decisions[j];
            const auto& topic=require_named(state.lab->catalog.research,topic_for(program.plan,j));
            if (!in_campaign(decision.day) || decision.day<earliest ||
                (program.cancelled_on && decision.day>*program.cancelled_on))
                invalid("Research decision bypassed its authorization or cadence.");
            earliest=advance_days(decision.day,program.plan.budget.interval);
            if (decision.project.empty()) {
                if (decision.paid.value()!=0 || completed_experiments(state,topic.id,decision.day)<topic.repeat_limit)
                    invalid("A skipped research item has no completed underlying topic.");
            } else {
                const auto& project=require_named(state.capacity->projects,decision.project);
                if (project.starts_on!=decision.day || decision.paid!=topic.cash || !allocated(project,program.plan.budget.allocation) ||
                    !research_projects.insert(project.id).second || !paid(project.id,decision.day,decision.paid) ||
                    std::ranges::none_of(state.lab->research,[&](const auto& row){return row.research_id==topic.id && row.project_id==project.id;}))
                    invalid("Queued research differs from its real paid experiment.");
                if (j+1<program.decisions.size() && (!project.stopped_on ||
                    (project.status==ProjectStatus::completed ? *project.stopped_on>=program.decisions[j+1].day : *project.stopped_on>program.decisions[j+1].day)))
                    invalid("A queued research experiment began before the previous item settled.");
            }
        }
    }
    std::set<std::string> models,evaluations,active_services;
    for (std::size_t i=0; i<future.refresh_programs.size(); ++i) {
        const auto& program=future.refresh_programs[i];
        const auto& plan=program.plan;
        check_refresh_plan(state,plan,program.authorized_on);
        const auto& test=require_named(state.lab->catalog.evaluations,plan.evaluation);
        if (program.id!=i+1 || !in_campaign(program.authorized_on) ||
            (program.cancelled_on && (!in_campaign(*program.cancelled_on) || *program.cancelled_on<program.authorized_on)) ||
            (frontier_refresh_active(program) && !active_services.insert(plan.service).second) ||
            program.runs.size()>plan.budget.runs || refresh_spent(program,test)>plan.budget.limit || program.waiting.size()>512)
            invalid("Saved refresh policy has invalid dates, costs or rollout limits.");
        auto earliest=advance_days(program.authorized_on,Days{1});
        for (std::size_t j=0; j<program.runs.size(); ++j) {
            const auto& run=program.runs[j];
            const auto id=frontier_refresh_id(program.id,j+1);
            const auto& original=deployment(state,run.previous_deployment);
            const auto& model=require_named(state.lab->models,id);
            const auto& training=require_named(state.capacity->projects,model.project_id);
            if (!in_campaign(run.starts_on) || run.starts_on<earliest || original.service_id!=plan.service ||
                run.starts_on<advance_days(original.deployed_on,plan.budget.interval) ||
                !fresh_method(state,run.starts_on) || model.created_on!=run.starts_on || model.recipe_id!=plan.recipe ||
                !model.parent_id.empty() || model.data_ids!=plan.data || training.total_work!=plan.work ||
                !allocated(training,plan.budget.allocation) || (program.cancelled_on && run.starts_on>*program.cancelled_on))
                invalid("Refresh candidate differs from the authorized service, data and finite work.");
            models.insert(id);
            if (run.finished_on) {
                if (!in_campaign(*run.finished_on) || *run.finished_on<run.starts_on || !text_valid(run.outcome,512) ||
                    (program.cancelled_on && *run.finished_on>*program.cancelled_on))
                    invalid("A refresh result lost its actual finish boundary.");
                earliest=advance_days(*run.finished_on,plan.budget.interval);
            } else if (j+1!=program.runs.size() || program.cancelled_on || run.rollout_deployment || !run.outcome.empty())
                invalid("An unfinished refresh was replaced or rolled out.");
            if (run.evaluated_on) {
                const auto& evidence=require_named(state.lab->evaluations,id);
                const auto& evaluation=require_named(state.capacity->projects,evidence.project_id);
                if (!training.stopped_on || training.status!=ProjectStatus::completed || *training.stopped_on>=*run.evaluated_on ||
                    !in_campaign(*run.evaluated_on) || evaluation.starts_on!=*run.evaluated_on ||
                    evidence.model_id!=id || evidence.plan_id!=plan.evaluation || !allocated(evaluation,plan.budget.allocation) ||
                    !paid(evaluation.id,*run.evaluated_on,test.cash) || (run.finished_on && *run.evaluated_on>*run.finished_on) ||
                    (run.finished_on && evaluation.status==ProjectStatus::active))
                    invalid("Refresh lacks its actual paid independent evaluation.");
                evaluations.insert(id);
                if (run.rollout_deployment) {
                    const auto& rollout=deployment(state,run.rollout_deployment);
                    const auto& prior_work=require_named(state.capacity->projects,original.project_id);
                    const auto& new_work=require_named(state.capacity->projects,rollout.project_id);
                    if (!run.finished_on || !evaluation.stopped_on || *evaluation.stopped_on>=*run.finished_on ||
                        !fresh_method(state,*run.finished_on) || !evidence.completed || !improvement(state,original,evidence,plan.improvement) ||
                        rollout.service_id!=plan.service || rollout.model_id!=id || rollout.evidence_id!=id || rollout.deployed_on!=*run.finished_on ||
                        rollout.region_id!=original.region_id || rollout.human_review!=original.human_review ||
                        new_work.source_id!=prior_work.source_id || prior_work.stopped_on!=run.finished_on)
                        invalid("Automated rollout bypassed the qualified evidence or changed the service terms.");
                }
            } else if (run.rollout_deployment || (run.finished_on && training.status==ProjectStatus::active))
                invalid("A closed refresh retained active work or an untested rollout.");
        }
    }
    for (const auto& model : state.lab->models)
        if (model.id.starts_with(frontier_refresh_prefix) && !models.contains(model.id)) invalid("Unowned automated refresh candidate.");
    for (const auto& evidence : state.lab->evaluations)
        if (evidence.id.starts_with(frontier_refresh_prefix) && !evaluations.contains(evidence.id)) invalid("Unowned automated refresh evaluation.");
}
}
} // namespace pgp::core
