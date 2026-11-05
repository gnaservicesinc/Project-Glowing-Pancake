#pragma once
#include "json_support.hpp"
#include <pgp/core/lab.hpp>
namespace pgp::persistence::detail {
[[nodiscard]] Json lab_json(const core::LabState& state);
[[nodiscard]] core::LabState parse_lab(const Json& value);
}
