#pragma once
#include "json_support.hpp"
#include <pgp/runtime/collaboration.hpp>
namespace pgp::persistence::detail {
Json collaborations_json(const std::vector<runtime::ResearchCollaboration>&);
std::vector<runtime::ResearchCollaboration> parse_collaborations(const Json&);
}
