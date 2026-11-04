# Playability and evaluation checkpoints

Updated 2026-11-04. The game is in foundation implementation. The development
probe displays interface/resource/audio checks; it does not contain a campaign.
The [backlog](BACKLOG.md) records acceptance gates and the
[implementation log](IMPLEMENTATION_LOG.md) records measured progress.

| Checkpoint | What can be evaluated | State |
| --- | --- | --- |
| Foundation demonstration | Atomic cash reservations, stale-command refusals, irreversible commitments and morning-choice continuation | Runnable with `pgp-headless --foundation-demo` |
| M2-04: first business simulation | A three-year command-line scenario, reconciled accounts, finite capacity, contract economics, solvency and recovery | Requires foundation flows/persistence and M2-01 through M2-04 |
| M3-02: first player-facing scenario | Office/Lab decisions, data/research/training/evaluation, contracts, news and choices, start to ending | Requires M2, the UI shell, E1a authoring/inspection and the baseline event pack |
| M3-03: feature evaluation build | The scenario with delegation, warnings, recovery and scoring | Earliest complete slice for ordinary feature feedback |
| M3-04: validated first playable | Five independent 30–45-minute sessions; three complete unaided; material comprehension problems fixed | Human testing follows the playable build |

There is no calendar estimate inferred from task counts: tasks vary substantially
in size, and native packaging, authors and players have separate evidence gates.
Full century simulation, politics, the complete graphical editor canvas and final
art are later milestones. The full editor canvas remains required before M4;
E1a forms/inspection are required before the slice presentation.

## Inspect the foundation now

```sh
./make.sh --core-only --test --jobs 2
build/core-release/bin/pgp-headless --foundation-demo
build/core-release/bin/pgp-headless --validate-content tests/content/fixtures/foundation.json
```

These commands run engineering examples. They do not create a personal save or
launch the graphical application. Content fixtures are developer evidence and
are excluded from any future game payload. Save-file IO, the business scenario
and the playable UI remain upcoming work.

## Batch tracking

Track both accepted tasks and implemented work whose prerequisite qualification
is still open. M1-02 builds the typed content boundary; the M1-03 batch adds core
transactions, immutable snapshots, a serialized worker and persistent entity
rows. M1-03 remains open while M0-03 native/deployment acceptance is outstanding.
This distinguishes useful engineering progress from a passed platform gate.
The next gameplay-enabling foundation is M1-06's shared flow runtime, followed by
M1-04 file persistence and M1-07 event dispatch. M2 then supplies the first
business loop. The UI shell can proceed alongside those core modules.
