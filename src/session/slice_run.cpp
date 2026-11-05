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
void deploy(runtime::Runtime& engine,const content::ScenarioDefinition& scenario,std::string_view product_id,
            std::string_view model_id,std::string_view evidence_id,bool tables) {
    const auto& state=engine.current_state().world.simulation;
    if (find(state.market->products,product_id)) return;
    const auto* evidence=find(state.lab->evaluations,evidence_id);
    if (!evidence || !evidence->completed) return;
    const auto* plan=find(state.lab->catalog.evaluations,evidence->plan_id);
    const auto* model=find(state.lab->models,model_id);
    if (!model || evidence->model_id!=model_id || !plan || !plan->independent ||
        plan->coverage[0].value()==0 || evidence->lower[0]<plan->required[0] ||
        (tables && (plan->coverage[1].value()==0 || evidence->lower[1]<plan->required[1])))
        throw std::runtime_error("Baseline deployment lacks the promised independent subgroup evidence");
    const auto template_row=std::find_if(scenario.service_catalog->offers.begin(),scenario.service_catalog->offers.end(),[&](const auto& row){return row.product.id==product_id;});
    if (template_row==scenario.service_catalog->offers.end()) throw std::runtime_error("Baseline service template is missing");
    auto serving=template_row->serving;
    serving.starts_on=state.date;
    const auto* recipe=find(state.lab->catalog.recipes,model->recipe_id);
    if (!recipe) throw std::runtime_error("Deployed model has no recipe");
    serving.inference_milli_CU_per_task=recipe->inference_per_task;
    serving.requested=MilliCUPerDay{required_inference(serving.tasks_per_day,recipe->inference_per_task).value()};
    if (!find(state.capacity->projects,serving.id)) apply(engine,ReserveProject{serving});
    apply(engine,AddMarketProduct{template_row->product});
    const auto* customer=find(scenario.service_catalog->segments,"segment.enterprise");
    const auto preference=customer?std::find_if(customer->preferences.begin(),customer->preferences.end(),[&](const auto& row){return row.product_id==product_id;}):
        std::vector<MarketPreference>::const_iterator{};
    if (!customer || preference==customer->preferences.end()) throw std::runtime_error("Baseline service has no authored customer preference");
    apply(engine,AddMarketPreference{customer->id,*preference});
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
}
SliceRun run_slice_baseline(const runtime::CompiledProgram& program,std::optional<core::OrdinalDay> stop_before,
                            std::optional<runtime::Checkpoint> resume) {
    const auto* definition=program.collection.find("scenario.first_playable");
    const auto* scenario=definition?std::get_if<content::ScenarioDefinition>(&definition->data):nullptr;
    if (!scenario) throw std::invalid_argument("First-playable scenario is missing");
    auto initial=resume?std::move(*resume):runtime::begin_scenario(program,definition->id,"US-CA");
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
        choices(engine); policy(engine,*scenario); apply(engine,AdvanceDay{});
    }
    throw std::runtime_error("Baseline scenario exceeded its authored three-year day bound");
}
}
