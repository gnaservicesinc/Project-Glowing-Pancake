#include <pgp/session/slice_run.hpp>
#include <pgp/runtime/scenario.hpp>
#include <algorithm>
#include <stdexcept>

namespace pgp::session {
namespace {
using namespace core;
void apply(runtime::Runtime& engine,CommandAction action) {
    const auto result=engine.submit({engine.current_state().world.simulation.revision,std::move(action)});
    if (!result.accepted()) throw std::runtime_error(result.explanation);
}
template<class Rows> const auto* find(const Rows& rows,std::string_view id) {
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
    return found==rows.end()?nullptr:&*found;
}
bool owns_data(const runtime::Runtime& engine,std::string_view id) {
    const auto& licenses=engine.current_state().world.simulation.lab->licenses;
    return std::any_of(licenses.begin(),licenses.end(),[&](const auto& row){return row.offer_id==id;});
}
void choices(runtime::Runtime& engine) {
    if (engine.current_state().world.simulation.management) {
        const auto notices=engine.current_state().world.simulation.management->notices;
        for (const auto& n:notices) if (n.required && !n.acknowledged) apply(engine,AcknowledgeManagementNotice{n.id});
    }
    for (unsigned count=0;count<32;++count) {
        const auto& state=engine.current_state();
        const auto waiting=std::find_if(state.runtime.presentations.begin(),state.runtime.presentations.end(),[](const auto& row) {
            return row.kind==runtime::PresentationKind::dialogue && !row.resolved;
        });
        if (waiting==state.runtime.presentations.end()) return;
        const auto id=waiting->id;
        auto result=engine.choose(state.world.simulation.revision,id,"accept");
        if (!result.accepted()) result=engine.choose(engine.current_state().world.simulation.revision,id,"decline");
        if (!result.accepted()) throw std::runtime_error(result.explanation);
    }
    throw std::runtime_error("Baseline strategy exceeded its per-morning decision bound");
}
bool frozen_model(const runtime::Runtime& engine,std::string_view id) {
    const auto& state=engine.current_state().world.simulation;
    const auto* model=find(state.lab->models,id);
    const auto* work=model?find(state.capacity->projects,model->project_id):nullptr;
    return work && work->status==ProjectStatus::completed;
}
void deploy(runtime::Runtime& engine,const content::ScenarioDefinition&,std::string_view product_id,
            std::string_view model_id,std::string_view evidence_id,bool) {
    const auto& state=engine.current_state().world.simulation;
    if (find(state.market->products,product_id)) return;
    const auto* evidence=find(state.lab->evaluations,evidence_id);
    if (!evidence || !evidence->completed) return;
    const auto definition=std::find_if(state.deployments->catalog.begin(),state.deployments->catalog.end(),[&](const auto& row){return row.product.id==product_id;});
    const auto* model=find(state.lab->models,model_id);
    const auto* recipe=model?find(state.lab->catalog.recipes,model->recipe_id):nullptr;
    const auto* supply=find(state.capacity->sources,"lease.initial");
    if (!recipe || !supply || definition==state.deployments->catalog.end()) throw std::runtime_error("Baseline deployment lacks authored resources");
    auto reserved=INT64_C(0);
    for (const auto& project:state.capacity->projects) if (project.source_id==supply->id && project.status==ProjectStatus::active) reserved=checked_add(reserved,project.requested.value());
    // A warned supplier setback can postpone expansion. It never grants extra
    // capacity or erases an existing serving commitment.
    if (required_inference(definition->serving.tasks_per_day,recipe->inference_per_task).value()>supply->effective.value()-reserved) return;
    apply(engine,DeployModel{std::string{product_id},std::string{model_id},std::string{evidence_id},"lease.initial",false});
}
void policy(runtime::Runtime& engine,const content::ScenarioDefinition& scenario) {
    const auto& start=engine.current_state().world.simulation;
    if (!start.lab || !start.market || !scenario.service_catalog) throw std::runtime_error("Baseline strategy requires the first-playable lab and service catalogs");
    if (scenario.team_people<3 || scenario.leased_capacity.value()<5) throw std::runtime_error("Baseline strategy requires its authored opening team and compute lease");
    const auto rate=MilliCUPerDay{scenario.leased_capacity.value()*3/5};
    const auto testing=LabAllocation{"lease.initial",MilliCUPerDay{scenario.leased_capacity.value()/5},1};
    const auto* recipe=find(start.lab->catalog.recipes,"recipe.document_baseline");
    if (!recipe) throw std::runtime_error("Baseline training recipe is missing");
    const auto recipe_id=recipe->id;
    const auto budget=checked_unit_add(recipe->compute_scale,recipe->compute_scale);
    if (!owns_data(engine,"data.licensed_documents")) apply(engine,BuyData{"data.licensed_documents"});
    if (engine.current_state().world.simulation.lab->research.empty())
        apply(engine,StartResearch{"research.held_out_evaluation",{"lease.initial",testing.daily_compute,2}});
    if (!find(engine.current_state().world.simulation.lab->models,"baseline"))
        apply(engine,StartTraining{{"baseline",recipe_id,"",{"data.licensed_documents"},"lease.initial",rate,scenario.team_people-2,budget}});
    if (frozen_model(engine,"baseline") && !find(engine.current_state().world.simulation.lab->evaluations,"baseline_test"))
        apply(engine,StartEvaluation{"baseline_test","baseline","evaluation.held_out",testing});
    deploy(engine,scenario,"product.documents","baseline","baseline_test",false);
    if (find(engine.current_state().world.simulation.market->products,"product.documents")) {
        const auto& accepted=engine.current_state().world.simulation.business_offers->accepted;
        if (std::none_of(accepted.begin(),accepted.end(),[](const auto& row){return row.offer_id=="offer.enterprise_service";}))
            apply(engine,AcceptBusinessOffer{"offer.enterprise_service"});
        if (!owns_data(engine,"data.consented_scans")) apply(engine,BuyData{"data.consented_scans"});
        if (!find(engine.current_state().world.simulation.lab->models,"targeted"))
            apply(engine,StartTraining{{"targeted",recipe_id,"",{"data.licensed_documents","data.consented_scans"},"lease.initial",rate,scenario.team_people-2,budget}});
    }
    if (frozen_model(engine,"targeted") && !find(engine.current_state().world.simulation.lab->evaluations,"targeted_test"))
        apply(engine,StartEvaluation{"targeted_test","targeted","evaluation.held_out",testing});
    deploy(engine,scenario,"product.documents_tables","targeted","targeted_test",true);
}
void specialist_policy(runtime::Runtime& engine,const content::ScenarioDefinition& scenario) {
    const auto& opening=engine.current_state().world.simulation;
    if (!opening.management) throw std::runtime_error("Specialist strategy needs the management catalog");
    if (opening.management->specialists.empty()) apply(engine,HireSpecialist{"specialist.evaluation"});
    if (!owns_data(engine,"data.licensed_documents")) apply(engine,BuyData{"data.licensed_documents"});
    const auto completed=[&](std::string_view id) {
        const auto& research=engine.current_state().world.simulation.lab->research;
        return std::any_of(research.begin(),research.end(),[&](const auto& run){return run.research_id==id && run.completed;});
    };
    const auto started=[&](std::string_view id) {
        const auto& research=engine.current_state().world.simulation.lab->research;
        return std::any_of(research.begin(),research.end(),[&](const auto& run){return run.research_id==id;});
    };
    for (const auto* id:{"research.reproducible_tooling","research.held_out_evaluation"})
        if (!started(id)) apply(engine,StartResearch{id,{"lease.initial",MilliCUPerDay{20000},2}});
    if (completed("research.reproducible_tooling") && !started("research.efficient_serving"))
        apply(engine,StartResearch{"research.efficient_serving",{"lease.initial",MilliCUPerDay{20000},2}});
    if (completed("research.efficient_serving") && !find(engine.current_state().world.simulation.lab->models,"specialist")) {
        const auto* recipe=find(engine.current_state().world.simulation.lab->catalog.recipes,"recipe.efficient");
        if (!recipe) throw std::runtime_error("Efficient specialist recipe is missing");
        apply(engine,StartTraining{{"specialist",recipe->id,"",{"data.licensed_documents"},"lease.initial",MilliCUPerDay{60000},4,checked_unit_add(recipe->compute_scale,recipe->compute_scale)}});
        DelegationPolicy policy;policy.id="specialist_checks";policy.target_id="specialist";policy.plan_id="evaluation.held_out";
        policy.allocation={"lease.initial",MilliCUPerDay{20000},1};policy.budget=Cents{400000};policy.reserve_floor=Cents{10000000};policy.maximum_runs=2;policy.interval=Days{180};
        apply(engine,AddDelegationPolicy{policy});
    }
    if (frozen_model(engine,"specialist")) deploy(engine,scenario,"product.document_specialist","specialist","delegated.specialist_checks.1",false);
}
void competition_policy(runtime::Runtime& engine,const content::ScenarioDefinition& scenario) {
    // This engineering route buys the same disclosed financing and protected
    // customer contract exposed in the Office; the intro specialist is unchanged.
    const auto& funding=engine.current_state().world.simulation.management->financing;
    if (std::none_of(funding.begin(),funding.end(),[](const auto& row){return row.offer_id=="finance.bridge_equity";}))
        apply(engine,TakeFinancing{"finance.bridge_equity"});
    specialist_policy(engine,scenario);
    if (frozen_model(engine,"specialist")) deploy(engine,scenario,"product.documents","specialist","delegated.specialist_checks.1",false);
    const auto& state=engine.current_state().world.simulation;
    if (find(state.market->products,"product.documents") &&
        std::none_of(state.business_offers->accepted.begin(),state.business_offers->accepted.end(),[](const auto& row){return row.offer_id=="offer.enterprise_service";}))
        apply(engine,AcceptBusinessOffer{"offer.enterprise_service"});
}
}
SliceRun run_slice(const runtime::CompiledProgram& program,std::optional<core::OrdinalDay> stop_before,
                  std::optional<runtime::Checkpoint> resume,bool specialist,std::string_view scenario_id="scenario.first_playable") {
    const auto* definition=program.collection.find(scenario_id);
    const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
    if (!scenario) throw std::invalid_argument("First-playable scenario is missing");
    auto initial=resume?std::move(*resume):runtime::begin_scenario(program,definition->id,"US-CA");
    if (!initial.world.scenario_id.empty() && initial.world.scenario_id!=scenario_id) throw std::invalid_argument("Resume the same authored scenario as the saved company");
    if (!resume && initial.world.enabled_flows) {
        auto& roots=*initial.world.enabled_flows;
        roots.erase(std::remove_if(roots.begin(),roots.end(),[&](const auto& id) {
            const auto& flow=std::get<content::FlowDefinition>(program.collection.find(id)->data);
            return std::find(flow.tags.begin(),flow.tags.end(),"story")!=flow.tags.end();
        }),roots.end());
    }
    runtime::Runtime engine{program,std::move(initial)};
    for (unsigned iterations=0;iterations<=1096;++iterations) {
        const auto& simulation=engine.current_state().world.simulation;
        if (simulation.phase==DayPhase::finished) return {SliceStop::finished,engine.checkpoint()};
        if (simulation.phase==DayPhase::closed) return {SliceStop::closed,engine.checkpoint()};
        if (stop_before && simulation.date>=*stop_before) return {SliceStop::requested_boundary,engine.checkpoint()};
        if (simulation.liquidity && liquidity_report(*simulation.liquidity).pending_warning) return {SliceStop::liquidity_warning,engine.checkpoint()};
        const auto ready=engine.prepare_opening(simulation.revision);
        if (!ready.accepted()) throw std::runtime_error(ready.explanation);
        choices(engine);
        if (scenario->competition) competition_policy(engine,*scenario);
        else if (specialist) specialist_policy(engine,*scenario);else policy(engine,*scenario);
        apply(engine,AdvanceDay{});
    }
    throw std::runtime_error("Baseline scenario exceeded its authored three-year day bound");
}
SliceRun run_slice_baseline(const runtime::CompiledProgram& program,std::optional<core::OrdinalDay> stop_before,std::optional<runtime::Checkpoint> resume) {
    return run_slice(program,stop_before,std::move(resume),false);
}
SliceRun run_slice_specialist(const runtime::CompiledProgram& program,std::optional<core::OrdinalDay> stop_before,std::optional<runtime::Checkpoint> resume) {
    return run_slice(program,stop_before,std::move(resume),true);
}
SliceRun run_competition_specialist(const runtime::CompiledProgram& program,std::string_view id,std::optional<core::OrdinalDay> stop_before,std::optional<runtime::Checkpoint> resume) {
    const auto* definition=program.collection.find(id);const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
    if (!scenario || !scenario->competition) throw std::invalid_argument("Choose an authored weekly-planning competition scenario");
    return run_slice(program,stop_before,std::move(resume),true,id);
}

}
