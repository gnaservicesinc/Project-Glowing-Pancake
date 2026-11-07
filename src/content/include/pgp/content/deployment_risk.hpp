#pragma once
#include <pgp/core/deployment_risk.hpp>
#include <string_view>

namespace pgp::content {
[[nodiscard]] core::DeploymentRiskRules load_deployment_risk_rules(std::string_view bytes);
[[nodiscard]] std::string canonical_deployment_risk_rules(const core::DeploymentRiskRules& rules);
}
