#pragma once

#include <pgp/core/accounts.hpp>
#include <pgp/core/capacity.hpp>

namespace pgp::core {

inline constexpr std::size_t market_segment_limit = 16;
inline constexpr std::size_t market_product_limit = 64;
inline constexpr std::size_t market_contract_limit = 256;
inline constexpr std::size_t market_day_limit = 40960;
inline constexpr std::size_t market_record_limit = 262144;
inline constexpr std::uint64_t market_task_limit = 1'000'000'000;
inline constexpr std::int64_t market_price_limit = 100'000'000;
inline constexpr std::uint64_t market_utility_limit = 1'000'000;

struct MarketProduct {
    std::string id;
    std::string capacity_project_id;
    Cents price_per_task{0};
    bool available = true;
    auto operator<=>(const MarketProduct&) const = default;
};
struct MarketPreference {
    std::string product_id;
    std::uint64_t utility = 0;
    Cents willingness_to_pay{0};
    std::uint64_t switching_cost = 0;
    bool eligible = true;
    auto operator<=>(const MarketPreference&) const = default;
};
struct MarketSegment {
    std::string id;
    std::uint64_t daily_tasks = 0;
    Cents daily_budget{0};
    std::uint64_t outside_utility = 0;
    std::uint64_t reservation_threshold = 0;
    BasisPoints maximum_monthly_migration{10000};
    std::vector<MarketPreference> preferences;
    auto operator<=>(const MarketSegment&) const = default;
};
enum class ContractStatus { pending, active, completed, missed, cancelled };
struct MarketContract {
    std::string id;
    std::string segment_id;
    std::string product_id;
    std::uint64_t tasks_per_day = 0;
    Cents monthly_fee{0};
    Cents usage_price{0};
    BasisPoints service_credit_basis_points{10000};
    Cents missing_task_penalty{0};
    Cents missed_start_penalty{0};
    Cents cancellation_penalty{0};
    OrdinalDay accepted_on{0};
    OrdinalDay start_by{0};
    OrdinalDay ends_before{1};
    std::optional<OrdinalDay> started_on;
    std::optional<OrdinalDay> stopped_on;
    ContractStatus status = ContractStatus::pending;
    std::string cause;
    auto operator<=>(const MarketContract&) const = default;
};
struct ContractDayResult {
    std::string contract_id;
    std::uint64_t promised_tasks = 0;
    std::uint64_t delivered_tasks = 0;
    Cents gross_fee{0};
    Cents service_credit{0};
    Cents usage_charge{0};
    Cents penalty{0};
    auto operator<=>(const ContractDayResult&) const = default;
};
struct MarketDelivery {
    std::string segment_id;
    std::string product_id;
    std::uint64_t tasks = 0;
    Cents charged{0};
    auto operator<=>(const MarketDelivery&) const = default;
};
struct MarketSegmentDay {
    std::string segment_id;
    std::uint64_t demand = 0;
    Cents budget{0};
    std::uint64_t contract_tasks = 0;
    std::uint64_t discretionary_tasks = 0;
    std::uint64_t outside_tasks = 0;
    Cents charged{0};
    Cents unspent{0};
    std::string outside_reason;
    auto operator<=>(const MarketSegmentDay&) const = default;
};
struct MarketDay {
    OrdinalDay day{0};
    std::vector<ContractDayResult> contracts;
    std::vector<MarketDelivery> discretionary;
    std::vector<MarketSegmentDay> segments;
    std::uint64_t allocation_rounds = 0;
    auto operator<=>(const MarketDay&) const = default;
};
struct InstalledShare {
    std::string product_id; // "outside" is the explicit non-AI alternative.
    BasisPoints share{0};
    auto operator<=>(const InstalledShare&) const = default;
};
struct MarketAdoption {
    OrdinalDay reviewed_on{0};
    std::string segment_id;
    std::uint64_t observed_tasks = 0;
    std::vector<InstalledShare> shares;
    auto operator<=>(const MarketAdoption&) const = default;
};
struct MarketState {
    std::string owner = "company.player";
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> closed_through;
    std::vector<MarketProduct> products;
    std::vector<MarketSegment> segments;
    std::vector<MarketContract> contracts;
    std::vector<MarketDay> days;
    std::vector<MarketAdoption> adoptions;
    auto operator<=>(const MarketState&) const = default;
};
struct MarketReport {
    std::uint64_t delivered_tasks = 0;
    std::uint64_t outside_tasks = 0;
    Cents customer_charges{0};
    Cents service_credits{0};
    Cents penalties{0};
    std::uint32_t active_contracts = 0;
    auto operator<=>(const MarketReport&) const = default;
};
[[nodiscard]] MarketState opening_market(OrdinalDay starts_on = OrdinalDay{0},
                                         std::string owner = "company.player");
[[nodiscard]] MarketReport market_report(const MarketState& state);
void validate_market(const MarketState& state);
void validate_market_links(const MarketState& state, const AccountsState& accounts,
                           const CapacityState& capacity);
void add_market_product(MarketState& state, const CapacityState& capacity, MarketProduct product);
void add_market_segment(MarketState& state, MarketSegment segment);
void set_product_available(MarketState& state, std::string_view product_id, bool available);
void accept_contract(MarketState& state, const CapacityState& capacity, OrdinalDay day, MarketContract contract);
void start_contract(MarketState& state, const CapacityState& capacity, OrdinalDay day, std::string_view contract_id);
void cancel_contract(MarketState& state, AccountsState& accounts, OrdinalDay day, std::string_view contract_id);
// Atomically plan contracts first, allocate residual demand/budgets, close actual
// serving/training work and recognize invoices. Caller closes accounts next.
void close_market_day(MarketState& state, AccountsState& accounts,
                      CapacityState& capacity, OrdinalDay day);
} // namespace pgp::core
