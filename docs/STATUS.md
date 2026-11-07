# Project status and next handoff

Updated 2026-11-07 · Implementation started against planning baseline 1.2

## Implementation progress

**Initial feature coverage is active.** Lab Data now supports paid commissions
and opted-in feedback. Staff workload and recovery are the next core gap.

The original roadmap has **42 of 52 acceptance steps checked**. Its ten open
steps are deferred human, balance, platform and release gates; they do not
block current implementation. The coverage pass below has delivered 21
additional feature batches within earlier milestones. That batch count is
separate from the 52 acceptance steps and does not imply complete design
coverage. Completed milestones include
M0-01/02/03/05, M1-01 through M1-07, M2-01 through M2-04, M3-01/M3-02/M3-03/M3-05, E1A-01 through E1A-05, E1B-01/02, M4-01 through M4-03, M5-01 through M5-03, E2-01 through E2-03, M6-01 through M6-03 M7-01 through M7-03 and M8-01.
**Per the 2026-11-05 user instruction, human testing waits until all components
are in place and the user is ready for v1 work.** M3-04 and E1B-03 remain open
and do not block later component implementation. The local session kit is
prepared for that later phase; no testers are being arranged and no human
sessions have been conducted.

The remaining ten checkboxes measure later acceptance work. The
[initial feature coverage notes](#initial-feature-coverage) track playable
changes within the earlier milestones.

| Remaining work | Steps | When it advances |
| --- | --- | --- |
| Player and editor author sessions | M3-04, E1B-03 | When the components are in place and the v1 feedback phase begins |
| Balance, endurance, performance and offline audit | M8-02, M8-03 | After feedback and event expansion |
| Native and editor packages, clean-device checks, itch and Snap | M0-04, E2-04, M9-01 through M9-03, M10-01 | During explicit release preparation and publication |

On 2026-11-05, core/runtime/session daily validation and state-copy bottlenecks were
eliminated, reducing the three-year business scenario test (`session.business-scenario`)
from **174.34s to ~5.4s** and `session.business-recovery` from **8.09s to ~0.4s**.
The roadmap and backlog have been updated so `M3` (`M3-01` -> `M3-05` -> `M3-02` -> `M3-03`)
is no longer blocked behind Content Studio authoring steps (`E1A-04`, `E1A-05`, `E1b`).
See the [implementation log](IMPLEMENTATION_LOG.md) for checks and limitations and
[build instructions](BUILDING.md) for runnable commands.

**Mac-first development, per the 2026-11-04 user instruction:** use incremental
local optimized builds and focused changed-behavior checks. Native matrices are
manual-only; cross-platform/full release qualification is deferred and does not
block Mac implementation. Keep build caches between batches.

- M8-01 expands both century scenarios to 120 flows, 60 research topics,
  24 data offers, 18 rack families and the staged 15-company roster. A real
  pilot recovers from a missed start with optional stories disabled. With the
  story enabled, its saved reminder links to the same settlement. Three
  seeded regional incidents and a paid cooperative lease expire on their
  disclosed dates. Music and credits work by keyboard at 100% and 200% text
  with native Core Audio and Metal. All **79 optimized Mac CTests** pass.
  Reviewed content **0.14.0-dev.1**, generation **14**, verifies all seven
  scenarios, 21 exact headquarters saves and eight asset seals.
  [Coverage and source records](CONTENT_AND_ASSETS.md).
- M7-03 adds the six century objectives, strict month-end checks, audited
  founder wealth, same-day ASI ties and recorded failure reviews. Players can
  continue scoring other distinctions, end at a postmortem, or continue in
  sandbox or observer mode. A final-decade fixture wins all five deadline
  goals, loses Domination on a last-day branch, and reloads at the boundary.
  Metal controls at 100% and 200% text reopen the actual breach. Reviewed
  content **0.13.0-dev.1**, generation **13**, validates seven scenarios
  and 21 headquarters openings. [Objectives and rules](../src/runtime/OBJECTIVES.md).
- M7-02 adds Leadership to the century scenarios. Named successor cohorts,
  actual board votes and paid preparation produce dated handovers. Executive
  and personal-chair terms end, while the original founder trust and company
  obligations persist. Actual paid compensation and acquisition consideration
  reconcile to retained beneficial ownership and group book reports. All 77
  optimized Mac CTests pass, including 77 succession/legacy checks and 90
  Studio checks. Metal controls at 100%/200% text use a near-retirement fixture
  and reopen the exact 536,905-byte save. The bundled 200% introductory route
  still reaches its solvent 2017 ending. Reviewed content **0.12.0-dev.1**,
  generation **12**, passes all seven scenarios and 21 headquarters saves.
  [Leadership, retirement and founder legacy](../src/core/SUCCESSION.md).
- M7-01 adds Frontier and two 2014–2114 scenarios with seeded conditions,
  advanced recipes/hardware, independent game AGI/ASI evidence, bounded
  research queues/standing tracks and evaluated product refresh. The 188
  focused checks exercise both player ASI routes, a plateau, rival ASI,
  cooperating systems and exact assessment/replication windows. All 76
  optimized Mac CTests pass across the suite and focused Studio rerun.
  Metal walkthroughs at 100%/200% text use a deliberately late opening and
  an actual trained/deployed service, then reopen the exact 473,735-byte save.
  The bundled introductory route still reaches its solvent 2017 ending.
  Reviewed content **0.11.0-dev.1**, generation **11**, validates all seven
  scenarios and 21 headquarters saves. Final objective accounting and century
  endurance are the next M7/M8 work.
  [Frontier controls and bounds](../src/core/FRONTIER.md).
- M6-03 adds the Institutions room for testimony, public commitments, six
  audience records, executive and public disputes, and four funded agreement
  types. Actual staffing, compute, evidence and regional delivery determine
  authority. Procurement and resistance affect market settlement. All 75
  optimized Mac tests passed across the suite and the corrected fixture rerun,
  including 67 focused authority checks. Bundled Metal walkthroughs at
  100% and 200% text exercise testimony, negotiation, publication and an
  operating delegation with exact reopening of a 695,571-byte save. The shared
  writer validated content **0.10.0-dev.1**, generation **10**, across all
  15 headquarters openings. [Institutions controls and rules](../src/runtime/AUTHORITY.md).
- M6-02 adds World with five welfare indices, fourteen equally weighted
  components, regional minima, six audience reputations and disclosed media.
  Settled work from all sellers reaches welfare after the authored lag.
  Standards require a quorum and separate member implementation dates;
  restrictions apply to real delivery and preserve signed obligations.
  All **74 optimized Mac CTests passed in 98.38s**, including **61 society
  checks**. Most full-suite time is in authoring snapshot checks; the later
  performance target remains open. Metal controls at **100%/200% text**
  confirm both paid actions, stop at the actual month boundary and reopen
  the exact **562,154-byte** save. Finalized content **0.9.0-dev.1**,
  generation **9**, passes all 15 headquarters checks.
  [World controls and source reconciliation](../src/runtime/SOCIETY.md).
- M6-01 adds a Civic room with House and staggered Senate seats, the
  presidential calendar, state institutions and the public docket. Typed
  laws govern new US service launches and state grid permits after votes,
  executive decisions, review and agency implementation. All **73 optimized
  Mac CTests passed**. The **39 focused checks** include reload across the 2016 election with actual
  settled domestic adoption, pending agency work and political disclosures.
  The **200% text Metal** walkthrough pays for advocacy, stops at the
  committee boundary and reloads the exact **227,860-byte** save. Finalized
  content **0.8.0-dev.1**, generation **8**, passes all 15 headquarters checks.
  [Civic controls and rules](../src/core/CIVIC.md)
- E2-03 compares finalized generations by their recorded hashes.
  Matched fixtures show changed reach and actual committed effects. Typed
  save maps preserve nested waits, pending decisions, occurrence counters,
  RNG, history and owned modifiers in a separately validated copy. The
  receipt records both identities and the exact original and copy hashes.
  All **72 optimized Mac CTests passed**, including **46 revision checks**
  for incompatible chains, unknown fields, stale inputs, a 512-node graph
  and natural continuous cleanup. The **200% text Metal** walkthrough
  reviewed an explicit flow mapping and reopened the **151,891-byte**
  export. [Revision workflow](../src/studio/SNAPSHOTS.md#compare-finalized-revisions-and-review-a-campaign-copy)
- E2-02 adds catalog search and structured edits for research, recipes,
  data, hardware, products and people. Reviewed renames show typed uses
  across the collection and commit through shared undo. The library retains
  disabled and deprecated definitions. Locale tables retain their English
  fallback and follow key refactors. The asset register supports local
  import, hash and provenance verification, and pinned credits checked
  by the snapshot writer. Reusable groups have named typed inputs and
  outputs, exact saved continuations and receipts for bound destinations.
  All **71 optimized Mac CTests passed**, including **69 catalog checks**.
  The **1280×720 Metal** catalog and world walkthroughs passed at **200%
  text**. The catalog run exercised rename review, localization, asset credit
  editing and pinning, group binding and exact reopen of a **788,839-byte**
  draft against content **0.7.0-dev.1**. [Studio workflow](../src/studio/CATALOGS.md).

- E2-01 adds the separate scenario/world workspace on the shared typed
  document. Forms cover current opening resources, ownership, rivals, slice
  objectives, management defaults, regions, site profiles and resilience.
  Atomic sections and staged cross-section batches preserve totals and
  dependent dates. Pending/stale values remain reviewable, with shared undo,
  draft conflict checks and graphical finalization. Actual opening previews
  use the game runtime/save codec and show fact/trigger/effect target bindings.
  All **70 optimized Mac CTests pass**, including **68 authoring checks** across
  the five presets. The actual **1280×720 Metal** path passes at **100%/200%
  text**, including 11 QML inputs, coupled ownership/scoring, pointer/keyboard
  selection of 51 state profiles, **900-pixel pane switching** and exact
  reopen of the **786,054-byte** draft. Runtime content remains finalized at
  **0.7.0-dev.1**. Century objectives arrive with M7; initial Lab deployments
  require real model/evidence records. [Author workflow](../src/studio/WORLD.md).

- M5-03 adds authored seasonal exposure for all 51 domestic jurisdictions,
  six physical hazards, five paid protection mechanisms, finite geographic
  replication and insurance. Repairs require time and payment; replication
  reserves real compute/staff and preserves one product's delivery history.
  Insurance spends a finite insurer fund and reimburses actual covered damage.
  Site outcomes use shared committed news and saved random history. All
  **69 optimized Mac CTests pass in 31.61s**, including **45 resilience checks**.
  The bundled **1280×720 / 200% text Metal** path purchases a generator and
  policy, completes owned work/wholesale and reloads the exact **868,111-byte**
  save. Content **0.7.0-dev.1**, generation 7, passes all 15 headquarters save
  checks. Exposure values are fictional authored balance. Snapshot history
  now links full immutable revisions by hash, preserving detailed review
  within its metadata bound. [Resilience controls and contract](../src/core/RESILIENCE.md).

- M5-02 adds 48 customer groups across eight regions, twelve templates in
  six product channels, territory selection and regional delivery reports.
  Monthly installed work shapes switching. On-premise and open deployments
  consume a shared customer hardware budget. Funded wholesale agreements
  reserve exclusive slices of commissioned racks in the planned rosters.
  The integration run earned **$12,499.28 operating profit over 47 days**
  with two racks, four contracts and no trained model. All **68 optimized
  Mac CTests** pass, including 48 regional and wholesale checks. The
  bundled 1280×720 / 200% Metal check signs and settles an agreement and
  reloads the exact **832,298-byte** save. A separate standard-roster run
  reaches 2017 solvent and reloads its **19,677,487-byte** save. Reviewed
  content **0.6.0-dev.1**, generation 6, passes all 15 headquarters save checks.
  [Player controls and settlement rules](../src/runtime/REGIONAL_MARKETS.md).

- M5-01 adds the offline map and searchable list for all 50 states and DC.
  State profiles change construction cost, grid waits, water, cooling, hiring
  and equipment fit. Paid sites, operators and compatible delivered racks join
  the shared capacity pool after inspection. Actual work drives energy and
  monthly bills; capex, depreciation, resale and lease termination reconcile.
  Remote/delegated appointments keep travel optional and ordinary management
  remains usable during trips. All **67 optimized Mac CTests** pass, including
  **42 physical/financial checks, 22 integration checks** and map reproduction.
  The bundled **1280×720 / 200% text Metal** path tests pointer and keyboard
  selection, a conflicting appointment, management while traveling, real
  curtailed Lab work, lease notice and an exact **401,500-byte** reload. The
  shared writer finalizes content **0.5.0-dev.1**, generation 5, across five
  presets and all 15 headquarters save checks. [Player path and contract](../src/runtime/INFRASTRUCTURE.md).

- M4-03 adds alternative financing counteroffers, explicit seat-based board
  votes, real debt/covenants and paid waivers; named executives agree cash-only
  or cash-plus-equity compensation and concrete publication promises. Grants
  reserve the existing pool, vest cumulatively, dilute once and keep earned
  ownership on warned departure. Approved pool expansion discloses dilution.
  Cash/equity acquisitions preserve every subsidiary obligation and service;
  capital funding and earned dividends conserve both ledgers and group totals.
  All **64/64 optimized Mac CTests pass in 19.79s**, including **77 corporate
  checks**. The bundled **1280×720 / 200% text Metal** path exercises financing,
  compensation, board/pool votes, acquisition, actual group funding/dividend and
  full vesting, then finishes with **$374,040.93** and an exact **22,183,897-byte**
  reload. The shared writer finalizes content **0.4.0-dev.1**, generation 4,
  advancing four full-roster scenarios to revision 3; all 15 headquarters/save
  checks pass. Game/editor remain planning placeholders and release gates stay
  unresolved. [Corporate contract](../src/runtime/CORPORATE.md).

- M4-02 adds public-evidence heavyweight reviews, watch/mobilize/compete/
  consolidate states, two-review hysteresis and at least 90 days' warning.
  One finite ledger pays for real specialist/compute orders before delivery;
  withdrawal preserves consumed costs and fixed-term bills. Optional publication
  and pilot agreements reserve the issuer's cash and pay only on actual work.
  Office briefings explain pressure, budgets, dates and keyboard-reviewed terms.
  All **63/63 optimized Mac CTests pass in 18.32s**, including **50 heavyweight
  assertions**. Standard/expanded ending saves are **22,036,080 / 23,606,264
  bytes**, under the unchanged 25 MiB limit; expanded reaches 130 public notices.
  The bundled **1280×720 / 200% text Metal** run declines publication, accepts
  and completes a pilot, then finishes with **$1,547,040.93** and an exact
  **21,825,962-byte** reload. The shared writer finalizes content **0.3.0-dev.1**,
  generation 3, with all 15 headquarters/save checks. Game/editor stay planning
  placeholders; ownership/mergers and century balance remain later steps.
  [Response and agreement contract](../src/runtime/HEAVYWEIGHTS.md).

- M4-01 adds the standard 15-company schedule, 7/20-company alternatives,
  six initial startups and a disclosed finite catch-up equity preset. Weekly
  strategies use private saved RNG, own budgets/work and bounded public-market
  observations; all actual actions use core accounting/research/management
  commands. One industry allocator conserves customer work and money across
  companies, preserves contract priority and derives monthly adoption from real
  delivery. Public research has a 30-day adoption lag and one finite 25% work
  credit with prerequisites/cash costs intact. Setup, publish/adopt and prices
  are exposed in the game. All **62/62 optimized Mac CTests pass in 16.47s**
  (four workers), including **51 competition assertions**. Standard and expanded
  presets reach 2017 with exact saves of **22,501,764 / 24,610,924 bytes**, below
  the unchanged 25 MiB cap, using bounded lossless history storage. The bundled
  **200% text Metal** run reaches 2017 with **$1,503,361.57**, 3,481,282 tasks and
  **94/100**, then reloads its full-roster save exactly. The shared writer
  finalizes content **0.2.0-dev.1**, generation 2, with all 15 headquarters/save
  checks; game/editor remain **0.0.0-dev** and the game requirement is unresolved.
  Heavyweight response/deals, century balance/save scaling and human/release
  qualification remain separate later work. [Competition contract](../src/runtime/COMPETITION.md).

- E1B-02 adds graphical saved-source selection, version/summary controls and
  before/after definition/text review around the existing snapshot writer.
  Authors inspect actual validation, file hashes, revision increments and save/
  game-version requirements, then finalize and reopen the reviewed runtime.
  The recovery action preserves concurrent edits and restores interrupted
  multi-file batches. All **61/61 optimized Mac CTests pass in 13.12s**, including
  **51 graphical-controller checks**. The native Metal forms/canvas/finalizer
  walkthrough reviews six definitions, preserves a stale external edit, writes
  one internal batch, verifies a no-op, recovers an interrupted write and
  explicitly resolves a game-version requirement in an isolated copy. The
  primary content stays **0.1.0-dev.1**, with game/editor **0.0.0-dev** and its
  game requirement unresolved. No human sessions or release qualification are
  implied. [Graphical workflow and checks](../src/studio/SNAPSHOTS.md).

- E1B-01 adds the typed node canvas on the same document/compiler as the
  forms/outline: execution, predicate, value and entity ports; searchable
  contextual creation; shared logical undo/save; and exact type/unit/scope
  rejection. Saved-runtime overlays show clause truth, target receipts, actual
  selected branches and waiting siblings/nested subflow returns. Grid moves
  commit on drag release; shared layout files have independent undo/save,
  bounded integer metadata, external-conflict refusal and recovery copies.
  Selection/pan/zoom stay in ignored local session data. All **60/60 optimized
  Mac CTests pass in 12.18s**, including **84 canvas assertions**. The actual
  Metal QML pointer/keyboard walkthrough at **1280×720** also verifies palette
  creation, rejected ports, exact waits/reload, layout/viewport reopen,
  **900-pixel pane switching** and **200% canvas zoom** without runtime changes.
  [Canvas contract and commands](../src/studio/CANVAS.md).

- E1A-05 supplies the reproducible editor engineering walkthrough: **36 actual
  QML form submissions**, keyboard outline, a branching/delayed flow, funded
  positive-versus-negative cash receipts, wrong-scope refusal and all five
  lifecycle save/reopens. Exact waits, dialogue and hidden consequences resume
  through the game save codec. The path caught and fixed Save as draft calling
  an invalid JavaScript string method. Metal runs at **1280×720** render the
  accepted result and inspection report. The isolated saved batch finalizes as
  0.2.0-dev.1 with all three headquarters/save checks and no lints; the primary
  game stays on 0.1.0-dev.1. Game/editor resource lists remain separate. Both
  focused Studio CTests pass in **1.31s** and the existing offscreen view check
  passes. [Walkthrough and limits](../src/studio/WALKTHROUGH.md).

- E1A-04 adds the shared internal snapshot writer and explicit maintainer CLI:
  review, finalize, check and recover. It validates with the actual compiler,
  runtime and save loader; records one revision increment per changed definition;
  preserves drafts; rejects stale plans; and journals multi-file replacements
  with rollback or committed-generation verification. The first reviewed
  content snapshot is **0.1.0-dev.1**, with 15 revision-1 definitions and all
  three headquarters opening/save checks. Builds and the bundled game use its
  exact collection hash. Game/editor stay **0.0.0-dev**; the required game bump
  to **0.1.0-dev.1** is unresolved and release readiness remains false.
  All **59/59 optimized Mac CTests pass in 13.09s**, including **76 snapshot
  assertions**, and the finalized content completes the 200% text Metal smoke.
  [Writer commands and recovery](../src/studio/SNAPSHOTS.md).

- M3-03 adds finite evaluation and paid lease-renewal policies with saved
  budgets/cursors, conservative term commitments, reserve/resource checks,
  suspension and decision logs. Fixed equity/debt and a paid specialist support
  recovery; the efficient document service supplies a profitable specialist
  route. A dated supplier setback warns before reducing capacity and requires
  acknowledgment; nominal capacity returns on its actual recovery date.
  Optional auto-pause preferences and milestone controls preserve required
  choices and actual deadline stops. Slice scoring is explicit and uses actual
  delivered work, profit, independent evidence, survival and founder stake.
  All **58/58 optimized Mac CTests pass in 11.71s**, including **352 management
  integration assertions** and **20 bridge management checks**. Complete Metal
  UI runs at 1280×720 with 100%/200% text exercise funding, hiring, policies,
  milestone, exact reload, setback/restoration and a scored 2017 ending.
  The efficient specialist finishes with **$1,437,387.12**, **3,081,000 tasks**,
  **$237,387.12 operating profit** and **84/100**, without new financing.
  The broad baseline remains solvent at **$2,276,020.41**, **3,985,648 tasks**
  and **99/100** after the new setback. [Management contract](../src/core/MANAGEMENT.md).

- M3-02 supplies the bundled playable Mac campaign: headquarters and optional
  stories, data/research/training/evaluation, deployment, customer offers,
  contracts, accounts, public rival notices, guide and an outcome screen.
  Deployment requires independent evidence of the exact stopped checkpoint and
  commercial training rights. Review adds actual staff/cost and reduces throughput;
  document-only scope and targeted retraining offer the other responses.
  Replacements retain actual delivery, invoices and contracts through dated
  serving bindings. Full keyboard/Metal runs at 1280×720, 100% and 200% text,
  reach 2017 solvent and reload exactly. **57/57 optimized Mac CTests pass in
  10.10s**. Reader/device/release qualification and the human study remain open.
  Launch `build/release/bin/pgp-game`; [player path](PLAYABILITY.md) and
  [build/launch options](BUILDING.md#open-the-playable-campaign).

- M3-05 implements the eight base-event families in `data/runtime/first-playable.json`,
  real research/evaluation/pilot causes, dated demand incidents, paid supplier and
  recovery terms, two financed startup routines and a heavyweight that waits 90
  days after a visible customer-delivery warning. The baseline starts without a
  product and finishes 2017-01-01 with **$2,298,658.09**, 4,001,600 delivered tasks
  and no new financing. Interrupted and uninterrupted states match exactly.
  All **55/55** optimized Mac CTests pass in **8.88s**, including **188** event,
  rival, Studio and complete-scenario checks. Removing redundant daily history
  replay/copies also reduces the existing business test to 2.01s. The pack is
  browsable and inspectable through Studio; M3-02 now provides player controls
  and tutorial. Snapshot finalization remains E1A-04/05. [Module contract](../src/runtime/BASE_EVENTS.md).

- M3-01 supplies four data offers, six research nodes, resource-bounded training,
  checkpoint lineage and held-out subgroup evaluation through shared commands,
  accounts and exact saves. The headless lab demonstration reports a scanned-table
  weakness, then verifies targeted-data/retraining recovery. Previews do not draw
  RNG, clones add no capability and evaluation changes evidence only. All 54
  optimized Mac CTests pass (13.40s); the new lab test passes 88 assertions in
  0.10s. [Lab contract and runnable example](../src/core/LAB.md).

- M2-01 now implements balanced accounts, daily actual-month accrual, protected
  month-end/overdue settlements, receivables/payables, debt/interest/repayment,
  conserving equity dilution and pure cash projections. Scenario creation,
  commands, snapshots, autosave policy and strict exact saves share the ledger.
  The opening example leaves **$972,000** after two months and one $12,000 license,
  including a reload before settlement. An optimized Mac build and eight focused
  CTests pass; [accounts evidence](evidence/M2-01-accounts.md) records the
  implemented contract and limits. The subsequent M2-04 batch supplies liquidity recovery.
- M2-02 now reserves shared source capacity and staff, checks memory/interconnect
  compatibility, protects serving during outages and records actual finite work.
  Cancellation preserves completed work and past costs; non-compute support has
  its own prorated obligation. The 60-day engineering example completes 3,600 CU
  of training plus 60,000 serving tasks on one 100 CU/day lease and resumes
  exactly. An optimized Mac build and seven focused CTests pass.
  [Capacity evidence](evidence/M2-02-capacity.md) records scope and limits.
- M2-03 now executes bounded common-snapshot allocation, protected contractual
  serving, actual-month fees/usage, netted service credits, penalties/deadlines
  and monthly installed shares with incumbent-based switching. Typed scenario
  data supplies market terms, utilities and profiles. Exact saves reconcile
  customer budgets, invoices and actual work. An optimized Mac build and eight
  focused CTests pass; the authored two-month example bills $50,000 for 59,000
  useful tasks and leaves $1,012,000 after costs/license. [Market evidence](evidence/M2-03-market.md)
  distinguishes engineering starting products from M3 model qualification.
- M2-04 adds saved liquidity warnings, immutable 30-day response windows,
  acknowledgment, debt/equity rescue through actual payments and terminal
  closure with a retained postmortem/autosave. The data-driven three-year
  expansion policy finishes 2017-01-01 with **$1,246,935.49**, 5,240,000 delivered
  tasks and no financing or overdue liabilities; midpoint continuation matches
  uninterrupted execution. [Business evidence](evidence/M2-04-business.md)
  records the focused checks and remaining engineering-product limits.

- M1-05 connects the Office, Lab and Inbox to a serialized campaign session.
  The desktop shell includes Play/Pause, Step day, required decisions, liquidity
  warnings, stable project and inbox rows, an inspector, and local saves and
  preferences. The 1280×720 Metal smoke passed with 200% text. The M1-03 gate
  and broader UI qualification remain open. [Room shell evidence](evidence/M1-05-room-shell.md)

- M1-06 now compiles/executes the current typed schema: five lifecycles,
  nested sequence/subflow delays and choices, selected root sources, owned
  modifiers, bounded atomic effects and committed/candidate traces. Weighted
  narrative branches also preserve exact outcomes across saves, delays and
  choices. Daily duration predicates retain sampled start/reset state and exact
  elapsed-day thresholds through reload. Content-declared scoped variables,
  flags, checked value expressions and transactional assignment are implemented.
  Related-entity selectors now bind compute sources, projects and contracts
  with stable-order any/all/count matching and privacy-aware traces.
  [Runtime evidence](evidence/M1-06-runtime.md),
  [weighted branches](evidence/M1-06-weighted-branches.md),
  [duration predicates](evidence/M1-06-duration.md) and
  [selectors](evidence/M1-06-selectors.md).
- M1-07 subscribes flows to settled project completions and missed contract
  starts. The inbox uses their actual cause and a required choice can suspend
  and resume dispatch without repeating consumed sources. Bounded seeded
  occurrences use independent saved markets and hazards streams. The weekly
  digest retains news sources and keeps required choices and active deadlines
  in their own action tier. [Committed-fact evidence](evidence/M1-07-committed-facts.md)
  [Seeded incident and digest evidence](evidence/M1-07-seeded-digest.md)
- E1A-01 adds the separate opt-in Content Studio.
  It opens an explicit checkout, indexes validated definition uses and used-by
  edges, and reports invalid, unknown and blocked documents. Its working
  document can edit flow tags, undo/redo and save drafts with conflict checks.
  [Studio library evidence](evidence/E1A-01-library.md)
  [Draft workflow evidence](evidence/E1A-01-drafts.md)
- E1A-02 now edits typed triggers, continuous exit conditions and all five
  lifecycles through the same working draft. Create scoped flows, enable them
  in scenarios, author ordered execution and scoped variables, and rename
  typed references. These forms are complete for Mac feature development.
  [Trigger forms](evidence/E1A-02-triggers.md)
  [Lifecycle and reference forms](evidence/E1A-02-lifecycles.md)
  [Execution and variable forms](evidence/E1A-02-execution.md)
- E1A-03 runs scenario tests on copied real state and RNG, including exact
  checkpoint reload, live clause/scope inspection, effects/ledger entries,
  nested queues, numerical boundaries and separate synthetic cleanup.
  It shares dialogue/results presentation with the game.
  [Inspection evidence](evidence/E1A-03-inspection.md)
- Weighted-branch commit `2a37284` passes 26/26 CTests in each of all six native
  jobs across five targets, including GCC/Clang/MSVC and native Windows arm64.
  [CI result](https://github.com/gnaservicesinc/Project-Glowing-Pancake/actions/runs/41895189273)
  qualifies this core behavior; native Qt deployment remains a separate gate.
- Duration commit `543c451` also passes 27/27 CTests in each of all six native
  jobs across five targets, with matching resumed-state hashes.
  [CI result](https://github.com/gnaservicesinc/Project-Glowing-Pancake/actions/runs/41903737919)
  verifies that core revision.
- M1-04 now writes/loads the entire implemented checkpoint with exact resumed
  state, recovery copies, strict identity/integrity checks, three autosave slots,
  preferences, Unicode filenames, portable/standard paths and export. Campaign
  policy saves before irreversible consumption/choices and at monthly/final
  boundaries, preserving live state on errors. Core Release passes 25 CTests;
  desktop Debug passes 32. [Save evidence](evidence/M1-04-persistence.md)
  and [application policy](../src/session/README.md) describe the implemented
  subset. Release qualification remains deferred.

- M1-02 implements typed content collections and extensible system/fact/effect
  metadata, with strict units, scopes, references, ownership, graph and input
  bounds. Eleven engineering definitions normalize the opening balance and
  seven example flows. Future catalog families fail explicitly; the loader
  neither executes effects nor finalizes release content.
  [Content evidence](evidence/M1-02-content.md)
- M1-03's local batch implements atomic revision-checked commands, prepared
  morning/choice continuations, protected reservations, immutable snapshots, a
  single-worker Qt adapter and persistent GUI models. Core Release passes 19
  CTests; desktop Release/Debug pass 26 each. The M0-03 prerequisite remains open;
  file saves and live room-view qualification remain their own tasks.
  [Command evidence](evidence/M1-03-commands.md)
- The expanded Qt-free foundation suite passes 19/19 CTests in all six native
  jobs/five targets after an MSVC naming-warning correction, including native
  Windows ARM64. [Native result](https://github.com/gnaservicesinc/Project-Glowing-Pancake/actions/runs/41869067266)
  covers content/commands and the original primitive oracle, not deployed Qt.

- M1-01 implements strong units/IDs, portable checked wide arithmetic,
  Gregorian dates, seven saved RNG streams and strict integer-token readers.
  Core Release/Debug each pass 13 CTests; desktop Release passes 19. An
  independent oracle freezes 1,000 daily checksums. Native five-target compiler
  parity passes in all six native CI jobs across five targets, including
  GCC/Clang/MSVC and native Windows ARM64.
  [Primitive evidence](evidence/M1-01-primitives.md)
  records bounds and excludes gameplay/save-loader/allocator claims.

- Inventoried the coherent existing Apple compiler/SDK and Qt modules/SBOMs,
  performed C++20 and Qt Core CLI compile/link/run probes, and recorded exact
  five-target Qt 6.12.0 archive metadata plus published checksum observations.
  [M0-01 evidence](evidence/M0-01-toolchain.md) distinguishes inventory from
  archive-body, deployment and physical-device qualification.
- Added separate C++20 core/headless targets, CMake/Ninja presets and a safe
  Release-default `make.sh`. Core-only builds need no Qt; desktop configuration
  validates the SDK needed for M0-03. Debug has a separate directory and explicit
  local diagnostics; Release compiles them out.
- Verified all four configurations (five CTests each), an additional LLVM core
  build (five CTests), and eleven wrapper cleanup/exit-code checks. Release
  headless runtime linkage contains only system libc++/libSystem. No application
  is launched by a build, and no dependency is downloaded.
- M0-05 now validates the shared manifest and embeds game/editor/content,
  compatibility, target and configure-time source identity in core/headless and
  probe diagnostics. SemVer ordering, independent tags and prospective artifact
  names share one offline reader. Core Release/Debug pass 7 CTests each; desktop
  Release passes 13; identity and wrapper suites pass 21 and 11 checks. Component
  versions were planning placeholders and the runtime content hash was
  unavailable at that stage. E1A-04 now supplies finalized internal content
  identity; release readiness still fails closed pending actual later gates.
- Added `pgp-probe`, original bundled PNG/JPEG art, independent graphics/resource/
  audio libraries and an always-available silent mode. Metal checks pass at
  1280×720 with 100%/200% text, visible keyboard focus and accessible button name;
  explicit offscreen software also passes. Desktop Release/Debug and native-audio
  Release each pass 10 CTests; core-only still passes five without Qt/audio.
  [Local evidence and screenshot](evidence/M0-03-local-probe.md) and
  [audio evidence](evidence/M0-03-audio.md) record actual results and limits.

## Completed planning

- Reviewed all 21 sections of the supplied Gemini4 recommendations against baseline 1.1 (`a218fc6`), the owning specifications, fresh local evidence and primary technical sources. [Disposition record](RECOMMENDATION_REVIEW.md) explains every adopted, corrected or deferred recommendation and its implementation gate.
- Updated game/simulation/technical contracts, roadmap/backlog, Studio/versioning, QA, resource inventory and role briefs together. Clarified checked wide arithmetic/strong units, constant-2014 cents, prepared mornings and saved continuation stacks, explicit effects/anchors, finite inference, M5 wholesale capacity, persistent UI models and objective-breach feedback.
- Split E1 into E1a forms/library/inspection and safe internal snapshots before M3 content, then E1b required node canvas/graphical finalizer/three-author study before M4. Full authoring scope remains required. Effort is resequenced, with no measured calendar-saving claim.
- Added concrete scoped evaluation findings, reserved-pool equity compensation and bounded late-game policies. Deferred blanket refunds, automatic Qt downgrades, rigid hardware growth, speculative spin-off entrants and softened victory windows.
- Preserved the two original drafts and newly archived Gemini4 review verbatim with SHA-256 fingerprints. Sources remain advisory; no embedded commands were executed. User-supplied files outside the repository are unchanged.
- Six illustrative JSON contracts now cover balance, model report, supplier opportunity, six lifecycle graphs including nested continuation state, effect-direction controls and numerical/window boundaries. These are design examples, not production schemas or passing runtime fixtures.
- Existing planning-only `project-versions.json` remains unchanged. No game/editor/content release identity was fabricated. Original offline privacy, separate editor packaging, content/provenance and no-auto-launch rules remain in force.

## Implementation state

The Mac game runs the complete 2014–2017 slice through ordinary Office/Lab
controls, with lab qualification, paid contracts, bounded rivals, recovery,
delegation, exact saves and scoring. The Civic room adds domestic policy
and elections. Content Studio browses the checkout,
edits typed flows through forms/outline and inspects copied runtime state.
The shared writer finalizes validated internal snapshots through both graphical
review and the maintainer CLI. E1A-05 demonstrates the authoring path, E1B-01
supplies the typed canvas and E1B-02 completes graphical review/finalization.
E2-01 through E2-03 supply scenario forms, catalogs, localization, asset credits,
typed reusable flows, finalized revision comparisons and reviewed save copies.
Human studies wait for v1 preparation after all components are in place.
14 steps remain unchecked, including those studies and deferred platform/release
work. [Playability checkpoints](PLAYABILITY.md) distinguish implemented paths
from unperformed human and release qualification.

M0-01 supersedes review-time spot checks: the Xcode roots resolve to one
installation, three installed SPDX files were parsed, and exact Qt archive
metadata was found for all five targets. Approximately 23.35 GiB was free during
inventory. Qt Multimedia is absent; it is not required. Desktop discovery warns
about optional `Qt6QmlAssetDownloaderPrivate` needing absent `Qt6TaskTree`;
The selected Basic-controls imports render without that unused plugin, and local
Metal/Core Audio evidence now exists. Archive-body acquisition, Actions execution,
Windows/Linux graphics/audio, deployed licensing/relocation and clean-device
qualification remain unproved.

## Planning verification

Historical planning-baseline checks passed before implementation:

- 28 active Markdown documents and 86 relative file/anchor links; tables, fences and whitespace.
- Six example JSON files plus the unchanged planning manifest; integer-only values and explicit effect targets.
- 50 unique open tasks, resolved acyclic dependencies and E1a/M3/E1b/M4 gates.
- Seven flow examples and 29 nodes covering five lifecycles; resolved/reachable acyclic links and a hand-walk of nested saved continuations in the specified order.
- Wide/single-round arithmetic, constrained budget residues, opening finance/ownership, serving capacity, effect boundaries/direction, exact 60/120-month windows and the 36,524-day campaign interval (2100 is not a leap year).
- Roadmap effort/reserve arithmetic; three archive hashes/bytes and all 21 review dispositions; component manifest unchanged from `a218fc6`.

Those historical checks cover planning documents/data/examples only. Current
runtime evidence is recorded above and in the implementation log; UI performance,
native packages, human usability and balance remain later gates. M2-03 now
implements the bounded allocation policy described in its local evidence; it
does not claim an optimal-utility solver or a viable three-year strategy.

## Initial feature coverage

**Next implementation closes initial feature gaps against the game design.**
The 2026-11-06 user instruction sets the order as initial features, expansion
and refinement with human feedback, substantial event expansion, then balance.
Extended endurance runs and broad regression are deferred. Use short checks
for changed behavior. The existing 42 checked batches do not prove complete
coverage of the full design.

Campaign setup now includes company/founder names, optional pronouns, local
name generation and an editable full-width world seed. Names appear in the
company and leadership records, and the same content reloads custom-seed
campaigns exactly. Old saves and default callers retain their existing setup.
The new form shows the selected scenario's opening cash, team and compute.
The optimized build, three short save/leadership checks and offscreen form
checks at 100%/200% text pass.

Campaign setup now also offers all four founder backgrounds and three
organizational charters. Backgrounds change actual opening resources and
ordinary contact offers. Charters change nonplayer board voting priorities
and fund explicit monthly employee programs. The board can approve an amendment,
which replaces future program charges and preserves prior votes and costs.
The setup preview includes the combined opening commitments. Internal content
0.15.0-dev.1 records these packages; their numbers remain provisional.
The optimized build and three focused setup/governance checks pass in 3.11s.
Short offscreen QML checks at 100%/200% text start a company, propose/vote
a charter amendment and preserve the saved state.

Explicit difficulty is now implemented. Forgiving, Standard and Demanding
profiles preview and apply opening capital after background adjustments,
player financing proceeds, rival strategy review/reaction timing and training
forecast margins. The profiles change conditions and information rather than
model, evaluation or objective rules. Saves record the selection and reconcile
its derived catalogs. Content 0.16.0-dev.1 has 21 exact opening round trips.
The optimized build and three focused setup checks pass in 2.52s. Short
offscreen QML checks at 100%/200% text verify the real selected terms, forecast
precision, settings-refresh selection preservation, exact saves and invalid
choice recovery.

The Lab now also supports pause/resume and immutable checkpoints while source
training continues. Paused finite work releases compute/staff; resume requests
the original allocation and refuses safely if resources are busy. Existing
obligations keep accruing. A snapshot pins actual settled work and can be
evaluated or continued independently of its running source. Pause history and
snapshot provenance reload exactly. The optimized build and three focused
capacity/Lab checks pass in 2.28s; short offscreen controls pass at 100%/200%
text. Content remains 0.16.0-dev.1; these are additive engine/save controls.

Office → Policies and alerts now authorizes bounded product pricing alongside
evaluation and lease renewal. Players choose floor/ceiling, adjustment step,
low/high utilization thresholds, 7–365-day cadence, reserve floor and at most
10,000 reviews. Reviews use up to seven settled operating days and hold through
known capacity reductions or a changed serving allocation. Protected contracts
keep their agreed terms. Manual price changes suspend the policy; explicit resume
retains its counters and requires the current quote inside the original range.
Append-only price events preserve earlier same-day decisions after an override.
The decision log shows 128 records per page, with all prior pages accessible.
The optimized build and four focused checks pass (5.43s); short offscreen QML
checks at 100%/200% text pass through authorization, review, override and reopening.

The Office adds two more bounded policies. An operating reserve sets a company
cash target for new spending, including delegated research and procurement.
Accepted bills and principal can draw on the reserve. Receipts refill it
automatically and the decision log retains its authorizations and releases.
Hardware replacement buys a compatible successor at the original rack's rated
useful-life date under purchase and monthly maintenance ceilings. It stages the
successor at the same site, then waits for commissioning, enough usable capacity
and clearance of active or paused work and protected commitments before retiring
the original. Both actions reconcile against actual paid hardware records.
The optimized build and five focused checks pass, with offscreen QML form and
save/reopen checks at 100% and 200% text.

Both century scenarios now have an IPO. It uses a disclosed age and settled
revenue/profit window, and the majority vote rechecks eligibility. Listing
raises contributed equity and adds a public shareholder director. The parent
company files quarterly results from closed accounts. After a reported loss,
the director opposes acquisitions and pool expansion. The public issuance also
enters the existing heavyweight financing observation.
Internal content 0.17.0-dev.1 finalizes these terms. The optimized build, four
focused checks and offscreen review, vote and filing checks at 100% and 200% pass.

All seven scenarios now offer a restricted grant for three research topics and
an expiring credit for the opening lease. Awards activate the next morning.
The grant pays part of eligible Lab and delegated research fees within its cap.
The cloud credit reduces actual lease payables as the daily charges accrue.
Both keep saved claim and redemption records. Internal content 0.18.0-dev.1
passes review across all seven scenarios and 21 headquarters openings. The
optimized build, six focused checks and short QML save/reopen checks at 100%
and 200% text pass.

The opening enterprise offer in all seven scenarios now includes a $20,000
advance. Accepting adds cash and a matching customer liability. Only earned
delivery counts as revenue. Any unused advance becomes a payable at the end
of the contract or on cancellation. Compare the terms in Office → Compare
offers and follow usage and refunds in Office → Contracts. Internal content
0.19.0-dev.1 passed the seven-scenario and 21-opening review. The optimized
Mac build, eight focused checks and short QML runs at 100% and 200% text passed.

Office Accounts now has separate statements, cash scenarios, and bills and
debt views. The statements show current balances and 30 settled days of
earnings, cash movements and product or wholesale unit economics. The
3- and 12-month scenarios compare 50%, 100% and 150% of recent discretionary
delivery at current prices within today's usable capacity. They apply signed
terms, advance balances, recurring and owned-site costs, and debt payments
and show the first projected unpaid obligation. The optimized Mac build,
eight focused checks and short keyboard checks at 100% and 200% text pass.

All seven scenarios now use a fictional 15% quarterly corporate tax. Booked
income and deductions determine the base. A limited loss carry and a credit
on net research fees can reduce the bill. Quarter-end assessments create
payables due 30 days later. The same rules apply to planned rivals from
entry, and the Office shows the terms, current base and actual settlement.
Internal content 0.20.0-dev.1 passed review for all 21 headquarters openings.
The optimized build, 12 focused checks and short keyboard checks at 100%
and 200% text pass.

Lab → Deploy can stage a qualified candidate alongside a live release for a
limited share of discretionary orders. Production keeps signed work. The trial
reserves staff and compute and pays support until it ends. Office → Services
shows settled trial deliveries and lets the player promote, discard or restore
the previous distinct qualified release. Promotion and rollback recheck full
capacity, signed promises and current launch rules.
The optimized Mac build and 12 focused checks pass. Short offscreen keyboard
checks at 100% and 200% text cover stage, settled work, promotion and rollback.

Office Services can now publish a compatibility term or a binding retirement
date. Replacements and trials must honor published capacity, quality, review
and buyer hardware terms. Existing customers favor supported integrations.
After a retirement notice, the latest installed share caps discretionary
adoption and new contracts must end by retirement. The deadline ends serving
and support costs after the final contracted day. Cash scenarios apply the
published end date. The optimized Mac build and 11 focused checks pass.

Office Services can now set a daily waitlist limit, pause admissions, or
reopen enrollment. Signed work goes first and recent customers get priority
within discretionary capacity. The longest-waiting eligible groups receive
new slots first. The waitlist records interest and actual admissions. The
market and ledger recognize only settled deliveries. The optimized Mac build
and 10 focused checks pass.

Lab Deploy now reviews public reuse or a local-use grant for one eligible
buyer group. Qualified publication shares the frozen weights and hardware
requirements. Previously served groups keep their copies when the supported
version changes or retires. They can run the retained checkpoint on compatible
hardware through the same regional demand pool. Office Services shows retained
buyers and actual local work. The optimized Mac build and 11 focused checks pass.

Agent services now offer draft-only, internal, and external permissions with
individual approval or batch review. Lab Deploy previews attempt capacity and
repair exposure. Office Services shows actual successes, caught failures,
executed failed writes, and repair payables. Staging, promotion, rollback and
compatibility terms retain their control settings.

The Lab now trains and measures six capability axes. Four new datasets target
software and reasoning, science and industry, social interaction, and
autonomous workflows. The independent capability suite qualifies these domains
for coding APIs, research toolkits, social companions and robotics agents.
The introductory document and scanned-table tests retain their scope. Reviewed
content **0.21.0-dev.1**, generation **21**, passes all 21 opening round trips.
[Lab capabilities and deployment](../src/core/LAB.md).

Office Services now compares settled work for the same release and buyer
before and during an authored workload change. A material completion drop
pauses the next morning and links the release report's measured coverage.
Staged candidates keep separate results. Two regional demand incidents also
change the mix of agent tasks until their expiry. Generation **22** of reviewed
content is **0.22.0-dev.1** and passes all 21 prepared opening round trips.

Lab Data now generates verified document or workflow examples using a frozen
teacher, independent evidence and a separately licensed verification source.
Paid finite work reserves compute and staff, supports pause/resume and retains
spent fees on cancellation. Completed output becomes a fresh training input
with saved lineage, coverage and rights. Shared source diversity limits and
teacher fingerprints prevent cloning from multiplying available information.
Reviewed content **0.23.0-dev.1**, generation **23**, validates all seven
scenarios and 21 exact opening saves. The optimized build, 13 focused CTests
and offscreen keyboard checks at 100%/200% text pass.

Office Services shows fictional risk bands for workflow and robotics agents,
with separate reports for staged releases. Monthly draws use prior settled work,
the scenario's risk lookup and a saved independent stream. A due incident stops
the affected release's work, creates a customer compensation bill and pauses
the morning for review. Other releases retain their resources. A paid
investigation releases containment after actual staff and compute work.
Replacing the release preserves its incident and bill. The Inbox records
discovery and recovery on their actual dates. Content **0.24.0-dev.1**,
generation **24**, validates seven scenarios and 21 exact prepared openings.

Lab Data now offers commissioned table transcriptions, licensed university
experiments, and opt-in agreements for enterprise document customers. Jobs
reserve staff and compute, retain fees on cancellation, and deliver immutable
corpora after actual work. Feedback counts only settled deliveries for the
named service and buyer group inside the accepted consent window. The grant
limits the opted-in share and redistribution rights. Completed output becomes
available for fresh training. Reviewed content **0.25.0-dev.1**, generation
**25**, validates seven scenarios and 21 exact prepared openings.

**Next initial feature:** staff workload and recovery. Make sustained load
visible, let the player reduce pressure by changing staffing or commitments,
and carry the effect on effective staff work across saves.

**Deferred until v1 preparation:** M3-04's five independent human sessions
(including two non-ML players and three unaided completions) and E1B-03's
three-author study. The user has no testers available and wants all components
in place first. Keep their acceptance criteria and the
[prepared session kit](playtests/M3-session-guide.md); do not schedule sessions
or treat their absence as an implementation blocker. Engineering runs are not
substitutes for later human evidence. Native packaging remains deferred.

The 2026-11-04 event clarification adds M1-07 (built-in detection/dispatch) and
M3-05 (AI-assisted playable base-event pack), with human stories augmenting the
baseline. The eight-flow matrix covers milestones, evaluation/data recovery,
contract results, liquidity, rivals, supplier opportunities, random world demand
and recovery. See [event coverage, AI assignments and modular extension
rules](EVENT_SYSTEM.md). The Mac event pack and interactive scenario are implemented; the remaining
slice, authoring, human-validation and release gates stay open.

Native platform, package/device, human study and public-release tasks remain
recorded for their owning milestones. Continue the Mac feature sequence without
waiting for those deferred checks. See [needs and access](NEEDS_AND_ACCESS.md)
for later human and release requirements.

Provisional estimate: 13–21 person-weeks through initial M3 with E1a; 15–25 including E1b before M4; 44–72 through M9 before reserve (55–90 with 25%). Re-estimate after M0, first gameplay and author testing. [Planning history](PLAN_CHANGELOG.md) records the changes.
