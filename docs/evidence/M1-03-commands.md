# M1-03 command and morning-state foundation

Implemented 2026-11-04. This is a command/state foundation exercised by engineering
fixtures. It is not the M2 business simulation or a playable campaign. M0-03's
native/deployment qualification remains open; this report does not close that
prerequisite or claim complete M1-03 acceptance.

## Owning API and boundaries

[`simulation.hpp`](../../src/core/include/pgp/core/simulation.hpp) and
[`simulation.cpp`](../../src/core/simulation.cpp) add a Qt-free `Simulation` with
one worker-owner contract. Callers submit a `Command` carrying the revision they
observed and a typed action variant:

- `ReserveFunds` reserves a positive amount of constant-2014 cents.
- `ConsumeReservation` permanently consumes an authorized reserved amount.
- `CancelReservation` releases only unconsumed, reversible funds.
- `AdvanceDay` settles the current foundation day and prepares the next morning.
- `SetPaused` changes the time-control state without advancing time.
- `ResolveOpeningChoice` accepts or declines the matching suspended opening action.

A `CommandResult` returns a stable `CommandReason`, current revision, explanation
and, on acceptance, the command receipt ID and optional new reservation ID.
Rejections have no committed receipt. Each accepted command increments the state
revision once; automatic opening consequences receive their own monotonically
identified causal receipts in that same transaction. The caller must deliver
command results independently of coalesced presentation snapshots.

Validation and mutation operate on a state copy. Only successful complete work
replaces the authoritative state. Stale revisions, invalid inputs, unavailable
funds, dependencies, irreversible commitments, calendar/arithmetic bounds and
count limits return stable refusal codes. Failure preserves the complete state,
including receipts, identifiers, cursor and all seven saved random streams.

A reservation is a cash commitment, not a recognized expense. Consuming it
reduces cash and the reserved balance together; cancelling its remainder never
increases cash or erases prior consumption. Signed irreversible terms remain
binding. Pending opening work protects its reserved funds from direct
consumption or cancellation; loaded state cannot promise more future opening
consumption than the reservation holds. M2-01 supplies the later balanced domain
ledger rather than treating these foundation changes as revenue/expense reports.

## Morning cursor and copied state

`SimulationState` is a copyable in-memory persistence boundary containing the
revision, date, exclusive campaign end, phase, pause state, cash, reservations,
next identifiers, ordered opening actions, opening cursor, suspended choice,
causal receipts and named RNG states. Constructor validation checks canonical
identities/order, units, bounds, references, phase consistency and future
commitment totals before exposing state.

An `opening` state processes due actions in `(due date, action ID)` order before
becoming `work_ready`. A required decision changes the phase to
`awaiting_choice`, retains the current opening cursor, and pauses time. Ordinary
planning, cancellation and day advancement are refused until that choice is
resolved. Acceptance/decline continues the remaining same-morning ordered work.
Restoring `work_ready` or `awaiting_choice` copies does not replay opening work.
The final campaign date is exclusive: reaching it sets `finished` and does not
execute a new morning's deliveries.

`SimulationSnapshot` is published as `shared_ptr<const SimulationSnapshot>` and
contains only date/revision/phase/time controls, visible cash/available cash,
pending choice ID and reservation rows. Rows carry stable ID, name, unconsumed
reserved cents, consumed cents, reversibility and status. Prior snapshots remain
unchanged after later commands. Snapshots expose no RNG state, future opening
queue or future random outcomes. Snapshot queries and current foundation actions
draw no random numbers.

## Deterministic bounds

The foundation bounds reservations and opening actions at 4,096 each and causal
receipts at 65,536. Reaching a limit refuses the entire transaction; it does not
truncate old causes, discard required work, wrap identifiers or silently change
dates. Later history compaction/persistence must preserve the relevant records
under the save/history contract. These limits are engineering guards, not a
claimed century-campaign capacity result.

## Observed verification

On the existing arm64 Mac, the following direct compiler check completed with
exit code 0:

```sh
/usr/bin/clang++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Isrc/core/include src/core/simulation.cpp src/core/calendar.cpp \
  src/core/random.cpp src/core/checked_arithmetic.cpp \
  tests/simulation_test.cpp -o .work/simulation-test
.work/simulation-test
```

The executable reported: `Simulation command atomicity, morning continuation,
cancellation and bounds pass.` The assertions exercise:

- stale and over-budget commands, invalid units and exact whole-state rejection;
- reservation-versus-expense semantics, partial consumption/cancellation,
  irreversible agreements and refusal to consume or cancel dependent funds;
- initial and next-morning delivery order, required choice gating, acceptance,
  decline, later siblings, copied-state continuation and no duplicated delivery;
- exclusive campaign-end behavior and restored final state;
- immutable old snapshots and unchanged random streams;
- maximum signed cash, revision/identifier exhaustion and receipt-count refusal;
- atomic rollback when receipt exhaustion occurs after the advance receipt and
  a due delivery would otherwise have changed cash;
- malformed prepared cursors, duplicate IDs, over-reservation and impossible
  future opening commitments.

Integrated build/CTest and bridge evidence are recorded by the parent integration
in the implementation log. The direct result above does not certify native
Windows/Linux parity, deployed Qt, running QML focus/scroll behavior, a file save
loader or game balance. No component/version identity was changed and no
production save format was introduced.

## Integrated worker and persistent models

`src/bridge` adds a `SimulationWorker` moved to one Qt thread and a persistent
`ReservationModel` owned by the GUI thread. Worker commands use queued delivery;
running timer snapshots are capped at 10 Hz, with immediate snapshots and a
separate result for every command. Required opening choices stop time. A driver
fault emits `executionHalted` even if receipt/revision exhaustion also prevents
recording pause; a consumer must preserve that required alert rather than
coalescing it with obsolete snapshots.

The model uses insert/remove/move and changed-role notifications, preserves IDs
and persistent indexes/selection, rejects obsolete snapshots, and allows reset
only through explicit campaign replacement. Authoritative IDs and cent amounts
are exposed as decimal strings to avoid QML numeric rounding. The model rejects
updates from another thread and duplicate row IDs before modifying rows.

Desktop Release/Debug each pass 26 CTests. The bridge test supplies 200 updates
with sorting, insertions/removals, role changes and a retained selection/index
anchor. A real queued worker test checks stale overspend, immutable prior
snapshots, running/pause, required choices and explicit halt on exhausted
revision state. These are reusable bridge contracts: actual room-view keyboard/
edit focus, scroll and 200% rendering remain M1-05 qualification work.
