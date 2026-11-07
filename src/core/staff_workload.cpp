#include <pgp/core/staff_workload.hpp>

#include <algorithm>
#include <stdexcept>

namespace pgp::core {
void validate_staff_workload(const StaffWorkloadRules& rules) {
    if(rules.high_load<=rules.recovery_load || rules.daily_strain.value()==0 ||
        rules.daily_recovery.value()==0 || rules.strain_tolerance.value()==10000 ||
        rules.maximum_output_loss.value()==0 || rules.maximum_output_loss.value()>5000)
        throw std::invalid_argument("Staff workload needs distinct pressure and recovery thresholds and a bounded output loss");
}
BasisPoints staff_load(std::uint64_t assigned,std::uint32_t available) {
    if(assigned>available) throw std::invalid_argument("Settled staff workload exceeds the available team");
    return available>0?BasisPoints{mul_div_floor(assigned,10000,available).quotient}:BasisPoints{0};
}
BasisPoints next_staff_strain(const StaffWorkloadRules& rules,BasisPoints strain,BasisPoints load) {
    if(load>=rules.high_load) return BasisPoints{std::min<std::int64_t>(10000,strain.value()+rules.daily_strain.value())};
    if(load<=rules.recovery_load) return BasisPoints{std::max<std::int64_t>(0,strain.value()-rules.daily_recovery.value())};
    return strain;
}
BasisPoints staff_output_factor(const StaffWorkloadRules& rules,BasisPoints strain) {
    if(strain<=rules.strain_tolerance) return BasisPoints{10000};
    const auto reduction=mul_div_floor(static_cast<std::uint64_t>(strain.value()-rules.strain_tolerance.value()),
        static_cast<std::uint64_t>(rules.maximum_output_loss.value()),
        static_cast<std::uint64_t>(10000-rules.strain_tolerance.value())).quotient;
    return BasisPoints{10000-reduction};
}
std::uint64_t staffed_output(std::uint64_t full_rate,BasisPoints factor) {
    if(full_rate==0 || factor.value()==10000) return full_rate;
    return std::max<std::uint64_t>(1,mul_div_floor(full_rate,static_cast<std::uint64_t>(factor.value()),10000).quotient);
}
} // namespace pgp::core
