#pragma once

#include <pgp/core/deployment.hpp>

namespace pgp::core::deployment_detail {
[[nodiscard]] const ServiceDefinition& service(const DeploymentState&,std::string_view id);
[[nodiscard]] CapacityProject qualified_project(const ServiceDefinition&,const LabState&,const CapacityState&,
    const DeployModel&,OrdinalDay,std::string project_id);
void require_compatible(const DeploymentState&,const LabState&,const CapacityState&,const MarketState&,
    const DeployModel&,const CapacityProject&,OrdinalDay);
} // namespace pgp::core::deployment_detail
