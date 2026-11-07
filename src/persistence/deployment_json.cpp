#include "deployment_json.hpp"
#include "lab_profile_json.hpp"
#include "agent_service_json.hpp"
#include "deployment_risk_json.hpp"
#include <pgp/content/deployment_risk.hpp>
namespace pgp::persistence::detail {
namespace {
std::string label(const Json& j) { return text(j,"deployment/text",256); }
core::Cents cents(const Json& j) { return core::Cents{i64(j,"deployment/cents")}; }
Json release(const core::DeploymentRecord& row) {
    Json result{{"service",row.service_id},{"model",row.model_id},{"evidence",row.evidence_id},
        {"project",row.project_id},{"review",row.human_review},{"day",row.deployed_on.value()}};
    if (!row.region_id.empty()) result["region"]=row.region_id;
    if (row.restored_release>0) result["restored_release"]=row.restored_release;
    if (!row.weight_audience.empty()) result["weight_audience"]=row.weight_audience;
    if (row.agent_controls) result["agent_controls"]=agent_controls_json(*row.agent_controls);
    return result;
}
core::DeploymentRecord parse_release(const Json& row) {
    fields(row,"deployment/record",{"service","model","evidence","project","review","day"},{"region","restored_release","weight_audience","agent_controls"});
    return {label(row.at("service")),label(row.at("model")),label(row.at("evidence")),label(row.at("project")),
        boolean(row.at("review"),"deployment/review"),core::OrdinalDay{i64(row.at("day"),"deployment/day")},
        row.contains("region")?label(row.at("region")):std::string{},
        row.contains("restored_release")?u64(row.at("restored_release"),"deployment/restore",core::lab_record_limit):0,
        row.contains("weight_audience")?label(row.at("weight_audience")):std::string{},
        row.contains("agent_controls")?std::optional{parse_agent_controls(row.at("agent_controls"))}:std::nullopt};
}
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
            {"cause",w.cause},{"required",lab_profile_json(row.required)},{"review_credit",lab_profile_json(row.review_credit)},{"review_staff",row.review_staff},
            {"review_cost",row.review_monthly_cost.value()},{"review_tasks",row.review_tasks_per_day},{"preferences",std::move(preferences)},{"required_recipe",row.required_recipe}});
        if (p.channel != core::ProductChannel::legacy) catalog.back()["channel"] = core::channel_key(p.channel);
        if (p.monthly_migration.value() != 10000) catalog.back()["migration"] = p.monthly_migration.value();
        if (!row.required_research.empty()) catalog.back()["research"] = row.required_research;
        if (!row.workload_domain.empty()) catalog.back()["workload_domain"]=row.workload_domain;
        if (row.incident_risk) catalog.back()["incident_risk"]=Json::parse(content::canonical_deployment_risk_rules(*row.incident_risk));
    }
    for (const auto& row:state.records) records.push_back(release(row));
    Json result{{"catalog",std::move(catalog)},{"records",std::move(records)}};
    if (!state.rollouts.empty()) {
        result["rollouts"]=Json::array();
        for (const auto& row:state.rollouts) result["rollouts"].push_back({
            {"candidate",release(row.candidate)},{"incumbent_release",row.incumbent_release},
            {"resolved_on",row.resolved_on?Json(row.resolved_on->value()):Json(nullptr)},{"promoted_release",row.promoted_release}});
    }
    if (state.risk) result["risk"]=deployment_risk_json(*state.risk);
    return result;
}
core::DeploymentState parse_deployment(const Json& j) {
    fields(j,"deployment",{"catalog","records"},{"rollouts","risk"});
    array(j.at("catalog"),"deployment/catalog",core::market_product_limit);
    array(j.at("records"),"deployment/records",core::lab_record_limit);
    core::DeploymentState state;
    for (const auto& row:j.at("catalog")) {
        fields(row,"deployment/service",{"id","project","price","available","source","starts","tasks","inference","staff","memory",
            "interconnect","support","cause","required","review_credit","review_staff","review_cost","review_tasks","preferences"},
            {"required_recipe","channel","migration","research","workload_domain","incident_risk"});
        core::ServiceDefinition value;
        value.product={label(row.at("id")),label(row.at("project")),cents(row.at("price")),boolean(row.at("available"),"deployment/available")};
        if (row.contains("required_recipe")) value.required_recipe=text(row.at("required_recipe"),"deployment/required_recipe",64,true);
        if (row.contains("workload_domain")) value.workload_domain=text(row.at("workload_domain"),"deployment/workload_domain",64);
        if (row.contains("incident_risk")) value.incident_risk=content::load_deployment_risk_rules(row.at("incident_risk").dump());
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
        value.required=parse_lab_profile(row.at("required")); value.review_credit=parse_lab_profile(row.at("review_credit"));
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
    for (const auto& row:j.at("records")) state.records.push_back(parse_release(row));
    if (j.contains("rollouts")) {
        const auto& rollouts=j.at("rollouts");
        array(rollouts,"deployment/rollouts",core::service_trial_limit);
        for (const auto& row:rollouts) {
            fields(row,"deployment/rollout",{"candidate","incumbent_release","resolved_on","promoted_release"});
            const auto& resolved=row.at("resolved_on");
            state.rollouts.push_back({parse_release(row.at("candidate")),
                u64(row.at("incumbent_release"),"deployment/rollout/incumbent",core::lab_record_limit),
                resolved.is_null()?std::nullopt:std::optional{core::OrdinalDay{i64(resolved,"deployment/rollout/resolved_on")}},
                u64(row.at("promoted_release"),"deployment/rollout/promotion",core::lab_record_limit)});
        }
    }
    if (j.contains("risk")) state.risk=parse_deployment_risk(j.at("risk"));
    core::validate_service_catalog(state.catalog);
    return state;
}
}
