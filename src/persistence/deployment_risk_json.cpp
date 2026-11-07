#include "deployment_risk_json.hpp"
#include <pgp/core/market.hpp>
namespace pgp::persistence::detail {
namespace {
Json optional_day(const std::optional<core::OrdinalDay>& day) {return day?Json(day->value()):Json(nullptr);}
core::OrdinalDay day(const Json& value) {return core::OrdinalDay{i64(value,"deployment/risk/day")};}
std::optional<core::OrdinalDay> parse_optional_day(const Json& value) {return value.is_null()?std::nullopt:std::optional{day(value)};}
std::string label(const Json& value) {return text(value,"deployment/risk/id",256);}
}
Json deployment_risk_json(const core::DeploymentRiskState& state) {
    Json samples=Json::array(),incidents=Json::array(),responses=Json::array(),alerts=Json::array();
    for(const auto& row:state.samples) samples.push_back({{"project_id",row.project_id},{"sampled_on",row.sampled_on.value()},
        {"event_on",row.event_on.value()},{"prior_attempts",row.prior_attempts},{"stream_before",row.stream_before},{"stream_after",row.stream_after},
        {"cell",row.cell},{"incident",row.incident}});
    for(const auto& row:state.incidents) incidents.push_back({{"id",row.id},{"project_id",row.project_id},{"sample_index",row.sample_index},
        {"discovered_on",row.discovered_on.value()},{"recovered_on",optional_day(row.recovered_on)},
        {"last_response",row.last_response?Json(*row.last_response):Json(nullptr)}});
    for(const auto& row:state.responses) responses.push_back({{"incident_id",row.incident_id},{"project_id",row.project_id},{"started_on",row.started_on.value()}});
    for(const auto& row:state.alerts) alerts.push_back({{"id",row.id},{"incident_id",row.incident_id},{"day",row.day.value()},
        {"recovery",row.recovery},{"acknowledged",row.acknowledged}});
    return {{"starts_on",state.starts_on.value()},{"stream_origin",state.stream_origin},{"samples",std::move(samples)},
        {"incidents",std::move(incidents)},{"responses",std::move(responses)},{"alerts",std::move(alerts)},
        {"open_incidents",state.open_incidents},{"sampled_on",optional_day(state.sampled_on)},
        {"prepared_on",optional_day(state.prepared_on)},{"alert_cursor",state.alert_cursor}};
}
core::DeploymentRiskState parse_deployment_risk(const Json& value) {
    fields(value,"deployment/risk",{"starts_on","stream_origin","samples","incidents","responses","alerts","open_incidents","sampled_on","prepared_on","alert_cursor"});
    core::DeploymentRiskState result;
    result.starts_on=day(value.at("starts_on"));result.stream_origin=u64(value.at("stream_origin"),"deployment/risk/origin");
    result.sampled_on=parse_optional_day(value.at("sampled_on"));result.prepared_on=parse_optional_day(value.at("prepared_on"));
    result.alert_cursor=static_cast<std::size_t>(u64(value.at("alert_cursor"),"deployment/risk/cursor",core::deployment_incident_alert_limit));
    array(value.at("samples"),"deployment/risk/samples",core::deployment_risk_sample_limit);
    for(const auto& row:value.at("samples")) {
        fields(row,"deployment/risk/sample",{"project_id","sampled_on","event_on","prior_attempts","stream_before","stream_after","cell","incident"});
        result.samples.push_back({label(row.at("project_id")),day(row.at("sampled_on")),day(row.at("event_on")),
            u64(row.at("prior_attempts"),"deployment/risk/prior_attempts"),u64(row.at("stream_before"),"deployment/risk/before"),
            u64(row.at("stream_after"),"deployment/risk/after"),static_cast<std::uint32_t>(u64(row.at("cell"),"deployment/risk/cell",47)),
            boolean(row.at("incident"),"deployment/risk/incident")});
    }
    array(value.at("incidents"),"deployment/risk/incidents",core::deployment_incident_limit);
    for(const auto& row:value.at("incidents")) {
        fields(row,"deployment/risk/incident",{"id","project_id","sample_index","discovered_on","recovered_on","last_response"});
        result.incidents.push_back({label(row.at("id")),label(row.at("project_id")),u64(row.at("sample_index"),"deployment/risk/sample_index",core::deployment_risk_sample_limit-1),
            day(row.at("discovered_on")),parse_optional_day(row.at("recovered_on")),row.at("last_response").is_null()?std::nullopt:
                std::optional{u64(row.at("last_response"),"deployment/risk/last_response",core::deployment_response_limit-1)}});
    }
    array(value.at("responses"),"deployment/risk/responses",core::deployment_response_limit);
    for(const auto& row:value.at("responses")) {
        fields(row,"deployment/risk/response",{"incident_id","project_id","started_on"});
        result.responses.push_back({label(row.at("incident_id")),label(row.at("project_id")),day(row.at("started_on"))});
    }
    array(value.at("alerts"),"deployment/risk/alerts",core::deployment_incident_alert_limit);
    for(const auto& row:value.at("alerts")) {
        fields(row,"deployment/risk/alert",{"id","incident_id","day","recovery","acknowledged"});
        result.alerts.push_back({label(row.at("id")),label(row.at("incident_id")),day(row.at("day")),boolean(row.at("recovery"),"deployment/risk/recovery"),
            boolean(row.at("acknowledged"),"deployment/risk/acknowledged")});
    }
    array(value.at("open_incidents"),"deployment/risk/open",core::deployment_incident_limit);
    for(const auto& entry:value.at("open_incidents")) result.open_incidents.push_back(u64(entry,"deployment/risk/open_index",core::deployment_incident_limit-1));
    return result;
}
}
