#pragma once
#include "json_support.hpp"
#include <pgp/core/deployment_risk.hpp>
namespace pgp::persistence::detail {
Json deployment_risk_json(const core::DeploymentRiskState& state);
core::DeploymentRiskState parse_deployment_risk(const Json& value);
}
