# Project status and next handoff

Updated 2026-11-04 · Implementation started against planning baseline 1.2

## Implementation progress

**5 of 52 tasks accepted: M0-01, M0-02, M0-05, M1-01 and M1-02; M0-03 and M1-03/04/06 are in progress.** M0-03's local
interface/resource/audio implementation passes; native/deployment gates remain.
M0 remains open. See the
[implementation log](IMPLEMENTATION_LOG.md) for checks and limitations and
[build instructions](BUILDING.md) for runnable commands.

- M1-06 now compiles/executes the current typed schema: five lifecycles,
  nested sequence/subflow delays and choices, selected root sources, owned
  modifiers, bounded atomic effects and committed/candidate traces. Broader
  variables/expressions, duration, selectors/quantifiers and random branches
  remain required work. [Runtime evidence](evidence/M1-06-runtime.md)
- M1-04 now writes/loads the entire implemented checkpoint with exact resumed
  state, recovery copies, strict identity/integrity checks, three autosave slots,
  preferences, Unicode filenames, portable/standard paths and export. Campaign
  policy saves before irreversible consumption/choices and at monthly/final
  boundaries, preserving live state on errors. Core Release passes 25 CTests;
  desktop Debug passes 32. [Save evidence](evidence/M1-04-persistence.md)
  and [application policy](../src/session/README.md) describe the implemented
  subset. These tasks remain open for their full contracts/prerequisites.

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
worker/model bridge, CLI and development QML interface probe exist.
Business gameplay, Content Studio, finalized runtime
content and packages do not. Existing native CI proves primitive parity on all
five targets; the expanded foundation suite is checked separately. The headless
command validates local content, demonstrates commands/flows and can explicitly
save/reload suspended continuations. [Playability checkpoints](PLAYABILITY.md) separate this engineering
demonstration from the M2 business scenario and M3 player-facing slice.
E1a enables early gameplay tests;
M4 requires both M3 and E1b. E2 closes before bulk content.
47 implementation tasks remain unchecked, including the locally implemented
M1-03 task awaiting its prerequisite gate.

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

These are document/data/example checks only. No game/editor/runtime, UI performance, native package, human usability or balance tests have run. Expected fixture assertions are not passing application evidence. The numerical multi-segment allocation procedure remains an explicit M2-03 implementation/qualification gate, not a completed optimizer.

## Next task

**Next gameplay-enabling implementation: finish M1-06's broader flow language**
and persist its additional pending state: registered variables/expressions,
duration predicates, related selectors/quantifiers and weighted random branches.
Then M1-07 adds built-in committed-fact/seeded occurrence dispatch. M1-03 has a
locally tested worker/model implementation; its formal acceptance remains gated
by M0-03. M1-05's room shell can proceed alongside core work; M2-01 begins the
balanced business accounts after the foundation contracts. These
local implementations do not close the outstanding platform gate. Content
finalization, save migrations and packaging remain later work.

The 2026-11-04 event clarification adds M1-07 (built-in detection/dispatch) and
M3-05 (AI-assisted playable base-event pack), with human stories augmenting the
baseline. The eight-flow matrix covers milestones, evaluation/data recovery,
contract results, liquidity, rivals, supplier opportunities, random world demand
and recovery. See [event coverage, AI assignments and modular extension
rules](EVENT_SYSTEM.md). These two tasks remain planned, with prerequisites.

**Finish M0-03 qualification**, using the UI/build-release briefs and the checked
local evidence. Run native Linux OpenGL and Windows OpenGL/D3D11 recovery,
resource/audio checks and deployed import/dependency/license/relocation probes on
hosted/native environments. Preserve the small local disk budget. Physical loss/
driver/reader gaps must remain explicit. No store publication. M0-04 follows
M0-03; M0 remains open despite M0-05 completion.

No user answer blocks initial M0 inspection. Later needs include native ARM devices, manually arranged player/author sessions, final art/music/branding and publishing access; see [needs and access](NEEDS_AND_ACCESS.md).

Provisional estimate: 13–21 person-weeks through initial M3 with E1a; 15–25 including E1b before M4; 44–72 through M9 before reserve (55–90 with 25%). Re-estimate after M0, first gameplay and author testing. [Planning history](PLAN_CHANGELOG.md) records the changes.
