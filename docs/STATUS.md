# Project status and next handoff

Updated 2026-11-05 · Implementation started against planning baseline 1.2

## Implementation progress

**30 of 52 steps complete for Mac feature development. M5-02 is next.** Completed work includes
M0-01/02/03/05, M1-01 through M1-07, M2-01 through M2-04, M3-01/M3-02/M3-03/M3-05, E1A-01 through E1A-05, E1B-01/02, M4-01 through M4-03 and M5-01.
**Per the 2026-11-05 user instruction, human testing waits until all components
are in place and the user is ready for v1 work.** M3-04 and E1B-03 remain open
and do not block later component implementation. The local session kit is
prepared for that later phase; no testers are being arranged and no human
sessions have been conducted.
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
delegation, exact saves and scoring. Content Studio browses the checkout,
edits typed flows through forms/outline and inspects copied runtime state.
The shared writer finalizes validated internal snapshots through both graphical
review and the maintainer CLI. E1A-05 demonstrates the authoring path, E1B-01
supplies the typed canvas and E1B-02 completes graphical review/finalization.
E2 expands authoring as later schemas arrive.
Human studies wait for v1 preparation after all components are in place.
26 steps remain unchecked, including those studies and deferred platform/release
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

## Next task

**Next implementation: M5-02 international profiles and product channels.**
Add eight regions, six model channels, twelve product templates and shared
regional useful-work accounting. Implement paid wholesale reservations from
owned capacity with compatibility, curtailment terms and balanced ledgers.
An infrastructure provider needs a viable revenue path without model training.

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
