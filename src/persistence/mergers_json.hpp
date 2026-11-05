#pragma once
#include "json_support.hpp"
#include <pgp/runtime/mergers.hpp>
namespace pgp::persistence::detail {
Json acquisitions_json(const std::vector<runtime::AcquisitionRecord>&);
std::vector<runtime::AcquisitionRecord> parse_acquisitions(const Json&);
}
