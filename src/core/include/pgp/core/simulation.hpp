#pragma once

#include <pgp/core/calendar.hpp>
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

// Foundation commands reserve cash, not recognized accounting expenses. M2 adds
// domain ledgers. This module never supplies forecasts with hidden random state.
enum class DayPhase { opening, awaiting_choice, work_ready, finished };
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
};

struct ReserveFunds { std::string name; Cents amount; bool reversible = true; };
struct ConsumeReservation { std::uint64_t id; Cents amount; };
struct CancelReservation { std::uint64_t id; };
struct AdvanceDay {};
struct SetPaused { bool paused; };
struct ResolveOpeningChoice { std::uint64_t id; bool accept; };
using CommandAction = std::variant<ReserveFunds, ConsumeReservation, CancelReservation,
                                   AdvanceDay, SetPaused, ResolveOpeningChoice>;
struct Command { std::uint64_t expected_revision; CommandAction action; };
struct CommandResult {
    CommandReason reason;
    std::uint64_t revision;
    std::optional<std::uint64_t> receipt_id;
    std::optional<std::uint64_t> reservation_id;
    std::string explanation;
    [[nodiscard]] bool accepted() const noexcept { return reason == CommandReason::accepted; }
};

class Simulation {
public:
    explicit Simulation(SimulationState initial);
    explicit Simulation(Cents cash);
    [[nodiscard]] CommandResult submit(const Command& command);
    [[nodiscard]] std::shared_ptr<const SimulationSnapshot> snapshot() const;
    [[nodiscard]] SimulationState checkpoint() const { return state_; }
private:
    SimulationState state_;
};

} // namespace pgp::core
