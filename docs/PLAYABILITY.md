# Playability and evaluation checkpoints

Updated 2026-11-05. The bundled 2014–2017 Mac development campaign can be
played from headquarters selection to its ending through the Office, Lab,
Operations and Inbox. The [backlog](BACKLOG.md) records remaining acceptance gates and the
[implementation log](IMPLEMENTATION_LOG.md) records checks. Delegation, recovery, milestone controls and slice scoring are implemented;
independent human validation and release qualification remain open.

| Checkpoint | What can be evaluated | State |
| --- | --- | --- |
| Foundation demonstration | Atomic cash reservations, stale-command refusals, irreversible commitments and morning-choice continuation | Runnable with `pgp-headless --foundation-demo` |
| Event and save demonstration | Real event execution, nested delays/choices, owned modifiers, local save/reload and exact resumed state | Runnable with `pgp-headless --flow-demo FILE [--save FILE.pgpsave]` |
| Opening accounts demonstration | Two actual calendar months of costs, one data license, balanced reports and exact pre-settlement save/resume | Runnable with `pgp-headless --accounts-demo FILE`; [evidence](evidence/M2-01-accounts.md) |
| Shared capacity demonstration | 60-day concurrent serving/training, protected shared compute/staff, separate support costs and exact reload | Runnable with `pgp-headless --capacity-demo FILE`; [evidence](evidence/M2-02-capacity.md) |
| Enterprise market demonstration | Data-defined contract demand, fees, actual serving and exact pre-settlement save/reload through two months | Runnable with `pgp-headless --market-demo FILE`; [evidence](evidence/M2-03-market.md) |
| M2-04: first business simulation | A three-year command-line scenario, reconciled accounts, finite capacity, contract economics, solvency and recovery | Runnable with `pgp-headless --business-run FILE expansion/baseline/rescue`; [engineering evidence](evidence/M2-04-business.md) |
| M1-05: campaign room shell | Office/Lab/Inbox, time, required decisions, project inspector and local saves/preferences | Runnable with `pgp-game --content FILE`; [Mac evidence](evidence/M1-05-room-shell.md) |
| M3-01: lab development loop | Four data offers, six research nodes, finite training, lineage, held-out findings and targeted recovery | Runnable with `pgp-headless --lab-demo data/runtime/first-playable.json`; [lab contract](../src/core/LAB.md) |
| M3-05: base events and bounded rivals | Eight flow families, paid opportunities, temporary demand, financed competition and exact three-year continuation | Runnable with `pgp-headless --events-demo data/runtime/first-playable.json`; [contract/seeds](../src/runtime/BASE_EVENTS.md) |
| M3-02: first player-facing scenario | Office/Lab decisions, data/research/training/evaluation, contracts, news and choices, start to ending | Implemented on Mac; Metal keyboard runs at 100%/200% text complete and reload exactly |
| M3-03: feature evaluation build | The scenario with delegation, warnings, recovery and scoring | Implemented on Mac; finite policies, actual deadline stops, warned setback, financing/specialist routes and transparent score |
| M4-01: full rival planning | Default 15-company roster; 7/20-company alternatives; budgeted weekly strategies, shared customers, pricing and public research | Implemented on Mac; exact three-year standard/expanded saves and bundled 200% Metal ending |
| M4-02: heavyweight response and agreements | Public pressure, 90-day warning, finite paid mobilization/withdrawal and escrowed publication/pilot fees | Implemented on Mac; hysteresis/failure/save checks and actual keyboard accept/decline/paid milestone at 200% text |
| M4-03: corporate governance and acquisitions | Negotiated capital, executive cash/equity terms, cumulative vesting, board votes, covenants, retained subsidiaries and conserving group transfers | Implemented on Mac; 77 corporate checks and the bundled 200% Metal path through a solvent ending/exact reload |
| M5-01: owned compute and domestic map | Compare 50 states and DC, buy and staff sites, schedule inspections, run and meter actual owned work, end the opening lease | Implemented on Mac; pointer/keyboard at 200% text and exact saved continuation. [Operations guide](../src/runtime/INFRASTRUCTURE.md) |
| M5-02: regional markets and wholesale | Launch across eight regions and six channels, inspect local useful work, and sell paid rack capacity in the planned rosters | Implemented on Mac; regional allocation, curtailment credits, provider profit and keyboard confirmation with exact reload. [Player controls](../src/runtime/REGIONAL_MARKETS.md) |
| M5-03: site hazards and resilience | Inspect seasonal exposure, install protections, reserve a funded service replica, insure damage and manage repair/recovery | Implemented on Mac; 45 physical/financial/replay checks, bundled 200% Metal purchases and exact reload. [Resilience controls](../src/core/RESILIENCE.md) |
| E2-01: scenario/world authoring | Edit current scenario settings, slice scoring, resources, rivals, policies and state/region profiles; inspect actual opening impacts | Implemented on Mac; 68 authoring checks, Metal forms at 100%/200% text, map/list and 900-pixel pane switching, exact draft reopen. [Studio workflow](../src/studio/WORLD.md) |
| E2-02: catalog and flow reuse | Edit research and catalogs, review typed reference changes, localize messages, pin reviewed assets and bind reusable group calls | Implemented on Mac with 69 focused checks, saved nested calls and a 200% text Metal walkthrough. [Studio workflow](../src/studio/CATALOGS.md) |
| M6-01: domestic politics | Inspect seats and dockets, submit evidence, fund bounded advocacy and plan around US and state rules | Implemented on Mac with saved elections, agency dates and disclosures. The 200% text Metal walkthrough paid and reloaded the actual hearing checkpoint. [Civic room](../src/core/CIVIC.md) |
| M6-02: regional society and media | Inspect regional work, five welfare indices and minima; review member standards dates and paid media/disclosures | Implemented on Mac with source-reconciled outcomes and saves. Metal controls at 100%/200% text confirm both paid actions, stop at a monthly boundary and reload exactly. [World room](../src/runtime/SOCIETY.md) |
| M6-03: disputes and institutional authority | Disclose evidence, make public commitments, negotiate responses and operate named regional agreements | Implemented on Mac. Bundled Metal checks at 100% and 200% text cover testimony, a dated response, publication, a funded delegation and exact save reload. [Institutions room](../src/runtime/AUTHORITY.md) |
| M3-04: validated first playable | Five independent 30–45-minute sessions; three complete unaided; material comprehension problems fixed | Deferred until all components are in place and the user is ready for v1 work; no sessions conducted |

There is no calendar estimate inferred from task counts: tasks vary substantially
in size, and native packaging, authors and players have separate evidence gates.
Full century simulation and final art are
later milestones. The required typed editor canvas and graphical finalizer are
implemented on Mac; human author qualification remains deferred.

## Play the current campaign

After an optimized build, explicitly run `build/release/bin/pgp-game`. Choose
California, Texas or Washington and whether to include optional story layers.
Choose the competition preset: the default schedules six domestic startups, six international startups and three heavyweights, with three startups in each group active initially. Reduced schedules seven competitors; expanded schedules twenty. Forgiving discloses one optional $300,000 equity round for 15% ownership per rival. The introductory roster keeps the earlier two-startup/one-heavyweight campaign.

The base events remain enabled. Time begins paused; saves and preferences stay
local. Play/Pause, Step day and 1/10/30/60 days per second stay in the header.

For the original small-campaign opening route, select **Introductory roster**:

1. In **Lab → Data**, inspect and license the commercially cleared business documents.
2. In **Research**, select held-out evaluation and start its finite experiment.
3. In **Train**, choose the compact document classifier and licensed document mixture. The suggested allocation fits alongside research. Inspect time, uncertain intervals, data cost and the forecast of existing bills, then start training and advance time.
4. Pause when the checkpoint is stopped. In **Evaluate**, select that checkpoint and the independent document/scanned-table holdout. Pay for the test and let it finish. Documents qualify, while the scanned-table lower bound is well below the broader promise.
5. In **Deploy**, choose document-only scope, that checkpoint and its independent evidence. In **Office → Compare offers**, inspect and accept the enterprise document-service terms.
6. To broaden the promise, license consented scans and train a **fresh** model on both datasets. Evaluate that model, then deploy the documents-and-tables service with its matching evidence. Paid human review is another offered response, with lower throughput, extra staff and a real recurring cost.
7. Keep the company funded while time advances through January 2017. Required choices stop work. Inspect public rival notices, contracts, unpaid obligations and the tiered inbox. Save/load preserves the exact campaign boundary.

Office → **Funding and team** shows fixed equity/loan terms and a paid specialist.
Equity reduces retained founder ownership; the loan creates interest and a real
principal deadline. Repay principal in Accounts. The efficient document service
requires tooling/serving research and a tested efficient model; it can remain a
single-domain business rather than pursuing the broader task promise.

Use **Operations → Map & states** to compare grid lead times, site costs, cooling
and hiring terms. Reserve power and order construction, then buy hardware and
hire operators under **Owned sites**. Book an inspection in **Calendar**; use
**Next site milestone** for actual arrival and readiness dates. A commissioned
rack can run compatible Lab and serving work. Compare metered site costs with
the opening cloud lease before giving notice, and move dependent service first.
You can continue normal management during an on-site trip.

**Operations → Resilience** shows the selected state's published monthly
exposure and paid site protections. Generators cover limited emergency power;
offsite copies shorten data restoration; inspections, hardening and cooling
affect their named hazards. A service replica needs another state's owned
rack, real spare capacity and synchronization work. Insurance terms disclose
the premium, waiting period, deductible, limit and claim delay. Site incidents
pause for review, with repairs and settlements recorded in Accounts and Inbox.

Office → **Policies and alerts** authorizes finite independent-evaluation or
paid compute-lease renewals. Set total budget, cash floor, run count, interval
and resource/monthly ceilings. Refused attempts spend nothing and suspend the
policy. Renewals count a conservative full-term commitment against the budget;
actual bills remain in Accounts. Review/suspend policies and inspect every
committed/refused decision there. Policies cannot answer stories or deploy models.

Settings separately control optional project, evaluation-result and advance-alert
pauses. Required choices, the supplier setback and actual debt/lease/start
boundaries still stop time. **Advance to target** captures an existing finite
project, evaluation or actual known deadline; it stays interruptible by required
choices. Normal speed is one day/second, with the same optional faster ceilings.

The supplier announces a temporary opening-source reduction two weeks before it
starts. Serving keeps priority, optional work can finish later, and recovery is
on a disclosed date. Acknowledge the start, then choose whether to wait, reduce
optional work or pay for capacity elsewhere. Past costs are retained. The ending
and Office score panel explain survival, delivered work, operating profit,
independent deployed evidence and retained founder stake. Century objectives
are not awarded by this slice rubric.

The guide derives its stage from actual company progress, can be hidden, and can
be restored from Settings. Project cancellation keeps consumed work and costs;
a dependent customer contract must be cancelled before stopping its service.
Service replacement keeps prior delivery/invoices and current promises.

In the full-roster presets, competitors pay real staff, data, compute and service costs, compete for one customer pool and can close. They see public releases/prices and bounded estimates. **Lab → Research** can publish completed work or adopt a genuine paper after 30 days for one 25% experiment-work credit; prerequisites and cash costs still apply. **Lab → Deploy** exposes the selected service price in constant-2014 cents. A new price preserves past invoices and signed contract prices. **Office → Rivals** shows actual public activity and released quotes.

Office → **Rivals** also explains heavyweight response pressure from public
deliveries/prices, demonstrated services, public demand and announced financing.
Two high reviews announce at least 90 days' warning; lower exit thresholds
require two reviews. Mobilization pays real staffing and compute bills before
delivery. Each program has one finite allocation and can withdraw. Private
contract terms, cash and models do not drive these reviews.

The funded agreements list offers optional publication partnerships and customer
pilot evaluations. Inspect the exact fee, milestone and deadline before accepting
or declining. Acceptance pays nothing: publication must occur after acceptance,
and the pilot requires actual additional qualified customer work. The issuer
reserves its own fee and pays once on fulfillment. Ignoring or missing terms
releases unspent funds; no extra demand or customer work is created.
[Response and agreement contract](../src/runtime/HEAVYWEIGHTS.md).

Office → **Board and people** exposes compensation, ownership, financing
counteroffers, board positions/votes and warned disputes. Choose cash-only or
hybrid executive terms with real payroll. Grants use the reserved pool, vest
cumulatively and retain earned ownership on departure. Compare alternative
financing terms, then submit a motion and vote; a new equity percentage and a
board seat are separate rights. Cash covenants and publication promises show
real response deadlines and remedies.

The acquisitions page reviews actual seller due diligence before a cash/equity
proposal. A completed purchase retains the subsidiary's licenses, products,
staff, contracts, debt and bills. Paid owner funding and dividends use actual
cash; a dividend requires undistributed profit earned after acquisition and
retained cash for future bills. Group figures eliminate internal investment
assets and dividend income. [Corporate rules](../src/runtime/CORPORATE.md).

The M4-02 funded efficient headless routes finished the standard and expanded
presets with $2,086,666.76 and $1,993,686.26 respectively. The current bundled
corporate UI route also hires an executive, approves financing/pool motions,
acquires an operating startup, funds it and receives an earned dividend, then
finishes with $374,040.93. These are deterministic engineering routes, not a human balance
assessment. [Shared-market and planning contract](../src/runtime/COMPETITION.md).

The original introductory rendered Mac smoke exercises this path at 1280×720 with 100% and 200% text,
including exact save/reload and a solvent ending above $2.25m. These are scripted
checks, not independent human playtests or screen-reader/device qualification.

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
build/core-release/bin/pgp-headless --events-demo data/runtime/first-playable.json
# Explicit optional save path; use a chosen local .pgpsave filename:
build/core-release/bin/pgp-headless --flow-demo tests/content/fixtures/foundation.json --save /chosen/local/demo.pgpsave
```

These commands run engineering examples. Only the explicit `--save` invocation
writes the selected local save; the graphical application is not launched.
Content fixtures are developer evidence and excluded from any future game
payload. The three-year business fixture starts with an engineering product;
research/models also run through the lab and three-year baseline demonstrations.
The bundled player campaign adds authoritative qualification and interactive controls.
See [desktop launch instructions](BUILDING.md#open-the-playable-campaign).

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
The playable campaign connects those modules through ordinary controls. The current count is 31 completed Mac feature steps. M3-01 through M3-03, M3-05, E1A-04's internal snapshots, E1A-05's authoring walkthrough, E1B-01/02's typed canvas and graphical finalizer, and M4-01/02/03's competition, heavyweight responses and corporate governance are implemented, along with M5-01/02's owned compute, domestic map, regional markets and paid wholesale agreements. Per the 2026-11-05 user instruction, human player/author studies wait until all components are in place and the user is ready for v1 work. The [session kit](playtests/M3-session-guide.md) is prepared for later; no sessions are being arranged. Later components proceed with engineering checks. Deferred platform and release work stays recorded in the backlog.
