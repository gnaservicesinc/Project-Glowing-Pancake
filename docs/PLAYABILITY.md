# Playability and evaluation checkpoints

Updated 2026-11-05. The game is in foundation implementation. The development
probe covers interface, resources and audio. A separate desktop shell opens
engineering campaigns through the session layer.
The [backlog](BACKLOG.md) records acceptance gates and the
[implementation log](IMPLEMENTATION_LOG.md) records measured progress.

| Checkpoint | What can be evaluated | State |
| --- | --- | --- |
| Foundation demonstration | Atomic cash reservations, stale-command refusals, irreversible commitments and morning-choice continuation | Runnable with `pgp-headless --foundation-demo` |
| Event and save demonstration | Real event execution, nested delays/choices, owned modifiers, local save/reload and exact resumed state | Runnable with `pgp-headless --flow-demo FILE [--save FILE.pgpsave]` |
| Opening accounts demonstration | Two actual calendar months of costs, one data license, balanced reports and exact pre-settlement save/resume | Runnable with `pgp-headless --accounts-demo FILE`; [evidence](evidence/M2-01-accounts.md) |
| Shared capacity demonstration | 60-day concurrent serving/training, protected shared compute/staff, separate support costs and exact reload | Runnable with `pgp-headless --capacity-demo FILE`; [evidence](evidence/M2-02-capacity.md) |
| Enterprise market demonstration | Data-defined contract demand, fees, actual serving and exact pre-settlement save/reload through two months | Runnable with `pgp-headless --market-demo FILE`; [evidence](evidence/M2-03-market.md) |
| M2-04: first business simulation | A three-year command-line scenario, reconciled accounts, finite capacity, contract economics, solvency and recovery | Runnable with `pgp-headless --business-run FILE expansion/baseline/rescue`; [engineering evidence](evidence/M2-04-business.md) |
| M1-05: campaign room shell | Office/Lab/Inbox, time, required decisions, project inspector and local saves/preferences | Runnable with `pgp-game --content FILE`; [Mac evidence](evidence/M1-05-room-shell.md) |
| M3-01: lab development loop | Four data offers, six research nodes, finite training, lineage, held-out findings and targeted recovery | Runnable with `pgp-headless --lab-demo assets/content/first-playable.json`; [lab contract](../src/core/LAB.md) |
| M3-02: first player-facing scenario | Office/Lab decisions, data/research/training/evaluation, contracts, news and choices, start to ending | Active next milestone (`M3-01` -> `M3-05` -> `M3-02`); decoupled from remaining E1a Studio steps |
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
build/core-release/bin/pgp-headless --market-demo tests/content/fixtures/market.json
build/core-release/bin/pgp-headless --business-run tests/content/fixtures/business.json expansion
# Explicit optional save path; use a chosen local .pgpsave filename:
build/core-release/bin/pgp-headless --flow-demo tests/content/fixtures/foundation.json --save /chosen/local/demo.pgpsave
```

These commands run engineering examples. Only the explicit `--save` invocation
writes the selected local save; the graphical application is not launched.
Content fixtures are developer evidence and excluded from any future game
payload. The three-year business fixture starts with an engineering product;
research/models now run through the separate lab demonstration. Deployment qualification and the full player-facing scenario remain upcoming work.
See [desktop launch instructions](BUILDING.md#open-the-campaign-shell) for the room shell.

## Batch tracking

The current tracker counts Mac feature implementations with focused checks.
The user deferred native platform and release qualification. M1-03 supplies
transactions, immutable snapshots, a serialized worker and persistent entity
rows. Completed Mac feature prerequisites allow the next feature batch.
M1-06 now executes the current typed schema and M1-04 persists its whole state.
Application policy performs recovery saves before irreversible actions and
monthly/final autosaves, preserving live play on IO errors. Scoped variables,
checked expressions and [related-entity selectors](evidence/M1-06-selectors.md)
are implemented. Weighted narrative branches now retain
their selected outcomes across delays, choices and saves; the
[test evidence](evidence/M1-06-weighted-branches.md) is an engineering check.
Daily duration predicates also preserve their opening-sample start/reset state
and exact thresholds through choices and reload; see [duration evidence](evidence/M1-06-duration.md).
M1-07 then adds
built-in occurrence detection/dispatch. M2-01 through M2-04 implement accounts,
shared capacity, contracts and the three-year scenario with warned recovery.
The room shell connects those modules to an explicit desktop launch. The current count is 19 completed Mac feature steps. M3-01 is implemented; M3-05 base events and rivals precede M3-02 presentation and M3-03 delegation/scoring. Deferred platform and release work stays recorded in the backlog.
