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
void prepare_service_lifecycle(SimulationState& state) {
    prepare_deployment_risk(state);
    expire_service_rollouts(state);
    if (retire_due_services(state)>0) (void)receipt(state,"service_retirement_effective");
}
void prepare_opening(SimulationState& state, const std::vector<MarketAdoption>* industry_adoptions = nullptr) {
    if (state.phase != DayPhase::opening) {
        return;
    }
    prepare_civic(state, industry_adoptions);
    prepare_governance(state);
    prepare_succession(state);
    prepare_management(state);
    prepare_service_lifecycle(state);
    prepare_infrastructure(state);
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

void validate(const SimulationState& state,bool deep=true,std::size_t first_receipt=0) {
    const auto invalid = [](const char* message) { throw std::invalid_argument{message}; };
    (void)from_ordinal(state.date);
    (void)from_ordinal(state.end_date);
    if (state.date > state.end_date || state.cash.value() < 0) {
        invalid("Invalid campaign dates or negative unfinanced cash");
    }
    if (state.accounts) {
        if (deep && !state.market && !state.liquidity) validate_accounts(*state.accounts);
        const auto& ledger = *state.accounts;
        if (ledger.postings.totals().customer_advance_entries>0 && (!state.market || !state.business_offers))
            invalid("Customer advances require their market and offer catalog");
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
            auto ends_before=project.stopped_on;
            if (!ends_before && state.market && project.kind==ProjectKind::serving) {
                const auto trial=std::ranges::find(state.market->trials,project.id,&MarketTrial::project_id);
                if (trial!=state.market->trials.end()) ends_before=trial->ends_before;
                const auto product=std::ranges::find(state.market->products,project.id,&MarketProduct::capacity_project_id);
                if (product!=state.market->products.end() && product->retirement) ends_before=product->retirement->retires_on;
            }
            if (term == state.accounts->recurring.end() || term->monthly_amount != project.monthly_support_cost ||
                term->starts_on != project.starts_on || term->ends_before != ends_before)
                invalid("Serving support terms differ from their owning capacity commitment");
        }
    }
    if (state.market) {
        if (!state.accounts || !state.capacity) invalid("A market requires accounts and shared capacity");
        if (deep) {
            for (const auto& product:state.market->products) if (product.retirement &&
                (!state.deployments || product.retirement->retires_on>state.end_date))
                invalid("Retirement needs a qualified release and a date within the campaign");
            for (const auto& term:state.market->compatibility)
                if (!state.deployments || term.ends_before>state.end_date)
                    invalid("Compatibility needs a qualified release and a date within the campaign");
        }
        if (!state.deployments && !state.market->trials.empty()) invalid("Service trials require qualified release records");
        if (deep) validate_market_links(*state.market, *state.accounts, *state.capacity);
        else if (state.market->owner!=state.accounts->owner || state.market->closed_through!=state.accounts->closed_through)
            invalid("Market settlement differs from the owned account cursor");
    }
    if (state.lab) {
        if (!state.accounts || !state.capacity) invalid("Lab requires business accounts and shared capacity");
        validate_lab(*state.lab, *state.capacity, *state.accounts, state.random, false);
    }
    if (state.management) {validate_management(state,deep);validate_governance(state,deep);}
    validate_funding_support(state,deep);
    validate_succession(state,deep);
    validate_infrastructure(state,deep);
    validate_resilience(state,deep);
    validate_civic(state,deep);
    if (state.deployments) {
        if (!state.lab || !state.market || !state.capacity) invalid("Deployments require lab, market and capacity");
        if (deep) validate_deployments(*state.deployments,*state.lab,*state.capacity,*state.market);
    }
    validate_data_acquisitions(state,deep);
    validate_deployment_risk(state,deep);
    validate_frontier(state,deep);
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
    if (uncommitted_cash(state).value() < 0) {
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
    if (first_receipt > state.receipts.size()) invalid("A transaction discarded committed receipts");
    auto prior_receipt = first_receipt == 0 ? std::uint64_t{0} : state.receipts[first_receipt - 1].id;
    for (auto i = first_receipt; i < state.receipts.size(); ++i) {
        const auto& item = state.receipts[i];
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

Cents uncommitted_cash(const SimulationState& state) {
    return checked_unit_subtract(state.cash,reserved_cash(state));
}

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
void extend_campaign(SimulationState& state, OrdinalDay end_date,
                     const std::vector<MarketAdoption>* industry_adoptions) {
    (void)from_ordinal(end_date);
    if (end_date <= state.date || end_date < state.end_date)
        throw std::invalid_argument("A continuation needs a later campaign end");
    state.end_date = end_date;
    state.paused = true;
    if (state.phase != DayPhase::finished) return;
    state.phase = DayPhase::opening;
    // Site sampling skipped the original terminal morning.
    sample_site_hazards(state);
    try {
        prepare_opening(state, industry_adoptions);
    } catch (const Refusal& failure) {
        throw std::invalid_argument(failure.explanation);
    }
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
        const auto previous_receipts = state_.receipts.size();
        const bool reserve_applies=!std::holds_alternative<AdvanceDay>(command.action) &&
            !std::holds_alternative<ResolveOpeningChoice>(command.action) &&
            !std::holds_alternative<SettleInvoice>(command.action) &&
            !std::holds_alternative<RepayDebt>(command.action) &&
            operating_reserve_target(state_).value()>0;
        const auto uncommitted_before=reserve_applies?uncommitted_cash(state_):Cents{0};
        auto next = outer_transaction?std::move(state_):state_;
        ++next.revision;
        std::optional<std::uint64_t> new_reservation;
        const auto command_receipt = next.next_receipt_id;
        const auto require_technology = [&](FrontierTechnology kind,std::string_view id) {
            if (auto blocked=frontier_technology_block(next,kind,id,next.date); !blocked.empty())
                refuse(CommandReason::dependent_commitment,std::move(blocked));
        };
        std::visit([&](const auto& action) {
            using Action = std::decay_t<decltype(action)>;
            if constexpr (!std::is_same_v<Action, ResolveOpeningChoice> &&
                          !std::is_same_v<Action, SetPaused> && !std::is_same_v<Action, SetDevicePower>) {
                if (next.phase != DayPhase::work_ready) {
                    refuse(CommandReason::morning_not_ready,
                           "Resolve the required opening choice before planning or advancing work.");
                }
            }
            if constexpr (std::is_same_v<Action, SelectCampaignObjectives> || std::is_same_v<Action, AcknowledgeObjectiveBreach> ||
                          std::is_same_v<Action, EndWithPostmortem> || std::is_same_v<Action, ContinueAfterEnding>) {
                refuse(CommandReason::invalid_argument,"Campaign objectives belong to the shared world transaction.");
            } else if constexpr (std::is_same_v<Action, ReserveFunds>) {
                if (!valid_text(action.name) || action.amount.value() <= 0) {
                    refuse(CommandReason::invalid_argument,
                           "A reservation needs a short name and a positive amount of cents.");
                }
                if (next.reservations.size() >= simulation_reservation_limit) {
                    refuse(CommandReason::limit_exceeded, "The reservation count limit was reached.");
                }
                if (action.amount > uncommitted_cash(next)) {
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
                if (!action.market_influences.empty() && !outer_transaction)
                    refuse(CommandReason::invalid_argument,"Regional market conditions belong to the shared world transaction.");
                if (!action.wholesale_work.empty() && (!outer_transaction || !next.infrastructure || !next.capacity))
                    refuse(CommandReason::invalid_argument,"Only the world can settle exported work from an owned site.");
                if ((!action.industry_adoptions.empty() && !action.market_allocation) ||
                    (!next.market && (action.market_allocation || !action.industry_adoptions.empty())))
                    refuse(CommandReason::invalid_argument,"Shared industry context requires an actual market and allocation.");
                if (required_management_notice(next)) refuse(CommandReason::morning_not_ready,"Review the required management alert before advancing.");
                if (required_deployment_incident(next)) refuse(CommandReason::morning_not_ready,"Review the deployment incident before advancing work.");
                if (required_site_alert(next)) refuse(CommandReason::morning_not_ready,"Review the site disruption before advancing work.");
                if (required_governance_notice(next)) refuse(CommandReason::morning_not_ready,"Review the corporate concern, deadline or outcome before advancing.");
                if (required_succession_notice(next)) refuse(CommandReason::morning_not_ready,"Review the leadership deadline or handover before advancing.");
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
                    close_market_day(*next.market, accounts(next), capacity(next), next.date, false,
                        action.market_allocation ? &*action.market_allocation : nullptr,
                        action.market_allocation ? &action.industry_adoptions : nullptr, action.wholesale_work, action.market_influences);
                else if (next.capacity) close_capacity_day(*next.capacity, next.date, false, action.wholesale_work);
                if (next.lab) {
                    close_data_acquisitions(next);
                    close_lab_day(*next.lab, *next.capacity, next.date, next.random);
                }
                close_infrastructure_day(next);
                if (next.accounts) {
                    const auto credits=redeem_cloud_credits(next);
                    close_accounts_day(*next.accounts, next.date, reserved_cash(next), false, credits);
                    next.cash = accounts_report(*next.accounts).cash;
                    close_public_reporting_day(next);
                }
                (void)receipt(next, "advance_day", 0, checked_unit_subtract(next.cash, cash_before));
                next.date = advance_days(next.date, Days{1});
                prepare_frontier(next);
                sample_site_hazards(next);
                prepare_governance(next);
                prepare_succession(next);
                prepare_management(next);
                prepare_service_lifecycle(next);
                prepare_infrastructure(next);
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
                    prepare_opening(next, action.market_allocation ? &action.industry_adoptions : nullptr);
                }
            } else if constexpr (std::is_same_v<Action, SetPaused>) {
                if (!action.paused && required_deployment_incident(next)) refuse(CommandReason::morning_not_ready,"Review the deployment incident before resuming time.");
                if (!action.paused && required_site_alert(next)) refuse(CommandReason::morning_not_ready,"Review the site disruption before resuming time.");
                if (!action.paused && required_management_notice(next)) refuse(CommandReason::morning_not_ready,"Acknowledge the required management warning before starting time.");
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
                if (action.invoice.id.starts_with("deployment.claim/"))
                    refuse(CommandReason::dependent_commitment,"Only a discovered incident can create its compensation claim.");
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
                company_equity(next, action.investor, action.proceeds, action.acquired, action.key);
                next.cash = accounts_report(*next.accounts).cash;
                (void)receipt(next, "invest_equity", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, ReserveProject>) {
                if (action.project.kind==ProjectKind::incident_response || action.project.id.starts_with("deployment.response/"))
                    refuse(CommandReason::dependent_commitment,"Start incident work through its reviewed response.");
                if (action.project.id.starts_with("frontier.trial/"))
                    refuse(CommandReason::dependent_commitment,"Commission independent work through its frontier protocol.");
                if (action.project.customer_inference)
                    refuse(CommandReason::invalid_argument, "Customer hardware is assigned by a qualified local deployment.");
                if (next.lab && action.project.id.starts_with("lab/"))
                    refuse(CommandReason::invalid_argument, "Lab projects must be created through their data, training, research or evaluation command.");
                if (next.deployments && action.project.kind==ProjectKind::serving)
                    refuse(CommandReason::invalid_argument,"Create services through a qualified model deployment.");
                reserve_project(capacity(next), next.date, action.project);
                if (action.project.monthly_support_cost.value() > 0)
                    add_recurring(accounts(next), {"support/" + action.project.id, action.project.monthly_support_cost,
                        next.date, std::nullopt, "Serving support: " + action.project.id});
                (void)receipt(next, "reserve_project");
            } else if constexpr (std::is_same_v<Action, PauseProject> || std::is_same_v<Action, ResumeProject>) {
                const bool owned=next.lab && (std::any_of(next.lab->research.begin(),next.lab->research.end(),[&](const auto& r){return r.project_id==action.id;}) ||
                    std::any_of(next.lab->models.begin(),next.lab->models.end(),[&](const auto& r){return r.project_id==action.id;}) ||
                    std::any_of(next.lab->evaluations.begin(),next.lab->evaluations.end(),[&](const auto& r){return r.project_id==action.id;}) ||
                    std::any_of(next.lab->generated.begin(),next.lab->generated.end(),[&](const auto& r){return r.project_id==action.id;}) ||
                    std::ranges::any_of(next.lab->acquisitions,[&](const auto& r){return r.project_id==action.id;}));
                const bool response = next.deployments && next.deployments->risk &&
                    std::ranges::any_of(next.deployments->risk->responses,[&](const auto& row){return row.project_id==action.id;});
                if(!owned && !response) refuse(CommandReason::invalid_argument,"Choose a finite laboratory or incident response project owned by this company");
                if constexpr(std::is_same_v<Action,PauseProject>) {
                    if(next.management && next.management->governance) for(const auto& d:next.management->governance->disputes)
                        if(d.review_project==action.id && !d.resolved_on) refuse(CommandReason::dependent_commitment,"The negotiated executive response reserves this review team until its promise is resolved or its deadline passes");
                    pause_project(capacity(next),next.date,action.id);(void)receipt(next,"pause_project");
                } else {resume_project(capacity(next),next.date,action.id);(void)receipt(next,"resume_project");}
            } else if constexpr (std::is_same_v<Action, CancelProject>) {
                if (next.deployments) for (const auto& rollout:next.deployments->rollouts) if (!rollout.resolved_on &&
                    (rollout.candidate.project_id==action.id ||
                     next.deployments->records.at(static_cast<std::size_t>(rollout.incumbent_release-1)).project_id==action.id))
                    refuse(CommandReason::dependent_commitment,"Promote or discard the staged release before stopping its serving work.");
                if(next.management && next.management->governance) for(const auto& dispute:next.management->governance->disputes)
                    if(dispute.review_project==action.id && !dispute.resolved_on)
                        refuse(CommandReason::dependent_commitment,"The negotiated executive response reserves this review team until the promise is resolved or its deadline passes.");
                if (next.resilience) for (const auto& replica:next.resilience->replicas) if (!replica.stopped_on &&
                    (replica.sync_project==action.id || (next.market && std::any_of(next.market->products.begin(),next.market->products.end(),[&](const auto& product){
                        return product.id==replica.service_id && product.capacity_project_id==action.id;
                    })))) refuse(CommandReason::dependent_commitment,"Stop the service replica before changing either of its workloads.");
                if (next.market) for (const auto& product:next.market->products) {
                    if (product.capacity_project_id!=action.id) continue;
                    if (product.retirement || market_compatibility(*next.market,product.id,next.date))
                        refuse(CommandReason::dependent_commitment,"Keep serving through the published compatibility or retirement period.");
                    for (const auto& contract:next.market->contracts)
                        if (contract.product_id==product.id && (contract.status==ContractStatus::active || contract.status==ContractStatus::pending))
                            refuse(CommandReason::dependent_commitment,"Cancel the dependent customer contract before stopping its service.");
                }
                cancel_project(capacity(next), next.date, action.id);
                const auto found = std::find_if(next.capacity->projects.begin(), next.capacity->projects.end(),
                    [&](const auto& project) { return project.id == action.id; });
                if (found->monthly_support_cost.value() > 0)
                    end_recurring(accounts(next), "support/" + found->id, next.date);
                (void)receipt(next, "cancel_project");
            } else if constexpr (std::is_same_v<Action, WithdrawOperations>) {
                if (next.infrastructure && std::any_of(next.infrastructure->sites.begin(),next.infrastructure->sites.end(),[](const auto& site){return !site.closed_on;}))
                    refuse(CommandReason::dependent_commitment,"Close owned sites and their obligations before withdrawing operations.");
                if (!next.accounts || !next.capacity || reserved_cash(next).value()!=0)
                    refuse(CommandReason::dependent_commitment,"Release funded commitments before withdrawing operations.");
                if (next.market) for (const auto& contract:next.market->contracts)
                    if (contract.status==ContractStatus::active || contract.status==ContractStatus::pending)
                        refuse(CommandReason::dependent_commitment,"Resolve customer contracts before withdrawing operations.");
                if (next.market) for (const auto& product:next.market->products)
                    if ((product.retirement && next.date<product.retirement->retires_on) || market_compatibility(*next.market,product.id,next.date))
                        refuse(CommandReason::dependent_commitment,"Wait until the published service commitments end before withdrawing operations.");
                discard_service_rollouts(next);
                withdraw_pending_specialists(next);
                withdraw_people(next);
                std::vector<std::string> projects,recurring,sources;
                for (const auto& row:next.capacity->projects)
                    if (row.status==ProjectStatus::active || row.status==ProjectStatus::paused) projects.push_back(row.id);
                for (const auto& row:next.accounts->recurring)
                    if ((!row.ends_before || *row.ends_before>std::max(row.starts_on,next.date)) &&
                        !row.id.starts_with("offer.lease/") && !row.id.starts_with("offer.renew/") &&
                        !row.id.starts_with("leadership/")) recurring.push_back(row.id);
                for (const auto& row:next.capacity->sources) sources.push_back(row.id);
                for (const auto& id:projects) cancel_project(*next.capacity,next.date,id);
                for (const auto& id:recurring) {
                    const auto term=std::ranges::find(next.accounts->recurring,id,&RecurringObligation::id);
                    schedule_recurring_end(*next.accounts,next.date,id,std::max(term->starts_on,next.date));
                }
                if (next.capacity->available_staff>0) remove_staff_capacity(*next.capacity,next.date,"withdrawal/"+std::to_string(next.revision),next.capacity->available_staff);
                for (const auto& id:sources) update_effective_capacity(*next.capacity,next.date,id,MilliCUPerDay{0});
                if (next.management) for (auto& policy:next.management->policies) policy.enabled=false;
                (void)receipt(next,"withdraw_operations");
            } else if constexpr (std::is_same_v<Action, SetEffectiveCapacity>) {
                if (next.capacity && std::any_of(next.capacity->sources.begin(),next.capacity->sources.end(),[&](const auto& source){
                    return source.id==action.source_id && (source.kind==CapacitySourceKind::imported ||
                        (next.infrastructure && source.kind==CapacitySourceKind::owned));
                })) refuse(CommandReason::invalid_argument,"Owned and imported capacity follow actual site power and supplier terms.");
                update_effective_capacity(capacity(next), next.date, action.source_id, action.available);
                (void)receipt(next, "capacity_availability");
            } else if constexpr (std::is_same_v<Action, OpenLiaisonOffice>) {
                const auto before=next.cash;open_liaison_office(next,action.offer,reserved_cash(next));(void)receipt(next,"open_liaison_office",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, CloseLiaisonOffice>) {
                close_liaison_office(next,action.id);(void)receipt(next,"close_liaison_office");
            } else if constexpr (std::is_same_v<Action, BuyPersonalDevice>) {
                const auto before=next.cash;buy_personal_device(next,action.offer,reserved_cash(next));(void)receipt(next,"buy_personal_device",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, ServicePersonalDevice>) {
                const auto before=next.cash;service_personal_device(next,action.id,reserved_cash(next));(void)receipt(next,"service_personal_device",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, SetDevicePower>) {
                set_device_power(next,action);(void)receipt(next,"device_power");
            } else if constexpr (std::is_same_v<Action, RetirePersonalDevice>) {
                retire_personal_device(next,action.id);(void)receipt(next,"retire_personal_device");
            } else if constexpr (std::is_same_v<Action, ExpandHeadquarters>) {
                const auto before=next.cash;expand_headquarters(next,action,reserved_cash(next));(void)receipt(next,"expand_headquarters",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, CreateFacilityBlueprint>) {
                create_facility_blueprint(next,action.blueprint);(void)receipt(next,"create_facility_blueprint");
            } else if constexpr (std::is_same_v<Action, BuildFromBlueprint>) {
                const auto before=next.cash;build_from_blueprint(next,action,reserved_cash(next));(void)receipt(next,"build_from_blueprint",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, BuildOwnedSite>) {
                const auto before=next.cash;build_owned_site(next,action,reserved_cash(next));(void)receipt(next,"build_owned_site",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, OrderOwnedHardware>) {
                require_technology(FrontierTechnology::hardware,action.family_id);
                const auto before=next.cash;order_owned_hardware(next,action,reserved_cash(next));(void)receipt(next,"order_owned_hardware",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, HireSiteOperators>) {
                const auto before=next.cash;hire_site_operators(next,action,reserved_cash(next));(void)receipt(next,"hire_site_operators",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, SetSitePowerLimit>) {
                set_site_power_limit(next,action);(void)receipt(next,"site_power_budget");
            } else if constexpr (std::is_same_v<Action, RetireOwnedHardware>) {
                const auto before=next.cash;retire_owned_hardware(next,action.id);(void)receipt(next,"retire_owned_hardware",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, CloseOwnedSite>) {
                const auto before=next.cash;close_owned_site(next,action.id);(void)receipt(next,"close_owned_site",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, ScheduleSiteAppointment>) {
                const auto before=next.cash;schedule_site_appointment(next,action,reserved_cash(next));(void)receipt(next,"schedule_site_appointment",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, CancelSiteAppointment>) {
                cancel_site_appointment(next,action.id);(void)receipt(next,"cancel_site_appointment");
            } else if constexpr (std::is_same_v<Action, NoticeInitialLeaseTermination>) {
                notice_initial_lease_termination(next);(void)receipt(next,"notice_initial_lease_termination");
            } else if constexpr (std::is_same_v<Action, BuySiteProtection>) {
                const auto before=next.cash;buy_site_protection(next,action,reserved_cash(next));
                (void)receipt(next,"buy_site_protection",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, InsureSite>) {
                const auto before=next.cash;insure_site(next,action,reserved_cash(next));
                (void)receipt(next,"insure_site",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, ReplicateService>) {
                const auto before=next.cash;replicate_service(next,action,reserved_cash(next));
                (void)receipt(next,"replicate_service",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, StopServiceReplica>) {
                stop_service_replica(next,action.id);(void)receipt(next,"stop_service_replica");
            } else if constexpr (std::is_same_v<Action, RespondDeploymentIncident>) {
                const auto before=next.cash;
                respond_deployment_incident(next,action,reserved_cash(next));
                (void)receipt(next,"respond_deployment_incident",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, AcknowledgeDeploymentIncident>) {
                acknowledge_deployment_incident(next,action.id);
                (void)receipt(next,"acknowledge_deployment_incident");
            } else if constexpr (std::is_same_v<Action, AcknowledgeSiteAlert>) {
                if (!next.resilience) refuse(CommandReason::invalid_argument,"This scenario has no site alerts.");
                const auto row=std::find_if(next.resilience->alerts.begin(),next.resilience->alerts.end(),[&](const auto& alert){return alert.id==action.id;});
                if (row==next.resilience->alerts.end() || row->acknowledged) refuse(CommandReason::invalid_status,"This site alert is absent or already acknowledged.");
                row->acknowledged=true;(void)receipt(next,"acknowledge_site_alert");
            } else if constexpr (std::is_same_v<Action, EnableMarket>) {
                if (next.market) refuse(CommandReason::invalid_status, "The customer market is already active.");
                (void)capacity(next);
                next.market = opening_market(next.date, accounts(next).owner);
                (void)receipt(next, "enable_market");
            } else if constexpr (std::is_same_v<Action, AddMarketProduct>) {
                if (next.deployments) refuse(CommandReason::invalid_argument,"Use qualified deployment or stop the serving project.");
                add_market_product(market(next), capacity(next), action.product);
                (void)receipt(next, "add_market_product");
            } else if constexpr (std::is_same_v<Action, AddMarketSegment>) {
                add_market_segment(market(next), action.segment);
                (void)receipt(next, "add_market_segment");
            } else if constexpr (std::is_same_v<Action, AddMarketPreference>) {
                if (next.deployments) refuse(CommandReason::invalid_argument,"Use qualified deployment or stop the serving project.");
                add_market_preference(market(next),action.segment_id,action.preference);
                (void)receipt(next,"introduce_customer_product");
            } else if constexpr (std::is_same_v<Action, SetMarketProductAvailable>) {
                if (next.deployments) refuse(CommandReason::invalid_argument,"Use qualified deployment or stop the serving project.");
                set_product_available(market(next), action.id, action.available);
                (void)receipt(next, "market_product_availability");
            } else if constexpr (std::is_same_v<Action, SetMarketPrice>) {
                set_market_price(market(next), next.date, action.id, action.price);
                suspend_pricing_policies(next,action.id);
                (void)receipt(next, "market_price_change");
            } else if constexpr (std::is_same_v<Action, SetServiceWaitlist>) {
                set_market_waitlist(market(next),capacity(next),next.date,action.id,action.new_tasks_per_day);
                (void)receipt(next,action.new_tasks_per_day?"set_service_waitlist":"reopen_service_enrollment");
            } else if constexpr (std::is_same_v<Action, AcceptContract>) {
                if (action.contract.customer_advance!=Cents{0})
                    refuse(CommandReason::invalid_argument,"Customer advances require an authored offer.");
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
                require_technology(FrontierTechnology::data,action.offer_id);
                const auto before = next.cash;
                buy_data(lab(next), accounts(next), next.date, action.offer_id, reserved_cash(next));
                next.cash = accounts_report(*next.accounts).cash;
                (void)receipt(next, "buy_data", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, StartResearch>) {
                require_technology(FrontierTechnology::research,action.research_id);
                const auto before = next.cash;
                start_funded_research(next,action.research_id,action.allocation,reserved_cash(next));
                (void)receipt(next, "start_research", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, PublishResearch>) {
                publish_research(lab(next),capacity(next),next.date,action.research_id);
                (void)receipt(next,"publish_research");
            } else if constexpr (std::is_same_v<Action, AcquireResearchInsight>) {
                acquire_research_insight(lab(next),next.date,action.insight);
                (void)receipt(next,"acquire_public_research");
            } else if constexpr (std::is_same_v<Action, StartDataAcquisition>) {
                const auto before=next.cash;
                start_data_acquisition(next,action,reserved_cash(next));
                (void)receipt(next,"collect_training_data",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, GenerateDataset>) {
                const auto before=next.cash;
                generate_dataset(lab(next),accounts(next),capacity(next),next.date,action,reserved_cash(next));
                next.cash=accounts_report(*next.accounts).cash;
                (void)receipt(next,"generate_and_verify_data",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, StartTraining>) {
                if (action.request.model_id.starts_with(frontier_refresh_prefix))
                    refuse(CommandReason::dependent_commitment,"Automated candidate identities belong to their authorized refresh.");
                require_technology(FrontierTechnology::recipe,action.request.recipe_id);
                start_training(lab(next), capacity(next), next.date, action.request);
                (void)receipt(next, "start_training");
            } else if constexpr (std::is_same_v<Action, CheckpointModel>) {
                if(action.id.starts_with(frontier_refresh_prefix)) refuse(CommandReason::dependent_commitment,"Automated candidate identities belong to their authorized refresh");
                checkpoint_model(lab(next),capacity(next),next.date,action.source_id,action.id);(void)receipt(next,"checkpoint_model");
            } else if constexpr (std::is_same_v<Action, CloneModel>) {
                if (action.id.starts_with(frontier_refresh_prefix))
                    refuse(CommandReason::dependent_commitment,"Automated candidate identities belong to their authorized refresh.");
                clone_model(lab(next), capacity(next), next.date, action.source_id, action.id);
                (void)receipt(next, "clone_checkpoint");
            } else if constexpr (std::is_same_v<Action, StartEvaluation>) {
                if (action.id.starts_with(frontier_refresh_prefix))
                    refuse(CommandReason::dependent_commitment,"Automated evidence identities belong to their authorized refresh.");
                if (!action.specialist_id.empty()) {
                    const auto& terms=accounts(next).recurring;
                    const auto salary=std::ranges::find(terms,action.specialist_id,&RecurringObligation::id);
                    if (salary==terms.end() || salary->starts_on>next.date ||
                        (salary->ends_before && *salary->ends_before<=next.date))
                        refuse(CommandReason::invalid_argument,"Select a specialist whose paid appointment is active");
                }
                const auto before = next.cash;
                start_evaluation(lab(next), accounts(next), capacity(next), next.date,
                    action.id, action.model_id, action.plan_id, action.allocation, reserved_cash(next), action.specialist_id);
                next.cash = accounts_report(*next.accounts).cash;
                (void)receipt(next, "start_evaluation", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, RegisterFrontierSystem>) {
                register_frontier_system(next,action);
                (void)receipt(next,"register_frontier_system");
            } else if constexpr (std::is_same_v<Action, StartFrontierTrial>) {
                const auto before=next.cash;
                start_frontier_trial(next,action,reserved_cash(next));
                (void)receipt(next,"commission_frontier_trial",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, AuthorizeFrontierResearch>) {
                authorize_frontier_research(next,action.plan);
                (void)receipt(next,"authorize_frontier_research");
            } else if constexpr (std::is_same_v<Action, AuthorizeFrontierRefresh>) {
                authorize_frontier_refresh(next,action.plan);
                (void)receipt(next,"authorize_frontier_refresh");
            } else if constexpr (std::is_same_v<Action, CancelFrontierPolicy>) {
                cancel_frontier_policy(next,action);
                (void)receipt(next,"cancel_frontier_policy");
            } else if constexpr (std::is_same_v<Action, ProposeSuccession>) {
                propose_succession(next,action);(void)receipt(next,"propose_succession");
            } else if constexpr (std::is_same_v<Action, VoteSuccession>) {
                const auto before=next.cash;vote_succession(next,action,reserved_cash(next));
                (void)receipt(next,"vote_succession",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, CancelSuccession>) {
                cancel_succession(next,action.id);(void)receipt(next,"cancel_succession");
            } else if constexpr (std::is_same_v<Action, RetireFounderChair>) {
                retire_founder_chair(next);(void)receipt(next,"retire_founder_chair");
            } else if constexpr (std::is_same_v<Action, AcknowledgeSuccession>) {
                acknowledge_succession(next,action.id);(void)receipt(next,"acknowledge_succession");
            } else if constexpr (std::is_same_v<Action, DeployModel>) {
                if (next.resilience && std::any_of(next.resilience->replicas.begin(),next.resilience->replicas.end(),[&](const auto& row){
                    return row.service_id==action.service_id && !row.stopped_on;
                })) refuse(CommandReason::dependent_commitment,"Stop the service replica before replacing its qualified checkpoint.");
                if (!next.deployments) refuse(CommandReason::invalid_status,"This scenario has no deployable service catalog.");
                if (auto blocked = civic_launch_block(next.civic ? &*next.civic : nullptr, next, action, next.date); !blocked.empty())
                    refuse(CommandReason::invalid_argument, std::move(blocked));
                deploy_model(*next.deployments,lab(next),accounts(next),capacity(next),market(next),next.date,action);
                (void)receipt(next,"deploy_model");
            } else if constexpr (std::is_same_v<Action, StageServiceRollout>) {
                stage_service_rollout(next,action);(void)receipt(next,"stage_service_rollout");
            } else if constexpr (std::is_same_v<Action, FinishServiceRollout>) {
                finish_service_rollout(next,action);(void)receipt(next,action.promote?"promote_service_rollout":"discard_service_rollout");
            } else if constexpr (std::is_same_v<Action, RollbackService>) {
                rollback_service(next,action);(void)receipt(next,"rollback_service");
            } else if constexpr (std::is_same_v<Action, CommitServiceCompatibility>) {
                commit_service_compatibility(next,action);(void)receipt(next,"commit_service_compatibility");
            } else if constexpr (std::is_same_v<Action, AnnounceServiceRetirement>) {
                announce_service_retirement(next,action);(void)receipt(next,"announce_service_retirement");
            } else if constexpr (std::is_same_v<Action, FundPolicyAdvocacy>) {
                const auto before = next.cash;
                fund_policy_advocacy(next, action, reserved_cash(next));
                (void)receipt(next, "fund_policy_advocacy", 0, checked_unit_subtract(next.cash, before));
            } else if constexpr (std::is_same_v<Action, SubmitPolicyEvidence>) {
                submit_policy_evidence(next, action);
                (void)receipt(next, "submit_policy_evidence");
            } else if constexpr (std::is_same_v<Action, BuyMediaCampaign> || std::is_same_v<Action, EndorseForumStandard> ||
                std::is_same_v<Action, SubmitPublicTestimony> || std::is_same_v<Action, MakePublicPromise> ||
                std::is_same_v<Action, RequestInstitutionalMandate> || std::is_same_v<Action, NegotiatePublicCase> ||
                std::is_same_v<Action, NegotiateExecutiveDispute> || std::is_same_v<Action, AcknowledgePublicCase> ||
                std::is_same_v<Action, RelinquishInstitutionalMandate> || std::is_same_v<Action, DirectRegionalProcurement>) {
                refuse(CommandReason::invalid_argument,"Regional media and standards require the owning world transaction.");
            } else if constexpr (std::is_same_v<Action, TakeFinancing>) {
                const auto before=next.cash;take_financing(next,action.id);(void)receipt(next,"take_financing",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, HireEmployee>) {
                const auto before=next.cash;hire_employee(next,action.id,reserved_cash(next));(void)receipt(next,"hire_employee",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, SetRecruitingTerms>) {
                set_recruiting_terms(next,action);(void)receipt(next,"set_recruiting_terms");
            } else if constexpr (std::is_same_v<Action, HireSpecialist>) {
                const auto before=next.cash;hire_specialist(next,action.id,reserved_cash(next));(void)receipt(next,"hire_specialist",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, NegotiateCorporateFinance>) {
                negotiate_corporate_finance(next,action.terms_id);(void)receipt(next,"negotiate_corporate_finance");
            } else if constexpr (std::is_same_v<Action, TransferCompanyOwnership>) {
                if (!next.accounts || (next.management && next.management->governance) || action.parent.empty() || action.parent.size()>64 || action.parent==next.accounts->owner ||
                    std::any_of(next.accounts->holdings.begin(),next.accounts->holdings.end(),[](const auto& h){return h.holder.starts_with("parent/");}))
                    refuse(CommandReason::dependent_commitment,"This company cannot transfer its control or outstanding executive rights through a simple merger.");
                for (auto& h:next.accounts->holdings) h.shares=BasisPoints{0};
                next.accounts->holdings.push_back({"parent/"+action.parent,BasisPoints{10000},false});
                (void)receipt(next,"transfer_company_ownership");
            } else if constexpr (std::is_same_v<Action, TransferGroupFunds>) {
                if (!next.accounts || action.amount.value()<=0 || action.sequence==0 || action.sequence>2048 || action.parent.empty() || action.target.empty() || action.parent==action.target ||
                    next.accounts->owner!=(action.parent_leg?action.parent:action.target)) refuse(CommandReason::invalid_argument,"Group transfers require real consideration and identified parent/subsidiary legs.");
                if (!action.parent_leg && std::none_of(next.accounts->holdings.begin(),next.accounts->holdings.end(),[&](const auto& h){return h.holder=="parent/"+action.parent && h.shares.value()==10000;}))
                    refuse(CommandReason::dependent_commitment,"The sending subsidiary lacks full retained parent control.");
                const bool outgoing=action.funding==action.parent_leg;
                if (outgoing && action.amount>checked_unit_subtract(next.cash,reserved_cash(next))) refuse(CommandReason::insufficient_funds,"Group transfer exceeds actual unreserved cash.");
                const auto offset=action.funding?(action.parent_leg?Account::assets:Account::equity):(action.parent_leg?Account::revenue:Account::equity);
                const auto key="group."+std::string(action.funding?"funding/":"dividend/")+action.target+"/"+std::to_string(action.sequence);
                record_cash(*next.accounts,next.date,Cents{outgoing?-action.amount.value():action.amount.value()},offset,action.funding?"Owned subsidiary capital":"Earned subsidiary dividend",key);
                next.cash=accounts_report(*next.accounts).cash;
                (void)receipt(next,action.funding?"subsidiary_funding":"subsidiary_dividend",0,Cents{outgoing?-action.amount.value():action.amount.value()});
            } else if constexpr (std::is_same_v<Action, AppointExecutive>) {
                const auto before=next.cash;appoint_executive(next,action.candidate,action.hybrid,reserved_cash(next));(void)receipt(next,"appoint_executive",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, ProposePoolExpansion>) {
                propose_pool_expansion(next,action.shares);(void)receipt(next,"propose_pool_expansion");
            } else if constexpr (std::is_same_v<Action, ProposeCharter>) {
                propose_charter(next,action.id);(void)receipt(next,"propose_charter");
            } else if constexpr (std::is_same_v<Action, VoteBoardMotion>) {
                const auto before=next.cash;vote_board_motion(next,action.id,action.support,reserved_cash(next));(void)receipt(next,"vote_board_motion",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, RespondCorporateDispute>) {
                const auto before=next.cash;respond_corporate_dispute(next,action.id,action.buy_waiver,reserved_cash(next));(void)receipt(next,"respond_corporate_dispute",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, AddDelegationPolicy>) {
                add_delegation_policy(next,action.policy);(void)receipt(next,"authorize_delegation");
            } else if constexpr (std::is_same_v<Action, SetDelegationEnabled>) {
                set_delegation_enabled(next,action.id,action.enabled);(void)receipt(next,"set_delegation_enabled");
            } else if constexpr (std::is_same_v<Action, ReviewDelegation>) {
                const auto before=next.cash;review_delegation(next,reserved_cash(next));(void)receipt(next,"review_delegation",0,checked_unit_subtract(next.cash,before));
            } else if constexpr (std::is_same_v<Action, AcknowledgeManagementNotice>) {
                acknowledge_management_notice(next,action.id);(void)receipt(next,"acknowledge_management_notice");
            } else if constexpr (std::is_same_v<Action, RenewComputeLease>) {
                if (!next.business_offers) refuse(CommandReason::invalid_status,"No compute lease is available to renew.");
                const auto before=next.cash;renew_compute_lease(*next.business_offers,accounts(next),capacity(next),next.date,action.id,reserved_cash(next));
                next.cash=accounts_report(*next.accounts).cash;(void)receipt(next,"renew_compute_lease",0,checked_unit_subtract(next.cash,before));
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
        if (reserve_applies) require_operating_reserve(uncommitted_before,next);
        if (!std::holds_alternative<AdvanceDay>(command.action)) {prepare_governance(next);prepare_succession(next);prepare_infrastructure(next);}
        prepare_access(next);
        if (next.liquidity && !std::holds_alternative<AdvanceDay>(command.action))
            refresh_liquidity(*next.liquidity,*next.accounts);
        // The input was validated at construction; every daily subsystem owns
        // its checked append. Replaying the full ledger/capacity/market history
        // here would turn a campaign into quadratic validation work. External
        // restores, explicit state validation and non-daily commands stay deep.
        const bool advanced = std::holds_alternative<AdvanceDay>(command.action);
        validate(next,!advanced,advanced ? previous_receipts : 0);
        state_ = std::move(next);
        return {CommandReason::accepted, state_.revision, command_receipt, new_reservation,
                "The action was committed."};
    } catch (const Refusal& failure) {
        return {failure.reason, state_.revision, {}, {}, failure.explanation};
    } catch (const std::overflow_error&) {
        return {CommandReason::arithmetic_overflow, state_.revision, {}, {},
                "The action exceeds a checked arithmetic bound; nothing was changed."};
    } catch (const std::invalid_argument& error) {
        return {CommandReason::invalid_argument, state_.revision, {}, {}, std::string{error.what()}+"; nothing was changed."};
    } catch (const std::out_of_range&) {
        return {CommandReason::invalid_argument, state_.revision, {}, {},
                "The action exceeds a unit or calendar bound; nothing was changed."};
    }
}

std::shared_ptr<const SimulationSnapshot> Simulation::snapshot() const {
    return std::make_shared<const SimulationSnapshot>(SimulationSnapshot{
        state_.revision, state_.date, state_.phase, state_.paused, state_.cash,
        discretionary_cash(state_), state_.pending_choice, state_.reservations,
        state_.accounts ? std::optional{accounts_report(*state_.accounts)} : std::nullopt,
        state_.capacity ? std::optional{capacity_report(*state_.capacity)} : std::nullopt,
        state_.market ? std::optional{market_report(*state_.market)} : std::nullopt,
        state_.liquidity ? std::optional{liquidity_report(*state_.liquidity)} : std::nullopt,
        state_.lab ? std::optional{lab_report(*state_.lab, *state_.capacity)} : std::nullopt,
        state_.business_offers});
}
} // namespace pgp::core
