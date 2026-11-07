#include <pgp/core/deployment.hpp>
#include <pgp/core/simulation.hpp>
#include "deployment_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
template<class Rows> const auto& find(const Rows& rows,std::string_view id) {
    const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.id==id;});
    if (row==rows.end()) invalid("Deployment refers to a missing model, evidence, recipe or capacity record");
    return *row;
}
}
namespace deployment_detail {
const ServiceDefinition& service(const DeploymentState& state,std::string_view id) {
    const auto row=std::find_if(state.catalog.begin(),state.catalog.end(),[&](const auto& value){return value.product.id==id;});
    if (row==state.catalog.end()) invalid("Choose an offered service scope");
    return *row;
}
DeployModel with_agent_controls(const ServiceDefinition& definition,DeployModel request) {
    if (definition.product.channel==ProductChannel::agent) {
        if (!request.agent_controls) request.agent_controls=AgentControls{};
    } else if (request.agent_controls) invalid("Tool permissions and agent oversight apply to agent services");
    return request;
}
std::optional<AgentPlan> agent_plan(const ServiceDefinition& definition,const LabState& lab,const DeployModel& request) {
    if (!request.agent_controls) return {};
    const auto& model=find(lab.models,request.model_id);
    auto fit=INT64_C(10000);
    for (std::size_t i=0;i<lab_domain_count;++i)
        if (definition.required[i].value()>0) fit=std::min(fit,model.capability[i].value());
    return AgentPlan{*request.agent_controls,BasisPoints{10000-fit},agent_remedy_rate(*request.agent_controls,definition.product.price_per_task)};
}
CapacityProject qualified_project(const ServiceDefinition& definition,const LabState& lab,const CapacityState& capacity,
                                 const DeployModel& request,OrdinalDay day,std::string id) {
    const auto& model=find(lab.models,request.model_id);
    if (!definition.required_recipe.empty() && model.recipe_id!=definition.required_recipe) invalid("This specialist service requires its efficient recipe and research");
    for (const auto& required : definition.required_research) {
        const auto research = std::find_if(lab.research.begin(), lab.research.end(), [&](const auto& row) {
            if (row.research_id != required || !row.completed) return false;
            const auto& work = find(capacity.projects, row.project_id);
            return work.stopped_on && *work.stopped_on < day;
        });
        if (research == lab.research.end()) invalid("Complete the distribution channel's required research before launch");
    }
    const auto& evidence=find(lab.evaluations,request.evidence_id);
    const auto& plan=find(lab.catalog.evaluations,evidence.plan_id);
    const auto& test=find(capacity.projects,evidence.project_id);
    if (model.created_on>day) invalid("Model did not exist at this deployment boundary");
    if (!model.project_id.empty()) {
        const auto& training=find(capacity.projects,model.project_id);
        if (training.status==ProjectStatus::active || !training.stopped_on || *training.stopped_on>day)
            invalid("Stop training or wait for completion before deploying this checkpoint");
    }
    if (!evidence.completed || evidence.model_id!=model.id || evidence.model_work!=model.consumed_work ||
        !plan.independent || test.status!=ProjectStatus::completed || !test.stopped_on || *test.stopped_on>day)
        invalid("Choose a completed independent evaluation of this exact stopped checkpoint");
    const LabModel* ancestor=&model;
    for (std::size_t visited=0;;++visited) {
        if (visited>=lab_record_limit) invalid("Model lineage exceeds its bound");
        for (const auto& data_id:ancestor->data_ids) {
            const auto& rights = find(lab.catalog.data,data_id);
            if (!rights.commercial_use)
                invalid("This model includes research-only data; train a new commercially cleared model");
            if (definition.product.channel == ProductChannel::open_weights && !rights.open_weight_release)
                invalid("Open weight release requires that right across the model's data lineage");
        }
        if (ancestor->parent_id.empty()) break;
        ancestor=&find(lab.models,ancestor->parent_id);
    }
    if (request.human_review && (definition.review_staff==0 || definition.review_tasks_per_day==0))
        invalid("Human review is not offered for this service scope");
    if (definition.product.channel == ProductChannel::agent && !request.human_review)
        invalid("Agent work requires the reviewed service scope");
    if (request.agent_controls && definition.product.channel!=ProductChannel::agent)
        invalid("Tool permissions and agent oversight apply to agent services");
    for (std::size_t i=0;i<lab_domain_count;++i) {
        if (definition.required[i].value()==0) continue;
        const auto credit=request.human_review?definition.review_credit[i].value():0;
        if (plan.coverage[i].value()==0 || evidence.lower[i].value()+credit<definition.required[i].value())
            invalid("The measured lower bound does not qualify this task promise; retrain with targeted data, use an offered review control, or narrow the scope");
    }
    auto project=definition.serving;
    const auto& recipe=find(lab.catalog.recipes,model.recipe_id);
    project.id=std::move(id); project.source_id=request.source_id; project.starts_on=day;
    project.tasks_per_day=request.human_review?definition.review_tasks_per_day:project.tasks_per_day;
    if (request.human_review) {
        project.staff+=definition.review_staff;
        project.monthly_support_cost=checked_unit_add(project.monthly_support_cost,definition.review_monthly_cost);
    }
    if (request.agent_controls) project.tasks_per_day=agent_review_capacity(project.tasks_per_day,*request.agent_controls);
    project.inference_milli_CU_per_task=recipe.inference_per_task;
    project.customer_inference=customer_hosted(definition.product.channel);
    if (project.customer_inference) {
        if (!request.source_id.empty()) invalid("Choose customer hardware for this distribution channel");
        project.requested = MilliCUPerDay{0};
    } else {
        project.requested = MilliCUPerDay{required_inference(project.tasks_per_day, recipe.inference_per_task).value()};
    }
    project.minimum_memory_mib=std::max(project.minimum_memory_mib,recipe.minimum_memory_mib);
    project.interconnect=recipe.interconnect;
    return project;
}
void require_publication_audience(const ServiceDefinition& definition,const MarketState& market,std::string_view region,
                                  std::string_view audience,const CapacityProject& work) {
    auto product=definition.product;
    product.region_id=region;
    if (!audience.empty() && product.channel!=ProductChannel::open_weights)
        invalid("Only an open-weight release can name a weight audience");
    const auto can_serve=[&](const ServicePreference& pref) {
        if (!audience.empty() && pref.segment_id!=audience) return false;
        const auto& segment=find(market.segments,pref.segment_id);
        return pref.preference.eligible && market_access(product,segment) &&
            (!work.customer_inference || (work.minimum_memory_mib<=segment.customer_memory_mib &&
                work.interconnect==segment.customer_interconnect && work.inference_milli_CU_per_task.value()<=segment.customer_compute.value()));
    };
    if ((product.channel!=ProductChannel::legacy || !region.empty()) && std::ranges::none_of(definition.preferences,can_serve))
        invalid("The release has no eligible buyers in its audience and territory with compatible hardware");
}
}
using namespace deployment_detail;
void validate_service_catalog(const std::vector<ServiceDefinition>& catalog) {
    if (catalog.empty() || catalog.size()>market_product_limit) invalid("Service catalog must be bounded and nonempty");
    std::set<std::string> ids;
    for (const auto& row:catalog) {
        if (row.product.id.empty() || !ids.insert(row.product.id).second || row.product.capacity_project_id!=row.serving.id ||
            row.serving.kind!=ProjectKind::serving || row.serving.status!=ProjectStatus::active || row.serving.stopped_on ||
            row.serving.completed_work.value()!=0 || row.serving.useful_tasks!=0 || row.serving.tasks_per_day==0 || row.serving.staff==0 ||
            row.review_staff>1'000'000 || row.review_monthly_cost.value()<0 || row.review_tasks_per_day>row.serving.tasks_per_day ||
            (row.review_staff==0)!=(row.review_tasks_per_day==0) || row.preferences.empty() || row.preferences.size()>market_segment_limit ||
            row.serving.customer_inference || !row.product.region_id.empty() || row.product.retirement || row.required_research.size()>16 ||
            (row.product.channel == ProductChannel::open_weights && row.product.price_per_task.value()!=0) ||
            (row.product.channel == ProductChannel::agent && row.review_staff==0) ||
            std::none_of(row.required.begin(),row.required.end(),[](auto value){return value.value()>0;}))
            invalid("Service catalog contains invalid scope, review or serving terms");
        (void)channel_key(row.product.channel);
        std::set<std::string> prerequisites;
        for (const auto& research : row.required_research) {
            if (research.empty() || !prerequisites.insert(research).second)
                invalid("Service research prerequisites must be distinct");
        }
        std::set<std::string> segments;
        for (const auto& pref:row.preferences)
            if (pref.preference.product_id!=row.product.id || !segments.insert(pref.segment_id).second)
                invalid("Service preferences must name their offered product and unique customer segments");
    }
}
namespace {
CapacityProject preview_release(const SimulationState& state,const DeployModel& selection,ProductChannel channel) {
    if (!state.deployments || !state.lab || !state.capacity || !state.market)
        invalid("This company has no qualified release catalog");
    const auto& deployments=*state.deployments;
    const auto& definition=service(deployments,selection.service_id);
    if (definition.product.channel!=channel) invalid("Choose a service in the reviewed channel");
    const auto request=with_agent_controls(definition,selection);
    if (deployments.records.size()>=lab_record_limit || pending_rollout(deployments,request.service_id))
        invalid("Resolve the pending trial and check the release history before publishing");
    const auto product=std::ranges::find(state.market->products,request.service_id,&MarketProduct::id);
    const auto region=product==state.market->products.end()?request.region_id:product->region_id;
    if (product!=state.market->products.end() && ((product->retirement && product->retirement->retires_on<=state.date) ||
        (!request.region_id.empty() && request.region_id!=region))) invalid("Publication must use the live service's territory and retirement terms");
    if (const auto blocked=civic_launch_block(state.civic?&*state.civic:nullptr,state,request,state.date);!blocked.empty())
        throw std::invalid_argument(blocked);
    auto work=qualified_project(definition,*state.lab,*state.capacity,request,state.date,definition.serving.id);
    require_publication_audience(definition,*state.market,region,request.weight_audience,work);
    require_compatible(deployments,*state.lab,*state.capacity,*state.market,request,work,state.date);
    return work;
}
}
CapacityProject preview_publication(const SimulationState& state,const DeployModel& request) {
    return preview_release(state,request,ProductChannel::open_weights);
}
CapacityProject preview_agent_release(const SimulationState& state,const DeployModel& request) {
    return preview_release(state,request,ProductChannel::agent);
}
void deploy_model(DeploymentState& state,const LabState& lab,AccountsState& accounts,CapacityState& capacity,
                  MarketState& market,OrdinalDay day,const DeployModel& selection) {
    if (state.records.size()>=lab_record_limit) invalid("Deployment history is full");
    const auto& definition=service(state,selection.service_id);
    const auto request=with_agent_controls(definition,selection);
    if (pending_rollout(state,request.service_id))
        invalid("Promote or discard the service's staged release before replacing production.");
    const auto existing=std::find_if(market.products.begin(),market.products.end(),[&](const auto& row){return row.id==request.service_id;});
    const auto retirement=existing!=market.products.end()?existing->retirement:std::nullopt;
    if (retirement && day>=retirement->retires_on) invalid("This service retired on its published date. Choose another service.");
    const auto region = existing == market.products.end() ? request.region_id : existing->region_id;
    if (existing != market.products.end() && !request.region_id.empty() && request.region_id != region)
        invalid("A live service keeps its launch territory when replacing a model");
    const auto id=state.records.empty() || std::none_of(capacity.projects.begin(),capacity.projects.end(),[&](const auto& row){return row.id==definition.serving.id;})?
        definition.serving.id:definition.serving.id+"."+std::to_string(state.records.size()+1);
    auto work=qualified_project(definition,lab,capacity,request,day,id);
    require_compatible(state,lab,capacity,market,request,work,day);
    auto product = definition.product;
    product.region_id = region;
    require_publication_audience(definition,market,region,request.weight_audience,work);
    const auto plan=agent_plan(definition,lab,request);
    std::uint64_t promised=0;
    for (const auto& contract:market.contracts)
        if (contract.product_id==request.service_id && (contract.status==ContractStatus::active || contract.status==ContractStatus::pending))
            promised+=contract.tasks_per_day;
    if (promised>work.tasks_per_day) invalid("The replacement would shrink capacity below an existing customer promise; cancel the contract first");
    if (existing!=market.products.end()) {
        const auto old=find(capacity.projects,existing->capacity_project_id);
        if (old.status==ProjectStatus::active) {
            cancel_project(capacity,day,old.id);
            if (old.monthly_support_cost.value()>0) end_recurring(accounts,"support/"+old.id,day);
        }
    }
    reserve_project(capacity,day,work);
    if (work.monthly_support_cost.value()>0)
        add_recurring(accounts,{"support/"+work.id,work.monthly_support_cost,day,
            retirement?std::optional{retirement->retires_on}:std::nullopt,"Serving support: "+work.id});
    if (existing==market.products.end()) {
        product.capacity_project_id=work.id;
        add_market_product(market,capacity,std::move(product),request.weight_audience,plan);
        for (const auto& preference:definition.preferences) add_market_preference(market,preference.segment_id,preference.preference);
    } else {
        replace_product_capacity(market,capacity,day,request.service_id,work.id,request.weight_audience,plan);
        set_product_available(market,request.service_id,true);
    }
    state.records.push_back({request.service_id,request.model_id,request.evidence_id,work.id,request.human_review,day,region,0,
        request.weight_audience,request.agent_controls});
}
void validate_deployments(const DeploymentState& state,const LabState& lab,const CapacityState& capacity,const MarketState& market) {
    validate_service_catalog(state.catalog);
    if (state.records.size()>lab_record_limit || state.records.size()!=market.capacity_bindings.size())
        invalid("Deployed services differ from their actual market bindings");
    for (std::size_t i=0;i<state.records.size();++i) {
        const auto& row=state.records[i]; const auto& work=find(capacity.projects,row.project_id);
        const auto& definition=service(state,row.service_id);
        const auto& product=find(market.products,row.service_id);
        if (product.channel!=definition.product.channel || product.monthly_migration!=definition.product.monthly_migration ||
            product.region_id!=row.region_id)
            invalid("The saved service changed its authored channel or launch territory");
        const DeployModel request{row.service_id,row.model_id,row.evidence_id,work.source_id,row.human_review,row.region_id,row.weight_audience,row.agent_controls};
        const auto expected=qualified_project(definition,lab,capacity,request,row.deployed_on,row.project_id);
        require_publication_audience(definition,market,row.region_id,row.weight_audience,expected);
        auto comparable=work; comparable.status=ProjectStatus::active; comparable.stopped_on.reset(); comparable.useful_tasks=0; comparable.completed_work=MilliCU{0};
        if (comparable!=expected || market.capacity_bindings[i]!=ProductCapacityBinding{row.service_id,row.project_id,row.deployed_on,
            row.weight_audience,agent_plan(definition,lab,request)})
            invalid("Deployment differs from its qualified evidence, review costs or actual serving work");
        if (row.restored_release>0) {
            if (row.restored_release>i) invalid("Rollback must restore an earlier production release");
            const auto& prior=state.records[static_cast<std::size_t>(row.restored_release-1)];
            if (prior.service_id!=row.service_id || prior.model_id!=row.model_id || prior.evidence_id!=row.evidence_id ||
                prior.human_review!=row.human_review || prior.region_id!=row.region_id ||
                prior.weight_audience!=row.weight_audience ||
                prior.agent_controls.value_or(AgentControls{})!=row.agent_controls.value_or(AgentControls{}) ||
                find(capacity.projects,prior.project_id).source_id!=work.source_id)
                invalid("Rollback differs from the retained qualified release");
        }
    }
    for (const auto& product:market.products) {
        if (std::none_of(state.records.begin(),state.records.end(),[&](const auto& row){return row.service_id==product.id;}))
            invalid("Market product has no qualified deployment");
    }
    validate_service_rollouts(state,lab,capacity,market);
    validate_service_support(state,lab,capacity,market);
}
}
