#include <pgp/core/simulation.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>

using namespace pgp::core;
namespace {
void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error{std::string{message}};
}
CommandResult apply(Simulation& simulation, CommandAction action) {
    return simulation.submit({simulation.snapshot()->revision, std::move(action)});
}
void refused_unchanged(Simulation& simulation, Command command, CommandReason reason) {
    const auto before = simulation.checkpoint();
    const auto result = simulation.submit(command);
    require(result.reason == reason, "Unexpected stable command refusal reason");
    require(!result.receipt_id && !result.reservation_id, "A rejection claimed a committed receipt");
    require(simulation.checkpoint() == before,
            "Rejected command changed cash, RNG, cursor, revision, receipts or commitments");
}
void commands_and_cancellation() {
    Simulation simulation{Cents{1000}};
    const auto initial = simulation.snapshot();
    require(initial->phase == DayPhase::work_ready && initial->paused &&
            initial->revision == 0 && initial->available == Cents{1000},
            "Initial morning is prepared before paused decisions");
    const auto reserved = apply(simulation, ReserveFunds{"experiment", Cents{600}});
    require(reserved.accepted() && reserved.reservation_id == 1 && reserved.receipt_id == 1,
            "Accepted command generates stable identities");
    require(simulation.snapshot()->cash == Cents{1000} &&
            simulation.snapshot()->available == Cents{400}, "Reservation is not an expense");
    require(initial->reservations.empty() && initial->revision == 0,
            "Previously published immutable snapshot changed");
    refused_unchanged(simulation, {0, ReserveFunds{"stale", Cents{600}}},
                      CommandReason::stale_revision);
    refused_unchanged(simulation, {1, ReserveFunds{"overspend", Cents{500}}},
                      CommandReason::insufficient_funds);
    refused_unchanged(simulation, {1, ReserveFunds{"invalid", Cents{-1}}},
                      CommandReason::invalid_argument);
    refused_unchanged(simulation, {1, ConsumeReservation{1, Cents{601}}},
                      CommandReason::insufficient_funds);
    require(apply(simulation, ConsumeReservation{1, Cents{250}}).accepted(), "Consume funds");
    require(apply(simulation, CancelReservation{1}).accepted(), "Cancel unconsumed funds");
    const auto after = simulation.snapshot();
    require(after->cash == Cents{750} && after->available == Cents{750} &&
            after->reservations[0].consumed == Cents{250} &&
            after->reservations[0].reserved == Cents{0} &&
            after->reservations[0].status == ReservationStatus::cancelled,
            "Cancellation refunded consumed information or failed to release unconsumed funds");
    refused_unchanged(simulation, {after->revision, CancelReservation{1}}, CommandReason::invalid_status);
    const auto fixed = apply(simulation, ReserveFunds{"signed data license", Cents{100}, false});
    require(fixed.accepted(), "Reserve an irreversible agreement");
    refused_unchanged(simulation, {simulation.snapshot()->revision, CancelReservation{2}},
                      CommandReason::irreversible_commitment);
    require(apply(simulation, ConsumeReservation{2, Cents{100}}).accepted(), "Deliver signed license");
    require(simulation.snapshot()->reservations[1].status == ReservationStatus::completed,
            "Completed reservation remains active");
    require(simulation.checkpoint().random == RandomStreams{0}.snapshot(),
            "Foundation commands or snapshot reads consumed random draws");
    static_assert(std::is_const_v<typename decltype(initial)::element_type>);
}
SimulationState morning_fixture() {
    SimulationState state;
    state.cash = Cents{1000};
    state.end_date = OrdinalDay{3};
    state.next_reservation_id = 2;
    state.reservations.push_back({1, "ordered commitment", Cents{600}, Cents{0},
                                  true, ReservationStatus::active});
    state.opening_actions = {
        {11, OrdinalDay{0}, 1, Cents{100}, false, "initial_delivery"},
        {12, OrdinalDay{1}, 1, Cents{100}, false, "next_morning_delivery"},
        {13, OrdinalDay{1}, 1, Cents{150}, true, "opening_choice_delivery"},
        {14, OrdinalDay{1}, 1, Cents{50}, false, "choice_followup_delivery"},
        {15, OrdinalDay{3}, 1, Cents{200}, false, "must_not_execute_after_deadline"}
    };
    return state;
}
void opening_cursor_and_restore() {
    Simulation simulation{morning_fixture()};
    require(simulation.snapshot()->cash == Cents{900} &&
            simulation.checkpoint().opening_cursor == 1 &&
            simulation.checkpoint().receipts.size() == 1,
            "Initial due delivery was not committed once before decisions");
    Simulation prepared{simulation.checkpoint()};
    require(prepared.checkpoint() == simulation.checkpoint(), "Restoring prepared morning replayed delivery");
    require(apply(simulation, AdvanceDay{}).accepted(), "Advance to required opening choice");
    const auto opening = simulation.snapshot();
    require(opening->date == OrdinalDay{1} && opening->phase == DayPhase::awaiting_choice &&
            opening->pending_choice == 13 && opening->cash == Cents{800} && opening->paused,
            "Opening delivery/choice order is wrong");
    refused_unchanged(simulation, {opening->revision, AdvanceDay{}}, CommandReason::morning_not_ready);
    refused_unchanged(simulation, {opening->revision, ReserveFunds{"blocked", Cents{1}}},
                      CommandReason::morning_not_ready);
    refused_unchanged(simulation, {opening->revision, CancelReservation{1}},
                      CommandReason::morning_not_ready);
    refused_unchanged(simulation, {opening->revision, SetPaused{false}}, CommandReason::morning_not_ready);
    refused_unchanged(simulation, {opening->revision, ResolveOpeningChoice{999, true}},
                      CommandReason::wrong_choice);
    Simulation restored{simulation.checkpoint()};
    require(restored.checkpoint() == simulation.checkpoint(), "Suspended choice replayed opening work");
    require(apply(simulation, ResolveOpeningChoice{13, true}).accepted() &&
            apply(restored, ResolveOpeningChoice{13, true}).accepted(), "Resolve saved choice");
    require(restored.checkpoint() == simulation.checkpoint(), "Checkpoint continuation diverged");
    require(simulation.snapshot()->phase == DayPhase::work_ready &&
            simulation.snapshot()->cash == Cents{600} && simulation.checkpoint().opening_cursor == 4,
            "Choice or later ordered opening delivery did not execute exactly once");
    refused_unchanged(simulation,
        {simulation.snapshot()->revision, ResolveOpeningChoice{13, true}}, CommandReason::no_pending_choice);
    require(apply(simulation, AdvanceDay{}).accepted(), "Advance past resolved opening");
    require(simulation.snapshot()->cash == Cents{600}, "Resume repeated a delivery");
    require(apply(simulation, AdvanceDay{}).accepted(), "Settle last campaign day");
    require(simulation.snapshot()->date == OrdinalDay{3} &&
            simulation.snapshot()->phase == DayPhase::finished && simulation.snapshot()->cash == Cents{600},
            "Campaign executed a new morning at its exclusive ending date");
    refused_unchanged(simulation, {simulation.snapshot()->revision, AdvanceDay{}},
                      CommandReason::campaign_finished);
    Simulation final_restore{simulation.checkpoint()};
    require(final_restore.checkpoint() == simulation.checkpoint(), "Final cursor failed restore");

    Simulation declined{morning_fixture()};
    require(apply(declined, AdvanceDay{}).accepted(), "Reach choice to decline");
    require(apply(declined, ResolveOpeningChoice{13, false}).accepted(), "Decline choice");
    require(declined.snapshot()->cash == Cents{750} &&
            declined.checkpoint().opening_cursor == 4, "Declining charged choice or skipped its sibling");
}
void atomic_opening_failure_and_limits() {
    Simulation opening{morning_fixture()};
    refused_unchanged(opening, {0, CancelReservation{1}}, CommandReason::dependent_commitment);
    refused_unchanged(opening, {0, ConsumeReservation{1, Cents{1}}},
                      CommandReason::dependent_commitment);
    auto state = opening.checkpoint();
    while (state.receipts.size() < simulation_receipt_limit - 1) {
        state.receipts.push_back({state.next_receipt_id++, 0, OrdinalDay{0},
                                  "retained_cause", 0, Cents{0}});
    }
    Simulation simulation{state};
    refused_unchanged(simulation, {0, AdvanceDay{}}, CommandReason::limit_exceeded);

    Simulation maximum{Cents{std::numeric_limits<std::int64_t>::max()}};
    require(apply(maximum, ReserveFunds{"whole balance", Cents{std::numeric_limits<std::int64_t>::max()}})
                .accepted(), "Reserve signed maximum without overflowing an intermediate");
    refused_unchanged(maximum, {1, ReserveFunds{"one cent too many", Cents{1}}},
                      CommandReason::insufficient_funds);

    Simulation empty{Cents{1000}};
    auto revision_limit = empty.checkpoint();
    revision_limit.revision = std::numeric_limits<std::uint64_t>::max();
    Simulation exhausted_revision{revision_limit};
    refused_unchanged(exhausted_revision,
        {revision_limit.revision, ReserveFunds{"unchanged", Cents{1}}}, CommandReason::arithmetic_overflow);
    auto identity_limit = empty.checkpoint();
    identity_limit.next_reservation_id = std::numeric_limits<std::uint64_t>::max();
    Simulation exhausted_identity{identity_limit};
    refused_unchanged(exhausted_identity, {0, ReserveFunds{"unchanged", Cents{1}}},
                      CommandReason::limit_exceeded);
    auto receipt_limit = empty.checkpoint();
    for (std::size_t index = 0; index < simulation_receipt_limit; ++index) {
        receipt_limit.receipts.push_back({static_cast<std::uint64_t>(index + 1), 0,
            OrdinalDay{0}, "retained_cause", 0, Cents{0}});
    }
    receipt_limit.next_receipt_id = static_cast<std::uint64_t>(simulation_receipt_limit + 1);
    Simulation exhausted_receipts{receipt_limit};
    refused_unchanged(exhausted_receipts, {0, ReserveFunds{"unchanged", Cents{1}}},
                      CommandReason::limit_exceeded);
    require(exhausted_receipts.checkpoint().receipts.front().cause == "retained_cause",
            "Overflow discarded a required prior transaction cause");
}
void malformed_checkpoint() {
    auto state = morning_fixture();
    state.phase = DayPhase::work_ready;
    try {
        Simulation invalid{state};
        throw std::runtime_error{"Accepted a ready morning with unhandled delivery"};
    } catch (const std::invalid_argument&) {}
    state = morning_fixture();
    state.reservations.push_back(state.reservations.front());
    try {
        Simulation invalid{state};
        throw std::runtime_error{"Accepted duplicate stable reservation identities"};
    } catch (const std::invalid_argument&) {}
    state = morning_fixture();
    state.opening_actions[1].amount = Cents{501};
    try {
        Simulation invalid{state};
        throw std::runtime_error{"Accepted impossible future opening commitments"};
    } catch (const std::invalid_argument&) {}
    state = morning_fixture();
    state.cash = Cents{599};
    try {
        Simulation invalid{state};
        throw std::runtime_error{"Accepted over-reserved loaded state"};
    } catch (const std::invalid_argument&) {}
}
} // namespace
int main() {
    try {
        commands_and_cancellation();
        opening_cursor_and_restore();
        atomic_opening_failure_and_limits();
        malformed_checkpoint();
        std::cout << "Simulation command atomicity, morning continuation, cancellation and bounds pass.\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
