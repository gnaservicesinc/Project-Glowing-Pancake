#pragma once
#include "json_support.hpp"
#include <pgp/core/departments.hpp>
namespace pgp::persistence::detail {
Json departments_json(const core::DepartmentsState&);
core::DepartmentsState parse_departments(const Json&);
}
