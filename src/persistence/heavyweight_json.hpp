#pragma once
#include "json_support.hpp"
#include <pgp/runtime/heavyweight.hpp>
namespace pgp::persistence::detail {
[[nodiscard]] Json heavyweight_json(const runtime::HeavyweightState&);
[[nodiscard]] runtime::HeavyweightState parse_heavyweight(const Json&);
[[nodiscard]] Json competitive_offers_json(const std::vector<runtime::CompetitiveOffer>&);
[[nodiscard]] std::vector<runtime::CompetitiveOffer> parse_competitive_offers(const Json&);
}
