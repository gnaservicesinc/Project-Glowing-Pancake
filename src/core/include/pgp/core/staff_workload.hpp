#pragma once
#include <pgp/core/units.hpp>

namespace pgp::core {
struct StaffWorkloadRules {
    BasisPoints high_load{9500}, recovery_load{7500};
    BasisPoints daily_strain{80}, daily_recovery{150};
    BasisPoints strain_tolerance{2000}, maximum_output_loss{2500};
    auto operator<=>(const StaffWorkloadRules&) const = default;
};
struct StaffWorkload {
    StaffWorkloadRules rules;
    BasisPoints strain{0};
    auto operator<=>(const StaffWorkload&) const = default;
};

void validate_staff_workload(const StaffWorkloadRules&);
[[nodiscard]] BasisPoints staff_load(std::uint64_t assigned,std::uint32_t available);
[[nodiscard]] BasisPoints next_staff_strain(const StaffWorkloadRules&,BasisPoints strain,BasisPoints load);
[[nodiscard]] BasisPoints staff_output_factor(const StaffWorkloadRules&,BasisPoints strain);
[[nodiscard]] std::uint64_t staffed_output(std::uint64_t full_rate,BasisPoints factor);
} // namespace pgp::core
