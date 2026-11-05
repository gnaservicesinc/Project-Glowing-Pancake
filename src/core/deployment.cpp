#include <pgp/core/deployment.hpp>
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
const ServiceDefinition& service(const DeploymentState& state,std::string_view id) {
    const auto row=std::find_if(state.catalog.begin(),state.catalog.end(),[&](const auto& value){return value.product.id==id;});
    if (row==state.catalog.end()) invalid("Choose an offered service scope");
    return *row;
}
CapacityProject qualified_project(const ServiceDefinition& definition,const LabState& lab,const CapacityState& capacity,
                                 const DeployModel& request,OrdinalDay day,std::string id) {
    const auto& model=find(lab.models,request.model_id);
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
            if (!find(lab.catalog.data,data_id).commercial_use)
                invalid("This model includes research-only data; train a new commercially cleared model");
        }
        if (ancestor->parent_id.empty()) break;
        ancestor=&find(lab.models,ancestor->parent_id);
    }
    if (request.human_review && (definition.review_staff==0 || definition.review_tasks_per_day==0))
        invalid("Human review is not offered for this service scope");
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
    project.inference_milli_CU_per_task=recipe.inference_per_task;
    project.requested=MilliCUPerDay{required_inference(project.tasks_per_day,recipe.inference_per_task).value()};
    project.minimum_memory_mib=std::max(project.minimum_memory_mib,recipe.minimum_memory_mib);
    project.interconnect=recipe.interconnect;
    return project;
}
}
void validate_service_catalog(const std::vector<ServiceDefinition>& catalog) {
    if (catalog.empty() || catalog.size()>market_product_limit) invalid("Service catalog must be bounded and nonempty");
    std::set<std::string> ids;
    for (const auto& row:catalog) {
        if (row.product.id.empty() || !ids.insert(row.product.id).second || row.product.capacity_project_id!=row.serving.id ||
            row.serving.kind!=ProjectKind::serving || row.serving.status!=ProjectStatus::active || row.serving.stopped_on ||
            row.serving.completed_work.value()!=0 || row.serving.useful_tasks!=0 || row.serving.tasks_per_day==0 || row.serving.staff==0 ||
            row.review_staff>1'000'000 || row.review_monthly_cost.value()<0 || row.review_tasks_per_day>row.serving.tasks_per_day ||
            (row.review_staff==0)!=(row.review_tasks_per_day==0) || row.preferences.empty() || row.preferences.size()>market_segment_limit ||
            std::none_of(row.required.begin(),row.required.end(),[](auto value){return value.value()>0;}))
            invalid("Service catalog contains invalid scope, review or serving terms");
        std::set<std::string> segments;
        for (const auto& pref:row.preferences)
            if (pref.preference.product_id!=row.product.id || !segments.insert(pref.segment_id).second)
                invalid("Service preferences must name their offered product and unique customer segments");
    }
}
void deploy_model(DeploymentState& state,const LabState& lab,AccountsState& accounts,CapacityState& capacity,
                  MarketState& market,OrdinalDay day,const DeployModel& request) {
    if (state.records.size()>=lab_record_limit) invalid("Deployment history is full");
    const auto& definition=service(state,request.service_id);
    const auto existing=std::find_if(market.products.begin(),market.products.end(),[&](const auto& row){return row.id==request.service_id;});
    const auto id=state.records.empty() || std::none_of(capacity.projects.begin(),capacity.projects.end(),[&](const auto& row){return row.id==definition.serving.id;})?
        definition.serving.id:definition.serving.id+"."+std::to_string(state.records.size()+1);
    auto work=qualified_project(definition,lab,capacity,request,day,id);
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
        add_recurring(accounts,{"support/"+work.id,work.monthly_support_cost,day,std::nullopt,"Serving support: "+work.id});
    if (existing==market.products.end()) {
        auto product=definition.product; product.capacity_project_id=work.id;
        add_market_product(market,capacity,std::move(product));
        for (const auto& preference:definition.preferences) add_market_preference(market,preference.segment_id,preference.preference);
    } else {
        replace_product_capacity(market,capacity,day,request.service_id,work.id);
        set_product_available(market,request.service_id,true);
    }
    state.records.push_back({request.service_id,request.model_id,request.evidence_id,work.id,request.human_review,day});
}
void validate_deployments(const DeploymentState& state,const LabState& lab,const CapacityState& capacity,const MarketState& market) {
    validate_service_catalog(state.catalog);
    if (state.records.size()>lab_record_limit || state.records.size()!=market.capacity_bindings.size())
        invalid("Deployed services differ from their actual market bindings");
    for (std::size_t i=0;i<state.records.size();++i) {
        const auto& row=state.records[i]; const auto& work=find(capacity.projects,row.project_id);
        const auto expected=qualified_project(service(state,row.service_id),lab,capacity,
            {row.service_id,row.model_id,row.evidence_id,work.source_id,row.human_review},row.deployed_on,row.project_id);
        auto comparable=work; comparable.status=ProjectStatus::active; comparable.stopped_on.reset(); comparable.useful_tasks=0; comparable.completed_work=MilliCU{0};
        if (comparable!=expected || market.capacity_bindings[i]!=ProductCapacityBinding{row.service_id,row.project_id,row.deployed_on})
            invalid("Deployment differs from its qualified evidence, review costs or actual serving work");
    }
    for (const auto& product:market.products) {
        if (std::none_of(state.records.begin(),state.records.end(),[&](const auto& row){return row.service_id==product.id;}))
            invalid("Market product has no qualified deployment");
    }
}
}
