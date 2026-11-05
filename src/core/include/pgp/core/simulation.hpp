#pragma once

#include <pgp/core/calendar.hpp>
#include <pgp/core/accounts.hpp>
#include <pgp/core/capacity.hpp>
#include <pgp/core/market.hpp>
#include <pgp/core/liquidity.hpp>
#include <pgp/core/lab.hpp>
#include <pgp/core/deployment.hpp>
#include <pgp/core/business_offers.hpp>
#include <pgp/core/random.hpp>
#include <pgp/core/units.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pgp::core {

// Reservations are commitments; consumption recognizes an expense in scenarios
// with accounts. Forecasts never receive hidden random state.
enum class DayPhase { opening, awaiting_choice, work_ready, finished, closed };
enum class ReservationStatus { active, completed, cancelled };
enum class CommandReason {
    accepted, stale_revision, invalid_argument, insufficient_funds,
    missing_reservation, invalid_status, irreversible_commitment, dependent_commitment,
    morning_not_ready, no_pending_choice, wrong_choice, limit_exceeded,
    arithmetic_overflow, campaign_finished
};
[[nodiscard]] std::string_view to_string(DayPhase value) noexcept;
[[nodiscard]] std::string_view to_string(ReservationStatus value) noexcept;
[[nodiscard]] std::string_view to_string(CommandReason value) noexcept;

struct ReservationRow {
    std::uint64_t id;
    std::string name;
    Cents reserved;
    Cents consumed;
    bool reversible;
    ReservationStatus status;
    auto operator<=>(const ReservationRow&) const = default;
};

// Due work is already authorized by an owning system. A choice can accept the
// named consumption or decline it; neither path refunds prior consumption.
struct OpeningAction {
    std::uint64_t id;
    OrdinalDay due;
    std::uint64_t reservation_id;
    Cents amount;
    bool requires_choice;
    std::string cause;
    auto operator<=>(const OpeningAction&) const = default;
};
struct TransactionReceipt {
    std::uint64_t id;
    std::uint64_t revision;
    OrdinalDay day;
    std::string cause;
    std::uint64_t reservation_id;
    Cents cash_delta;
    auto operator<=>(const TransactionReceipt&) const = default;
};

inline constexpr std::size_t simulation_reservation_limit = 4096;
inline constexpr std::size_t simulation_opening_limit = 4096;
inline constexpr std::size_t simulation_receipt_limit = 65536;

// Copyable persistence boundary; no serializer is introduced by this task.
// opening_cursor is the first unhandled action in (due,id) order. A suspended
// choice remains at that cursor. Restoring prepared state never prepares twice.
struct SimulationState {
    std::uint64_t revision = 0;
    OrdinalDay date{0};
    OrdinalDay end_date{36524};
    DayPhase phase = DayPhase::opening;
    bool paused = true;
    Cents cash{0};
    std::uint64_t next_reservation_id = 1;
    std::uint64_t next_receipt_id = 1;
    std::vector<ReservationRow> reservations;
    std::vector<OpeningAction> opening_actions;
    std::size_t opening_cursor = 0;
    std::optional<std::uint64_t> pending_choice;
    std::vector<TransactionReceipt> receipts;
    RandomSnapshot random = RandomStreams{0}.snapshot();
    // nullopt preserves legacy engineering fixtures without inventing history.
    // Business scenarios reconcile the cash projection against their ledger.
    std::optional<AccountsState> accounts{};
    std::optional<CapacityState> capacity{};
    std::optional<MarketState> market{};
    std::optional<LiquidityState> liquidity{};
    std::optional<LabState> lab{};
    std::optional<BusinessOffersState> business_offers{};
    std::optional<DeploymentState> deployments{};
    auto operator<=>(const SimulationState&) const = default;
};

struct SimulationSnapshot {
    std::uint64_t revision;
    OrdinalDay date;
    DayPhase phase;
    bool paused;
    Cents cash;
    Cents available;
    std::optional<std::uint64_t> pending_choice;
    std::vector<ReservationRow> reservations;
    std::optional<AccountsReport> accounts{};
    std::optional<CapacityReport> capacity{};
    std::optional<MarketReport> market{};
    std::optional<LiquidityReport> liquidity{};
    std::optional<LabReport> lab{};
    std::optional<BusinessOffersState> business_offers{};
};

struct ReserveFunds { std::string name; Cents amount; bool reversible = true; };
struct ConsumeReservation { std::uint64_t id; Cents amount; };
struct CancelReservation { std::uint64_t id; };
struct AdvanceDay {};
struct SetPaused { bool paused; };
struct ResolveOpeningChoice { std::uint64_t id; bool accept; };
struct AddRecurringObligation { RecurringObligation obligation; };
struct IssueInvoice { Invoice invoice; };
struct SettleInvoice { std::string id; Cents amount; std::string key; };
struct DrawDebt { Debt debt; std::string cause; };
struct RepayDebt { std::string id; Cents amount; std::string key; };
struct InvestEquity { std::string investor; Cents proceeds; BasisPoints acquired; std::string key; };
struct ReserveProject { CapacityProject project; };
struct CancelProject { std::string id; };
struct SetEffectiveCapacity { std::string source_id; MilliCUPerDay available; };
struct EnableMarket {};
struct AddMarketProduct { MarketProduct product; };
struct AddMarketSegment { MarketSegment segment; };
struct AddMarketPreference { std::string segment_id; MarketPreference preference; };
struct SetMarketProductAvailable { std::string id; bool available; };
struct AcceptContract { MarketContract contract; };
struct StartContract { std::string id; };
struct CancelContract { std::string id; };
struct AcknowledgeLiquidityWarning { std::uint64_t id; };
struct BuyData { std::string offer_id; };
struct StartResearch { std::string research_id; LabAllocation allocation; };
struct StartTraining { TrainingRequest request; };
struct CloneModel { std::string source_id, id; };
struct StartEvaluation { std::string id, model_id, plan_id; LabAllocation allocation; };
struct AcceptBusinessOffer { std::string id; };
struct AddMarketDemandChange { MarketDemandChange change; };
using CommandAction = std::variant<ReserveFunds, ConsumeReservation, CancelReservation,
                                   AdvanceDay, SetPaused, ResolveOpeningChoice,
                                   AddRecurringObligation, IssueInvoice, SettleInvoice,
                                   DrawDebt, RepayDebt, InvestEquity,
                                   ReserveProject, CancelProject, SetEffectiveCapacity,
                                   EnableMarket, AddMarketProduct, AddMarketSegment, AddMarketPreference,
                                   SetMarketProductAvailable, AcceptContract, StartContract, CancelContract,
                                   AcknowledgeLiquidityWarning, BuyData, StartResearch, StartTraining, CloneModel, StartEvaluation,
                                   AcceptBusinessOffer, AddMarketDemandChange, DeployModel>;
struct Command { std::uint64_t expected_revision; CommandAction action; };
struct CommandResult {
    CommandReason reason;
    std::uint64_t revision;
    std::optional<std::uint64_t> receipt_id;
    std::optional<std::uint64_t> reservation_id;
    std::string explanation;
    [[nodiscard]] bool accepted() const noexcept { return reason == CommandReason::accepted; }
};

struct TrustedSimulationState {};
void validate_simulation(const SimulationState& state);

class Simulation {
public:
    explicit Simulation(SimulationState initial);
    Simulation(SimulationState initial, TrustedSimulationState) noexcept : state_{std::move(initial)} {}
    explicit Simulation(Cents cash);
    [[nodiscard]] CommandResult submit(const Command& command);
    // The caller already owns a copied transaction candidate and MUST discard
    // this Simulation on refusal. Avoid a second full-history copy in that case.
    [[nodiscard]] CommandResult submit_in_transaction(const Command& command);
    [[nodiscard]] SimulationState release_checkpoint() && noexcept { return std::move(state_); }
    [[nodiscard]] std::shared_ptr<const SimulationSnapshot> snapshot() const;
    [[nodiscard]] const SimulationState& checkpoint() const noexcept { return state_; }
private:
    [[nodiscard]] CommandResult submit_impl(const Command& command,bool outer_transaction);
    SimulationState state_;
};

} // namespace pgp::core
