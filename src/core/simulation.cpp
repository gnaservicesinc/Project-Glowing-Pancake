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
Cents reserved_cash(const SimulationState& state) {
    Cents reserved{0};
    for (const auto& row : state.reservations) {
        reserved = checked_unit_add(reserved, row.reserved);
    }
    return reserved;
}
Cents available(const SimulationState& state) {
    return checked_unit_subtract(state.cash, reserved_cash(state));
}
AccountsState& accounts(SimulationState& state) {
    if (!state.accounts) refuse(CommandReason::invalid_status, "This engineering fixture has no business accounts.");
    return *state.accounts;
}
CapacityState& capacity(SimulationState& state) {
    if (!state.capacity) refuse(CommandReason::invalid_status, "This checkpoint has no capacity sources.");
    return *state.capacity;
}
MarketState& market(SimulationState& state) {
    if (!state.market) refuse(CommandReason::invalid_status, "This checkpoint has no customer market.");
    return *state.market;
}
LabState& lab(SimulationState& state) {
    if (!state.lab) refuse(CommandReason::invalid_status, "This scenario has no lab catalog.");
    return *state.lab;
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
    if (state.accounts) record_cash(*state.accounts, state.date, Cents{-amount.value()}, Account::expenses,
        row.name, "reservation.consume:" + std::to_string(state.next_receipt_id));
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
    if (state.business_offers) expire_business_offers(*state.business_offers,*state.capacity,state.date);
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

void validate(const SimulationState& state,bool deep=true) {
    const auto invalid = [](const char* message) { throw std::invalid_argument{message}; };
    (void)from_ordinal(state.date);
    (void)from_ordinal(state.end_date);
    if (state.date > state.end_date || state.cash.value() < 0) {
        invalid("Invalid campaign dates or negative unfinanced cash");
    }
    if (state.accounts) {
        if (deep && !state.market && !state.liquidity) validate_accounts(*state.accounts);
        const auto& ledger = *state.accounts;
        if (accounts_report(ledger).cash != state.cash || ledger.starts_on > state.date ||
            (ledger.closed_through ? advance_days(*ledger.closed_through, Days{1}) != state.date : ledger.starts_on != state.date) ||
            (deep && std::any_of(ledger.postings.begin(), ledger.postings.end(), [&](const auto& entry) { return entry.day > state.date; })))
            invalid("Business accounts do not reconcile with the saved cash or daily cursor");
    }
    if (state.capacity) {
        if (deep && !state.market) validate_capacity(*state.capacity);
        const auto& pool = *state.capacity;
        if (!state.accounts || pool.owner != state.accounts->owner || pool.starts_on != state.accounts->starts_on ||
            pool.closed_through != state.accounts->closed_through)
            invalid("Capacity and accounts do not share the same owner and daily cursor");
        for (const auto& project : pool.projects) {
            if (project.monthly_support_cost == Cents{0}) continue;
            const auto term = std::find_if(state.accounts->recurring.begin(), state.accounts->recurring.end(),
                [&](const auto& obligation) { return obligation.id == "support/" + project.id; });
            if (term == state.accounts->recurring.end() || term->monthly_amount != project.monthly_support_cost ||
                term->starts_on != project.starts_on || term->ends_before != project.stopped_on)
                invalid("Serving support terms differ from their owning capacity commitment");
        }
    }
    if (state.market) {
        if (!state.accounts || !state.capacity) invalid("A market requires accounts and shared capacity");
        if (deep) validate_market_links(*state.market, *state.accounts, *state.capacity);
        else if (state.market->owner!=state.accounts->owner || state.market->closed_through!=state.accounts->closed_through)
            invalid("Market settlement differs from the owned account cursor");
    }
    if (state.lab) {
        if (!state.accounts || !state.capacity) invalid("Lab requires business accounts and shared capacity");
        validate_lab(*state.lab, *state.capacity, *state.accounts, state.random, false);
    }
    if (state.business_offers) {
        if (!state.accounts || !state.capacity) invalid("Business offers require accounts and capacity");
        validate_business_offers(*state.business_offers,*state.accounts,*state.capacity,state.market,false);
    }
    if (state.liquidity) {
        if (!state.accounts) invalid("Liquidity requires owned business accounts");
        validate_liquidity_links(*state.liquidity, *state.accounts, deep && !state.market);
        if ((state.liquidity->status == LiquidityStatus::closed) != (state.phase == DayPhase::closed) ||
            (liquidity_report(*state.liquidity).pending_warning && !state.paused))
            invalid("Liquidity closure or required warning differs from the daily phase");
    } else if (state.phase == DayPhase::closed) invalid("Closed company has no recorded liquidity cause");
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
    case DayPhase::closed:
        if (state.pending_choice || !state.paused) invalid("A closed company must retain its paused final state");
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
    case DayPhase::closed: return "closed";
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

void validate_simulation(const SimulationState& state) {
    validate(state);
    if (state.lab) validate_lab(*state.lab, *state.capacity, *state.accounts, state.random);
    if (state.business_offers) validate_business_offers(*state.business_offers,*state.accounts,*state.capacity,state.market);
}

Simulation::Simulation(Cents cash) : Simulation{[cash] {
    SimulationState state;
    state.cash = cash;
    return state;
}()} {}
Simulation::Simulation(SimulationState initial) : state_{std::move(initial)} {
    validate_simulation(state_);
    if (state_.phase == DayPhase::opening) {
        try {
            prepare_opening(state_);
        } catch (const Refusal& failure) {
            throw std::invalid_argument{failure.explanation};
        }
        validate(state_);
    }
}

CommandResult Simulation::submit(const Command& command) {
    return submit_impl(command,false);
}
CommandResult Simulation::submit_in_transaction(const Command& command) {
    return submit_impl(command,true);
}
CommandResult Simulation::submit_impl(const Command& command,bool outer_transaction) {
    if (command.expected_revision != state_.revision) {
        return {CommandReason::stale_revision, state_.revision, {}, {},
                "The company changed after this action was prepared. Review the current state."};
    }
    try {
        if (state_.revision == std::numeric_limits<std::uint64_t>::max()) {
            refuse(CommandReason::arithmetic_overflow, "The state revision range is exhausted.");
        }
        if (state_.phase == DayPhase::finished || state_.phase == DayPhase::closed) {
            refuse(CommandReason::campaign_finished, "The campaign ended; its final accounts remain available for inspection.");
        }
        auto next = outer_transaction?std::move(state_):state_;
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
                if (next.liquidity && liquidity_report(*next.liquidity).pending_warning)
                    refuse(CommandReason::morning_not_ready, "Acknowledge the insolvency warning and its 30-day response window before advancing.");
                if (next.liquidity && next.liquidity->status == LiquidityStatus::workout &&
                    next.date >= *liquidity_report(*next.liquidity).deadline) {
                    close_liquidity_workout(*next.liquidity,*next.accounts);
                    next.phase = DayPhase::closed;
                    next.paused = true;
                    (void)receipt(next,"insolvency_closure");
                    return; // The response window ended before today's work.
                }
                const auto cash_before = next.cash;
                if (next.market)
                    close_market_day(*next.market, accounts(next), capacity(next), next.date, false);
                else if (next.capacity) close_capacity_day(*next.capacity, next.date, false);
                if (next.lab) close_lab_day(*next.lab, *next.capacity, next.date, next.random);
                if (next.accounts) {
                    close_accounts_day(*next.accounts, next.date, reserved_cash(next), false);
                    next.cash = accounts_report(*next.accounts).cash;
                }
                (void)receipt(next, "advance_day", 0, checked_unit_subtract(next.cash, cash_before));
                next.date = advance_days(next.date, Days{1});
                if (next.liquidity) {
                    observe_liquidity(*next.liquidity, *next.accounts, next.date, false);
                    const auto report = liquidity_report(*next.liquidity);
                    if (report.pending_warning || report.status == LiquidityStatus::closed) next.paused = true;
                }
                if (next.liquidity && next.liquidity->status == LiquidityStatus::closed) {
                    next.phase = DayPhase::closed;
                } else if (next.date == next.end_date) {
                    next.phase = DayPhase::finished;
                    next.paused = true;
                } else {
                    next.phase = DayPhase::opening;
                    prepare_opening(next);
                }
            } else if constexpr (std::is_same_v<Action, SetPaused>) {
                if (!action.paused && next.liquidity && liquidity_report(*next.liquidity).pending_warning)
                    refuse(CommandReason::morning_not_ready, "Time remains paused until the insolvency warning is acknowledged.");
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
            } else if constexpr (std::is_same_v<Action, AddRecurringObligation>) {
                if (action.obligation.starts_on < next.date)
                    refuse(CommandReason::invalid_argument, "A new obligation cannot accrue before its accepted date.");
                add_recurring(accounts(next), action.obligation);
                (void)receipt(next, "add_recurring_obligation");
            } else if constexpr (std::is_same_v<Action, IssueInvoice>) {
                issue_invoice(accounts(next), next.date, action.invoice);
                (void)receipt(next, "issue_invoice");
            } else if constexpr (std::is_same_v<Action, SettleInvoice>) {
                const auto before = next.cash;
                settle_invoice(accounts(next), next.date, action.id, action.amount, reserved_cash(next), action.key);
                next.cash = accounts_report(*next.accounts).cash;
                (void)receipt(next, "settle_invoice", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, DrawDebt>) {
                if (action.debt.drawn_on != next.date)
                    refuse(CommandReason::invalid_argument, "Debt must record its actual acceptance date.");
                const auto before = next.cash;
                draw_debt(accounts(next), next.date, action.debt, action.cause);
                next.cash = accounts_report(*next.accounts).cash;
                (void)receipt(next, "draw_debt", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, RepayDebt>) {
                const auto before = next.cash;
                repay_debt(accounts(next), next.date, action.id, action.amount, reserved_cash(next), action.key);
                next.cash = accounts_report(*next.accounts).cash;
                (void)receipt(next, "repay_debt", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, InvestEquity>) {
                const auto before = next.cash;
                invest_equity(accounts(next), next.date, action.investor, action.proceeds, action.acquired, action.key);
                next.cash = accounts_report(*next.accounts).cash;
                (void)receipt(next, "invest_equity", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, ReserveProject>) {
                if (next.lab && action.project.id.starts_with("lab/"))
                    refuse(CommandReason::invalid_argument, "Lab projects must be created through their data, training, research or evaluation command.");
                reserve_project(capacity(next), next.date, action.project);
                if (action.project.monthly_support_cost.value() > 0)
                    add_recurring(accounts(next), {"support/" + action.project.id, action.project.monthly_support_cost,
                        next.date, std::nullopt, "Serving support: " + action.project.id});
                (void)receipt(next, "reserve_project");
            } else if constexpr (std::is_same_v<Action, CancelProject>) {
                cancel_project(capacity(next), next.date, action.id);
                const auto found = std::find_if(next.capacity->projects.begin(), next.capacity->projects.end(),
                    [&](const auto& project) { return project.id == action.id; });
                if (found->monthly_support_cost.value() > 0)
                    end_recurring(accounts(next), "support/" + found->id, next.date);
                (void)receipt(next, "cancel_project");
            } else if constexpr (std::is_same_v<Action, SetEffectiveCapacity>) {
                update_effective_capacity(capacity(next), next.date, action.source_id, action.available);
                (void)receipt(next, "capacity_availability");
            } else if constexpr (std::is_same_v<Action, EnableMarket>) {
                if (next.market) refuse(CommandReason::invalid_status, "The customer market is already active.");
                (void)capacity(next);
                next.market = opening_market(next.date, accounts(next).owner);
                (void)receipt(next, "enable_market");
            } else if constexpr (std::is_same_v<Action, AddMarketProduct>) {
                add_market_product(market(next), capacity(next), action.product);
                (void)receipt(next, "add_market_product");
            } else if constexpr (std::is_same_v<Action, AddMarketSegment>) {
                add_market_segment(market(next), action.segment);
                (void)receipt(next, "add_market_segment");
            } else if constexpr (std::is_same_v<Action, AddMarketPreference>) {
                add_market_preference(market(next),action.segment_id,action.preference);
                (void)receipt(next,"introduce_customer_product");
            } else if constexpr (std::is_same_v<Action, SetMarketProductAvailable>) {
                set_product_available(market(next), action.id, action.available);
                (void)receipt(next, "market_product_availability");
            } else if constexpr (std::is_same_v<Action, AcceptContract>) {
                accept_contract(market(next), capacity(next), next.date, action.contract);
                (void)receipt(next, "accept_contract");
            } else if constexpr (std::is_same_v<Action, StartContract>) {
                start_contract(market(next), capacity(next), next.date, action.id);
                (void)receipt(next, "start_contract");
            } else if constexpr (std::is_same_v<Action, CancelContract>) {
                cancel_contract(market(next), accounts(next), next.date, action.id);
                (void)receipt(next, "cancel_contract");
            } else if constexpr (std::is_same_v<Action, AcknowledgeLiquidityWarning>) {
                if (!next.liquidity) refuse(CommandReason::invalid_status,"There is no liquidity warning to acknowledge.");
                acknowledge_liquidity_warning(*next.liquidity,action.id);
                (void)receipt(next,"acknowledge_liquidity_warning");
            } else if constexpr (std::is_same_v<Action, BuyData>) {
                const auto before = next.cash;
                buy_data(lab(next), accounts(next), next.date, action.offer_id, reserved_cash(next));
                next.cash = accounts_report(*next.accounts).cash;
                (void)receipt(next, "buy_data", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, StartResearch>) {
                const auto before = next.cash;
                start_research(lab(next), accounts(next), capacity(next), next.date, action.research_id, action.allocation, reserved_cash(next));
                next.cash = accounts_report(*next.accounts).cash;
                (void)receipt(next, "start_research", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, StartTraining>) {
                start_training(lab(next), capacity(next), next.date, action.request);
                (void)receipt(next, "start_training");
            } else if constexpr (std::is_same_v<Action, CloneModel>) {
                clone_model(lab(next), capacity(next), next.date, action.source_id, action.id);
                (void)receipt(next, "clone_checkpoint");
            } else if constexpr (std::is_same_v<Action, StartEvaluation>) {
                const auto before = next.cash;
                start_evaluation(lab(next), accounts(next), capacity(next), next.date,
                    action.id, action.model_id, action.plan_id, action.allocation, reserved_cash(next));
                next.cash = accounts_report(*next.accounts).cash;
                (void)receipt(next, "start_evaluation", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, AcceptBusinessOffer>) {
                if (!next.business_offers) refuse(CommandReason::invalid_status,"This scenario has no business offers.");
                const auto before=next.cash;
                accept_business_offer(*next.business_offers,accounts(next),capacity(next),next.market,next.date,action.id,reserved_cash(next));
                next.cash=accounts_report(*next.accounts).cash;
                (void)receipt(next,"accept_business_offer",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, AddMarketDemandChange>) {
                add_market_demand_change(market(next),next.date,action.change);
                (void)receipt(next,"external_demand_change");
            }
        }, command.action);
        if (next.liquidity && !std::holds_alternative<AdvanceDay>(command.action))
            refresh_liquidity(*next.liquidity,*next.accounts);
        // The input was validated at construction; every daily subsystem owns
        // its checked append. Replaying the full ledger/capacity/market history
        // here would turn a campaign into quadratic validation work. External
        // restores, explicit state validation and non-daily commands stay deep.
        validate(next,!std::holds_alternative<AdvanceDay>(command.action));
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
        available(state_), state_.pending_choice, state_.reservations,
        state_.accounts ? std::optional{accounts_report(*state_.accounts)} : std::nullopt,
        state_.capacity ? std::optional{capacity_report(*state_.capacity)} : std::nullopt,
        state_.market ? std::optional{market_report(*state_.market)} : std::nullopt,
        state_.liquidity ? std::optional{liquidity_report(*state_.liquidity)} : std::nullopt,
        state_.lab ? std::optional{lab_report(*state_.lab, *state_.capacity)} : std::nullopt,
        state_.business_offers});
}
} // namespace pgp::core
