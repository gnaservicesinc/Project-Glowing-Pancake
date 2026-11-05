#include "deployment_json.hpp"
namespace pgp::persistence::detail {
namespace {
Json profile(const core::LabProfile& p) { return {p[0].value(),p[1].value()}; }
core::LabProfile parse_profile(const Json& j) {
    array(j,"deployment/profile",2);
    if (j.size()!=2) invalid("deployment/profile","Expected two task domains");
    return {core::BasisPoints{i64(j[0],"deployment/profile")},core::BasisPoints{i64(j[1],"deployment/profile")}};
}
std::string label(const Json& j) { return text(j,"deployment/text",256); }
core::Cents cents(const Json& j) { return core::Cents{i64(j,"deployment/cents")}; }
}
Json deployment_json(const core::DeploymentState& state) {
    Json catalog=Json::array(), records=Json::array();
    for (const auto& row:state.catalog) {
        const auto& p=row.product; const auto& w=row.serving;
        Json preferences=Json::array();
        for (const auto& pref:row.preferences) preferences.push_back({{"segment",pref.segment_id},{"product",pref.preference.product_id},
            {"utility",pref.preference.utility},{"willingness",pref.preference.willingness_to_pay.value()},
            {"switching",pref.preference.switching_cost},{"eligible",pref.preference.eligible}});
        catalog.push_back({{"id",p.id},{"project",p.capacity_project_id},{"price",p.price_per_task.value()},{"available",p.available},
            {"source",w.source_id},{"starts",w.starts_on.value()},{"tasks",w.tasks_per_day},{"inference",w.inference_milli_CU_per_task.value()},
            {"staff",w.staff},{"memory",w.minimum_memory_mib},{"interconnect",w.interconnect},{"support",w.monthly_support_cost.value()},
            {"cause",w.cause},{"required",profile(row.required)},{"review_credit",profile(row.review_credit)},{"review_staff",row.review_staff},
            {"review_cost",row.review_monthly_cost.value()},{"review_tasks",row.review_tasks_per_day},{"preferences",std::move(preferences)},{"required_recipe",row.required_recipe}});
        if (p.channel != core::ProductChannel::legacy) catalog.back()["channel"] = core::channel_key(p.channel);
        if (p.monthly_migration.value() != 10000) catalog.back()["migration"] = p.monthly_migration.value();
        if (!row.required_research.empty()) catalog.back()["research"] = row.required_research;
    }
    for (const auto& row:state.records) {
        records.push_back({{"service",row.service_id},{"model",row.model_id},{"evidence",row.evidence_id},
            {"project",row.project_id},{"review",row.human_review},{"day",row.deployed_on.value()}});
        if (!row.region_id.empty()) records.back()["region"] = row.region_id;
    }
    return {{"catalog",std::move(catalog)},{"records",std::move(records)}};
}
core::DeploymentState parse_deployment(const Json& j) {
    fields(j,"deployment",{"catalog","records"});
    array(j.at("catalog"),"deployment/catalog",core::market_product_limit);
    array(j.at("records"),"deployment/records",core::lab_record_limit);
    core::DeploymentState state;
    for (const auto& row:j.at("catalog")) {
        fields(row,"deployment/service",{"id","project","price","available","source","starts","tasks","inference","staff","memory",
            "interconnect","support","cause","required","review_credit","review_staff","review_cost","review_tasks","preferences"},
            {"required_recipe","channel","migration","research"});
        core::ServiceDefinition value;
        value.product={label(row.at("id")),label(row.at("project")),cents(row.at("price")),boolean(row.at("available"),"deployment/available")};
        if (row.contains("required_recipe")) value.required_recipe=text(row.at("required_recipe"),"deployment/required_recipe",64,true);
        if (row.contains("channel")) value.product.channel = core::parse_channel(label(row.at("channel")));
        if (row.contains("migration")) value.product.monthly_migration = core::BasisPoints{i64(row.at("migration"),"deployment/migration")};
        if (row.contains("research")) {
            array(row.at("research"), "deployment/research", 16);
            for (const auto& prerequisite : row.at("research")) value.required_research.push_back(label(prerequisite));
        }
        auto& w=value.serving; w.id=value.product.capacity_project_id; w.source_id=label(row.at("source"));
        w.kind=core::ProjectKind::serving; w.starts_on=core::OrdinalDay{i64(row.at("starts"),"deployment/starts")};
        w.tasks_per_day=u64(row.at("tasks"),"deployment/tasks",core::market_task_limit);
        w.inference_milli_CU_per_task=core::MilliCU{i64(row.at("inference"),"deployment/inference")};
        w.requested=core::MilliCUPerDay{core::required_inference(w.tasks_per_day,w.inference_milli_CU_per_task).value()};
        w.staff=static_cast<std::uint32_t>(u64(row.at("staff"),"deployment/staff",1'000'000));
        w.minimum_memory_mib=u64(row.at("memory"),"deployment/memory"); w.interconnect=label(row.at("interconnect"));
        w.monthly_support_cost=cents(row.at("support")); w.cause=label(row.at("cause"));
        value.required=parse_profile(row.at("required")); value.review_credit=parse_profile(row.at("review_credit"));
        value.review_staff=static_cast<std::uint32_t>(u64(row.at("review_staff"),"deployment/review_staff",1'000'000));
        value.review_monthly_cost=cents(row.at("review_cost")); value.review_tasks_per_day=u64(row.at("review_tasks"),"deployment/review_tasks",core::market_task_limit);
        array(row.at("preferences"),"deployment/preferences",core::market_segment_limit);
        for (const auto& pref:row.at("preferences")) {
            fields(pref,"deployment/preference",{"segment","product","utility","willingness","switching","eligible"});
            value.preferences.push_back({label(pref.at("segment")),{label(pref.at("product")),u64(pref.at("utility"),"deployment/utility",core::market_utility_limit),
                cents(pref.at("willingness")),u64(pref.at("switching"),"deployment/switching",core::market_utility_limit),boolean(pref.at("eligible"),"deployment/eligible")}});
        }
        state.catalog.push_back(std::move(value));
    }
    for (const auto& row:j.at("records")) {
        fields(row,"deployment/record",{"service","model","evidence","project","review","day"},{"region"});
        state.records.push_back({label(row.at("service")),label(row.at("model")),label(row.at("evidence")),label(row.at("project")),
            boolean(row.at("review"),"deployment/review"),core::OrdinalDay{i64(row.at("day"),"deployment/day")},
            row.contains("region") ? label(row.at("region")) : std::string{}});
    }
    core::validate_service_catalog(state.catalog);
    return state;
}
}
