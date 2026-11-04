#include <pgp/core/simulation.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pgp::core {
namespace {
struct Refusal {
    CommandReason reason;
    std::string explanation;
};
[[noreturn]] void refuse(CommandReason reason, std::string explanation) {
    throw Refusal{reason, std::move(explanation)};
}

std::uint64_t take_id(std::uint64_t& next) {
    if (next == 0 || next == std::numeric_limits<std::uint64_t>::max()) {
        refuse(CommandReason::limit_exceeded, "The stable identifier range is exhausted.");
    }
    return next++;
}
Cents available(const SimulationState& state) {
    Cents reserved{0};
    for (const auto& row : state.reservations) {
        reserved = checked_unit_add(reserved, row.reserved);
    }
    return checked_unit_subtract(state.cash, reserved);
}
bool valid_text(std::string_view text) {
    return !text.empty() && text.size() <= 256 &&
        std::all_of(text.begin(), text.end(), [](unsigned char character) {
            return character >= 32 && character != 127;
        });
}
ReservationRow& reservation(SimulationState& state, std::uint64_t id) {
    const auto found = std::find_if(state.reservations.begin(), state.reservations.end(),
        [id](const auto& row) { return row.id == id; });
    if (found == state.reservations.end()) {
        refuse(CommandReason::missing_reservation, "This reservation no longer exists.");
    }
    return *found;
}
std::uint64_t receipt(SimulationState& state, std::string cause,
                      std::uint64_t reservation_id = 0, Cents cash_delta = Cents{0}) {
    if (state.receipts.size() >= simulation_receipt_limit) {
        refuse(CommandReason::limit_exceeded,
               "The transaction receipt limit was reached; no required records were discarded.");
    }
    const auto id = take_id(state.next_receipt_id);
    state.receipts.push_back({id, state.revision, state.date, std::move(cause),
                              reservation_id, cash_delta});
    return id;
}
Cents future_commitment(const SimulationState& state, std::uint64_t id) {
    Cents total{0};
    for (std::size_t index = state.opening_cursor; index < state.opening_actions.size(); ++index) {
        const auto& action = state.opening_actions[index];
        if (action.reservation_id == id) {
            total = checked_unit_add(total, action.amount);
        }
    }
    return total;
}
void consume(SimulationState& state, std::uint64_t id, Cents amount) {
    if (amount.value() <= 0) {
        refuse(CommandReason::invalid_argument, "Consumption must be a positive amount of cents.");
    }
    auto& row = reservation(state, id);
    if (row.status != ReservationStatus::active) {
        refuse(CommandReason::invalid_status, "This reservation is no longer active.");
    }
    if (amount > row.reserved) {
        refuse(CommandReason::insufficient_funds,
               "Consumption exceeds the unconsumed reserved amount.");
    }
    state.cash = checked_unit_subtract(state.cash, amount);
    row.reserved = checked_unit_subtract(row.reserved, amount);
    row.consumed = checked_unit_add(row.consumed, amount);
    if (row.reserved == Cents{0}) {
        row.status = ReservationStatus::completed;
    }
}
void prepare_opening(SimulationState& state) {
    if (state.phase != DayPhase::opening) {
        return;
    }
    while (state.opening_cursor < state.opening_actions.size()) {
        const auto& due = state.opening_actions[state.opening_cursor];
        if (due.due > state.date) {
            break;
        }
        if (due.requires_choice) {
            state.phase = DayPhase::awaiting_choice;
            state.pending_choice = due.id;
            state.paused = true;
            return;
        }
        consume(state, due.reservation_id, due.amount);
        (void)receipt(state, due.cause, due.reservation_id, Cents{-due.amount.value()});
        ++state.opening_cursor;
    }
    state.phase = DayPhase::work_ready;
}

void validate(const SimulationState& state) {
    const auto invalid = [](const char* message) { throw std::invalid_argument{message}; };
    (void)from_ordinal(state.date);
    (void)from_ordinal(state.end_date);
    if (state.date > state.end_date || state.cash.value() < 0) {
        invalid("Invalid campaign dates or negative unfinanced cash");
    }
    if (state.reservations.size() > simulation_reservation_limit ||
        state.opening_actions.size() > simulation_opening_limit ||
        state.receipts.size() > simulation_receipt_limit ||
        state.opening_cursor > state.opening_actions.size() ||
        state.next_reservation_id == 0 || state.next_receipt_id == 0) {
        invalid("Simulation state exceeds a deterministic count or identifier bound");
    }
    std::vector<std::uint64_t> ids;
    for (const auto& row : state.reservations) {
        if (row.id == 0 || row.id >= state.next_reservation_id || !valid_text(row.name) ||
            row.reserved.value() < 0 || row.consumed.value() < 0 ||
            (row.status == ReservationStatus::active && row.reserved.value() == 0) ||
            (row.status != ReservationStatus::active && row.reserved.value() != 0)) {
            invalid("Invalid reservation identity, units, name or status");
        }
        switch (row.status) {
        case ReservationStatus::active: case ReservationStatus::completed:
        case ReservationStatus::cancelled: break;
        default: invalid("Unknown reservation status");
        }
        (void)checked_unit_add(row.reserved, row.consumed);
        ids.push_back(row.id);
    }
    if (!std::is_sorted(ids.begin(), ids.end()) ||
        std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
        invalid("Reservation IDs must have a unique canonical order");
    }
    if (available(state).value() < 0) {
        invalid("Reservations exceed available cash");
    }
    ids.clear();
    std::optional<std::pair<OrdinalDay, std::uint64_t>> previous;
    for (std::size_t index = 0; index < state.opening_actions.size(); ++index) {
        const auto& action = state.opening_actions[index];
        const auto key = std::pair{action.due, action.id};
        if (action.id == 0 || action.amount.value() <= 0 || !valid_text(action.cause) ||
            (previous && *previous >= key) || action.due > state.end_date ||
            (index < state.opening_cursor && action.due > state.date) ||
            (index >= state.opening_cursor && action.due < state.date) ||
            !std::binary_search(state.reservations.begin(), state.reservations.end(),
                ReservationRow{action.reservation_id, "", Cents{0}, Cents{0}, false,
                               ReservationStatus::active},
                [](const auto& left, const auto& right) { return left.id < right.id; })) {
            invalid("Invalid opening action, reservation reference, date or canonical order");
        }
        (void)from_ordinal(action.due);
        previous = key;
        ids.push_back(action.id);
    }
    std::sort(ids.begin(), ids.end());
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
        invalid("Duplicate opening action ID");
    }
    for (const auto& row : state.reservations) {
        if (future_commitment(state, row.id) > row.reserved) {
            invalid("Future opening commitments exceed their unconsumed reservation");
        }
    }
    const bool due = state.opening_cursor < state.opening_actions.size() &&
                     state.opening_actions[state.opening_cursor].due <= state.date;
    switch (state.phase) {
    case DayPhase::opening:
        if (state.pending_choice || state.date == state.end_date) {
            invalid("An unprepared morning cannot have a suspended choice or an ending date");
        }
        break;
    case DayPhase::awaiting_choice:
        if (!state.paused || !due || !state.pending_choice ||
            !state.opening_actions[state.opening_cursor].requires_choice ||
            *state.pending_choice != state.opening_actions[state.opening_cursor].id) {
            invalid("Suspended opening choice does not match its saved cursor");
        }
        break;
    case DayPhase::work_ready:
        if (state.pending_choice || due || state.date == state.end_date) {
            invalid("A prepared morning still has unfinished opening work");
        }
        break;
    case DayPhase::finished:
        if (state.date != state.end_date || state.pending_choice || !state.paused) {
            invalid("Invalid final campaign cursor");
        }
        break;
    default: invalid("Unknown daily phase");
    }
    std::uint64_t prior_receipt = 0;
    for (const auto& item : state.receipts) {
        if (item.id <= prior_receipt || item.id >= state.next_receipt_id ||
            item.revision > state.revision || item.day > state.date || !valid_text(item.cause) ||
            (item.reservation_id != 0 && !std::any_of(state.reservations.begin(),
                state.reservations.end(), [&item](const auto& row) {
                    return row.id == item.reservation_id;
                }))) {
            invalid("Invalid transaction receipt identity, cause or reference");
        }
        (void)from_ordinal(item.day);
        prior_receipt = item.id;
    }
}
} // namespace

std::string_view to_string(DayPhase value) noexcept {
    switch (value) {
    case DayPhase::opening: return "opening";
    case DayPhase::awaiting_choice: return "awaiting_choice";
    case DayPhase::work_ready: return "work_ready";
    case DayPhase::finished: return "finished";
    }
    return "invalid";
}
std::string_view to_string(ReservationStatus value) noexcept {
    switch (value) {
    case ReservationStatus::active: return "active";
    case ReservationStatus::completed: return "completed";
    case ReservationStatus::cancelled: return "cancelled";
    }
    return "invalid";
}
std::string_view to_string(CommandReason value) noexcept {
    switch (value) {
    case CommandReason::accepted: return "accepted";
    case CommandReason::stale_revision: return "stale_revision";
    case CommandReason::invalid_argument: return "invalid_argument";
    case CommandReason::insufficient_funds: return "insufficient_funds";
    case CommandReason::missing_reservation: return "missing_reservation";
    case CommandReason::invalid_status: return "invalid_status";
    case CommandReason::irreversible_commitment: return "irreversible_commitment";
    case CommandReason::dependent_commitment: return "dependent_commitment";
    case CommandReason::morning_not_ready: return "morning_not_ready";
    case CommandReason::no_pending_choice: return "no_pending_choice";
    case CommandReason::wrong_choice: return "wrong_choice";
    case CommandReason::limit_exceeded: return "limit_exceeded";
    case CommandReason::arithmetic_overflow: return "arithmetic_overflow";
    case CommandReason::campaign_finished: return "campaign_finished";
    }
    return "invalid";
}

Simulation::Simulation(Cents cash) : Simulation{[cash] {
    SimulationState state;
    state.cash = cash;
    return state;
}()} {}
Simulation::Simulation(SimulationState initial) : state_{std::move(initial)} {
    validate(state_);
    try {
        prepare_opening(state_);
    } catch (const Refusal& failure) {
        throw std::invalid_argument{failure.explanation};
    }
    validate(state_);
}

CommandResult Simulation::submit(const Command& command) {
    if (command.expected_revision != state_.revision) {
        return {CommandReason::stale_revision, state_.revision, {}, {},
                "The company changed after this action was prepared. Review the current state."};
    }
    try {
        if (state_.revision == std::numeric_limits<std::uint64_t>::max()) {
            refuse(CommandReason::arithmetic_overflow, "The state revision range is exhausted.");
        }
        if (state_.phase == DayPhase::finished) {
            refuse(CommandReason::campaign_finished, "The campaign has reached its final date.");
        }
        auto next = state_;
        ++next.revision;
        std::optional<std::uint64_t> new_reservation;
        const auto command_receipt = next.next_receipt_id;
        std::visit([&](const auto& action) {
            using Action = std::decay_t<decltype(action)>;
            if constexpr (!std::is_same_v<Action, ResolveOpeningChoice> &&
                          !std::is_same_v<Action, SetPaused>) {
                if (next.phase != DayPhase::work_ready) {
                    refuse(CommandReason::morning_not_ready,
                           "Resolve the required opening choice before planning or advancing work.");
                }
            }
            if constexpr (std::is_same_v<Action, ReserveFunds>) {
                if (!valid_text(action.name) || action.amount.value() <= 0) {
                    refuse(CommandReason::invalid_argument,
                           "A reservation needs a short name and a positive amount of cents.");
                }
                if (next.reservations.size() >= simulation_reservation_limit) {
                    refuse(CommandReason::limit_exceeded, "The reservation count limit was reached.");
                }
                if (action.amount > available(next)) {
                    refuse(CommandReason::insufficient_funds,
                           "Available cash is already reserved for other commitments.");
                }
                const auto id = take_id(next.next_reservation_id);
                next.reservations.push_back({id, action.name, action.amount, Cents{0},
                                             action.reversible, ReservationStatus::active});
                new_reservation = id;
                (void)receipt(next, "reserve_funds", id);
            } else if constexpr (std::is_same_v<Action, ConsumeReservation>) {
                const auto& row = reservation(next, action.id);
                const auto committed = future_commitment(next, action.id);
                if (committed.value() > 0 && action.amount.value() > 0 &&
                    action.amount <= row.reserved &&
                    action.amount > checked_unit_subtract(row.reserved, committed)) {
                    refuse(CommandReason::dependent_commitment,
                           "These funds are reserved for pending opening work and cannot be spent twice.");
                }
                consume(next, action.id, action.amount);
                (void)receipt(next, "consume_reservation", action.id, Cents{-action.amount.value()});
            } else if constexpr (std::is_same_v<Action, CancelReservation>) {
                auto& row = reservation(next, action.id);
                if (row.status != ReservationStatus::active) {
                    refuse(CommandReason::invalid_status, "This reservation is no longer active.");
                }
                if (!row.reversible) {
                    refuse(CommandReason::irreversible_commitment,
                           "The accepted terms are irreversible; cancellation cannot refund this commitment.");
                }
                if (future_commitment(next, action.id).value() != 0) {
                    refuse(CommandReason::dependent_commitment,
                           "Pending opening work depends on this reservation; resolve it before cancellation.");
                }
                row.reserved = Cents{0};
                row.status = ReservationStatus::cancelled;
                (void)receipt(next, "cancel_unconsumed_reservation", action.id);
            } else if constexpr (std::is_same_v<Action, AdvanceDay>) {
                (void)receipt(next, "advance_day");
                next.date = advance_days(next.date, Days{1});
                if (next.date == next.end_date) {
                    next.phase = DayPhase::finished;
                    next.paused = true;
                } else {
                    next.phase = DayPhase::opening;
                    prepare_opening(next);
                }
            } else if constexpr (std::is_same_v<Action, SetPaused>) {
                if (!action.paused && next.phase != DayPhase::work_ready) {
                    refuse(CommandReason::morning_not_ready,
                           "Time remains paused until the required opening choice is resolved.");
                }
                next.paused = action.paused;
                (void)receipt(next, action.paused ? "pause" : "resume");
            } else if constexpr (std::is_same_v<Action, ResolveOpeningChoice>) {
                if (next.phase != DayPhase::awaiting_choice || !next.pending_choice) {
                    refuse(CommandReason::no_pending_choice, "There is no opening choice to resolve.");
                }
                if (action.id != *next.pending_choice) {
                    refuse(CommandReason::wrong_choice, "The opening choice identity has changed.");
                }
                (void)receipt(next, action.accept ? "accept_opening_choice" : "decline_opening_choice");
                const auto& due = next.opening_actions[next.opening_cursor];
                if (action.accept) {
                    consume(next, due.reservation_id, due.amount);
                    (void)receipt(next, due.cause, due.reservation_id, Cents{-due.amount.value()});
                }
                ++next.opening_cursor;
                next.pending_choice.reset();
                next.phase = DayPhase::opening;
                prepare_opening(next);
            }
        }, command.action);
        validate(next);
        state_ = std::move(next);
        return {CommandReason::accepted, state_.revision, command_receipt, new_reservation,
                "The action was committed."};
    } catch (const Refusal& failure) {
        return {failure.reason, state_.revision, {}, {}, failure.explanation};
    } catch (const std::overflow_error&) {
        return {CommandReason::arithmetic_overflow, state_.revision, {}, {},
                "The action exceeds a checked arithmetic bound; nothing was changed."};
    } catch (const std::invalid_argument&) {
        return {CommandReason::invalid_argument, state_.revision, {}, {},
                "The action would violate a state invariant; nothing was changed."};
    } catch (const std::out_of_range&) {
        return {CommandReason::invalid_argument, state_.revision, {}, {},
                "The action exceeds a unit or calendar bound; nothing was changed."};
    }
}

std::shared_ptr<const SimulationSnapshot> Simulation::snapshot() const {
    return std::make_shared<const SimulationSnapshot>(SimulationSnapshot{
        state_.revision, state_.date, state_.phase, state_.paused, state_.cash,
        available(state_), state_.pending_choice, state_.reservations});
}
} // namespace pgp::core
