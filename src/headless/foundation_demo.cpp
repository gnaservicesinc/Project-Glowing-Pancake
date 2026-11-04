#include "foundation_demo.hpp"

#include <pgp/core/simulation.hpp>

#include <ostream>
#include <stdexcept>

void run_foundation_demo(std::ostream& output) {
    using namespace pgp::core;
    Simulation simulation{Cents{100'000}};
    const auto initial = simulation.snapshot();
    auto apply = [&](CommandAction action) {
        auto result = simulation.submit({simulation.snapshot()->revision, std::move(action)});
        if (!result.accepted()) throw std::runtime_error(result.explanation);
        return result;
    };
    const auto reservation = apply(ReserveFunds{"Reversible reservation", Cents{60'000}});
    const auto stale = simulation.submit({initial->revision, ReserveFunds{"Stale request", Cents{60'000}}});
    if (stale.reason != CommandReason::stale_revision) throw std::runtime_error("Stale command was accepted");
    (void)apply(ConsumeReservation{*reservation.reservation_id, Cents{20'000}});
    (void)apply(CancelReservation{*reservation.reservation_id});
    output << "Foundation engineering demonstration (constant-2014 cents)\n"
           << "Reserve 60000; consume 20000; cancel unused remainder.\n"
           << "Stale command: " << to_string(stale.reason) << '\n'
           << "Cash: " << simulation.snapshot()->cash.value()
           << "; available: " << simulation.snapshot()->available.value() << '\n';

    const auto committed = apply(ReserveFunds{"Signed commitment", Cents{30'000}, false});
    const auto cancellation = simulation.submit({simulation.snapshot()->revision,
                                                CancelReservation{*committed.reservation_id}});
    if (cancellation.reason != CommandReason::irreversible_commitment)
        throw std::runtime_error("Irreversible commitment was cancelled");
    auto saved = simulation.checkpoint();
    saved.opening_actions.push_back({1, OrdinalDay{1}, *committed.reservation_id, Cents{30'000},
                                    true, "Due commitment decision"});
    simulation = Simulation{std::move(saved)};
    (void)apply(AdvanceDay{});
    if (!simulation.snapshot()->pending_choice) throw std::runtime_error("Opening choice was skipped");
    output << "Next morning: " << iso_date(simulation.snapshot()->date)
           << "; phase: " << to_string(simulation.snapshot()->phase) << '\n';
    auto interrupted = simulation.checkpoint();
    simulation = Simulation{std::move(interrupted)};
    (void)apply(ResolveOpeningChoice{*simulation.snapshot()->pending_choice, true});
    const auto prepared = simulation.snapshot();
    simulation = Simulation{simulation.checkpoint()};
    if (simulation.snapshot()->cash != Cents{50'000} || simulation.snapshot()->phase != DayPhase::work_ready)
        throw std::runtime_error("Morning restore replayed or lost work");
    output << "Resume opening choice once; cash: " << prepared->cash.value()
           << "; phase: " << to_string(prepared->phase) << '\n'
           << "Demonstrated: atomic commands, cancellation boundaries and copied-state resume.\n"
           << "Business simulation, save files and playable presentation are later milestones.\n";
}
