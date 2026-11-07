#pragma once

#include <pgp/core/units.hpp>

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pgp::core {

inline constexpr std::size_t allocation_segment_limit = 64;
inline constexpr std::size_t allocation_product_limit = 512;
inline constexpr std::size_t allocation_edge_limit = 8192;
inline constexpr std::uint64_t allocation_task_limit = 1'000'000'000;
inline constexpr std::int64_t allocation_price_limit = 100'000'000;
inline constexpr std::uint64_t allocation_weight_limit = 1'000'000'000'000;

struct WeightedQuota {
    std::string id;
    std::uint64_t weight = 0;
};
struct ApportionedQuota {
    std::string id;
    std::uint64_t units = 0;
    auto operator<=>(const ApportionedQuota&) const = default;
};
// Hamilton apportionment. Results use ID order; tied fractions use ID order.
// All-positive total weight is required unless total is zero.
[[nodiscard]] std::vector<ApportionedQuota> apportion_units(
    std::uint64_t total, std::span<const WeightedQuota> weights);

struct AllocationProduct {
    std::string id;
    Cents price{0};
    std::uint64_t capacity = 0;
    MilliCU customer_compute_per_task{0};
};
struct AllocationEdge {
    std::string product_id;
    std::uint64_t weight = 0;
    std::optional<std::uint64_t> task_limit;
    std::uint64_t retained_tasks = 0;
    // The regular market ceiling before a waitlist limits new admissions.
    std::optional<std::uint64_t> waitlist_interest_limit{};
};
struct AllocationSegment {
    std::string id;
    std::uint64_t tasks = 0;
    Cents budget{0};
    std::uint64_t outside_weight = 0;
    std::vector<AllocationEdge> edges;
    MilliCUPerDay customer_compute{0};
};
struct AllocationDelivery {
    std::string product_id;
    std::uint64_t tasks = 0;
    Cents cost{0};
    auto operator<=>(const AllocationDelivery&) const = default;
};
enum class AllocationOutsideReason {
    preference, no_eligible_product, zero_utility, budget, capacity, edge_limit, customer_compute
};
struct AllocationOutside {
    AllocationOutsideReason reason = AllocationOutsideReason::preference;
    std::uint64_t tasks = 0;
    auto operator<=>(const AllocationOutside&) const = default;
};
struct AllocationInterest {
    std::string product_id;
    std::uint64_t tasks = 0;
    auto operator<=>(const AllocationInterest&) const = default;
};
struct AllocationSegmentResult {
    std::string id;
    std::vector<AllocationDelivery> deliveries;
    std::uint64_t outside_tasks = 0;
    Cents remaining_budget{0};
    std::vector<AllocationOutside> outside;
    std::vector<AllocationInterest> waitlist_interest{};
    auto operator<=>(const AllocationSegmentResult&) const = default;
};
struct AllocationResult {
    std::vector<AllocationSegmentResult> segments;
    std::size_t rounds = 0;
    std::size_t round_bound = 0;
    auto operator<=>(const AllocationResult&) const = default;
};
// Fixed-snapshot bounded redistribution; no RNG, state mutation or per-task
// iteration. Throws invalid_argument/overflow_error on bad input or failure to
// make proved progress. Outside is an explicit terminal destination.
[[nodiscard]] AllocationResult allocate_demand(
    std::span<const AllocationProduct> products,
    std::span<const AllocationSegment> segments);

} // namespace pgp::core
