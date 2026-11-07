#pragma once

#include "json_support.hpp"
#include <pgp/core/market.hpp>

namespace pgp::persistence::detail {
inline Json agent_controls_json(core::AgentControls controls) {
    return {{"tools",core::agent_tools_key(controls.tools)},{"oversight",core::agent_oversight_key(controls.oversight)}};
}
inline core::AgentControls parse_agent_controls(const Json& value) {
    fields(value,"agent_controls",{"tools","oversight"});
    return {core::parse_agent_tools(text(value.at("tools"),"agent_controls/tools",16)),
        core::parse_agent_oversight(text(value.at("oversight"),"agent_controls/oversight",16))};
}
inline Json agent_plan_json(const core::AgentPlan& plan) {
    return {{"controls",agent_controls_json(plan.controls)},{"failure_bp",plan.failure_rate.value()},
        {"remedy_per_action",plan.remedy_per_action.value()}};
}
inline std::optional<core::AgentPlan> optional_agent_plan(const Json& row) {
    if (!row.contains("agent_plan")) return {};
    const auto& value=row.at("agent_plan");
    fields(value,"agent_plan",{"controls","failure_bp","remedy_per_action"});
    return core::AgentPlan{parse_agent_controls(value.at("controls")),core::BasisPoints{i64(value.at("failure_bp"),"agent_plan/failure_bp")},
        core::Cents{i64(value.at("remedy_per_action"),"agent_plan/remedy_per_action")}};
}
inline Json agent_failures_json(const core::AgentFailures& outcome) {
    return {{"failed",outcome.failed},{"executed",outcome.executed},{"remedy",outcome.remedy.value()}};
}
inline core::AgentFailures parse_agent_failures(const Json& value) {
    fields(value,"agent_failures",{"failed","executed","remedy"});
    return {u64(value.at("failed"),"agent_failures/failed",core::market_task_limit),
        u64(value.at("executed"),"agent_failures/executed",core::market_task_limit),core::Cents{i64(value.at("remedy"),"agent_failures/remedy")}};
}
} // namespace pgp::persistence::detail
