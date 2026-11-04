# Playability and evaluation checkpoints

Updated 2026-11-04. The game is in foundation implementation. The development
probe displays interface/resource/audio checks; it does not contain a campaign.
The [backlog](BACKLOG.md) records acceptance gates and the
[implementation log](IMPLEMENTATION_LOG.md) records measured progress.

| Checkpoint | What can be evaluated | State |
| --- | --- | --- |
| Foundation demonstration | Atomic cash reservations, stale-command refusals, irreversible commitments and morning-choice continuation | Runnable with `pgp-headless --foundation-demo` |
| Event and save demonstration | Real event execution, nested delays/choices, owned modifiers, local save/reload and exact resumed state | Runnable with `pgp-headless --flow-demo FILE [--save FILE.pgpsave]` |
| Opening accounts demonstration | Two actual calendar months of costs, one data license, balanced reports and exact pre-settlement save/resume | Runnable with `pgp-headless --accounts-demo FILE`; [evidence](evidence/M2-01-accounts.md) |
| Shared capacity demonstration | 60-day concurrent serving/training, protected shared compute/staff, separate support costs and exact reload | Runnable with `pgp-headless --capacity-demo FILE`; [evidence](evidence/M2-02-capacity.md) |
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
./make.sh --core-only --jobs 2
build/core-release/bin/pgp-headless --foundation-demo
build/core-release/bin/pgp-headless --validate-content tests/content/fixtures/foundation.json
build/core-release/bin/pgp-headless --flow-demo tests/content/fixtures/foundation.json
build/core-release/bin/pgp-headless --accounts-demo tests/content/fixtures/foundation.json
build/core-release/bin/pgp-headless --capacity-demo tests/content/fixtures/foundation.json
# Explicit optional save path; use a chosen local .pgpsave filename:
build/core-release/bin/pgp-headless --flow-demo tests/content/fixtures/foundation.json --save /chosen/local/demo.pgpsave
```

These commands run engineering examples. Only the explicit `--save` invocation
writes the selected local save; the graphical application is not launched.
Content fixtures are developer evidence and excluded from any future game
payload. The business scenario and playable UI remain upcoming work.

## Batch tracking

Track both accepted tasks and implemented work whose prerequisite qualification
is still open. M1-02 builds the typed content boundary; the M1-03 batch adds core
transactions, immutable snapshots, a serialized worker and persistent entity
rows. M1-03 remains open while M0-03 native/deployment acceptance is outstanding.
This distinguishes useful engineering progress from a passed platform gate.
M1-06 now executes the current typed schema and M1-04 persists its whole state.
Application policy performs recovery saves before irreversible actions and
monthly/final autosaves, preserving live play on IO errors. The broader flow
language still needs related-entity selectors/quantifiers; scoped variables and
checked expressions are implemented. Weighted narrative branches now retain
their selected outcomes across delays, choices and saves; the
[test evidence](evidence/M1-06-weighted-branches.md) is an engineering check.
Daily duration predicates also preserve their opening-sample start/reset state
and exact thresholds through choices and reload; see [duration evidence](evidence/M1-06-duration.md).
M1-07 then adds
built-in occurrence detection/dispatch. M2-01/02 implement opening accounts and
shared capacity; M2-03/04 complete contracts and the first business loop.
The UI shell can proceed alongside those core modules. Accepted-task counts
remain conservative while the full contracts and prerequisites are open.
