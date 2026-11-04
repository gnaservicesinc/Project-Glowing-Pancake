# Implementation progress

Started 2026-11-03 against planning baseline 1.2. This log records completed
behavior and evidence; the [backlog](BACKLOG.md) remains the task dependency list.

| Task | State | Scope and evidence |
| --- | --- | --- |
| M0-01 | Complete | [Toolchain evidence](evidence/M0-01-toolchain.md), two reproducible inventory tools and [five-target metadata observations](evidence/M0-01-qt-archives.json); no SDK installation |
| M0-02 | Complete | [CMake/build entry points](BUILDING.md), separate core/headless targets, four configurations and safety checks below |
| M0-03 | Next | QML/renderer/resource-denial and optional local playback probe |

## First implementation slice

The requested implementation begins with the first two ordered M0 tasks. The
core library and headless entry point are separate CMake targets. Qt discovery
belongs to desktop configuration, with no Qt linkage in the core. Future content,
persistence, bridge and editor targets follow the dependency direction in the
[technical plan](TECHNICAL_PLAN.md); no speculative subsystem is added yet.

Each configuration has its own output directory. Tests check actual Release/Debug
diagnostic behavior and command failures. Wrapper checks cover toolchain identity,
cleanup ownership, symlink rejection and error propagation.

## Verification — 2026-11-03

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

No Linux/Windows compiler, CI workflow, renderer/audio device, deployed Qt closure
or clean package has been tested. The build currently has no audio dependency;
playback and the explicit silent application adapter arrive in M0-03. M0 is not
complete. M0-05 is independently unblocked; the next sequential task is M0-03.

There is no playable game, graphical editor, deployed package or public release
at this stage. Building never launches an application.
