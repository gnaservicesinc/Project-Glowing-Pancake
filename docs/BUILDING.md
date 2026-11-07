# Building the development skeleton

M0-02 implements build entry points and the headless scaffold. M0-03 adds the
development interface probe, resource guards and optional playback adapter.
M0-05 validates the shared component versions and embeds configure-time identity.
The engineering simulation runs headless or in the desktop room shell.
An opt-in Content Studio target browses/edits local checkout data and builds the
maintainer snapshot CLI. The bundled campaign remains a playable prototype.
The game now has a full-screen main menu and a local Mac application bundle;
editor usability and release qualification remain later feedback work.
The [implementation log](IMPLEMENTATION_LOG.md) records acceptance evidence.

## Requirements and commands

Use CMake 3.24+, Ninja, a C++20 compiler, Python 3.10+ (standard library only),
and Bash for `make.sh`. Python validates metadata at build time; the built core,
headless executable and probe have no Python runtime dependency. CTest is bundled
with CMake. The desktop configuration additionally requires Qt **6.12.0** Core,
Gui, Network, Qml, Quick and QuickControls2. It builds the campaign shell and probe without
linking Qt into core/headless targets. Qt Multimedia is not required. Nothing
downloads dependencies or launches a program during builds.

| Command | Implemented behavior |
| --- | --- |
| `./make.sh` | Optimized Release campaign shell and interface probe in `build/release`; no launch |
| `./make.sh --package --jobs 4` | Mac game app with bundled Qt, resources and icon in `dist`; no launch |
| `./script/build_and_run.sh` | Explicitly build, package and launch the Mac app; also used by the Codex Run action |
| `./make.sh --test` | Build then run CTest; propagate failures |
| `./make.sh --debugging` or `--debug` | Debug in `build/debug`, local developer diagnostics enabled |
| `./make.sh --core-only` | Release in `build/core-release`, no Qt discovery/linkage |
| `./make.sh --editor` | Separate Content Studio in `build/release/bin/pgp-editor` |
| `./make.sh --editor --test` | Build the editor and test its library, draft workflow and typed flow forms |
| `./make.sh --core-only --debug --test` | Debug core and tests in `build/core-debug` |
| `./make.sh --qt-root /path/to/Qt/prefix` | Select an installed desktop Qt prefix |
| `./make.sh --jobs 2` | Bound compilation parallelism |
| `./make.sh --core-only --clean` | Remove only the selected wrapper-owned tree; no rebuild |
| `./make.sh --help` | Show flags, paths and requirements |

`--all` remains reserved. On macOS, `--package` stages a relocatable local
development app in `dist/Project Glowing Pancake.app`, including its icon,
menu video, Qt frameworks/plugins/QML imports and notices. It is ad-hoc signed;
notarized distribution and other-platform packages remain later gates.
Cleaning uses the same configuration flags as building; for example
`./make.sh --debug --core-only --clean` removes only `build/core-debug`.
Unknown existing directories, symlinked build paths and owner-marker mismatches
are refused. Changed toolchain selections require an explicit clean of that
configuration. User saves and unrelated outputs are outside cleanup.

## Run matched century strategies

These workloads are deferred under the 2026-11-06 feature-first instruction.
Use short checks of changed behavior while initial features are incomplete.

The headless endurance driver uses the bundled century catalogs, ordinary
commands and saved independent random streams. Choose `specialist`, `open`,
`frontier` or `infrastructure` with the same scenario and seed to compare
funded strategies. Annual reports include cash, cumulative profit, rival
attrition, required decisions, deployments and independent Frontier results.
The concentration fields estimate AI-provider shares and HHI from the last
settled monthly installed shares, weighted by segment demand. They exclude
the outside alternative and group products by their publishing company.

```sh
build/release/bin/pgp-headless --endurance data/runtime/first-playable.json scenario.century_frontier 2114-01-01 --strategy frontier --save /path/to/frontier.pgpsave --checkpoint-years 25
```

`--resume /path/to/save.pgpsave` continues that exact content identity and
scenario. Pass the strategy again to continue its decisions. The default
strategy is specialist. The open route publishes research and deploys
open and licensed services. Infrastructure funds a Washington site and
offers actual prepaid wholesale terms. Frontier funds an eight-topic
queue, an advanced service and independent assessments and replications.

An explicit checkpoint interval saves/reopens at that many calendar years
and retains the prior valid save as `.bak`. On a refused operation it also
tries to save the last accepted state. The ordinary save limit is 25 MiB.
`--save-limit-mib 64` selects the existing diagnostic envelope for larger
engineering runs and reports when a checkpoint exceeds the default limit.
Intervals must be 1–500 years and explicit save limits 1–64 MiB. These
commands are engineering workloads for the bundled catalogs.

## Browse a checkout in Content Studio

```sh
./make.sh --editor --test
build/release/bin/pgp-editor --project "$PWD"
```

Run the second command explicitly from the checkout to open its library. The
editor also accepts an absolute checkout folder in the window. It can search
and filter validated definitions, follow their uses and used-by links, and
report invalid and unsupported documents. It reads `data/runtime/`,
`data/drafts/` and the existing engineering collection fixtures as separate
sources. Select a validated flow and use **Edit flow** to make a typed working
draft. Use the outline to edit predicates, lifecycles, scoped variables,
and links through ordered effects, events, dialogue, delays and subflows.
Renames update typed references in the working collection. Use **Save as draft**
with a new lowercase `.json` filename to copy a reviewed or engineering
collection into `data/drafts/`. Drafts can be saved in place with undo/redo
and source conflict checks. The **Test on isolated scenario state** section
selects a fixture/headquarters/seed and runs the selected flow or scenario
roots. Advance through a chosen date, respond to dialogue, reload the exact
checkpoint, inspect threshold boundaries, or run synthetic cleanup on a
separate copy. The report shows predicate truth, matched members, effects,
ledger entries and the continuation queue. Editing the draft resets its
previous test state. The regular game
build disables the editor. An opt-in `ContentStudio` install component defines
the editor payload for future deployment qualification.

To check loading, filtering and in-memory draft edits without showing a window, run
`QT_QPA_PLATFORM=offscreen build/release/bin/pgp-editor --software --check-view --project "$PWD"`.

**Build flow** now provides a typed canvas beside selected-box settings, with
the same outline as a keyboard alternative. Select an output and a compatible
input, or use its searchable creation palette. Save content and non-semantic
layout moves separately; viewport/selection stay in ignored local session data.
The [canvas guide](../src/studio/CANVAS.md) includes controls, limits and the
explicit `--canvas-walkthrough REPORT.json [--screenshot FILE.png]` engineering
check on a fresh checkout copy. It tests actual pointer/keyboard interaction;
it does not conduct a human study or publish content.

### Validate and finalize internal content

The editor build also provides the Qt-free `pgp-content-tool`, separate from the
game payload. Its explicit review/finalize/check/recover operations share the
runtime validator and journaled batch writer:

```sh
build/release/bin/pgp-content-tool --help
build/release/bin/pgp-content-tool check --checkout "$PWD"
```

See [snapshot commands and recovery](../src/studio/SNAPSHOTS.md) for a reviewed
draft promotion. The editor's **Review content revision** dialog selects a saved
collection, shows before/after differences and validation, finalizes through
that same writer and exposes interrupted-batch recovery. Authors do not need a
command or text editor. The explicit `--finalizer-walkthrough REPORT.json`
mode exercises those controls on a new isolated checkout copy; its optional
`--screenshot FILE.png` captures the batch differences before finalization.
The current content snapshot is 0.4.0-dev.1. Game/editor
remain 0.0.0-dev; its game-version requirement is deliberately unresolved and
release readiness is false. Builds embed the validated record's collection
hash; pending transactions block configuration until recovery. Finalization
does not publish, commit or automatically launch either application.

The [forms/inspection engineering walkthrough](../src/studio/WALKTHROUGH.md)
runs explicit QML form edits, sign/scope inspection, five lifecycle reopens and
delayed exact saves on an isolated checkout copy. It also captures a local
report/window using `pgp-editor --walkthrough REPORT.json --screenshot FILE.png`.
This development check is separate from the human studies deferred to v1
preparation. Use new report/draft paths; existing output is refused.

`QT_ROOT` is an alternative to `--qt-root`; ordinary CMake prefix discovery via
`CMAKE_PREFIX_PATH` is also supported. On this Mac only, the wrapper can use the
existing `/opt/Qt/6.12.0/macos` as a build-time fallback. No such path is embedded
in the core/headless executable's runtime dependencies.

## Routine Mac implementation

Use one incremental optimized Mac build per batch. Choose the core build for
Qt-free changes or the desktop build when the interface/bridge changes. Keep
the owned build tree between batches, then run only the small focused checks
for the changed behavior and relevant failure path. For a duration-runtime
change, for example:

```sh
./make.sh --core-only --jobs 2
ctest --test-dir build/core-release -R '^runtime.duration$' --output-on-failure
```

Use Debug for a specific investigation. Repeating Release and Debug builds,
running the full suite, and waiting for the remote native matrix are reserved
for a demonstrated concern or explicit release preparation. The preserved
six-job native matrix in `.github/workflows/core-primitives.yml` runs only on
manual `workflow_dispatch`; normal pushes and pull requests do not start it.
Cross-platform regression, packaging and clean-device qualification remain
release gates and do not block bounded Mac implementation. Keep portable code,
offline behavior, safe local saves and explicit app launch throughout.

The opening accounts example is runnable without Qt or a game window:

```sh
build/core-release/bin/pgp-headless --accounts-demo tests/content/fixtures/foundation.json
ctest --test-dir build/core-release -R '^(core.accounts|runtime.accounts-integration|headless.accounts-demo)$' --output-on-failure
```

It isolates the authored opening accounts, consumes one $12,000 data license,
settles January and February, and compares a resumed checkpoint. Its expected
cash is $972,000; it does not exercise contracts or a viable three-year strategy.

`pgp-headless --capacity-demo FILE` uses the same engineering content fixture
to run 60 days of 20 CU/day serving plus 60 CU/day training on a single
100 CU/day lease. It includes a separate $5,000/month support obligation and
the one-time data license, ending with $962,000 cash after settled January and
February costs. The targeted checks are `core.capacity`,
`runtime.capacity-integration` and `headless.capacity-demo`. This capacity-only
example excludes customer demand/revenue; the market example below exercises
those systems. Model qualification remains M3 work.

`pgp-headless --market-demo tests/content/fixtures/market.json` loads a typed
enterprise scenario with contract terms, utilities and inference/support prices
from data. Through March 1 it delivers 59,000 tasks, bills exactly $50,000 and
leaves $1,012,000 after baseline/support/license costs, with exact reload parity.
Focused checks are `core.allocation`, `content.market-setup`,
`runtime.market-integration` and `headless.market-demo`. The fixture starts with
an engineering product; research/model qualification remains M3 work.

The three-year runner uses a data-defined initial workload, customer expansion
and rescue terms. It prints monthly income/cost/work and balance reports:

```sh
build/core-release/bin/pgp-headless --business-run tests/content/fixtures/business.json expansion
build/core-release/bin/pgp-headless --business-run tests/content/fixtures/business.json baseline
build/core-release/bin/pgp-headless --business-run tests/content/fixtures/business.json rescue
```

`expansion` completes the 2014–2017 slice without additional financing. `baseline`
explicitly acknowledges its warning and exercises closure after the response
window. `rescue` records one authored equity investment and pays overdue bills;
it stops at a later warning instead of inventing more funding. Normal application
drivers stop at an unacknowledged warning. These commands use in-memory reloads,
write no player saves and launch no window. Focused checks are `core.liquidity`,
`runtime.liquidity-integration` and `session.business-scenario`.

The alternative efficient specialist strategy is also reproducible without a GUI:

```sh
build/release/bin/pgp-headless --specialist-demo data/runtime/first-playable.json
```

It uses paid tooling/serving research, one hire, a single-domain efficient service
and finite independent-evaluation policies, reaches 2017 profitable without new
capital, and prints the same content-defined slice scoring rubric.

<a id="open-the-campaign-shell"></a>

## Open the playable campaign

Build, then explicitly launch the bundled offline 2014–2017 campaign:

```sh
./make.sh --jobs 4
open "build/release/bin/Project Glowing Pancake.app"
# Self-contained Mac app (no Qt SDK or checkout needed at runtime):
./make.sh --package --jobs 4
open "dist/Project Glowing Pancake.app"
# Optional separate development saves/preferences, with larger text:
open -n "dist/Project Glowing Pancake.app" --args --portable-root "$PWD/build/release/local-campaign" --text-scale 200
```

The game opens full screen to the main menu. New Game opens the existing
prototype setup, which offers three headquarters and optional story layers. The
in-game guide covers data, research, training, held-out evaluation, qualification
and customer contracts. Office also supplies bounded policies, fixed financing,
hiring, alerts and the provisional/final slice score. Clock milestone targets
use existing projects and actual known deadlines. See the [player path](PLAYABILITY.md).
Without `--portable-root`, saves/settings use the application's standard local
directories. Builds never launch a window.

Open **Settings → Music and credits** to enable the local soundtrack, set its
volume or read the bundled notices. New Mac builds include the optional
Core Audio backend. Configure with `-DPGP_ENABLE_AUDIO=OFF` to build the silent
adapter. The explicit `--smoke-test --music-smoke --portable-root DIR`
walkthrough requires a fresh scratch directory and checks native audio output.

An explicit `--content FILE` still opens an engineering collection. `--scenario`
selects an authored scenario (otherwise the first one is used); `--headquarters`
defaults to `US-CA` for this explicit fixture path. The bundled game chooses
headquarters through its welcome screen.

`--smoke-test` requires a fresh, explicitly selected portable directory. With
bundled content it drives the complete keyboard/form path, including financing, paid hiring,
bounded evaluation/renewal policies and the recoverable capacity setback, to a
scored solvent ending,
including exact save/load and required decisions. With an engineering business
fixture it retains the smaller room/clock/save/error/preferences check:

```sh
game_binary='build/release/bin/Project Glowing Pancake.app/Contents/MacOS/Project Glowing Pancake'
"$game_binary" --menu-smoke --smoke-test --portable-root .work/menu-check --screenshot .work/main-menu.png
"$game_binary" --smoke-test --portable-root .work/playable-check
"$game_binary" --smoke-test --text-scale 200 --portable-root .work/playable-check-200
"$game_binary" --content tests/content/fixtures/business.json \
  --smoke-test --portable-root .work/engineering-check
```

On Linux/Windows, use the ordinary `pgp-game` executable. The explicit menu
check covers startup, keyboard navigation, credits, 200% text, settings
round-trip, audio, still/video selection, the existing New Game setup, Resume,
the local picker open/cancel and the save-load handler's success/error paths,
plus Escape/quit confirmation, cancellation and an actual confirmed exit.
On Mac it also crosses the bundled ten-second video loop. The explicit
`QT_QPA_PLATFORM=offscreen` / `--software` path keeps the desktop undisturbed,
uses silent audio output and activates the system decoder for the loop check.
Native picker row selection and VoiceOver still need human checks. The older campaign checks
enter the prototype directly so their simulation paths remain focused.

The background is an embedded image plus a separate silent local-video layer.
On Mac that layer uses system AVFoundation and pauses when the menu is hidden,
the app is inactive or reduced motion is selected. Other platforms currently
use the still image. `assets/menu/PROVENANCE.md` records the original art,
selection effect and reproducible source scripts. Isometric/3D environments
and the gameplay interface are outside this menu milestone.

Optional `--screenshot PATH.png` is limited to smoke mode. It captures local
rendered output; the complete campaign smoke also captures Office, training and
evidence views beside that file. These commands launch only when invoked.

## Compiler and configuration isolation

On macOS, set `DEVELOPER_DIR` to an existing Xcode `Contents/Developer` directory
or let the wrapper read `xcode-select -p`. It canonicalizes the installation and
resolves its C++ compiler and macOS SDK through `xcrun` before configuration.
It records arm64/x86_64 target architecture and a macOS 14.4 deployment floor.
Conflicting compiler environment overrides and changed cached selections fail
with a recovery instruction. Global Xcode settings remain untouched.

Release and Debug use different presets/output directories. CMake supplies
compiler-specific optimized Release flags; project code receives diagnostics
as a **0/1** macro and uses `#if`, so a prior Debug build cannot enable Release
diagnostics. Explicit test checks remain active under `NDEBUG`. No telemetry is
implemented in either configuration.

The portable direct-CMake path requires no Bash:

```sh
cmake --preset core-release
cmake --build --preset core-release
ctest --preset core-release
```

On Windows, use a developer shell for the native MSVC architecture. Existing
native foundation evidence does not qualify Windows/Linux graphics or packages;
those checks remain deferred release work. Direct desktop CMake selects/verifies one Xcode before enabling
the compiler. Direct core-only CMake may select another compiler explicitly for
comparison. Use a separate directory for that experiment:

```sh
cmake -S . -B .work/other-compiler -G Ninja -DPGP_CORE_ONLY=ON -DCMAKE_CXX_COMPILER=/path/to/clang++
cmake --build .work/other-compiler
ctest --test-dir .work/other-compiler --output-on-failure
```

Direct CMake trees have no wrapper ownership marker. The wrapper refuses to adopt
or clean them. Keep direct and wrapper output directories separate.

## Explicit interface and audio checks

After a desktop build, launch only when wanted:

```sh
build/release/bin/pgp-probe
build/release/bin/pgp-probe --smoke-test
build/release/bin/pgp-probe --smoke-test --text-scale 2
```

The smoke command resizes to 1280×720, checks local image readiness, keyboard
activation/Tab order, accessible name, visible focus and sound-button fixture
loading, then exits. The native-audio build can play the tone during this explicit
smoke command; default builds remain silent. Add
`--screenshot /absolute/path.png` to save a local image. `--software` explicitly
probes the software renderer. `--text-scale` accepts finite values from 1 to 2.
These are development-probe commands, including in Release; full game UI,
reader/device/privacy and deployed-package acceptance remains unqualified.

CTest uses offscreen resource/CLI checks and silent audio tests; it never opens
a visible probe window or audio output device. Native playback requires an
explicit separate configuration and manual invocation:

```sh
cmake -S . -B .work/native-audio -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt -DPGP_ENABLE_AUDIO=ON
cmake --build .work/native-audio --parallel 2
ctest --test-dir .work/native-audio --output-on-failure
.work/native-audio/bin/pgp-audio-test --native-play
```

`PGP_ENABLE_AUDIO` defaults OFF. Either mode decodes bounded local PCM16 WAV/Ogg;
native mode additionally compiles the pinned vendored playback backend. The app
plays only when Check sound is activated. Failed/unavailable/lost devices retain
silence. No Qt Multimedia or FFmpeg runtime is needed; no build fetches audio
sources. See [local evidence](evidence/M0-03-local-probe.md) and
[audio dependencies/limits](evidence/M0-03-audio.md).

## Extension points

`src/core` builds `pgp::core`, a standard C++ library without Qt or network
dependencies. `src/headless` supplies the CLI and links that library. Public core
headers live under `src/core/include/pgp/core`; project-only build options are an
interface target in `cmake/ProjectOptions.cmake`. Tests link the layer exercised.
`src/app` wires the probe/controller to independent graphics/resource/audio
targets in `src/platform`; QML stays in `qml/probe`. M1-01 adds Qt-free strong
units/IDs, checked arithmetic, calendar, RNG and integer-token parsing in core.
The [1,000-day primitive fixture](evidence/M1-01-primitives.md) is a test-only
target and is compared against an independent oracle by CTest/native CI.
`src/content` now builds the Qt-free `pgp::content` adapter with a pinned,
vendored JSON reader and typed registry/collection validation. JSON types stay
out of core and public content headers. `src/bridge` supplies a serialized Qt
worker and persistent reservation model; it does not add a playable room shell.
`src/runtime` adds executable typed flows/inspection, `src/persistence` adds
strict save/settings IO, and `src/session` applies campaign autosave/load policy.
These targets remain Qt-free. The separately packaged editor is later work.

The engineering foundation can be inspected without Qt:

```sh
./make.sh --core-only --jobs 2
build/core-release/bin/pgp-headless --foundation-demo
build/core-release/bin/pgp-headless --validate-content tests/content/fixtures/foundation.json
build/core-release/bin/pgp-headless --flow-demo tests/content/fixtures/foundation.json
```

The demonstration exercises reservations, stale commands, irreversible terms
and a suspended morning choice using copied in-memory state. The flow demo
executes the seven normalized flows and verifies exact serialized continuation;
add `--save /chosen/local/demo.pgpsave` to write/read that explicit local path.
Neither demonstration is the business scenario. Validation reads at most 1 MiB
plus one sentinel byte from the selected local file, returns typed diagnostics
and never writes content. Engineering fixtures are not finalized game content.
See [playability checkpoints](PLAYABILITY.md) for the path to feature testing.

`project-versions.json` remains unchanged: all three component versions are
`0.0.0-dev`, with `phase: planning`. A Release build selects optimization; it does
not turn those placeholders into a release. Configuration validates the manifest
through the same offline reader used by diagnostics and future packaging tools.
CMake receives the game's numeric SemVer core; the full suffix/build metadata
remains in the generated core identity.

```sh
build/core-release/bin/pgp-headless --build-info
python3 tools/project_versions.py read
python3 tools/project_versions.py compare 0.2.0-rc.2 0.2.0-rc.10
python3 tools/project_versions.py tag --product editor
python3 tools/project_versions.py artifact --product game --os macos --arch arm64
python3 tools/project_versions.py check-release --product game --tag game-v0.0.0-dev
```

`compare` prints -1/0/1 using SemVer precedence, including numeric prerelease
ordering and ignoring build metadata for precedence. Artifact stems identify a
prospective product/version/target; no archive is created and development binary
names stay `pgp-headless`/`pgp-probe`. Probe `--version` and `--build-info` use the
same identity (the latter exits before graphics/resource/audio setup).

Git commit/state are explicitly a **configure-time observation**. The wrapper
configures on every build. A direct `cmake --build` may retain the old observation
after an ordinary source edit or new commit; rerun `cmake --preset core-release`
(or the corresponding preset) before relying on it. Manifest/reader changes
automatically trigger CMake reconfiguration. Exported trees or unavailable Git
show `unavailable`, never an invented clean commit. The manifest SHA-256 is the
raw version-file hash; runtime content hashing does not exist yet and is reported
as `unavailable`.

`check-release` inspects live Git and component identity, reports known blockers,
and always exits nonzero with `release_ready: false` while later content,
migration, payload-reuse and package qualification gates lack implementations.
Passing a version/tag comparison cannot qualify a package. See the exact
[version contract and remaining gates](VERSIONING.md). No dependency fetch,
package upload or product-version rewrite is hidden in configuration.

Build behavior follows the CMake contracts for [presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html),
[build types](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html)
and [Apple SDK selection before language initialization](https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_SYSROOT.html).

### Planned competition checks

```sh
build/release/bin/pgp-headless --competition-demo data/runtime/first-playable.json standard 1096
ctest --test-dir build/release -R '^runtime.(competition|heavyweights|governance)$' --output-on-failure
"$game_binary" --competition-smoke --smoke-test \
  --portable-root "$PWD/.work/competition-smoke-new" --text-scale 200 \
  --screenshot "$PWD/.work/competition-smoke-new.png"
```

Use a fresh owned portable directory. `--competition-smoke` exercises the bundled
standard roster, explicit publication/adoption and prices, public heavyweight
briefings, declining a publication offer, keyboard review/acceptance of a pilot
and its actual paid milestone, executive compensation, financing/board votes,
explicit pool dilution, acquisition, paid group funding/earned dividend,
cumulative vesting, the normal funded Office/Lab path and exact final
save/reload through real QML controls. It does
not conduct a human session. The headless presets are `standard`, `reduced`,
`expanded` and `forgiving`; requested days are bounded to 1–1096 and the output
reports the actual stop date, including real warnings/closures. Century
qualification, century balance and objective windows remain later work.
