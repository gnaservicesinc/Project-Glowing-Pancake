#pragma once

#include <pgp/core/accounts.hpp>
#include <pgp/core/agent_service.hpp>
#include <pgp/core/capacity.hpp>
#include <pgp/core/allocation.hpp>
#include <functional>

namespace pgp::core {

inline constexpr std::size_t market_segment_limit = allocation_segment_limit;
inline constexpr std::size_t market_product_limit = 64;
inline constexpr std::size_t market_contract_limit = 256;
inline constexpr std::size_t market_day_limit = retained_campaign_days;
inline constexpr std::size_t market_record_limit = retained_campaign_months * market_segment_limit;
// Retention for one segment summary and delivery per region per day over
// the supported history. Active products and commitments have their own caps.
inline constexpr std::size_t market_settlement_limit = market_day_limit * market_segment_limit * 2;
inline constexpr std::uint64_t market_task_limit = 1'000'000'000;
inline constexpr std::int64_t market_price_limit = 100'000'000;
inline constexpr std::uint64_t market_utility_limit = 1'000'000;

enum class ProductChannel { legacy, business, api, consumer, licensed, open_weights, agent };
[[nodiscard]] std::string_view channel_key(ProductChannel channel);
[[nodiscard]] ProductChannel parse_channel(std::string_view key);
[[nodiscard]] bool customer_hosted(ProductChannel channel);

struct ProductRetirement {
    OrdinalDay announced_on{0}, retires_on{0};
    auto operator<=>(const ProductRetirement&) const = default;
};
struct MarketProduct {
    std::string id;
    std::string capacity_project_id;
    Cents price_per_task{0};
    bool available = true;
    ProductChannel channel = ProductChannel::legacy;
    BasisPoints monthly_migration{10000};
    std::string region_id{};
    std::optional<ProductRetirement> retirement{};
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
    std::string region_id{};
    MilliCUPerDay customer_compute{0};
    std::uint64_t customer_memory_mib = 0;
    std::string customer_interconnect{};
    bool requires_local = false;
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
    Cents customer_advance{0};
    Cents advance_applied{0};
    [[nodiscard]] Cents unearned_advance() const { return checked_unit_subtract(customer_advance, advance_applied); }
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
    [[nodiscard]] Cents customer_charge() const {
        return checked_unit_add(checked_unit_subtract(gross_fee, service_credit), usage_charge);
    }
    auto operator<=>(const ContractDayResult&) const = default;
};
struct MarketDelivery {
    std::string segment_id;
    std::string product_id;
    std::uint64_t tasks = 0;
    Cents charged{0};
    MilliCU customer_compute{0};
    std::uint64_t trial_tasks=0;
    std::string local_copy_project{};
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
struct MarketWaitlistDay {
    std::string product_id, segment_id;
    std::uint64_t admitted_tasks = 0, waiting_tasks = 0;
    std::optional<OrdinalDay> waiting_since{};
    auto operator<=>(const MarketWaitlistDay&) const = default;
};
struct AgentDayResult {
    std::string product_id, segment_id;
    AgentFailures production, trial;
    auto operator<=>(const AgentDayResult&) const = default;
};
struct MarketDay {
    OrdinalDay day{0};
    DayRows<ContractDayResult> contracts;
    DayRows<MarketDelivery> discretionary;
    DayRows<MarketSegmentDay> segments;
    std::uint64_t allocation_rounds = 0;
    std::optional<std::uint64_t> total_demand{};
    DayRows<MarketWaitlistDay> waitlists{};
    DayRows<AgentDayResult> agent_results{};
    void share_unchanged(const MarketDay& previous) {
        contracts.reuse_equal(previous.contracts);
        discretionary.reuse_equal(previous.discretionary);
        segments.reuse_equal(previous.segments);
        waitlists.reuse_equal(previous.waitlists);
        agent_results.reuse_equal(previous.agent_results);
    }
    auto operator<=>(const MarketDay&) const = default;
};
struct MarketHistoryCount {
    std::size_t rows = 0;
    std::size_t waitlist_rows = 0;
    std::size_t agent_rows = 0;
    void add(const MarketDay& day);
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
using MarketAdoptionHistory = PagedHistory<MarketAdoption, 16>;
struct MarketDemandChange {
    std::string id, segment_id;
    OrdinalDay starts_on{0}, expires_before{0};
    BasisPointDelta delta{0};
    BasisPoints shifted_share{0};
    auto operator<=>(const MarketDemandChange&) const = default;
};
struct ProductCapacityBinding {
    std::string product_id, project_id;
    OrdinalDay starts_on{0};
    // Empty is the public reuse grant. A named group receives local-use terms.
    std::string weight_audience{};
    std::optional<AgentPlan> agent_plan{};
    auto operator<=>(const ProductCapacityBinding&) const = default;
};
inline constexpr std::size_t service_trial_limit=256;
inline constexpr std::uint32_t service_trial_days_limit=30;
inline constexpr std::int64_t service_trial_share_limit=5000;
struct MarketTrial {
    std::string product_id, project_id;
    OrdinalDay starts_on{0}, ends_before{0};
    BasisPoints share{0};
    std::optional<OrdinalDay> stopped_on{};
    std::string weight_audience{};
    std::optional<AgentPlan> agent_plan{};
    auto operator<=>(const MarketTrial&) const = default;
};
inline constexpr std::uint32_t service_term_min_days=30;
inline constexpr std::uint32_t service_term_max_days=1096;
inline constexpr std::size_t service_compatibility_limit=4096;
struct ServiceCompatibility {
    std::string product_id, project_id;
    OrdinalDay starts_on{0}, ends_before{0};
    auto operator<=>(const ServiceCompatibility&) const = default;
};
inline constexpr std::size_t market_waitlist_change_limit = 4096;
inline constexpr std::uint32_t waitlist_lookback_days = 7;
struct MarketWaitlistChange {
    std::string product_id;
    OrdinalDay starts_on{0};
    // A zero limit pauses admissions. An absent limit reopens enrollment.
    std::optional<std::uint64_t> new_tasks_per_day{};
    auto operator<=>(const MarketWaitlistChange&) const = default;
};
struct WeightInstallation {
    std::string product_id, segment_id, project_id;
    OrdinalDay received_on{0};
    std::uint64_t tasks_per_day=0;
    auto operator<=>(const WeightInstallation&) const = default;
};
struct MarketPriceChange {
    std::string product_id;
    OrdinalDay starts_on{0};
    Cents price{0};
    auto operator<=>(const MarketPriceChange&) const = default;
};
inline constexpr std::size_t market_price_history_limit=65536;
struct MarketPriceEvent {
    std::string product_id;
    OrdinalDay day{0};
    Cents before{0},after{0};
    std::string actor; // Empty for a direct quote; otherwise the authorizing policy.
    auto operator<=>(const MarketPriceEvent&) const = default;
};
struct MarketState {
    std::string owner = "company.player";
    OrdinalDay starts_on{0};
    std::optional<OrdinalDay> closed_through;
    std::vector<MarketProduct> products;
    std::vector<MarketSegment> segments;
    std::vector<MarketContract> contracts;
    PagedHistory<MarketDay, 4, MarketHistoryCount> days;
    MarketAdoptionHistory adoptions;
    std::vector<MarketDemandChange> demand_changes{};
    std::vector<ProductCapacityBinding> capacity_bindings{};
    std::vector<MarketPriceChange> prices{};
    PagedHistory<MarketPriceEvent,64> price_events{};
    std::vector<MarketTrial> trials{};
    std::vector<ServiceCompatibility> compatibility{};
    std::vector<MarketWaitlistChange> waitlist_changes{};
    // Derived from settled supported work, with no second grant in the save.
    DayRows<WeightInstallation> weight_installations{};
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
struct MarketInfluence {
    std::string product_id, segment_id;
    BasisPoints awareness{0};
    bool eligible = true;
    bool trial_eligible = true;
    // Absent means retained copies inherit the ordinary edge restriction.
    std::optional<std::vector<std::string>> blocked_weight_projects{};
    auto operator<=>(const MarketInfluence&) const = default;
};
using MarketInfluences = std::vector<MarketInfluence>;
[[nodiscard]] MarketState opening_market(OrdinalDay starts_on = OrdinalDay{0},
                                         std::string owner = "company.player");
[[nodiscard]] MarketReport market_report(const MarketState& state);
[[nodiscard]] const MarketTrial* market_trial(const MarketState&,std::string_view product,OrdinalDay);
[[nodiscard]] const ServiceCompatibility* market_compatibility(const MarketState&,std::string_view product,OrdinalDay);
[[nodiscard]] std::optional<std::uint64_t> market_waitlist_limit(const MarketState&,const MarketProduct&,OrdinalDay);
[[nodiscard]] std::uint64_t waitlist_established_tasks(const MarketState&,std::string_view product,OrdinalDay);
void validate_waitlist_change(const MarketState&,const CapacityState&,OrdinalDay,std::string_view product,
                              std::optional<std::uint64_t> limit);
void set_market_waitlist(MarketState&,const CapacityState&,OrdinalDay,std::string_view product,std::optional<std::uint64_t> limit);
[[nodiscard]] ContractDayResult contract_delivery(const MarketContract&,OrdinalDay,std::uint64_t delivered_tasks);
[[nodiscard]] bool regional_market(const MarketState& state);
[[nodiscard]] bool market_access(const MarketProduct& product, const MarketSegment& segment);
[[nodiscard]] const ProductCapacityBinding* market_binding(const MarketState&,std::string_view product,OrdinalDay);
void rebuild_weight_installations(MarketState&);
[[nodiscard]] BasisPoints market_migration(const MarketProduct& product, const MarketSegment& segment);
void validate_market(const MarketState& state);
void validate_market_links(const MarketState& state, const AccountsState& accounts,
                           const CapacityState& capacity);
void add_market_product(MarketState& state, const CapacityState& capacity, MarketProduct product,std::string weight_audience={},
                        std::optional<AgentPlan> agent_plan={});
void replace_product_capacity(MarketState& state, const CapacityState& capacity, OrdinalDay day,
                              std::string_view product_id, std::string project_id,std::string weight_audience={},std::optional<AgentPlan> agent_plan={});
void add_market_segment(MarketState& state, MarketSegment segment);
void add_market_preference(MarketState& state,std::string_view segment_id,MarketPreference preference);
void set_product_available(MarketState& state, std::string_view product_id, bool available);
[[nodiscard]] Cents market_price_on(const MarketState& state, std::string_view product_id, OrdinalDay day);
void set_market_price(MarketState& state, OrdinalDay day, std::string_view product_id, Cents price,std::string actor={});
[[nodiscard]] std::uint64_t effective_market_demand(const MarketState& state, const MarketSegment& segment, OrdinalDay day);
inline constexpr std::int64_t market_shift_share_limit=5000;
[[nodiscard]] const MarketDemandChange* market_workload_shift(const MarketState&,std::string_view segment,OrdinalDay day);
[[nodiscard]] std::uint64_t public_market_demand(const MarketState& state, const MarketSegment& segment, OrdinalDay day);
[[nodiscard]] std::uint64_t maximum_daily_demand(const MarketSegment& segment);
void add_market_demand_change(MarketState& state, OrdinalDay day, MarketDemandChange change);
void accept_contract(MarketState& state, const CapacityState& capacity, OrdinalDay day, MarketContract contract);
void validate_contract_acceptance(const MarketState& state, const CapacityState& capacity,
                                  OrdinalDay day, const MarketContract& contract);
void start_contract(MarketState& state, const CapacityState& capacity, OrdinalDay day, std::string_view contract_id);
void cancel_contract(MarketState& state, AccountsState& accounts, OrdinalDay day, std::string_view contract_id);
[[nodiscard]] std::optional<Invoice> customer_refund(const MarketContract& contract);
// Atomically plan contracts first, allocate residual demand/budgets, close actual
// serving/training work and recognize invoices. Caller closes accounts next.
// validate_io=false is reserved for an already copied enclosing transaction;
// it appends in place and relies on that caller to discard any failed candidate.
struct MarketContractClaim {
    std::string segment_id;
    std::uint64_t promised = 0;
    Cents charged{0};
};
struct MarketOpportunity {
    std::vector<AllocationProduct> products;
    std::vector<AllocationSegment> segments;
    std::vector<MarketContractClaim> contracts;
};
// The same contract-priority/capacity/eligibility calculation is used for a
// single company and for shared multi-company demand. It never closes work.
[[nodiscard]] MarketOpportunity market_opportunity(const MarketState& state, const CapacityState& capacity, OrdinalDay day,
    const std::vector<MarketAdoption>* industry_adoptions = nullptr, const MarketInfluences& influences = {});
[[nodiscard]] CapacityDay preview_market_work(const MarketState& state, const CapacityState& capacity, OrdinalDay day,
    const AllocationResult& assigned, const std::vector<MarketAdoption>& industry_adoptions, const MarketInfluences& influences = {});
// An industry allocation is checked against this company's actual opportunity;
// other providers count as outside in its own report. The world coordinator
// additionally conserves the single customer demand/budget across all sellers.
void close_market_day(MarketState& state, AccountsState& accounts,
                      CapacityState& capacity, OrdinalDay day, bool validate_io = true,
                      const AllocationResult* assigned = nullptr,
                      const std::vector<MarketAdoption>* industry_adoptions = nullptr,
                      std::span<const WholesaleUse> wholesale = {}, const MarketInfluences& influences = {});
} // namespace pgp::core
