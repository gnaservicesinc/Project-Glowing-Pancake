# M1-06 shared flow runtime and copied continuation evidence

Implemented 2026-11-04 against baseline 1.2. This report covers the executable
foundation flow vocabulary and its typed compiler, not a playable business
scenario or native graphics/deployment acceptance. M1-03's platform prerequisite
remains open. The integration log records the complete parent build/CI results.

## One runtime, one authoritative state

`pgp::runtime` sits above `pgp::content` and `pgp::core`, with no Qt, JSON types,
file handles or network facilities in its public runtime API. It consumes the
same normalized typed definitions intended for the game and Studio. Runtime
construction recompiles supplied definitions against its actual installed
operation list; a registered but unavailable effect fails before execution.

`WorldState.simulation` is the sole owner of company cash, date, revision and
the seven saved random streams. The foundation's other fact values are checked
against registry metadata; a duplicate cash entry is refused. Company, world
and state bindings carry explicit stable IDs. A scenario's `enabled_flows` names
its trigger roots, so a reusable subflow is not independently activated merely
because it exists in the collection. The all-roots default is an engineering
fixture mode, not a player mod-loading mechanism.

Every ordinary command enters through `Runtime::submit`, which refuses work
until core opening work and content opening work are ready. A required dialogue
pauses time and leaves a saved opening continuation. The underlying core's
`work_ready` flag cannot bypass this additional content gate. A raw saved core
opening can resume delivery consumption, stop at its required choice and finish
content preparation without replaying an earlier delivery.

## Execution and persistence boundary

The plain `Checkpoint` contains world state and `RuntimeState`: monotonic
activation/receipt/presentation IDs; definition revisions; bound scopes;
current flow/node; started date; delay start/due dates; up to eight sequence or
subflow frames; active lifecycle identities; occurrence counts, cooldown start,
saved interval anchor/day/index; owned modifiers; effect receipts; and resolved
or pending localized presentations/choices. Each frame records its parent flow,
sequence/call node, next child index, return node and binding.

Execution visits numbered children in order. End nodes return through saved
frames rather than skipping later siblings. Delays retain their authored delay
node and exact start/due relation; a dialogue retains the selected/resolved
presentation identity. Resolution resumes the saved node/frame state. Restoring
or inspecting a checkpoint performs no live writes or random draws.

Restore validation checks definition revisions, IDs, counts, scope binding,
authored node/choice shape, exact delay timing and the complete frame chain.
Each frame must be reachable within its parent's active numbered child/call
region; an unrelated flow or sibling cannot be substituted. Noncompleted
activations must be their occurrence scope's sole active activation. Completed
and active-continuous entries must be on an end node. Cash receipts conserve
their explicitly recorded counterparty delta.

All five lifecycle modes execute through this runtime:

- Once fires at most once per bound scope.
- Recurring uses the authored positive cooldown, occurrence limit and
  cooldown-while-still-eligible policy.
- Interval resolves its relative/fixed/month anchor once and saves both the
  first due date and preferred calendar day. Jan 31 → Feb 28 → Mar 31 does not
  drift to the 28th. Missed or ineligible slots are skipped without backfill.
- Scheduled evaluates eligibility on its authored due date and applies skip
  semantics when that date is missed or ineligible.
- Continuous owns modifiers by activation/scope/key, applies entry once, leaves
  an unchanged active tick alone, and removes only its own contribution on a
  natural exit. Re-entry is explicit. Synthetic cleanup is separately labeled
  in the result trace and is exercised on a copied runtime.

The initial schema supports the foundation's scalar/all/any/not trigger shapes,
news, hidden effects, dialogue/results, delays, sequences and subflow nodes.
Future vocabulary requires a typed schema and an actual owning implementation;
arbitrary scripts, remote content and executable plugins have no runtime path.
No stochastic branch node exists in this initial schema. These deterministic
flows preserve the authoritative random streams unchanged; later seeded event
sources use their saved owning stream under M1-07.

## Transaction and explanation rules

Each public operation validates its observed revision and executes on copied
world/runtime state. Failure leaves cash, reservations, receipts, RNG, IDs,
date, lifecycle counters and continuations unchanged. Already accepted prior
choices are separate committed operations and cannot be rolled back by a later
failure. Hard limits return a recoverable typed error rather than truncate
required work, advance the date or hang; the calling driver must halt on that
error just as the existing worker does for command failures.

The foundation dispatches five registered operations: record a decision marker,
open a referenced negotiation without signing it, transfer cash, and register/
remove an owned modifier. Cash transfers use a caller-provided authorized
counterparty pool and equal opposite receipts. An unfunded grant or spending
reserved company cash is refused; no money is minted. This is a conservation
fixture boundary, not the complete M2 balanced accounts/obligations system.

Results expose ordered paths and safely redacted player-known clause traces.
Full author-fixture clause inspection remains the separate pure compiler
inspector. Receipts retain actual ordered target/before/after changes. Successful
operations mark traces as committed. A rejected operation marks paths as
rolled-back candidates and prefixes candidate clause paths; its partial work
cannot be presented as an actual committed consequence.

## Observed verification

The parent integration observed the new runtime target compiling with the
project's strict warnings-as-errors and passing its first integrated runtime,
CLI and save-continuation checks. Subsequent targeted restore/trace regressions
are included in the final parent suite; its implementation log records exact
commands, assertion counts and native CI evidence.

`tests/runtime/runtime_test.cpp` exercises the seven normalized engineering
flows, same-morning choice gating, stale/invalid choices, two nested saved
sequence frames, every-day copied continuation parity, delayed inner-before-
outer effect order, unchanged RNG, recurring cooldown/limit, calendar clamping,
scheduled skip, concurrent modifiers, natural exit, clean re-entry and labeled
synthetic copied cleanup. Financial cases check a rejected second effect rolls
back the first, an authorized transfer conserves both balances, receipt balance
validation and reservation protection. A bounded positive-delay loop refuses
its next step without losing its current date/continuation. Corrupt frame,
timing, duplicate cash, prepared cursor, missing active owner and continuous
node-boundary examples are refused. Raw core opening/choice resume is also
covered. A reusable subflow delays and reloads inside its call frame, then
returns once to its caller's later effects; explicit root selection prevents a
second independent child activation.

The parent adds file-save/reload parity and an inspectable flow demo using these
same runtime methods. That demonstrates real flow execution and local
continuation, while the first business economics remain M2 and the player-facing
Office/Lab slice remains M3. No graphical app, editor, product package or release
was created by these tests.
