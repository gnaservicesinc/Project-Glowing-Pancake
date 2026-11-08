#pragma once
#include "people_codec.hpp"
#include <pgp/core/departments.hpp>
namespace pgp::content::departments_codec {
inline core::DepartmentRules decode(const nlohmann::json& j) {
    people_codec::fields(j,{"operator_skill","maximum_assignments","minimum_availability_bp","operator_skill_credit_bp","manager_credit_bp","management_milli_CU_per_day"});
    core::DepartmentRules r;r.operator_skill=people_codec::count(j.at("operator_skill"));const auto limit=people_codec::integer(j.at("maximum_assignments"));if(limit<1 || limit>1024) throw std::invalid_argument("Operations assignment history is out of bounds");r.maximum_assignments=static_cast<std::uint32_t>(limit);
    r.minimum_availability=core::BasisPoints{people_codec::integer(j.at("minimum_availability_bp"))};r.operator_skill_credit=core::BasisPoints{people_codec::integer(j.at("operator_skill_credit_bp"))};r.manager_credit=core::BasisPoints{people_codec::integer(j.at("manager_credit_bp"))};
    r.management_compute=core::MilliCUPerDay{people_codec::integer(j.at("management_milli_CU_per_day"))};core::validate_department_rules(r);return r;
}
inline nlohmann::json encode(const core::DepartmentRules& r) {
    return {{"operator_skill",r.operator_skill},{"maximum_assignments",r.maximum_assignments},{"minimum_availability_bp",r.minimum_availability.value()},
        {"operator_skill_credit_bp",r.operator_skill_credit.value()},{"manager_credit_bp",r.manager_credit.value()},{"management_milli_CU_per_day",r.management_compute.value()}};
}
}
