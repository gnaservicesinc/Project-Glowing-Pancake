#include "frontier_internal.hpp"

#include <set>
#include <span>

namespace pgp::core {
namespace frontier_detail {
const FrontierSystem& system(const FrontierState& state, std::uint64_t id) {
    if (id == 0 || id > state.systems.size() || state.systems[id-1].id != id)
        invalid("Choose an existing assessed system.");
    return state.systems[id-1];
}
const DeploymentRecord& deployment(const SimulationState& state, std::uint64_t id) {
    if (!state.deployments || id == 0 || id > state.deployments->records.size())
        invalid("A frontier system must retain each actual deployed model version.");
    return state.deployments->records[id-1];
}
bool serves_interval(const CapacityProject& project, OrdinalDay first, OrdinalDay last) {
    return project.kind == ProjectKind::serving && project.starts_on <= first &&
        (!project.stopped_on || *project.stopped_on > last);
}
std::string allocation_block(const CapacityState& capacity, const LabAllocation& allocation) {
    const auto* supply = named(capacity.sources,allocation.source_id);
    if (!supply) return "Choose an available compute source.";
    const auto report = capacity_report(capacity);
    if (report.assigned_staff + allocation.staff > report.available_staff)
        return "The request exceeds staff left after existing commitments.";
    auto available = company_capacity(*supply).value();
    for (const auto& project : capacity.projects) if (project.status==ProjectStatus::active && project.source_id==supply->id) {
        const auto reserved = project.kind==ProjectKind::serving || project.kind==ProjectKind::replication ? project.requested.value() :
            std::min(project.requested.value(),project.total_work.value()-project.completed_work.value());
        available = checked_subtract(available,reserved);
    }
    if (allocation.daily_compute.value()>available)
        return "The request exceeds compute left after serving and other commitments.";
    return {};
}
}

namespace {
using namespace frontier_detail;
struct QualificationProgress {
    FrontierQualification view;
    std::optional<OrdinalDay> first_panel, last_panel, first_replication, last_replication;
};
QualificationProgress progress(std::span<const FrontierTrial> trials, std::uint64_t id, std::optional<OrdinalDay> through = {}) {
    QualificationProgress result;
    auto& view = result.view;
    for (const auto& trial : trials) {
        if (trial.system != id || !trial.result || !trial.result->valid || (through && trial.result->observed_on > *through)) continue;
        const auto day = trial.result->observed_on;
        if (trial.kind == FrontierTrialKind::assessment) {
            if (!result.first_panel) {
                result.first_panel = day;
                view.conservative = trial.result->lower;
            } else for (std::size_t i=0; i<view.conservative.size(); ++i) {
                view.conservative[i] = std::min(view.conservative[i],trial.result->lower[i]);
            }
            result.last_panel = day;
            ++view.assessments;
        }
        if (trial.kind == FrontierTrialKind::replication) {
            if (!result.first_replication) result.first_replication = day;
            result.last_replication = day;
            ++view.improvements;
        }
    }
    return result;
}
bool panels_complete(const QualificationProgress& result) {
    return result.view.assessments == 3 && result.first_panel && result.last_panel &&
        *result.last_panel >= advance_days(*result.first_panel,frontier_assessment_span);
}
std::string trial_id(std::uint64_t id) { return "frontier.trial/"+std::to_string(id); }
bool stopped_by(const CapacityProject& work, OrdinalDay day) {
    if (!work.stopped_on) return false;
    return work.status == ProjectStatus::cancelled ? *work.stopped_on <= day : *work.stopped_on < day;
}
void check_bundle(const SimulationState& state, const FrontierSystem& bundle, OrdinalDay day, bool historic) {
    const auto& future = *state.frontier;
    if (day < future.catalog.branches_after || !text_valid(bundle.name) || bundle.deployments.empty() ||
        bundle.deployments.size() > frontier_bundle_limit || !std::is_sorted(bundle.deployments.begin(),bundle.deployments.end()) ||
        std::adjacent_find(bundle.deployments.begin(),bundle.deployments.end()) != bundle.deployments.end())
        invalid("Register a named deployed system with up to four distinct members in the future era.");
    std::set<std::string_view> services, models;
    bool advanced = false;
    for (const auto id : bundle.deployments) {
        const auto& release = deployment(state,id);
        const auto& model = require_named(state.lab->models,release.model_id);
        const auto& evidence = require_named(state.lab->evaluations,release.evidence_id);
        const auto& method = require_named(state.lab->catalog.evaluations,evidence.plan_id);
        const auto& work = require_named(state.capacity->projects,release.project_id);
        if (!services.insert(release.service_id).second || !models.insert(release.model_id).second || !evidence.completed ||
            !method.independent || release.deployed_on > day || !serves_interval(work,day,historic ? OrdinalDay{day.value()-1} : day))
            invalid("Each system member needs its own operating service and completed independent Lab evidence.");
        advanced = advanced || named(future.catalog.recipes,model.recipe_id);
    }
    if (!advanced) invalid("Use at least one model trained with a frontier recipe.");
}
bool research_known(const SimulationState& state, std::string_view id, OrdinalDay day) {
    return std::ranges::any_of(state.lab->research,[&](const auto& run) {
        if (run.research_id != id || !run.completed) return false;
        const auto& work = require_named(state.capacity->projects,run.project_id);
        return work.stopped_on && *work.stopped_on < day;
    });
}
struct TrialRequirements {
    const FrontierWorkTerms& terms;
    std::optional<OrdinalDay> earliest_observation{};
};
TrialRequirements check_trial(const SimulationState& state, const StartFrontierTrial& request,
    OrdinalDay day, std::span<const FrontierTrial> prior, bool historic) {
    if (!state.frontier) invalid("This scenario has no frontier research program.");
    const auto& future = *state.frontier;
    const auto& bundle = system(future,request.system);
    if (day < bundle.registered_on) invalid("Register the deployed system before commissioning a trial.");
    check_bundle(state,bundle,day,historic);
    (void)frontier_key(request.kind);
    const FrontierTrial* pilot = nullptr;
    for (const auto& trial : prior) if (trial.system == request.system) {
        const auto& work = require_named(state.capacity->projects,trial.project);
        if (!stopped_by(work,day)) invalid("Finish or cancel this system's current trial before starting another.");
        if (!trial.result || !trial.result->valid || trial.result->observed_on > day) continue;
        if (trial.kind==request.kind && trial.protocol==request.protocol)
            invalid("This system already has a valid result from that protocol.");
        if (trial.kind == FrontierTrialKind::experiment && trial.protocol == request.protocol) pilot = &trial;
    }
    const auto assessments=progress(prior,request.system,day);
    if (request.kind==FrontierTrialKind::assessment) {
        const auto& panel=require_named(future.catalog.panels,request.protocol);
        return {panel.terms,assessments.view.assessments==2
            ? std::optional{advance_days(*assessments.first_panel,frontier_assessment_span)} : std::nullopt};
    }
    if (!future.catalog.asi_route || !panels_complete(assessments) || !meets_asi(assessments.view.conservative))
        invalid("Independent panels must establish the six advanced domains and novel science before experimental verification.");
    const auto& protocol = require_named(future.catalog.experiments,request.protocol);
    if (!research_known(state,protocol.research,day)) invalid("Complete the experiment's underlying Lab research first.");
    if (request.kind==FrontierTrialKind::experiment) return {protocol.pilot};
    if (!pilot || pilot->result->gain < protocol.required_gain)
        invalid("Complete the matching prototype experiment and demonstrate its improvement first.");
    const auto& original = require_named(state.capacity->projects,pilot->project);
    const auto& original_supply = require_named(state.capacity->sources,original.source_id);
    const auto& verification_supply = require_named(state.capacity->sources,request.allocation.source_id);
    if (original_supply.physical_id == verification_supply.physical_id)
        invalid("Independent replication must use a separate physical compute source.");
    return {protocol.replication,assessments.view.improvements==2
        ? std::optional{advance_days(*assessments.first_replication,frontier_replication_span)} : std::nullopt};
}
FrontierTrialQuote trial_terms(const FrontierSystem& bundle, const TrialRequirements& requirements,
    const LabAllocation& allocation, OrdinalDay day, OrdinalDay end) {
    const auto& terms=requirements.terms;
    if (allocation.daily_compute.value() <= 0 || allocation.daily_compute.value() > 1'000'000'000 ||
        allocation.staff < terms.minimum_staff * bundle.deployments.size() || allocation.staff > 128)
        invalid("The trial needs positive compute and its disclosed staff per deployed member.");
    const auto fee = mul_div_floor(static_cast<std::uint64_t>(terms.fee.value()),bundle.deployments.size(),1).quotient;
    const auto fixed_work = mul_div_floor(static_cast<std::uint64_t>(terms.work.value()),bundle.deployments.size(),1).quotient;
    const auto work = std::max(MilliCU{fixed_work},integrate(allocation.daily_compute,terms.minimum_days));
    const auto rate = allocation.daily_compute.value();
    const auto duration = work.value()/rate + (work.value()%rate != 0 ? 1 : 0);
    const auto earliest = advance_days(day,Days{duration});
    if (requirements.earliest_observation && earliest<*requirements.earliest_observation)
        throw std::invalid_argument("The final independent window is not yet eligible. Its result must be on or after "+
            iso_date(*requirements.earliest_observation)+". Start later or choose a longer work allocation.");
    if (earliest > end) invalid("The independent trial cannot finish within this campaign.");
    return {Cents{fee},work,earliest};
}
std::uint64_t useful_work(const SimulationState& state, std::string_view service, OrdinalDay first, OrdinalDay last) {
    const auto& market = *state.market;
    auto day = std::ranges::lower_bound(market.days,first,{},&MarketDay::day);
    std::uint64_t tasks = 0;
    for (; day != market.days.end() && day->day <= last; ++day) {
        for (const auto& row : day->discretionary) if (row.product_id == service) tasks += row.tasks;
        for (const auto& row : day->contracts)
            if (row.delivered_tasks && require_named(market.contracts,row.contract_id).product_id == service) tasks += row.delivered_tasks;
    }
    return tasks;
}
FrontierScores bundle_scores(const SimulationState& state, const FrontierSystem& bundle) {
    const auto& future = *state.frontier;
    FrontierScores scores{};
    bool first = true;
    for (const auto id : bundle.deployments) {
        const auto& release = deployment(state,id);
        const auto model = model_scores(future.catalog,future_path(future),state.lab->catalog,require_named(state.lab->models,release.model_id));
        for (std::size_t i=0; i<model.size(); ++i) {
            // Specialized members can cover different tasks. Transfer,
            // reliability, workflow and control inherit the weakest member.
            if (i < frontier_domain_count || i == 9) scores[i] = std::max(scores[i],model[i]);
            else scores[i] = first ? model[i] : std::min(scores[i],model[i]);
        }
        first = false;
    }
    const auto loss = static_cast<std::uint32_t>(future.catalog.integration_loss.value()) *
        static_cast<std::uint32_t>(bundle.deployments.size()-1);
    for (auto& score : scores) score = score > loss ? score-loss : 0;
    return scores;
}
}

namespace frontier_detail {
FrontierResult observe_trial(const SimulationState& state, const FrontierTrial& trial, OrdinalDay last) {
    const auto& future = *state.frontier;
    const auto& bundle = system(future,trial.system);
    FrontierResult result;
    result.observed_on = advance_days(last,Days{1});
    result.valid = true;
    const auto needed = static_cast<std::uint64_t>(result.observed_on.value()-trial.starts_on.value()) *
        future.catalog.minimum_field_tasks_per_day;
    for (const auto id : bundle.deployments) {
        const auto& release = deployment(state,id);
        const auto& service = require_named(state.capacity->projects,release.project_id);
        if (!serves_interval(service,trial.starts_on,last)) result.valid = false;
        const auto tasks = useful_work(state,release.service_id,trial.starts_on,last);
        result.field_tasks += tasks;
        if (tasks < needed) result.valid = false;
    }
    if (!result.valid) return result;
    const auto score = bundle_scores(state,bundle);
    if (trial.kind == FrontierTrialKind::assessment) {
        const auto margin = static_cast<std::uint32_t>(require_named(future.catalog.panels,trial.protocol).margin.value());
        for (std::size_t i=0; i<score.size(); ++i) result.lower[i] = score[i] > margin ? score[i]-margin : 0;
    } else {
        const auto& terms = require_named(future.catalog.experiments,trial.protocol);
        const auto gain = mul_div_floor(static_cast<std::uint64_t>(terms.possible_gain.value()),score[9],10000).quotient;
        const auto margin = trial.kind == FrontierTrialKind::replication ? static_cast<std::uint64_t>(terms.margin.value()) : 0;
        result.gain = BasisPoints{gain > margin ? gain-margin : 0};
        result.valid = result.gain >= terms.required_gain;
    }
    return result;
}
}

FrontierQualification frontier_qualification(const FrontierState& state, std::uint64_t id) {
    (void)frontier_detail::system(state,id);
    auto result = progress(state.trials,id);
    if (panels_complete(result)) {
        if (frontier_detail::meets_agi(result.view.conservative)) result.view.agi_on = result.last_panel;
        if (state.catalog.asi_route && frontier_detail::meets_asi(result.view.conservative) &&
            result.view.improvements == 3 && result.first_replication && result.last_replication &&
            *result.last_replication >= advance_days(*result.first_replication,frontier_replication_span))
            result.view.asi_on = std::max(*result.last_panel,*result.last_replication);
    }
    return result.view;
}
std::optional<OrdinalDay> earliest_frontier_qualification(const FrontierState& state, bool asi) {
    std::optional<OrdinalDay> earliest;
    for (const auto& bundle : state.systems) {
        const auto evidence = frontier_qualification(state,bundle.id);
        const auto date = asi ? evidence.asi_on : evidence.agi_on;
        if (date && (!earliest || *date < *earliest)) earliest = date;
    }
    return earliest;
}

void register_frontier_system(SimulationState& state, const RegisterFrontierSystem& request) {
    using namespace frontier_detail;
    if (!state.frontier || !state.deployments || state.frontier->systems.size() >= frontier_system_limit ||
        request.services.empty() || request.services.size() > frontier_bundle_limit)
        invalid("Choose up to four deployed frontier services within the system register.");
    auto& future = *state.frontier;
    FrontierSystem bundle{future.systems.size()+1,request.name,state.date,{}};
    const auto& records = state.deployments->records;
    for (const auto& id : request.services) {
        const auto release = std::find_if(records.rbegin(),records.rend(),[&](const auto& row){return row.service_id==id;});
        if (release == records.rend()) invalid("Register services that have actually been deployed.");
        bundle.deployments.push_back(static_cast<std::uint64_t>(records.rend()-release));
    }
    std::sort(bundle.deployments.begin(),bundle.deployments.end());
    check_bundle(state,bundle,state.date,false);
    if (std::ranges::any_of(future.systems,[&](const auto& row){return row.deployments==bundle.deployments;}))
        invalid("This exact deployed system is already registered.");
    future.systems.push_back(std::move(bundle));
}
FrontierTrialQuote quote_frontier_trial(const SimulationState& state, const StartFrontierTrial& request, Cents protected_cash) {
    using namespace frontier_detail;
    if (!state.frontier || !state.accounts || !state.capacity || state.frontier->trials.size() >= frontier_trial_limit)
        invalid("No frontier trial is available within this scenario's register.");
    const auto& future = *state.frontier;
    const auto& terms = check_trial(state,request,state.date,future.trials,false);
    const auto quote = trial_terms(system(future,request.system),terms,request.allocation,state.date,state.end_date);
    if (quote.fee > checked_unit_subtract(state.cash,protected_cash))
        invalid("The paid review exceeds unreserved cash.");
    if (auto blocked=allocation_block(*state.capacity,request.allocation); !blocked.empty())
        throw std::invalid_argument(std::move(blocked));
    return quote;
}
void start_frontier_trial(SimulationState& state, const StartFrontierTrial& request, Cents protected_cash) {
    using namespace frontier_detail;
    const auto quote = quote_frontier_trial(state,request,protected_cash);
    auto& future = *state.frontier;
    const auto id = future.trials.size()+1;
    const auto& supply = require_named(state.capacity->sources,request.allocation.source_id);
    CapacityProject work;
    work.id = trial_id(id);
    work.source_id = supply.id;
    work.interconnect = supply.interconnect;
    work.kind = request.kind == FrontierTrialKind::experiment ? ProjectKind::research : ProjectKind::evaluation;
    work.starts_on = state.date;
    work.requested = request.allocation.daily_compute;
    work.staff = request.allocation.staff;
    work.total_work = quote.work;
    work.cause = "Independent frontier work: " + request.protocol;
    reserve_project(*state.capacity,state.date,work);
    record_cash(*state.accounts,state.date,Cents{-quote.fee.value()},Account::expenses,work.cause,work.id);
    state.cash = accounts_report(*state.accounts).cash;
    future.trials.push_back({id,request.system,request.kind,request.protocol,work.id,state.date,{}});
}
void prepare_frontier(SimulationState& state) {
    if (!state.frontier) return;
    for (auto& trial : state.frontier->trials) if (!trial.result) {
        const auto& work = frontier_detail::require_named(state.capacity->projects,trial.project);
        if (work.status == ProjectStatus::completed && work.stopped_on && *work.stopped_on < state.date)
            trial.result = frontier_detail::observe_trial(state,trial,*work.stopped_on);
    }
}

void validate_frontier(const SimulationState& state, bool deep) {
    using namespace frontier_detail;
    if (!state.frontier) {
        if (deep && state.capacity && std::ranges::any_of(state.capacity->projects,[](const auto& p){return p.id.starts_with("frontier.trial/");}))
            invalid("Frontier work needs its owning scenario program.");
        return;
    }
    const auto& future = *state.frontier;
    if (!state.lab || !state.capacity || !state.deployments || !state.market || !state.accounts ||
        future.systems.size() > frontier_system_limit || future.trials.size() > frontier_trial_limit)
        invalid("Frontier systems require shared market, Lab and capacity records.");
    if (!deep) return;
    const auto expected = opening_frontier(future.catalog,future.seed);
    if (expected.path != future.path || expected.conditions != future.conditions)
        invalid("Future conditions differ from the scenario's seeded path.");
    const auto gated = [&](FrontierTechnology kind, std::string_view id, OrdinalDay date) {
        if (!frontier_technology_block(state,kind,id,date).empty()) invalid("A saved technology commitment bypassed its conditions gate.");
    };
    for (const auto& license : state.lab->licenses) gated(FrontierTechnology::data,license.offer_id,license.acquired_on);
    for (const auto& run : state.lab->research)
        gated(FrontierTechnology::research,run.research_id,require_named(state.capacity->projects,run.project_id).starts_on);
    for (const auto& model : state.lab->models) gated(FrontierTechnology::recipe,model.recipe_id,model.created_on);
    if (state.infrastructure) for (const auto& order : state.infrastructure->hardware)
        gated(FrontierTechnology::hardware,order.family_id,order.ordered_on);
    std::set<std::vector<std::uint64_t>> systems;
    for (std::size_t i=0; i<future.systems.size(); ++i) {
        const auto& bundle = future.systems[i];
        if (bundle.id != i+1 || bundle.registered_on > state.date || !systems.insert(bundle.deployments).second ||
            (i && bundle.registered_on < future.systems[i-1].registered_on))
            invalid("A registered frontier system repeats or has an invalid date.");
        check_bundle(state,bundle,bundle.registered_on,true);
    }
    std::set<std::string_view> projects;
    for (std::size_t i=0; i<future.trials.size(); ++i) {
        const auto& trial = future.trials[i];
        const auto& work = require_named(state.capacity->projects,trial.project);
        if (trial.id != i+1 || trial.project != trial_id(trial.id) || trial.starts_on != work.starts_on ||
            trial.starts_on > state.date || (i && trial.starts_on < future.trials[i-1].starts_on) ||
            work.kind != (trial.kind == FrontierTrialKind::experiment ? ProjectKind::research : ProjectKind::evaluation) ||
            !projects.insert(work.id).second)
            invalid("Frontier trial identity or work does not match its owning protocol.");
        const StartFrontierTrial request{trial.system,trial.kind,trial.protocol,{work.source_id,work.requested,work.staff}};
        const auto& terms = check_trial(state,request,trial.starts_on,std::span{future.trials}.first(i),true);
        const auto quote = trial_terms(system(future,trial.system),terms,request.allocation,trial.starts_on,state.end_date);
        if (work.total_work != quote.work || !std::ranges::any_of(state.accounts->postings,[&](const auto& row) {
            return row.key == work.id && row.day == work.starts_on && row.amount == quote.fee &&
                row.debit == Account::expenses && row.credit == Account::cash;
        })) invalid("Independent work has no exact paid capacity allocation.");
        std::optional<FrontierResult> expected_result;
        if (work.status == ProjectStatus::completed && work.stopped_on) {
            expected_result = observe_trial(state,trial,*work.stopped_on);
            if (expected_result->observed_on < quote.earliest_result || expected_result->observed_on > state.date)
                invalid("A frontier result bypassed its minimum independent window.");
        }
        if (trial.result != expected_result) invalid("Frontier evidence differs from paid tests and actual deployed work.");
    }
    for (const auto& work : state.capacity->projects)
        if (work.id.starts_with("frontier.trial/") && !projects.contains(work.id))
            invalid("A frontier project has no owning trial.");
    validate_policies(state);
}
} // namespace pgp::core
