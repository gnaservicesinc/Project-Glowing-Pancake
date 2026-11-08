#pragma once
#include <pgp/core/units.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace pgp::core {
struct SimulationState;
struct DepartmentRules {
    std::uint32_t operator_skill=50,maximum_assignments=512;
    BasisPoints minimum_availability{7000},operator_skill_credit{3000},manager_credit{1500};
    MilliCUPerDay management_compute{100};
    auto operator<=>(const DepartmentRules&) const = default;
};
struct OperationsAppointment {
    std::uint64_t id=0;std::string person,project,source;
    OrdinalDay starts_on{0},planned_until{0};std::optional<OrdinalDay> ends_before;
    auto operator<=>(const OperationsAppointment&) const = default;
};
struct DepartmentsState {
    std::vector<OperationsAppointment> operations;
    auto operator<=>(const DepartmentsState&) const = default;
};
struct AssignOperationsManager {std::string person,source;};
void validate_department_rules(const DepartmentRules&);
void initialize_departments(SimulationState&);
void prepare_departments(SimulationState&);
void assign_operations_manager(SimulationState&,const AssignOperationsManager&);
const OperationsAppointment* operations_manager_on(const SimulationState&,OrdinalDay);
BasisPoints operations_availability(const SimulationState&,OrdinalDay);
bool operations_project(const SimulationState&,std::string_view);
void validate_departments(const SimulationState&,bool deep=true);
}
