#pragma once
#include <pgp/core/staff_workload.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace pgp::content::staff_workload_codec {
inline core::StaffWorkloadRules decode(const nlohmann::json& value) {
    if(!value.is_object() || value.size()!=6)
        throw std::invalid_argument("Expected the six authored staff workload terms");
    const auto bp=[&](const char* key) {
        const auto& field=value.at(key);
        if(!field.is_number_integer()) throw std::invalid_argument("Staff workload uses integer basis points");
        return core::BasisPoints{field.get<std::int64_t>()};
    };
    const core::StaffWorkloadRules result{bp("high_load_bp"),bp("recovery_load_bp"),
        bp("daily_strain_bp"),bp("daily_recovery_bp"),bp("strain_tolerance_bp"),bp("maximum_output_loss_bp")};
    core::validate_staff_workload(result);
    return result;
}
inline nlohmann::json encode(const core::StaffWorkloadRules& rules) {
    return {{"high_load_bp",rules.high_load.value()},{"recovery_load_bp",rules.recovery_load.value()},
        {"daily_strain_bp",rules.daily_strain.value()},{"daily_recovery_bp",rules.daily_recovery.value()},
        {"strain_tolerance_bp",rules.strain_tolerance.value()},{"maximum_output_loss_bp",rules.maximum_output_loss.value()}};
}
} // namespace pgp::content::staff_workload_codec
