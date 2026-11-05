# Project status and next handoff

Updated 2026-11-04 · Implementation started against planning baseline 1.2

## Implementation progress

**17 of 52 steps complete for Mac feature development.** Completed work includes
M0-01/02/03/05, M1-01 through M1-07, M2-01 through M2-04 and E1A-01/02.
The user’s current instruction prioritizes game features on Mac. Implemented
Mac behavior and focused checks close feature steps, while native platform,
packaging and device qualification remain deferred. Earlier evidence retains
its historical prerequisite and acceptance language. See the
[implementation log](IMPLEMENTATION_LOG.md) for checks and limitations and
[build instructions](BUILDING.md) for runnable commands.

**Mac-first development, per the 2026-11-04 user instruction:** use incremental
local optimized builds and focused changed-behavior checks. Native matrices are
manual-only; cross-platform/full release qualification is deferred and does not
block Mac implementation. Keep build caches between batches.

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
  versions remain planning placeholders, the runtime content hash is explicitly
  unavailable, and release readiness fails closed pending actual later gates.
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

The build scaffold, shared version identity, deterministic primitives, typed
content/compiler/runtime, transactional state, local persistence/campaign policy,
worker/model bridge, business accounts, shared project capacity, contracts,
liquidity recovery, a three-year CLI scenario and development QML probe exist.
Content Studio can browse the local library, edit typed flows and save drafts.
Impact inspection, the player scenario, finalized runtime content and
packages remain open. Existing native CI proves primitive parity on all
five targets; the expanded foundation suite is checked separately. The headless
command validates local content, demonstrates commands/flows and can explicitly
save/reload suspended continuations. [Playability checkpoints](PLAYABILITY.md) separate this engineering
demonstration from the M2 business scenario and M3 player-facing slice.
E1a enables early gameplay tests;
M4 requires both M3 and E1b. E2 closes before bulk content.
35 steps remain unchecked, including the deferred platform/release work.

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

**Next Mac implementation: E1A-03 trigger-impact and lifecycle inspection.**
Run working flows on copied scenario state and expose condition truth, ordered
effects, presentations and continuations.
Content finalization and save migrations remain later feature work.
Platform packaging remains deferred.

The 2026-11-04 event clarification adds M1-07 (built-in detection/dispatch) and
M3-05 (AI-assisted playable base-event pack), with human stories augmenting the
baseline. The eight-flow matrix covers milestones, evaluation/data recovery,
contract results, liquidity, rivals, supplier opportunities, random world demand
and recovery. See [event coverage, AI assignments and modular extension
rules](EVENT_SYSTEM.md). M3-05 remains planned with prerequisites.

Native platform, package/device, human study and public-release tasks remain
recorded for their owning milestones. Continue the Mac feature sequence without
waiting for those deferred checks. See [needs and access](NEEDS_AND_ACCESS.md)
for later human and release requirements.

Provisional estimate: 13–21 person-weeks through initial M3 with E1a; 15–25 including E1b before M4; 44–72 through M9 before reserve (55–90 with 25%). Re-estimate after M0, first gameplay and author testing. [Planning history](PLAN_CHANGELOG.md) records the changes.
