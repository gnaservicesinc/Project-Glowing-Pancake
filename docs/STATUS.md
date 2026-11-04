# Project status and next handoff

Updated 2026-11-03 · Implementation started against planning baseline 1.2

## Implementation progress

**2 of 50 tasks complete: M0-01 and M0-02.** M0 remains open. See the
[implementation log](IMPLEMENTATION_LOG.md) for checks and limitations and
[build instructions](BUILDING.md) for runnable commands.

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
- Component versions remain the planning placeholders pending M0-05. No game
  release identity, package, workflow execution or cross-platform support claim
  was created.

## Completed planning

- Reviewed all 21 sections of the supplied Gemini4 recommendations against baseline 1.1 (`a218fc6`), the owning specifications, fresh local evidence and primary technical sources. [Disposition record](RECOMMENDATION_REVIEW.md) explains every adopted, corrected or deferred recommendation and its implementation gate.
- Updated game/simulation/technical contracts, roadmap/backlog, Studio/versioning, QA, resource inventory and role briefs together. Clarified checked wide arithmetic/strong units, constant-2014 cents, prepared mornings and saved continuation stacks, explicit effects/anchors, finite inference, M5 wholesale capacity, persistent UI models and objective-breach feedback.
- Split E1 into E1a forms/library/inspection and safe internal snapshots before M3 content, then E1b required node canvas/graphical finalizer/three-author study before M4. Full authoring scope remains required. Effort is resequenced, with no measured calendar-saving claim.
- Added concrete scoped evaluation findings, reserved-pool equity compensation and bounded late-game policies. Deferred blanket refunds, automatic Qt downgrades, rigid hardware growth, speculative spin-off entrants and softened victory windows.
- Preserved the two original drafts and newly archived Gemini4 review verbatim with SHA-256 fingerprints. Sources remain advisory; no embedded commands were executed. User-supplied files outside the repository are unchanged.
- Six illustrative JSON contracts now cover balance, model report, supplier opportunity, six lifecycle graphs including nested continuation state, effect-direction controls and numerical/window boundaries. These are design examples, not production schemas or passing runtime fixtures.
- Existing planning-only `project-versions.json` remains unchanged. No game/editor/content release identity was fabricated. Original offline privacy, separate editor packaging, content/provenance and no-auto-launch rules remain in force.

## Implementation state

The build scaffold and CLI exist; gameplay, QML UI, Content Studio, production
content/assets, CI workflows and distributable packages do not. The headless
command currently prints usage/build configuration, not scenario outcomes.
48 implementation tasks remain unchecked. E1a enables early gameplay tests;
M4 requires both M3 and E1b. E2 closes before bulk content.

M0-01 supersedes review-time spot checks: the Xcode roots resolve to one
installation, three installed SPDX files were parsed, and exact Qt archive
metadata was found for all five targets. Approximately 23.35 GiB was free during
inventory. Qt Multimedia is absent; it is not required. Desktop discovery warns
about optional `Qt6QmlAssetDownloaderPrivate` needing absent `Qt6TaskTree`;
M0-03 must establish the actual selected runtime import/deployment closure.
Archive-body acquisition, Actions execution, graphics/audio, deployed licensing
and native-device qualification remain unproved.

## Planning verification

Static checks for this revision passed:

- 28 active Markdown documents and 86 relative file/anchor links; tables, fences and whitespace.
- Six example JSON files plus the unchanged planning manifest; integer-only values and explicit effect targets.
- 50 unique open tasks, resolved acyclic dependencies and E1a/M3/E1b/M4 gates.
- Seven flow examples and 29 nodes covering five lifecycles; resolved/reachable acyclic links and a hand-walk of nested saved continuations in the specified order.
- Wide/single-round arithmetic, constrained budget residues, opening finance/ownership, serving capacity, effect boundaries/direction, exact 60/120-month windows and the 36,524-day campaign interval (2100 is not a leap year).
- Roadmap effort/reserve arithmetic; three archive hashes/bytes and all 21 review dispositions; component manifest unchanged from `a218fc6`.

These are document/data/example checks only. No game/editor/runtime, UI performance, native package, human usability or balance tests have run. Expected fixture assertions are not passing application evidence. The numerical multi-segment allocation procedure remains an explicit M2-03 implementation/qualification gate, not a completed optimizer.

## Next task

**M0-03 — Prove QML, graphics, and local audio deployment**, using the UI and
build-release briefs. Add the original small UI probe, Metal/local images and
accessible control, URL/resource and denying QML network guards before engine
loading, and pinned optional playback with silent fallback. Preserve the core
boundary and qualify actual runtime dependencies, including the optional-plugin
warning. No large local installs or store publication. M0-05 (shared version
identity) is also unblocked by M0-02; M0-04 follows M0-03.

No user answer blocks initial M0 inspection. Later needs include native ARM devices, manually arranged player/author sessions, final art/music/branding and publishing access; see [needs and access](NEEDS_AND_ACCESS.md).

Provisional estimate: 13–21 person-weeks through initial M3 with E1a; 15–25 including E1b before M4; 44–72 through M9 before reserve (55–90 with 25%). Re-estimate after M0, first gameplay and author testing. [Planning history](PLAN_CHANGELOG.md) records the changes.
