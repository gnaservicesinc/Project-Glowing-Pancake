#pragma once

#include <pgp/core/market.hpp>

namespace pgp::core {
struct SimulationState;
struct ManagementNotice;

struct ServiceMonitoringRules {
    Days window{7};
    std::uint64_t minimum_attempts=100;
    BasisPoints warning_drop{500};
    auto operator<=>(const ServiceMonitoringRules&) const = default;
};
inline constexpr std::string_view service_workload_notice="service_workload";
inline constexpr std::size_t service_monitoring_notice_limit=128;
inline constexpr std::string_view service_monitoring_limit_notice="service_monitoring_limit";
inline constexpr std::string_view service_monitoring_limit_id="service.monitoring.limit";
inline constexpr std::string_view service_monitoring_limit_subject="service_monitoring";
struct ServiceWorkSample {
    std::uint64_t completed=0;
    AgentFailures failures;
    [[nodiscard]] std::uint64_t attempts() const { return completed+failures.failed; }
    [[nodiscard]] std::optional<BasisPoints> completion() const;
};
struct ServiceWorkloadReview {
    // These point into the inspected snapshot.
    const MarketDemandChange* change;
    const ProductCapacityBinding* production;
    const MarketTrial* trial;
    ServiceWorkSample prior, shifted, candidate;
    std::optional<BasisPoints> completion_drop;
};

void validate_service_monitoring(const ServiceMonitoringRules&);
[[nodiscard]] std::vector<ServiceWorkloadReview> service_workload_reviews(const SimulationState&,std::string_view service);
[[nodiscard]] std::optional<ServiceWorkloadReview> service_workload_alert(const SimulationState&,const ManagementNotice&);
[[nodiscard]] bool service_monitoring_suspended(const SimulationState&);
void prepare_service_monitoring(SimulationState&);
} // namespace pgp::core
