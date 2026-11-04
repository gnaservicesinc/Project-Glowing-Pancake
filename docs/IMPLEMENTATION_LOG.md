# Implementation progress

Started 2026-11-03 against planning baseline 1.2. This log records completed
behavior and evidence; the [backlog](BACKLOG.md) remains the task dependency list.

| Task | State | Scope and evidence |
| --- | --- | --- |
| M0-01 | Complete | [Toolchain evidence](evidence/M0-01-toolchain.md), two reproducible inventory tools and [five-target metadata observations](evidence/M0-01-qt-archives.json); no SDK installation |
| M0-02 | Complete | [CMake/build entry points](BUILDING.md), separate core/headless targets, four configurations and safety checks below |
| M0-03 | In progress | Local QML/Metal probe, reusable resource guards and optional native/silent audio implemented; native Windows/Linux/deployment qualification remains open |
| M0-05 | Complete | Shared version manifest validation, generated core/headless/probe metadata, product tags/artifact stems and fail-closed readiness; verification below |
| M1-07 | Planned | Built-in major-event detection, scheduled/seeded world occurrences and news through the shared runtime; [event contract](EVENT_SYSTEM.md) |
| M3-05 | Planned | AI-assisted eight-definition playable base-event pack, with optional human story enrichment; prerequisite simulation/Studio work remains open |

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
