#pragma once

#include <pgp/core/accounts.hpp>

namespace pgp::core {
enum class LiquidityStatus { operating, workout, closed };
enum class LiquidityCauseKind { payable, matured_debt };
struct LiquidityCause {
    LiquidityCauseKind kind = LiquidityCauseKind::payable;
    std::string reference;
    OrdinalDay due{0};
    Cents unpaid{0};
    std::string cause;
    auto operator<=>(const LiquidityCause&) const = default;
};
struct LiquidityExposure {
    Cents unpaid_payables{0};
    Cents matured_principal{0};
    std::optional<LiquidityCause> oldest;
    auto operator<=>(const LiquidityExposure&) const = default;
};
struct LiquidityWorkout {
    std::uint64_t id = 0;
    OrdinalDay warned_on{0};
    OrdinalDay deadline{0};
    Cents initial_shortfall{0};
    LiquidityCause trigger;
    bool acknowledged = false;
    std::optional<OrdinalDay> resolved_on;
    std::optional<OrdinalDay> closed_on;
    auto operator<=>(const LiquidityWorkout&) const = default;
};
inline constexpr std::size_t liquidity_workout_limit = 4096;
inline constexpr std::uint32_t liquidity_workout_days = 30;
struct LiquidityState {
    std::string owner = "company.player";
    OrdinalDay starts_on{0};
    OrdinalDay observed_on{0};
    std::optional<OrdinalDay> checked_through;
    LiquidityStatus status = LiquidityStatus::operating;
    std::uint64_t next_workout_id = 1;
    LiquidityExposure exposure;
    std::vector<LiquidityWorkout> workouts;
    auto operator<=>(const LiquidityState&) const = default;
};
struct LiquidityReport {
    LiquidityStatus status = LiquidityStatus::operating;
    Cents unpaid_payables{0};
    Cents matured_principal{0};
    std::optional<std::uint64_t> pending_warning;
    std::optional<OrdinalDay> deadline;
    std::optional<LiquidityCause> oldest;
    auto operator<=>(const LiquidityReport&) const = default;
};
[[nodiscard]] LiquidityState opening_liquidity(OrdinalDay starts_on = OrdinalDay{0},
                                              std::string owner = "company.player");
[[nodiscard]] LiquidityExposure liquidity_exposure(const AccountsState& accounts,
                                                   std::optional<OrdinalDay> due_through);
[[nodiscard]] LiquidityReport liquidity_report(const LiquidityState& state);
void validate_liquidity(const LiquidityState& state);
void validate_liquidity_links(const LiquidityState& state, const AccountsState& accounts,
                              bool validate_accounts_state = true);
// After closing one accounts day, observe the next morning. The deadline
// morning remains available for rescue commands, but cannot execute more work.
void observe_liquidity(LiquidityState& state, const AccountsState& accounts, OrdinalDay observation_day,
                       bool validate_io = true);
// Recheck already-due obligations after a command, without moving the daily
// cursor or treating today's unsettled invoices as a missed payment.
void refresh_liquidity(LiquidityState& state, const AccountsState& accounts);
// At the deadline morning, close an unresolved workout before any further
// capacity, market or accounting activity. The ledger and date do not change.
void close_liquidity_workout(LiquidityState& state, const AccountsState& accounts);
void acknowledge_liquidity_warning(LiquidityState& state, std::uint64_t workout_id);
} // namespace pgp::core
