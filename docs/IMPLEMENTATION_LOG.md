# Implementation progress

Started 2026-11-03 against planning baseline 1.2. This log records completed
behavior and evidence; the [backlog](BACKLOG.md) remains the task dependency list.

| Task | State | Scope and evidence |
| --- | --- | --- |
| M0-01 | Complete | [Toolchain evidence](evidence/M0-01-toolchain.md), two reproducible inventory tools and [five-target metadata observations](evidence/M0-01-qt-archives.json); no SDK installation |
| M0-02 | Complete | [CMake/build entry points](BUILDING.md), separate core/headless targets, four configurations and safety checks below |
| M0-03 | Complete for Mac features | Local QML/Metal probe, reusable resource guards and optional native/silent audio implemented; native Windows/Linux/deployment qualification remains open |
| M0-05 | Complete | Shared version manifest validation, generated core/headless/probe metadata, product tags/artifact stems and fail-closed readiness; verification below |
| M1-01 | Complete | Strong units, checked arithmetic, dates, saved RNG streams and exact 1,000-day primitive parity across six native compiler jobs/five targets |
| M1-02 | Complete | Typed content schema/registry, bounded offline loader, 11-definition engineering fixture and 140 validation assertions; [evidence](evidence/M1-02-content.md) |
| M1-03 | Complete for Mac features | Local commands, morning continuation, immutable worker and stable model diffs pass; M0-03 prerequisite qualification remains open; [evidence](evidence/M1-03-commands.md) |
| M1-04 | Complete for Mac features | Exact current-state file saves, recovery, autosaves/settings/export and application persistence policy; [evidence](evidence/M1-04-persistence.md) |
| M1-05 | Complete for Mac features | Office, Lab, Inbox, shared clock, decisions, inspector and local saves/preferences; [room shell evidence](evidence/M1-05-room-shell.md) |
| M1-06 | Complete for Mac features | Shared compiler/executor, five lifecycles, nested waits/choices, modifiers, random branches, durations, scoped variables, expressions and related-entity quantifiers; [runtime](evidence/M1-06-runtime.md), [branches](evidence/M1-06-weighted-branches.md), [durations](evidence/M1-06-duration.md), [selectors](evidence/M1-06-selectors.md) |
| M1-07 | Complete for Mac features | Committed project/contract dispatch, bounded seeded world reviews, exact pending continuation and weekly digest. [Committed facts](evidence/M1-07-committed-facts.md), [seeded incidents and digest](evidence/M1-07-seeded-digest.md) |
| M2-01 | Complete for Mac features | Balanced accounts, actual-month costs, invoices, debt/equity, protected settlements and pure projections through commands/exact saves; [evidence](evidence/M2-01-accounts.md) |
| M2-02 | Complete for Mac features | Shared source/staff reservations, compatibility, serving-first outage allocation, actual finite work, cancellation, support costs and exact reload; [evidence](evidence/M2-02-capacity.md) |
| M2-03 | Complete for Mac features | Finite allocation, contracts/resource priority, billing/credits/deadlines and installed shares through typed scenarios/exact saves; [evidence](evidence/M2-03-market.md) |
| M2-04 | Complete for Mac features | Three-year scenario, monthly reconciliation, exact continuation, saved 30-day workout/rescue/closure; [evidence](evidence/M2-04-business.md) |
| E1A-01 | Complete for Mac features | Separate Content Studio, typed checkout library, flow tag draft editing, undo/redo and conflict-aware writes. [Library](evidence/E1A-01-library.md), [draft workflow](evidence/E1A-01-drafts.md) |
| E1A-02 | Complete for Mac features | Typed predicates, all five lifecycles, ordered execution, scoped variables and reference edits. [Trigger forms](evidence/E1A-02-triggers.md), [lifecycle forms](evidence/E1A-02-lifecycles.md), [execution forms](evidence/E1A-02-execution.md) |
| E1A-03 | Complete for Mac features | Isolated scenario/state/RNG tests, exact clause and scope traces, effect/ledger previews, boundary probes, shared presentations, natural lifecycle and exact reload. [Inspection](evidence/E1A-03-inspection.md) |
| M3-01 | Complete for Mac features | Paid data/research, finite training/lineage and held-out subgroup evidence; 88 lab checks |
| M3-05 | Complete for Mac features | Eight base-event families, paid offers, bounded rivals and exact three-year baseline; 188 event checks |
| M3-03 | Complete for Mac features | Saved bounded policies, optional pause/deadline/milestone routing, warned setback, paid specialist/financing routes and explicit slice scores; 352 integration checks and complete Metal UI runs |
| M3-02 | Complete for Mac features | Bundled interactive campaign, guide, measured qualification/controls and dated serving replacement; 29 deployment and 16 management checks plus complete Metal keyboard runs at 100%/200% |

## Mac feature scope update — 2026-11-04

The user clarified that the goal is the game’s features on Mac and that Linux,
Windows and release qualification should remain deferred. The tracker now
counts implemented Mac behavior with focused evidence as completed feature
work. M0-03, M1-03 through M1-07, M2-01 through M2-04 and E1A-01/02 join the
previous five completed steps, bringing the current count to **17/52**.
These twelve implementations already have linked evidence in the table.
The dated entries below retain the earlier count as history. Platform/device,
release and human qualification work remains recorded in its owning tasks.
E1A-03 is the next feature batch.

## First implementation slice

The requested implementation begins with the first two ordered M0 tasks. The
core library and headless entry point are separate CMake targets. Qt discovery
belongs to desktop configuration, with no Qt linkage in the core. Future content,
persistence, bridge and editor targets follow the dependency direction in the
[technical plan](TECHNICAL_PLAN.md); no speculative subsystem is added yet.

Each configuration has its own output directory. Tests check actual Release/Debug
diagnostic behavior and command failures. Wrapper checks cover toolchain identity,
cleanup ownership, symlink rejection and error propagation.

## M0-01/M0-02 verification — 2026-11-03

Native arm64 Mac, Apple Clang 21.0.0 with one canonical Xcode-beta/SDK 27.3,
CMake 4.4.4, Ninja 1.13.2; Qt 6.12.0 for desktop discovery. Full paths and
dependency/source evidence are in the M0-01 report. No gameplay/content fixture
or scenario seed applies to this build-only slice.

| Check | Actual result |
| --- | --- |
| `./make.sh --core-only --test` | Release builds; 5/5 CTests pass without Qt discovery/linkage |
| `./make.sh --test` | Desktop Release SDK discovery/build; 5/5 CTests pass |
| `./make.sh --core-only --debug --test` | Debug core builds; 5/5 CTests pass |
| `./make.sh --debug --test` | Desktop Debug SDK discovery/build; 5/5 CTests pass |
| Plain `./make.sh` after Debug | Selects Release; no Debug flags/diagnostics leak |
| Independent LLVM 23.1.3 direct core Release build | 5/5 CTests pass; system libc++/libSystem linkage |
| `python3 tools/tests/test_build_wrapper.py` | 11/11 checks pass using isolated mock build tools; cleanup preserves foreign data and rejects symlinks/unknown owners; configure/build/CTest failures propagate |
| `/bin/bash -n make.sh tools/build.sh` | Passes with macOS Bash 3.2 |
| Release/Debug generated Ninja inspection | Release `-O3 -DNDEBUG`, diagnostics=0; Debug `-g`, diagnostics=1; C++20 and selected SDK/arm64 explicit |
| `otool -L` on Release headless | Only system libc++/libSystem; no Qt/developer runtime path |
| Fresh direct desktop CMake/build/CTest | 5/5 pass; selected Apple compiler/SDK and explicit arm64 recorded before language initialization |
| Direct cache mismatch probes | Changed compiler, SDK and architecture each rejected before build |
| Documentation/data consistency | 30 active Markdown files; 101 relative file links resolve; JSON parses; task count is 2 complete/48 pending |

After verification, the four wrapper-owned build trees and this task's direct
LLVM/inventory scratch outputs were removed. Rebuild with the documented commands;
source and checked evidence remain in Git.

The compile/link probe caught and corrected a compiler-driver issue: resolving
the `clang++` symlink all the way to `clang` changes default C++ runtime linkage.
The build preserves the C++ driver invocation while canonicalizing its owning
directory. Direct desktop configuration also resolves architecture before
`project()` initializes CMake's host-processor variable.

Required Qt components resolve. Qt reports an optional plugin warning:
`Qt6QmlAssetDownloaderPrivate` cannot find `Qt6TaskTree`. M0-03 must qualify its
actual selected QML imports/deployed closure; the unused tool/plugin warning is
recorded without suppressing it or downloading another SDK.

At the end of M0-02 no renderer/audio device, native CI/deployment or package had
been tested. The build then had no audio dependency. The following M0-03 entry
records the new presentation targets and evidence; M0 remains incomplete.

There is no playable game, graphical editor, deployed package or public release
at this stage. Building never launches an application.

## M0-03 local implementation — 2026-11-03

Implemented a runnable QML preview with original local art, accessible controls
and lazy sound activation; independent graphics, resource-policy and audio
targets preserve the Qt-free core. The guards precede QML loading. Remote
resources/requests and canonical file escapes are denied; approved local reads
work without a network transport path in the manager.

- Desktop Release and Debug: **10/10 CTests each**; optional native-audio Release:
  **10/10**; core-only Release: **5/5**, with unchanged system-only linkage.
- Explicit Metal render at **1280×720**, 100%/200% text: bundled PNG/JPEG, Space/
  Tab behavior, accessible name and visible focus pass. Explicit offscreen
  software smoke also passes.
- The actual sound-button activation path also passes in default-silent and
  native-audio app smokes, including extraction/loading of the bundled fixture.
- Explicit native test opens **Core Audio**, consumes the short tone and returns
  non-playing; mute/volume and simulated device-loss/silent behavior pass. Human
  listening and physical cable/device removal are unverified.
- Source review corrected non-finite text-scale input. Larger-text testing led
  to automatic focus-reveal scrolling. Original fixture provenance and exact
  upstream audio source/license hashes are checked in.

[Local evidence/screenshot](evidence/M0-03-local-probe.md) and
[audio evidence](evidence/M0-03-audio.md) include reproducible commands and API/
dependency limits. **M0-03 remains unchecked** until native Windows/Linux and
deployed-closure qualifications pass. Two complete tasks plus this implemented
local substep is the progress measure; there is no new product release.

Documentation/data checks passed: 32 active Markdown documents, 116 relative
file links, JSON parsing and all six pinned source/license/audio fixture hashes.
After verification, this turn's three wrapper-owned build trees and root/agent
scratch builds were removed. Source assets and the checked screenshot/evidence
remain; unrelated ignored Finder files were preserved.

## M0-05 shared version identity — 2026-11-04

Implemented one standard-library Python reader for the checked-in manifest,
SemVer precedence, independent game/editor tags and prospective artifact names.
CMake validates before `project()`, passes its numeric version core separately,
and generates an immutable C++ identity without Qt/Python runtime dependencies.
Headless and probe diagnostics use that identity, including compatibility IDs,
target, version-file hash and explicitly configure-time Git commit/state.

The manifest remains unchanged: game/editor/content `0.0.0-dev`, phase
`planning`, compatibility integers 1. The runtime collection hash is
`unavailable`; a version-file hash is not substituted for it. The release checker
rejects planning identities and always reports `release_ready: false` until
content/migration/payload/package gates have actual implementations. Prospective
filenames create no archives. No version, tag, content release or package was
published. Python 3.10+ is now a documented build-time requirement only.

| Check | Actual result |
| --- | --- |
| `./make.sh --core-only --test --jobs 2` | Release: 7/7 CTests pass |
| `./make.sh --core-only --debug --test --jobs 2` | Debug: 7/7 CTests pass |
| `./make.sh --test --jobs 2` | Desktop Release: 13/13 CTests pass, including probe identity and existing resource/audio/graphics contracts |
| `python3 tools/tests/test_project_versions.py` | 21 tests pass: strict JSON/manifest fields and integers, malformed/deep data, SemVer precedence, CMake prerelease handling, independent tags/stems, source observations and fail-closed release checks |
| `python3 tools/tests/test_build_wrapper.py` | Existing 11/11 cleanup/configuration/failure checks pass |
| Headless `--build-info` and `otool -L` | Planning identities/compatibility and `macos/arm64` shown; only system libc++/libSystem runtime linkage |
| Planning `check-release` invocation | Expected exit 1, identity blockers and remaining gates reported; `release_ready: false` |
| Independent integration review | SemVer order/metadata/malformed cases pass; event dependency and opportunity-coverage gaps corrected |
| Documentation/data consistency | Relative file links, code fences, seven planning JSON files, 52 unique tasks and acyclic dependencies pass; whitespace check passes |

Native arm64 Mac using the existing Apple Clang 21.0.0/Xcode SDK and Qt 6.12.0;
no new SDKs downloaded. The existing optional Qt TaskTree/plugin warning remains
recorded in M0-03 evidence. These are build/identity checks, with no new visible
window, audio device, graphics/deployment qualification or gameplay claim.
Direct CMake builds need reconfiguration to refresh Git observations after source
edits/commits; diagnostics and build instructions state that scope explicitly.

The user also clarified event production and expansion. Added M1-07 and M3-05,
[the event contract](EVENT_SYSTEM.md) and [AI task brief](../agents/tasks/base-events.md).
The eight-definition slice budget now covers a built-in playable baseline and
selective story enrichment together. Minimal rival fact producers precede pack
validation, and three opportunity/recovery paths are explicit. Domain modules
will share typed facts/effects, transactions, persistence and one flow runtime.
These are planned event tasks; no event runtime or playable pack was implemented
in this foundation task.

Progress is **3 of 52 tasks complete; 49 unchecked**, with M0-03 still in progress.
Next independently unblocked code work is M1-01 deterministic primitives; native
M0-03 qualification remains an open platform gate. After verification, this
task's three wrapper-owned build trees and small root scratch checker are
removed; unrelated ignored Finder files are preserved.

## M1-01 deterministic primitives — 2026-11-04

Implemented Qt-free strong units/IDs, one portable checked 128-bit arithmetic
path, Gregorian dates, seven independently saved SplitMix64 streams and strict
authoritative integer-token readers. A test-only 1,000-day fixture matches an
independent Python big-integer/date oracle, including every daily checksum and
exact final state. Midpoint token/RNG restoration and preview-copy isolation
pass. [Detailed contracts, evidence and limits](evidence/M1-01-primitives.md)
identify the intermediate bounds, algorithm and frozen fixture fingerprint.

Local core Release/Debug each passed 13 CTests and desktop Release passed 19,
including six verifier unit checks. Existing 11 wrapper
checks pass. Strict LLVM, ASan/UBSan and 200,000 independent arithmetic oracle
comparisons pass. Headless linkage remains system-only. Independent module and
integration reviews found no blocking issue.

Added a pinned core-only native workflow for all five targets with GCC, Clang
and MSVC, including native Windows ARM64. All Linux/Windows jobs pass 13 CTests
and the exact frozen fixture. Hosted Mac subsequently passed the same checks.
The first Windows run found VS 2026 images instead of VS 2022; generator/instance
discovery now uses bundled vswhere and CMake capabilities and passes both targets.
**M1-01 is complete, 4 of 52 tasks complete; 48 unchecked**. Implementation
`56bf6db` plus CI correction `8dcf6e5` passes all six jobs in
[native run 41863103816](https://github.com/gnaservicesinc/Project-Glowing-Pancake/actions/runs/41863103816).
Every native job passes 13 CTests and the exact normalized fixture SHA-256
`fa9f8690ae06f649aa0f7f6332c28b46ccd5af7b0819744d84528a62089a0868`.
Windows logs directly confirm Hostarm64/arm64 and Hostx64/x64 compiler tools;
Mac uses Apple Clang 17, Linux GCC 13/Clang 18 and Windows MSVC 19.51.
Evidence uploads and final cleanup completed without logged errors.
This is not gameplay,
production JSON/save loading, a market allocator or M0 deployment qualification.
M1-02 content registry/validation is next. M0-03/04's native graphics/audio/
deployment gates remain open. This task's three wrapper-owned build trees and
small arithmetic/primitive scratch outputs were removed; unrelated Finder files
were preserved. No product release, SDK installation or app launch occurred.

## M1-02 and local M1-03 implementation batch — 2026-11-04

This batch implements several linked foundation steps: the typed content
boundary, atomic commands/morning state, the immutable worker/model bridge and
two inspectable headless paths. **5 of 52 tasks are accepted; 47 remain
unchecked.** M1-02 is complete. M1-03's local implementation passes, but its
checkbox remains open because M0-03 native/deployment acceptance is still open.
No dependency or gameplay gate was bypassed. [Playability checkpoints](PLAYABILITY.md)
identify M2-04 as the first business-simulation evaluation and M3-02/03 as the
player-facing scenario/recovery/scoring slice.

`pgp::content` validates versioned typed definitions and extensible
system/fact/effect metadata, rejects malformed/duplicate/remote/executable
shapes, checks units/scopes/references/ownership/cycles and bounds parser/graph
work. Eleven engineering definitions adapt the opening balance and seven flow
examples. New catalog families require typed schemas alongside their owning
mechanics; unsupported kinds fail explicitly. The pinned nlohmann/json 3.12.0
header and MIT license are vendored with exact source/size/SHA-256 records; no
download occurs during builds. [Schema](../src/content/README.md) and
[content evidence](evidence/M1-02-content.md) document the actual subset.

Core commands revalidate observed revisions and commit complete copied-state
transactions or stable refusals. They distinguish reserved/consumed funds,
irreversible terms and protected future dependencies. Prepared mornings and
suspended choices resume without repeating work; snapshots omit RNG/future
queues. The Qt adapter serializes commands on one worker thread, caps running
snapshots at 10 Hz, and delivers each command result separately. GUI model diffs
use stable IDs, role-specific updates and insert/remove/move notifications;
routine updates never reset the collection. Full-width IDs/cents cross QML as
decimal strings. [Command evidence](evidence/M1-03-commands.md) records bounds.

Independent review caught and corrected invalidation of future commitments,
exponential validation through converging graph branches, an eight-frame
off-by-one, missing intrinsic registry ranges and driver-stop error visibility.
The resulting regressions exercise those specific failures.

| Check | Actual result on existing arm64 Mac |
| --- | --- |
| `./make.sh --core-only --test --jobs 2` | Release: 19/19 CTests, including content/command integration and the original primitive oracle |
| `./make.sh --test --jobs 2` | Desktop Release: 26/26 CTests |
| `./make.sh --debug --test --jobs 2` | Desktop Debug: 26/26 CTests; final content boundary additions recompiled and 4/4 affected content CLI/loader checks pass |
| `pgp-content-test` | 140 assertions, including all five lifecycle schemas, duplicate keys, integer/seed limits, wrong scope/unit/reference, ownership/cycles, exact 8/9 frames and bounded converging branches |
| Core command tests | Stale/overspend/overflow/bounds leave exact state unchanged; partial cancellation, protected dependencies, suspended/prepared restore, final date and unchanged RNG pass |
| Bridge tests | 200 row updates preserve selection/persistent index anchors across sorting and structural changes without resets; changed-role/precision/thread checks pass |
| Queued worker tests | Serialized stale rejection, immutable prior snapshot, run/pause, required opening choice and explicit halt on revision exhaustion pass |
| CLI examples | Foundation demo resumes at 2014-01-02 with cash 50,000 cents, exactly once; local fixture validates as schema 1, version `0.0.0-dev`, 11 definitions; missing/invalid files fail |
| Dependency hashes and linkage | Pinned header/license SHA-256 match; Release headless links only system libc++/libSystem, with no Qt |

The expanded native CI runs the 19 Qt-free contracts across the existing six
compiler jobs/five targets; results are recorded separately once observed.
The existing optional Qt TaskTree/asset-downloader warning remains unchanged;
the selected imports and tests pass without downloading it.

There is still no business scenario, executed content flow, disk save, playable
room shell, editor or game package. The bridge checks prove persistent indexes
and selection, not live QML scroll/edit focus or 200% room-view qualification;
those arrive with M1-05. M1-06 flow execution, M1-04 persistence and M1-07 event
dispatch are next core work; M2 starts balanced business mechanics. Manifest
versions stay unchanged. Builds launch no graphical application and no product
was published.

The batch was committed/pushed as `fdbd460`. Verification outputs were removed
using the wrapper's selected-tree ownership checks; the small command-test
scratch executable was removed separately. Unrelated Finder files and existing
arithmetic scratch data were preserved.

The first expanded native run,
[41868492196](https://github.com/gnaservicesinc/Project-Glowing-Pancake/actions/runs/41868492196),
passes Mac and all three Linux compiler jobs. Both native MSVC builds reject
reused if-initializer names under warning C4456 with warnings-as-errors. The
validator now gives each typed pointer a distinct name, preserving the strict
compiler policy. Local syntax checking with additional `-Wshadow -Werror`
passes; the corrected native run is recorded after observation.

[Corrected run 41869067266](https://github.com/gnaservicesinc/Project-Glowing-Pancake/actions/runs/41869067266)
tests `73d84bd` and **passes all six native jobs**. Decoded job logs confirm
19/19 CTests for Mac arm64 Apple Clang 17, Linux amd64/arm64 GCC 13,
Linux amd64 Clang 18, Windows amd64 MSVC 19.51 and native Windows arm64 MSVC 19.51.
The original primitive oracle also passes in every job. This qualifies the
Qt-free foundation contracts across the five targets; it does not close M0's
graphics/audio/deployment gate or claim a playable game.

## M1-06 executable flows and M1-04 local saves — 2026-11-04

This batch adds the executable flow compiler/runtime, persistent checkpoint IO,
scenario initialization, campaign autosave policy and a runnable event/save
demonstration. **5 of 52 tasks remain accepted**: both tasks stay in progress
while their complete contracts and prerequisites remain open. The broader
variable/expression, duration-predicate, related-entity selector/quantifier and
weighted-random flow forms still need implementation; unsupported forms fail
explicitly. This is recorded in [compiler scope](../src/runtime/COMPILER.md),
[runtime evidence](evidence/M1-06-runtime.md) and
[save evidence](evidence/M1-04-persistence.md). No scope was removed to pass tests.

`pgp::runtime` compiles and revalidates typed definitions, reports waits that
block later numbered siblings, rejects illegal cross-sibling jumps, inspects
typed clauses with hidden-player redaction, and resolves calendar anchors
without month-end drift. It executes all five current lifecycles and existing
news/dialogue/result/effect/sequence/subflow/delay nodes. Eight saved frames
retain context and return position across nested waits and choices. Owned
modifiers enter/exit naturally without removing concurrent contributions;
synthetic cleanup is explicitly labeled. Transactions retain actual committed
receipts and distinguish rejected candidate traces. Cash fixtures conserve a
caller-provided counterparty pool; full accounts remain M2 work.

`pgp::persistence` serializes every implemented checkpoint field, validates
format/rules/content identity and integrity, and rejects corrupt/future/oversized/
malformed inputs. Native temporary-sibling flush/replacement preserves the
previous valid file. Settings, explicit export, standard/portable locations,
Unicode filenames and a three-generation ring are implemented. Canonical
definition hashes identify actual supported content; full shipped localization/
asset collection finalization remains later work. The writer enforces the same
byte/depth/event budgets as its reader before committing a file.

`pgp::session` applies recovery saves before accepted irreversible reservations,
consumption and core/flow choices. Rejected commands write nothing; precommit IO
failure retains live state. Monthly and scenario-end writes preserve completed
play on failure and request a driver halt, including required opening choices
and midmonth endings. Failed loads retain current play and the selected file.
[Policy contract](../src/session/README.md) explains driver obligations. The
new scenario helper initializes authored cash/date/seed/headquarters and selected
root flows; referenced subflows do not also activate independently.

| Verification | Observed result on the existing arm64 Mac |
| --- | --- |
| Core Release build/CTest | 25/25 pass, with warnings treated as errors |
| Desktop Debug build/CTest | 32/32 pass; existing optional TaskTree warning remains unchanged |
| Compiler tests | 62 assertions: malformed typed input, provider availability, canonical identity, 8/9 frames, cross-sibling boundaries, typed/redacted traces and calendar anchors |
| Runtime tests | 541 assertions: lifecycle timing, exact copied continuation, real nested subflow execution, owned-modifier preservation, balanced cash, atomic failure and strict restore validation |
| Persistence tests | 135 assertions: SHA-256 vectors/binary padding, corruption/version/UTF-8/unsigned bounds, exact runtime reload, atomic backups/conflicts, ring recovery/generations, Unicode paths and settings |
| File-save integration | Save during a nested delay, resume to a dialogue with both frames intact, save/reload there, execute inner/outer effects once and compare entire next-day serialized state with uninterrupted execution |
| Application policy | Actual precommit recovery snapshots, no writes for refused/disabled commands, month boundary into a core choice, monthly IO failure retaining play, midmonth ending save and automatic-error halt |
| Actual CLI disk example | `--flow-demo tests/content/fixtures/foundation.json --save build/core-release/flow-demo-output.pgpsave` saves/reloads and matches uninterrupted state on 2014-01-15 |
| Independent integrity check | Python `hashlib.sha256` over the actual saved canonical payload matches its stored digest |

Independent review found and fixed illegal sibling-region jumps, malformed
continuation ancestry/delay timing, inconsistent modifier/receipt ownership,
misleading rejected traces, backup-only ring generations and application
autosave boundaries. An early test failure was a truncated standard SHA-256
input literal; independent Python output confirmed and corrected that test,
then binary padding-boundary vectors were added. Final local suites pass.

Native CI for this expanded 25-contract suite is recorded after observation.
There is still no business scenario, player-facing Office/Lab game, editor,
finalized content pack, deployed package or release. Full flow-language support
and M1-07 built-in occurrence detection precede the next foundation acceptance;
M2 remains the first business-economics evaluation. Manifest component/format
identities remain unchanged. Tests use owned scratch files, never personal saves.

## M1-06 weighted-branch slice — 2026-11-04

Implemented explicit narrative-stream weighted branches through content
validation, canonical compilation, bounded execution, committed/candidate traces
and strict save/reload. Saved selections survive delays, choices and nested
sequence/subflow returns without rerolls. Downstream refusal rolls back the
draw and all candidate consequences. Independent review identified and closed
unselected continuation, unrelated-flow history and truncated-history gaps.
[Evidence and save compatibility](evidence/M1-06-weighted-branches.md) document
the exact bounds and retained origin/stream history.

| Verification | Observed result |
| --- | --- |
| Core Release | 26/26 CTests pass, strict Apple Clang arm64 |
| Core Debug | 26/26 CTests pass, strict Apple Clang arm64 |
| Focused schema/compiler | 174 content and 77 compiler checks pass |
| Weighted branch integration | 270 checks pass: frozen seed/weight boundaries, rejection sampling, exact continuation, stream isolation, atomic rollback and corrupt history refusal |
| [Native CI run 41895189273](https://github.com/gnaservicesinc/Project-Glowing-Pancake/actions/runs/41895189273) for `2a37284` | All six jobs across five targets pass 26/26 CTests each, primitive oracle and flow/save identity steps; GCC/Clang/MSVC include native Windows arm64 |

Accepted tasks remain **5/52**. M1-06 still needs daily duration state,
registered variables/expressions and related-entity selectors/quantifiers;
M1-04 will persist their added state. Native/deployment prerequisite acceptance,
M1-07 built-in occurrence dispatch and M2 business mechanics remain open. Next:
implement duration predicates with explicit elapsed-day and daily reset rules.

## M1-06 duration slice — 2026-11-04

Implemented typed `holds_for_days` conditions, per-flow predicate identity,
elapsed D+N boundaries, daily start/reset observations, nested postorder
sampling and pure author/player temporal traces. Enabled roots sample one
opening snapshot even when cooldown/interval/active/boolean gating would skip
their execution. Choices preserve that day's sample. Continuous exit timers
explicitly measure scope history rather than activation age.

Exact saves retain complete owned observations and the daily sample cursor.
Independent review found and closed dropped-root history and stale dialogue
sample gaps. Invalid/stale histories and budget failures refuse atomically;
older saves without temporal fields remain readable for non-duration content.
[Evidence](evidence/M1-06-duration.md) records the full contracts and limits.

| Verification | Observed result |
| --- | --- |
| Core Release | 27/27 CTests pass, strict Apple Clang arm64 |
| Core Debug | 27/27 CTests pass, strict Apple Clang arm64 |
| Focused schema/compiler | 206 content and 147 compiler checks pass |
| Duration integration | 119 checks pass: elapsed/reset/gap/nesting, frozen snapshots, continuous exits, exact threshold reload, privacy and refusal/rollback |
| [Native CI run 41903737919](https://github.com/gnaservicesinc/Project-Glowing-Pancake/actions/runs/41903737919) for `543c451` | All six jobs across five targets pass 27/27 CTests, with matching primitive/content/resumed-state hashes and no compiler/test warnings; includes native Windows arm64 MSVC |

Accepted tasks remain **5/52**. M1-06 still needs registered scoped
variables/flags, checked expressions and related-entity selectors/quantifiers;
M1-04 persists their future state. M0-03's native Qt/deployment gate, M1-07
occurrence dispatch and M2 business mechanics remain open. Next: the bounded
variable/expression vocabulary and its checked assignment/save contract.

## Mac-first development and variable/expression batch — 2026-11-04

The user prioritized implementation on this Mac and deferred extensive
cross-platform/release regression until release preparation. The native matrix
now runs only by manual dispatch. Routine work uses one optimized incremental
local build and a small changed-behavior check, with Debug/broader testing for a
specific problem. Build trees remain for reuse; deferred platform qualification
does not block Mac development.

Content now declares scoped integer/boolean/text variables with explicit units,
ranges, defaults and visibility. Variables are registered typed facts; owning
system facts cannot be replaced. Bounded expression trees provide literals,
same-scope fact reads, checked addition/subtraction and rational scaling with
one signed half-up rounding. Expressions work in comparison thresholds and
transactional `set_variable` effects. Sparse scoped overrides, defaults through
content identity, and actual before/after receipts survive save/reload. A later
range/overflow refusal rolls back prior assignments in that operation. Hidden
expression inputs are redacted before player inspection reads them.

Verified on Apple Clang arm64 with `./make.sh --core-only --jobs 4`: build passes.
The focused `runtime.variables` CTest passes (0.01 seconds): ordered assignments,
flag/threshold expressions, delayed save/resume, unchanged RNG, rollback and
unit/scope/privacy checks. Full suite, Debug, native matrix and packaging were
deliberately not run for this batch. Build outputs are retained and ignored.

Related-entity selectors/quantifiers remain M1-06 work as real business entities
arrive. Next Mac gameplay work: M2-01 balanced accounts and obligations, with
M1-05's room shell alongside the business loop. No platform/release qualification
or formal completion of the remaining flow-language task is claimed.

## M2-01 accounts and obligations — 2026-11-04

Implemented balanced posting-derived accounts, actual-calendar-month recurring
costs, protected partial settlements, receivables/payables, monthly debt
interest/explicit repayment, conserving equity investment and pure cash
projections. Scenario creation uses the authored opening terms. Commands,
immutable reports, recovery/autosave policy and strict exact saves share the
same ledger/cursor boundary. Legacy foundation saves retain their explicit
absence of financial history. [Evidence](evidence/M2-01-accounts.md) and the
[module contract](../src/core/ACCOUNTS.md) describe timing, rounding and limits.

One optimized incremental Mac build passes, followed by eight distinct focused
CTests at the changed accounting/command/save boundaries. The domain test passes
80 checks and command/save integration passes 59. The runnable opening example
leaves **$972,000** after two calendar months and one $12,000 data license, with
an exact Jan31 pre-settlement reload. An added test was rebuilt incrementally;
the full suite, Debug, native matrix, packaging and device checks were not run.

Formal accepted count remains **5/52**, retaining M1-03's prerequisite gate.
The implemented M2-01 work is ready for M2-02 shared capacity and commitments;
contracts, a viable three-year strategy and liquidity recovery remain M2-03/04.

## M2-02 shared capacity and commitments — 2026-11-04

Implemented source ownership/compatibility, shared compute/staff reservations,
protected serving, proportional outage allocation, actual finite work and
cancellation. Separate support obligations accrue through accounts; projects
do not bill the lease/power again. Exact daily work/source histories and
simulation/account cursors resume together. [Evidence](evidence/M2-02-capacity.md)
and the [module contract](../src/core/CAPACITY.md) record the semantics/limits.

One optimized incremental Mac build and seven focused CTests pass. Capacity
domain checks pass 88 assertions and command/save integration passes 52; the
affected accounting and autosave boundaries also pass. The runnable 60-day
example completes 3,600 CU training alongside 60,000 engineering serving tasks,
retains exact reload parity, and leaves $962,000 cash after settled costs,
support and one data license. No full/native/Debug/package qualification ran.

Formal accepted count stays **5/52**, with M2-01/02 now implemented locally while
their prerequisite gates remain open. Next Mac work is M2-03 contracts/demand:
freeze the common-snapshot finite allocation procedure and hand-work rational
budget/capacity cases before coding. The three-year solvent/recovery scenario
then follows under M2-04.

## M2-03 contracts and demand allocation — 2026-11-04

Implemented the finite common-snapshot allocation kernel, contract-priority
resource planning, actual-demand serving, calendar-month fees/usage, netted
service credits, penalties/deadlines and monthly installed shares. Typed scenario
data supplies prices, utility/eligibility, profiles and terms; canonical content
identity and exact saves preserve the complete business boundary. Switching costs
use incumbency, and customer fees/usage share one budget. [Evidence](evidence/M2-03-market.md)
and the [module contract](../src/core/MARKET.md) record semantics and limits.

The optimized incremental Mac build and eight focused CTests pass. Allocation
has 1,677 checks, content/setup has 56 and contract/save integration has 68.
The authored example delivers 59,000 tasks, bills $50,000 over January/February
and leaves $1,012,000 after baseline/support costs and one license. A Jan31 save
resumes identically. Review added immutable-eligibility reload checks; affected
targets were rebuilt and their focused checks pass. No full/native/Debug/package
qualification was run.

Formal accepted count stays **5/52** while the prerequisite gates remain open.
Next work is M2-04's three-year scenario and liquidity workout/rescue/closure.
The current fixture starts with an engineering product; M3 still owns actual
research/model qualification. Work pauses at this M2-03 handoff per user request.

## M2-04 three-year business scenario and recovery — 2026-11-04

Implemented a causal liquidity owner, required warning acknowledgment, immutable
30-day workouts, actual-payment rescues and terminal closure with retained
accounts/resource/flow state and an automatic postmortem save. Deadline-morning
rescue is permitted; unresolved advancement closes at W+30 before further work,
billing or date movement. Negative profit, zero cash and not-yet-due accruals do
not independently trigger insolvency. [Evidence](evidence/M2-04-business.md) and
the [liquidity contract](../src/core/LIQUIDITY.md) record the boundaries.

A typed, hashed business plan drives startup work, customer expansion and a
single equity rescue through ordinary commands. The [runner](../src/session/SCENARIO_RUN.md)
reports 36 calendar months. Its no-financing expansion completes 2017-01-01 with
124,693,549 cents cash, 429,677,419 revenue, 424,983,870 expenses, no unpaid bills
or debt, and 5,240,000 delivered tasks. A July 1, 2015 save/resume matches the
uninterrupted checkpoint, monthly reports and final canonical bytes exactly.

The optimized Mac build and seven distinct focused CTests pass across the batch.
The full healthy comparison takes 174.34 seconds; the final recovery check takes
8.21 seconds. The initial 180-second comparison timeout prompted removal of
duplicate internal ledger validation and repeated historical searches, retaining
all public input checks. Review corrected assertions that equated not-yet-due
accruals with overdue liabilities. Exact-deadline changes were checked with the
small core/runtime/recovery/application-policy cases rather than repeating the
unchanged healthy scenario. The actual baseline CLI warns 2015-03-01 and closes
2015-03-31, preserves all liabilities, and prints total/overdue amounts separately.
Full/native/Debug/package qualification was not run.

Formal accepted count remains **5/52** while M2-03/M1-04 prerequisites stay open.
This is an engineering-product business scenario; M3 still owns qualified
research/models, and century objectives/performance remain later work. Next Mac
implementation: M1-05's accessible Office/Lab campaign shell, using a session
worker, persistent project/inbox rows, shared status/time controls and safe
local save/settings actions. Related-entity flows and built-in event dispatch
remain foundation work alongside presentation.


## M1-05 accessible room shell — 2026-11-04

The separate `pgp-game` desktop executable opens explicitly selected content
through the existing compiler and session. A single worker serializes commands
and storage. Stable project and inbox rows update by ID. The shell surfaces
required opening decisions and liquidity warnings in every room. Settings
persist text size, compact layout, reduced motion and autosaves. Loading holds
the external clock until the player advances or resumes.

Local optimized Mac build, five focused CTests and the 1280×720 Metal smoke at
200% text passed. The live smoke covered keyboard room and clock controls,
running edit focus and selection, exact save/load, missing-file rejection, and
compact preference persistence. Model checks exercised repeated sorted row
insertion, removal and movement with stable selection and scroll context.
[Evidence and reproduction](evidence/M1-05-room-shell.md) record the scope.

The accepted count remains **5/52**. M1-05 retains the M1-03 prerequisite and
broader interactive qualification. Continue with M1-06 related-entity selectors
and quantifiers, followed by M1-07 built-in event dispatch.

## M1-06 related-entity selectors — 2026-11-04

Registered company/source ownership relations now feed the existing typed
compiler and prepared-morning condition runtime. The inspector reports stable
member bindings, truth and match-count bounds, and respects hidden membership
and hidden facts. An elapsed-day timer over completed training work resumes
exactly from a save alongside an active contract. Content using selectors
changes its canonical definition hash. Existing saves need no new fields.

The optimized Mac build, 32 selector assertions and 11 focused CTests passed.
[Evidence](evidence/M1-06-selectors.md) describes the tested boundaries. Accepted
count stays at **5/52** while the prerequisite gates remain open. Next Mac batch
is M1-07 built-in event dispatch.

## M1-07 committed facts — 2026-11-04

Typed subscriptions now dispatch real settled project completions and missed
contract starts through the shared runtime. An activation carries the dated
owning business result, and a saved cursor tracks the consumed prefix of a
morning's causes. A required choice suspends dispatch and exact reload resumes
it. The existing inbox and choice panel project the activation's source.

The optimized Mac build, 22 committed-fact assertions and 14 focused CTests
passed. [Evidence](evidence/M1-07-committed-facts.md) records the actual checks
and limits. Accepted count remains **5/52**. Next M1-07 work is seeded
occurrences and digest.

## M1-07 seeded occurrences and weekly digest — 2026-11-04

Scheduled and interval roots now reuse the weighted flow entry with separate
markets and hazards streams. Explicit quiet outcomes consume a saved review
without creating news or effects. A selected incident continues through a
delay and required decision from the original draw. Routine news is grouped
by campaign week. Choices and active liquidity deadlines keep action priority.

The optimized Mac build and 14 focused CTests passed. The weighted-branch
executable passed 316 assertions and the campaign model passed 420.
[Evidence](evidence/M1-07-seeded-digest.md) records the frozen seed, continuation
boundaries and release limits. Formal accepted count remains **5/52** pending
the platform prerequisite chain. Next Mac batch is E1A-01 Content Studio.

## E1A-01 checkout library — 2026-11-04

The opt-in Studio executable opens a selected source checkout. It indexes
reviewed and draft collections and separately labels engineering fixtures.
Both search/filter and reference navigation use definitions validated by the
shared runtime compiler. Unsupported and invalid documents remain visible
with diagnostic status. The opt-in install component and embedded resources
are separate from the game's target.

The optimized Mac editor build, 12 library assertions, 12 wrapper tests and
the hidden Qt checkout view check passed. The latter loaded and filtered 15
rows from the repository's three engineering collections. The game configure
has no editor target. [Evidence](evidence/E1A-01-library.md)

Accepted count remains **5/52**. E1A-01 next needs a working document,
undo/redo and conflict-aware draft saves.

## E1A-01 typed drafts — 2026-11-04

The Studio's working document validates each typed edit with the shared
compiler and retains bounded undo and redo history. Explicit Save As writes
only a new draft. In-place saves require matching source bytes and a supported
checkout, and retain the previous valid file. The first editing form adds and
removes flow tags. It preserves the collection and definition versions.

The optimized Mac build and 38 Studio assertions passed. The existing save
and session-policy tests also passed. The hidden Qt check exercises filtering
and a tag edit, undo and redo without saving to the checkout.
[Draft evidence](evidence/E1A-01-drafts.md)

Accepted count remains **5/52**. E1A-01 retains its prerequisite gates.
Next Mac work is E1A-02 typed flow forms and outline.

## E1A-02 trigger and expression forms

The Studio exposes typed condition and operand rows in the working flow
outline. Forms take scope, value type, units, range and help from the shared
registry. Each validated edit enters the same undo stack and draft writer.
The test authors a nested duration, relation/count and arithmetic trigger,
checks refused edits and compares the exact canonical collection after reopen.
The optimized Mac editor build, 69 Studio assertions and hidden Qt view check
passed. [Evidence](evidence/E1A-02-triggers.md)

## E1A-02 lifecycle and flow references

The Studio creates scoped flows, changes all five lifecycles and resolves
interval anchors through the shared scheduler. It renames a flow with its
typed references in one working draft change. The optimized Mac build,
103 Studio assertions, four focused CTests and the hidden Qt check passed.
[Evidence](evidence/E1A-02-lifecycles.md)

## E1A-02 execution and scoped variables

The typed outline edits ordered effects, continuations, dialogue, results,
news, weighted outcomes and scoped variables. Authored graphs execute through
the same compiler and runtime as the game. Link and ID edits are undoable
with the rest of the draft. The optimized Mac build, 179 Studio assertions,
five focused CTests and a hidden Qt keyboard check passed.
[Execution evidence](evidence/E1A-02-execution.md)

Accepted count remains **5/52**. E1A-02 retains its E1A-01 and M1-06 gates.
Next Mac work is E1A-03 inspection on copied scenario state.

## E1A-03 trigger-impact and lifecycle inspection

A Qt-free probe owns its fixture and validated program. The Studio selects a
scenario, headquarters, root mode and exact integer seed, then advances through
the real runtime. A timer yields between simulated days and stops on choices.
Inspection shows current predicates separately from executed condition truth,
ordered paths, committed effects, ledger postings, owned modifiers and nested
wait queues. Shared presentation helpers and one QML card serve both the game
and Studio. A document edit invalidates its prior test state.

The optimized Mac build passed. Studio has 180 library/form assertions and 43
copied-fixture assertions. Five focused runtime/Studio CTests passed. The
hidden Qt test exercises seeded tests, invalid-seed preservation, date steps,
shared dialogue/result cards and exact reload. The game’s Metal smoke passed at
200% text after the shared-presentation change. [Evidence](evidence/E1A-03-inspection.md)

The count at this batch was **18/52 Mac feature steps complete**. The subsequent
Mac-first sequencing prioritizes M3 before E1A-04.

## M3-01 data/research/train/evaluate loop — 2026-11-05

The first-playable catalog defines four data offers, six research nodes, recipe
unlocks and headline/held-out/audit plans. Shared transactions pay licenses and
research/evaluation costs, reserve finite staff/compute and preserve cancelled
work. Training implements the specified checked saturation formula. Immutable
reports separate player evidence from true capability and research-stream state.
Clones retain cumulative work and rights without score bonuses; changed mixtures
require fresh training. Research retries are explicit paid attempts.

The baseline model's independent scanned-table interval is 2468–2768 bp. A
targeted licensed mixture and another 60-day run produce 5641–5941 bp, with no
evaluation capability bonus. Mid-training and pending-evaluation reloads match
exactly. Strict saves check catalog identity, payments, prerequisite timing,
lineage, actual work and draw history. `--lab-demo data/runtime/first-playable.json`
exercises the recovery path without launching a GUI.

Verification: optimized incremental Mac build; all **54/54 CTests pass in 13.40s**,
including **88 lab assertions in 0.10s**; headless demo and `git diff --check` pass.
The pre-existing business test accounts for 10.28s of the suite. Broader native
qualification remains deferred. Deployment qualification, human-review/narrow-scope
responses and Office/Lab controls remain M3-02; no complete playable slice is
claimed. [Owning module contract](../src/core/LAB.md). Count at this batch: **19/52**.
The next batch implements M3-05's eight event families and bounded rival routines.

## M3-05 eight base-event families and bounded rivals — 2026-11-05

The Studio-browsable collection `data/runtime/first-playable.json` supplies eight
flows and localizable English messages. Actual research/evaluation/pilot
settlements supply captured causes. Paid supplier/recovery offers, dated demand
changes, two startup routines and a financed heavyweight share ordinary command,
ledger, capacity, choice and save boundaries. Heavyweight warning follows actual
customer delivery and preserves a 90-day response window. Private model training
does not trigger it. Optional story roots are not required by the baseline.

The baseline starts without a product, trains and independently evaluates,
deploys the narrower service, then learns and verifies targeted data before
expansion. Seed **20140101** finishes **2017-01-01** with **229,865,809 cents**,
**4,001,600 useful tasks**, four competitive notices and no new debt/equity.
Saving at mobilization and resuming produces the exact uninterrupted state.
Content remains internal `0.0.0-dev`; canonical SHA256 is
`3340d13fb552771ded1f5f796de4a0fd01fe64e14e64e736fbb770d195911df6`.
[Module contracts, exact seeds and runnable commands](../src/runtime/BASE_EVENTS.md)

Verification: optimized incremental Mac build, **55/55 CTests in 8.88s**, including
**188** focused event/rival/Studio/scenario checks; both headless demonstrations,
content validation and `git diff --check` pass. Deep load/save checks still replay
all histories; daily commands avoid redundant replay and nested history copies.
The existing business test falls from 10.28s to 2.01s despite the larger feature
set. Studio fact defaults now retain a scoped variable when adding compatible
read-only game facts, preserving its counter/refactor behavior.

Count: **20/52 Mac feature steps**. Next: M3-02's player controls, actual model
qualification, headquarters choice, event/rival presentation and tutorial, then
M3-03 delegation/scoring. Graphical snapshot finalization, human studies and
native/release qualification retain their separate gates. No player-ready UI,
finalized content version or release is claimed by this headless batch.


## M3-02 — Interactive scenario and measured deployment — 2026-11-05

`pgp-game` now bundles canonical playable content and opens headquarters/story
selection without a content path. Office/Lab forms cover paid data/research,
finite training/continuation/cloning, independent evaluation, customer offers,
qualified deployment, contracts, actual obligations and public rival notices.
A local skippable guide, 1/10/30/60-day clock, shared localized presentations and
outcome panel complete the start-to-ending path. Persistent models retain IDs,
selection, focus and scroll; exact checkpoints stay on the serialized worker.

The new core deployment transaction requires a stopped, commercially cleared
checkpoint and matching independent evidence. Document-only scope, paid human
review and targeted-data retraining provide distinct subgroup responses. Review
reserves extra staff/cost and caps throughput; it cannot improve hidden model
capability. Recipe-specific serving cost, dated product/capacity bindings and
atomic support replacement preserve actual delivery and active contracts.
Save loading checks those records against authored requirements. The headless
baseline now uses the same deployment command and retains its $2,298,658.09
ending and 4,001,600 useful tasks.

Verification: **57/57 optimized Mac CTests in 10.10s**, including **29 deployment**
and **16 management** assertions. Content validation and `git diff --check` pass.
Explicit Metal keyboard/form runs at **1280×720**, **100% and 200% text**, cover a
Texas start, licensed training, independent subgroup finding, narrower deployment,
customer contract, targeted retraining/broader deployment, public rivals, exact
manual reload and a solvent **2017-01-01** ending above **$2.25m**. The existing
engineering shell smoke still passes its failed-load and preference checks.
Rendered review corrected status/action wrapping at high text scale. Hidden
training panels no longer recompute cash forecasts while time runs; visible
forecasts are pure and capped at 90 days. No screen-reader, independent human,
Windows/Linux/package/device or release acceptance is inferred from these checks.

Count: **21/52 Mac feature steps**. Next: M3-03's bounded delegation, configurable
auto-pause, specialist/financing recovery paths and scoring. M3-04's five real
human sessions and the remaining Content Studio/native/release gates remain open.


## M3-03 — Delegation, recovery and slice scoring — 2026-11-05

The playable scenario now owns saved finite evaluation/lease-renewal policies,
fixed financing offers, one hireable specialist, a warned source setback and a
content-defined slice score. Every accepted action records actual cash/resources;
policy counters, reserve/commitment ceilings, intervals, suspensions/refusals and
next-review dates persist exactly. Lease renewal charges setup plus actual bills;
its policy debits a conservative full-term ceiling. A live checkpoint cannot be
tested and a policy cannot choose story responses or deploy a model.

The supplier warns 2014-04-17, reduces the opening source to 60 CU/day on May 1,
and restores 100 CU/day on May 15. Starting the setback requires acknowledgment
before work. The broad baseline now waits for feasible expansion and finishes
2017-01-01 with **227,602,041 cents**, **3,985,648 tasks** and **99/100**. The
single-domain efficient specialist, paid hire and two delegated tests finish with
**143,738,712 cents**, **3,081,000 tasks**, **23,738,712 cents operating profit**
and **84/100**, with no new financing. Its saved continuation also completes.
Scores derive from actual history and never award later century objectives.

Local settings separately control completion/evaluation/advance-alert pauses.
Required choices, the setback, actual debt/lease/start deadlines and the final
liquidity response day always stop the driver. Milestones capture real project
IDs/deadlines, normal speed is one day/second and running snapshots are capped at
10 Hz. Dated staff additions retain historical limits; the loan path also exposed
and fixed debt-interest validation incorrectly splitting IDs containing `/`.

Verification: **58/58 optimized Mac CTests in 11.71s**, including **352 management
integration assertions** and **20 bridge checks**, plus content validation and
`git diff --check`. Complete explicit Metal keyboard/form runs at **1280×720**,
**100% and 200% text**, exercise fixed equity dilution, paid hiring, finite
independent-evaluation and renewal policies, milestone control, exact reload,
required setback/restoration, rivals and a scored solvent ending. Rendered review
fixed truncation of the high-scale milestone selector. The legacy engineering
shell's keyboard/clock/live-focus/save/load/error/preferences smoke passes at
200%. The smaller smoke separates resume completion from editing, then verifies retained
Qt edit focus even when a terminal-launched window lacks native activation; it
also requires active edit focus when the native window is active.
No human, screen-reader, non-Mac device or release qualification is claimed.

Count: **22/52 Mac feature steps**. M3-04's five independent human sessions remain
required (including two non-ML players, three unaided completions). Local session
materials can be prepared while E1A-04 snapshot finalization and the remaining
editor work proceed. The goal remains active; no human study is fabricated.
[Owning mechanics and limits](../src/core/MANAGEMENT.md).

## E1A-04 — Reviewed internal content snapshots — 2026-11-05

Added the shared Qt-free snapshot writer and explicit `pgp-content-tool`
review/finalize/check/recover commands. Actual collection compilation, prepared
scenario openings and exact save roundtrips supply validation. A reviewed plan
includes semantic/text changes, once-per-batch definition revisions, versions,
hashes, summary and proposed writes. Finalization recomputes that plan, rejects
stale source/version evidence and preserves separate draft bytes. Durable
multi-file staging, a checksummed recovery journal and a last-written sequence
marker restore an incomplete generation or verify a committed one. Concurrent
third versions are preserved. Formatting/no-op edits do not bump content;
formatting writes still receive distinct transaction markers.

The first internal snapshot is **0.1.0-dev.1**, generation 1, with **15 revision-1
definitions**, one scenario and all **three headquarters opening/save checks**.
Collection hash:
`a09060be69908bea63cd9b6fa04e06f275f1226cfaf6977a7e0d7ec647b0c26f`.
The runtime manifest, immutable baseline, history and root versions are linked
by the final content-state marker. Game/editor remain **0.0.0-dev**, phase
planning; the required game version **0.1.0-dev.1** is unresolved. CMake embeds
the verified record's hash and the bundled game compares its actual compiled
resource to it. The normal game resource list excludes all authoring/editor
files. Release checking reports snapshot integrity and unresolved version
requirements separately and remains fail-closed. Saves from older content are
explicitly refused by the existing exact-content loader; migration is not claimed.

Verification: **59/59 optimized Mac CTests in 13.09s**, including **76 snapshot
assertions** covering text/three-definition batches, no-ops, old-save refusal,
stale plans, game-version resolution, interrupted staging/replacement/commit,
formatting-only rollback and concurrent-edit preservation. Version-reader tests
cover linked artifact integrity and pending-transaction refusal. Normalized
collection sorting exposed positional assumptions in slice tests; those now
select stable scenario/research/rival/offer IDs without weakening assertions.
A complete explicit Metal game smoke at **1280×720, 200% text** passes against
the finalized hash through a scored solvent 2017 ending. `git diff --check` passes.
[Writer commands, artifacts and recovery limits](../src/studio/SNAPSHOTS.md).

Prepared the local human-session kit, blank reports/results and explicit fresh
session launcher. **No human sessions have been conducted.** The user then
clarified that all components must be in place before v1 human testing begins.
M3-04 and E1B-03 remain open and are deferred to that phase; they do not block
M4 or later component implementation. The kit is held for later use, with no
recruitment or sessions underway. The roadmap, dependencies and role guidance
now reflect that instruction.

Count: **23/52 Mac feature steps**. Next: E1A-05's reproducible engineering
walkthrough, then E1B-01/02 canvas/graphical finalization and M4 components.

## E1A-05 — Forms and inspection engineering walkthrough — 2026-11-05

The runnable editor now has an explicit `--walkthrough REPORT.json` path for a
fresh selected checkout copy, with optional rendered PNG. It drives the actual
QML controls and keyboard outline, makes 36 typed form submissions, authors an
ordered branching/delayed flow, checks wrong-scope refusal and compares an
inverted +250-cent receipt with the corrected −250-cent receipt. A preceding
actual 1,000-cent debit funds the counterparty, so the sign experiment preserves
cash conservation. It saves/closes/reopens all five lifecycle examples, reloads
through the actual save codec, chooses the accepted dialogue branch and resumes
its hidden consequence exactly once. Form/inspection actions preserve reviewed
runtime and version bytes. Existing report/screenshot/draft outputs are refused.

This path caught a real UI bug that direct document tests missed: Save as draft
called JavaScript `trimmed()` instead of `trim()`. The button is fixed and the
walkthrough now exercises its real click handler. Named control targets and
the local report make the engineering demonstration repeatable. They add no
human findings, telemetry, publication or automatic launch to a build.

Verification: explicit optimized **Metal, 1280×720** runs pass; rendered review
shows the accepted result and live target/cash/continuation report. Focused
`studio.library` and `studio.snapshots` CTests pass in **1.31s**; the existing
offscreen library/forms/inspection check loads and filters 30 rows. The saved
draft is separately reviewed/finalized by `pgp-content-tool` in the isolated
checkout as content **0.2.0-dev.1**, generation 2, with five new revision-1 flows
and only the enabled scenario incremented to revision 2. All three headquarters
opening/save checks pass with no compiler lints; its collection hash is
`be4e89994465d11bac181f18c81e645b1e9e8ea1bbeae4802cdbed7d06f902ef`.
The primary game's content remains **0.1.0-dev.1**. Generated resource lists
contain only game QML and reviewed runtime data in the game, with Studio QML in
the separate editor target. Native package/device, canvas/finalizer and human
qualification are not inferred from this demonstration.

Count: **24/52 Mac feature steps**. Next: E1B-01 typed node canvas, then E1B-02
graphical finalization and M4 components. M3-04 and E1B-03 remain deferred until
all components are in place and the user is ready for v1 human testing.
[Reproducible commands and manual path](../src/studio/WALKTHROUGH.md).

## E1B-01 — Typed node canvas — 2026-11-05

Content Studio now projects the existing typed document as node boxes with
execution, predicate, value and entity ports. The contextual searchable palette,
port connections and shared form/outline selection use one compiler/undo/save
boundary. Exact kind/type/unit/reference/scope matching blocks coercions;
literal-only effects refuse live expressions. Owned predicate/value connections
copy trees and repair duplicated duration IDs. Execution retargeting prunes only
abandoned unreachable routes, with complete shared undo. Atomic effect order,
numbered children, choices, weighted branches and subflow returns stay explicit.

The canvas adds pointer dragging, keyboard grid moves/pan, zoom, a box navigator,
stable automatic layout and separate layout undo/save. Shared layout files use
bounded integer coordinates, explicit writes, source/version conflict checks and
recoverable prior files. Dragging writes position only on release. Opening,
selection and viewport gestures do not change runtime identity or revisions;
local views restore through ignored session metadata. The game resources remain
separate from all Studio/canvas/layout/draft/test payloads.

Runtime overlays derive from actual copied checkpoints: current clause values,
truth and selected scopes; committed receipts/targets; real selected outcomes;
and saved waiting children, queued siblings and nested call/return locations.
Synthetic cleanup remains separately labeled. A nested helper can show its
incoming-call context even when its independent automatic trigger is false.
Compiler notices explain why later numbered children wait. No canvas operation
predicts a future random branch or mutates a live campaign/RNG.

Verification: **60/60 optimized Mac CTests in 12.18s**, including **84 canvas
assertions** for graph/form roundtrips, typed refusals, stale models, timing-ID
repair, arithmetic, safe retarget/undo, actual sibling/nested-call waits, RNG and
exact reload, layout conflicts/bounds/links/future data and local view storage.
The explicit Metal QML walkthrough at **1280×720** extends the 36-submission
forms example with actual pointer port clicks and header dragging, contextual
creation, shared undo, visible queued ports, layout save/reopen and local
viewport restore. It verifies no layout write before pointer release and no
content changes from layout/pan/zoom. Native resize checks wait for settled Qt
geometry, then verify **900-pixel pane navigation** and **200% canvas zoom**.
Rendered inspection shows the selected waiting sequence, queued Step 2/return,
compiler notice and actual cash trace. Main reviewed content remains
**0.1.0-dev.1**, hash
`a09060be69908bea63cd9b6fa04e06f275f1226cfaf6977a7e0d7ec647b0c26f`.
No human, screen-reader, non-Mac device or packaged-release qualification is
inferred from these engineering checks.

Count: **25/52 Mac feature steps**. Next: E1B-02 graphical review/finalization
using the existing journaled writer, then M4 components. Human studies remain
deferred until all components are in place and the user is ready for v1 work.
[Canvas controls, storage and reproducible check](../src/studio/CANVAS.md).
