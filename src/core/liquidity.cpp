#include <pgp/core/liquidity.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace pgp::core {
namespace {
[[noreturn]] void invalid(const char* message) { throw std::invalid_argument(message); }
void text_valid(std::string_view value, std::size_t maximum = 256) {
    if (value.empty() || value.size() > maximum || std::any_of(value.begin(), value.end(),
        [](unsigned char c) { return c < 32 || c == 127; }))
        invalid("Liquidity text must be nonempty, bounded and contain no control characters");
}
Cents total(const LiquidityExposure& exposure) {
    return checked_unit_add(exposure.unpaid_payables, exposure.matured_principal);
}
OrdinalDay accounts_day(const AccountsState& state) {
    return state.closed_through ? advance_days(*state.closed_through, Days{1}) : state.starts_on;
}
void validate_cause(const LiquidityCause& cause) {
    text_valid(cause.reference); text_valid(cause.cause);
    (void)from_ordinal(cause.due);
    if ((cause.kind != LiquidityCauseKind::payable && cause.kind != LiquidityCauseKind::matured_debt) ||
        cause.unpaid.value() <= 0) invalid("Liquidity cause has an unknown kind or nonpositive unpaid amount");
}
void consider(LiquidityExposure& exposure, LiquidityCause cause) {
    if (!exposure.oldest || std::tie(cause.due, cause.kind, cause.reference) <
        std::tie(exposure.oldest->due, exposure.oldest->kind, exposure.oldest->reference))
        exposure.oldest = std::move(cause);
}
void apply_exposure(LiquidityState& state, LiquidityExposure exposure) {
    state.exposure = std::move(exposure);
    if (state.status == LiquidityStatus::closed) invalid("A closed company cannot restart through liquidity refresh");
    if (total(state.exposure) == Cents{0}) {
        if (state.status == LiquidityStatus::workout) state.workouts.back().resolved_on = state.observed_on;
        state.status = LiquidityStatus::operating;
        return;
    }
    if (state.status == LiquidityStatus::operating) {
        if (state.workouts.size() >= liquidity_workout_limit ||
            state.next_workout_id == std::numeric_limits<std::uint64_t>::max())
            invalid("Liquidity workout history or identifier bound was reached");
        state.workouts.push_back({state.next_workout_id++, state.observed_on,
            advance_days(state.observed_on, Days{liquidity_workout_days}), total(state.exposure),
            *state.exposure.oldest, false, std::nullopt, std::nullopt});
        state.status = LiquidityStatus::workout;
    } else if (state.observed_on > state.workouts.back().deadline)
        invalid("An unresolved liquidity workout cannot perform work after its deadline morning");
}
void validate_reference(const LiquidityCause& cause, const AccountsState& accounts) {
    if (cause.kind == LiquidityCauseKind::payable) {
        const auto found = std::find_if(accounts.invoices.begin(), accounts.invoices.end(),
            [&](const auto& invoice) { return invoice.id == cause.reference; });
        if (found == accounts.invoices.end() || found->kind != InvoiceKind::payable || found->due != cause.due ||
            found->amount < cause.unpaid || found->cause != cause.cause)
            invalid("Liquidity warning refers to an unknown or incompatible payable");
    } else {
        const auto found = std::find_if(accounts.debts.begin(), accounts.debts.end(),
            [&](const auto& debt) { return debt.id == cause.reference; });
        if (found == accounts.debts.end() || found->due != cause.due)
            invalid("Liquidity warning refers to an unknown or incompatible matured debt");
        const auto draw = std::find_if(accounts.postings.begin(), accounts.postings.end(), [&](const auto& posting) {
            return posting.document_id == cause.reference && posting.credit == Account::debt;
        });
        if (draw == accounts.postings.end() || draw->amount < cause.unpaid || draw->cause != cause.cause)
            invalid("Liquidity warning differs from its retained debt draw");
    }
}
} // namespace

LiquidityState opening_liquidity(OrdinalDay starts_on, std::string owner) {
    LiquidityState state;
    state.owner = std::move(owner); state.starts_on = starts_on; state.observed_on = starts_on;
    validate_liquidity(state);
    return state;
}
namespace {
// These helpers are called only after this public operation has validated the
// same immutable accounts value. Avoid rebuilding its ledger/subledger proof
// for each exposure query and candidate link check within one operation.
LiquidityExposure exposure_unchecked(const AccountsState& accounts, std::optional<OrdinalDay> due_through) {
    LiquidityExposure exposure;
    if (!due_through) return exposure;
    (void)from_ordinal(*due_through);
    for (const auto& invoice : accounts.invoices) {
        if (invoice.kind != InvoiceKind::payable || invoice.due > *due_through) continue;
        const auto unpaid = checked_unit_subtract(invoice.amount, invoice.settled);
        if (unpaid.value() == 0) continue;
        exposure.unpaid_payables = checked_unit_add(exposure.unpaid_payables, unpaid);
        consider(exposure, {LiquidityCauseKind::payable, invoice.id, invoice.due, unpaid, invoice.cause});
    }
    for (const auto& debt : accounts.debts) {
        if (debt.due > *due_through || debt.principal.value() == 0) continue;
        const auto draw = std::find_if(accounts.postings.begin(), accounts.postings.end(), [&](const auto& posting) {
            return posting.document_id == debt.id && posting.credit == Account::debt;
        });
        if (draw == accounts.postings.end()) invalid("Matured debt has no retained draw cause");
        exposure.matured_principal = checked_unit_add(exposure.matured_principal, debt.principal);
        consider(exposure, {LiquidityCauseKind::matured_debt, debt.id, debt.due, debt.principal, draw->cause});
    }
    return exposure;
}
void links_unchecked(const LiquidityState& state, const AccountsState& accounts) {
    validate_liquidity(state);
    if (state.owner != accounts.owner || state.starts_on != accounts.starts_on ||
        state.observed_on != accounts_day(accounts) || state.checked_through != accounts.closed_through ||
        state.exposure != exposure_unchecked(accounts, state.checked_through))
        invalid("Liquidity state differs from its owning accounts or committed payment exposure");
    for (const auto& workout : state.workouts) validate_reference(workout.trigger, accounts);
}
} // namespace
LiquidityExposure liquidity_exposure(const AccountsState& accounts, std::optional<OrdinalDay> due_through) {
    validate_accounts(accounts);
    return exposure_unchecked(accounts, due_through);
}
LiquidityReport liquidity_report(const LiquidityState& state) {
    LiquidityReport report;
    report.status = state.status;
    report.unpaid_payables = state.exposure.unpaid_payables;
    report.matured_principal = state.exposure.matured_principal;
    report.oldest = state.exposure.oldest;
    if (state.status == LiquidityStatus::workout) {
        const auto& workout = state.workouts.back();
        report.deadline = workout.deadline;
        if (!workout.acknowledged) report.pending_warning = workout.id;
    }
    return report;
}
void validate_liquidity(const LiquidityState& state) {
    text_valid(state.owner, 128);
    (void)from_ordinal(state.starts_on); (void)from_ordinal(state.observed_on);
    if (state.observed_on < state.starts_on || state.workouts.size() > liquidity_workout_limit ||
        state.next_workout_id != state.workouts.size() + 1 || state.exposure.unpaid_payables.value() < 0 ||
        state.exposure.matured_principal.value() < 0)
        invalid("Liquidity state has an invalid cursor, bound, identity or exposure");
    if (state.checked_through) {
        (void)from_ordinal(*state.checked_through);
        if (*state.checked_through < state.starts_on || advance_days(*state.checked_through, Days{1}) != state.observed_on)
            invalid("Liquidity observation must follow its completed accounts day");
    } else if (state.observed_on != state.starts_on)
        invalid("Liquidity state advanced without a completed accounts day");
    const auto shortfall = total(state.exposure);
    if ((shortfall.value() > 0) != state.exposure.oldest.has_value())
        invalid("Liquidity exposure must retain the oldest causal obligation");
    if (state.exposure.oldest) {
        validate_cause(*state.exposure.oldest);
        const auto& cause = *state.exposure.oldest;
        if (!state.checked_through || cause.due > *state.checked_through || cause.unpaid >
            (cause.kind == LiquidityCauseKind::payable ? state.exposure.unpaid_payables : state.exposure.matured_principal))
            invalid("Liquidity exposure cause differs from due units or observation");
    }
    std::optional<OrdinalDay> previous_terminal;
    for (std::size_t index = 0; index < state.workouts.size(); ++index) {
        const auto& workout = state.workouts[index];
        (void)from_ordinal(workout.warned_on); (void)from_ordinal(workout.deadline);
        validate_cause(workout.trigger);
        if (workout.id != index + 1 || workout.warned_on <= state.starts_on || workout.warned_on > state.observed_on ||
            workout.deadline != advance_days(workout.warned_on, Days{liquidity_workout_days}) ||
            workout.initial_shortfall < workout.trigger.unpaid || workout.trigger.due >= workout.warned_on ||
            (previous_terminal && workout.warned_on <= *previous_terminal) ||
            (workout.resolved_on && workout.closed_on))
            invalid("Liquidity workout has invalid identity, timing, initial cause or overlapping history");
        if (workout.resolved_on) {
            (void)from_ordinal(*workout.resolved_on);
            if (*workout.resolved_on < workout.warned_on || *workout.resolved_on > state.observed_on ||
                *workout.resolved_on > workout.deadline)
                invalid("Liquidity rescue lies outside its response window");
            previous_terminal = workout.resolved_on;
        } else if (workout.closed_on) {
            (void)from_ordinal(*workout.closed_on);
            if (*workout.closed_on != workout.deadline ||
                *workout.closed_on != state.observed_on || index + 1 != state.workouts.size())
                invalid("Liquidity closure does not follow its complete response window");
            previous_terminal = workout.closed_on;
        } else if (index + 1 != state.workouts.size())
            invalid("An unresolved liquidity workout cannot be followed by another warning");
    }
    switch (state.status) {
    case LiquidityStatus::operating:
        if (shortfall != Cents{0} || (!state.workouts.empty() && !state.workouts.back().resolved_on))
            invalid("Operating liquidity state still has an unresolved due obligation");
        break;
    case LiquidityStatus::workout:
        if (shortfall == Cents{0} || state.workouts.empty() || state.workouts.back().resolved_on ||
            state.workouts.back().closed_on || state.observed_on > state.workouts.back().deadline)
            invalid("Active liquidity workout has no shortfall or has passed its saved window");
        break;
    case LiquidityStatus::closed:
        if (shortfall == Cents{0} || state.workouts.empty() || !state.workouts.back().closed_on)
            invalid("Closed liquidity state has no terminal insolvency cause");
        break;
    default: invalid("Unknown liquidity status");
    }
}
void validate_liquidity_links(const LiquidityState& state, const AccountsState& accounts, bool validate_accounts_state) {
    if (validate_accounts_state) validate_accounts(accounts);
    links_unchecked(state, accounts);
}
void observe_liquidity(LiquidityState& state, const AccountsState& accounts, OrdinalDay observation_day, bool validate_io) {
    if (validate_io) { validate_liquidity(state); validate_accounts(accounts); }
    if (state.status == LiquidityStatus::closed || state.owner != accounts.owner || state.starts_on != accounts.starts_on ||
        observation_day != accounts_day(accounts) || !accounts.closed_through ||
        (state.status == LiquidityStatus::workout && observation_day > state.workouts.back().deadline) ||
        (observation_day != state.observed_on && observation_day != advance_days(state.observed_on, Days{1})))
        invalid("Liquidity observation must inspect the next completed accounts day of an operating company");
    if (observation_day == state.observed_on && state.checked_through != accounts.closed_through)
        invalid("Liquidity command refresh must preserve the current observation boundary");
    auto next = state;
    next.observed_on = observation_day;
    next.checked_through = accounts.closed_through;
    apply_exposure(next, exposure_unchecked(accounts, next.checked_through));
    if (validate_io) links_unchecked(next, accounts);
    state = std::move(next);
}
void refresh_liquidity(LiquidityState& state, const AccountsState& accounts) {
    validate_liquidity(state); validate_accounts(accounts);
    if (state.status == LiquidityStatus::closed || state.owner != accounts.owner || state.starts_on != accounts.starts_on ||
        state.observed_on != accounts_day(accounts) || state.checked_through != accounts.closed_through)
        invalid("Liquidity command refresh must preserve the current observation boundary");
    auto next = state;
    apply_exposure(next, exposure_unchecked(accounts, next.checked_through));
    links_unchecked(next, accounts);
    state = std::move(next);
}
void close_liquidity_workout(LiquidityState& state, const AccountsState& accounts) {
    validate_liquidity_links(state, accounts);
    if (state.status != LiquidityStatus::workout || state.observed_on < state.workouts.back().deadline ||
        total(state.exposure) == Cents{0})
        invalid("Only an unresolved workout at its deadline morning can close");
    auto next = state;
    next.workouts.back().closed_on = next.observed_on;
    next.status = LiquidityStatus::closed;
    links_unchecked(next, accounts);
    state = std::move(next);
}
void acknowledge_liquidity_warning(LiquidityState& state, std::uint64_t workout_id) {
    validate_liquidity(state);
    if (state.status != LiquidityStatus::workout || state.workouts.back().id != workout_id ||
        state.workouts.back().acknowledged)
        invalid("Liquidity warning is missing, stale or already acknowledged");
    auto next = state;
    next.workouts.back().acknowledged = true;
    validate_liquidity(next);
    state = std::move(next);
}
} // namespace pgp::core
