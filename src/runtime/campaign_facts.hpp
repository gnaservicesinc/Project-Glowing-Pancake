#pragma once
#include <pgp/runtime/runtime.hpp>

namespace pgp::runtime::detail {
[[nodiscard]] content::Scalar campaign_fact(const WorldState& world, std::string_view id);
}
