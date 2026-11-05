#pragma once
#include "json_support.hpp"
#include <pgp/core/infrastructure.hpp>
namespace pgp::persistence::detail {
[[nodiscard]] Json infrastructure_json(const core::InfrastructureState&);
[[nodiscard]] core::InfrastructureState parse_infrastructure(const Json&);
}
