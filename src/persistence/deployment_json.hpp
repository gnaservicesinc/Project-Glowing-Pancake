#pragma once
#include "json_support.hpp"
#include <pgp/core/deployment.hpp>
namespace pgp::persistence::detail {
Json deployment_json(const core::DeploymentState& state);
core::DeploymentState parse_deployment(const Json& value);
}
