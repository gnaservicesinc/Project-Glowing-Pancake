# Technical plan

Baseline 1.0 · 2026-11-03 · Proposed implementation; no game/build system exists yet

## 1. Stack decision

Use **C++20, Qt 6.12.x, Qt Quick/QML, CMake, and Ninja** for a native desktop game. The simulation is a standard C++ library, independent of Qt and the UI. A small headless executable runs scenarios and regression tests. The interface uses Qt Quick Controls, custom QML components, and prepared vector geometry. Use ordinary QML/C++ charts; do not add Qt Graphs, WebEngine, Qt Location, or an embedded browser to the initial dependency set.

This is a design selection based on the data-heavy UI, installed tooling, distribution goals, and a separable long-running simulation. It remains subject to an early packaging/rendering spike. `/opt/Qt/6.12.0/macos/bin/qmake -query QT_VERSION` returned `6.12.0` during planning. That local SDK does not prove other OS packages are available or the game works on them.

| Alternative | Assessment for this project |
| --- | --- |
| Qt Quick + C++ | Selected: maps, comparisons, tables, accessibility hooks, native desktop deployment, and a headless core fit the game. Packaging needs proof. |
| Godot | Viable fallback if native packaging or Qt UI development fails the spike. Better scene tooling; requires a different UI/data integration and build workflow. Do not migrate without a recorded decision. |
| Browser/TypeScript | Good for immediate browser access; introduces browser storage/distribution constraints and a different rendering/build plan. Not needed to satisfy the original desktop-or-browser choice. |
| GameMaker | Installed, but a gameplay-oriented toolchain adds friction to this particular ledger/map-heavy application and native architecture matrix. Not rejected on a claim that it cannot make complex simulations. |
| Ren'Py | Installed, but scene/dialogue strengths are not the main need here. A custom simulation and management UI would still need substantial engineering. Python performance alone is not a sufficient rejection reason. |

Desktop is the primary 1.0 delivery. Browser/WASM is deferred, with no claim it is a free export. Snapcraft follows the complete itch.io release. No Qt license purchase, cloud model subscription, remote GPU, hosted database, or game server is required by the selected architecture.

## 2. Platforms and rendering

The initial qualification matrix is:

| Artifact | Build host | Initial runtime baseline | Rendering policy | Package |
| --- | --- | --- | --- | --- |
| macOS arm64 | Native Apple Silicon GitHub runner | macOS 14.4+ | Metal | `.app` in ZIP; ad-hoc signed, not notarized |
| Linux amd64 | Ubuntu 24.04 x64 | Ubuntu 24.04/glibc 2.39+, compatible GPU/display stack | OpenGL | Portable directory in `.tar.gz` |
| Linux arm64 | Ubuntu 24.04 arm64 | Ubuntu 24.04/glibc 2.39+, compatible GPU/display stack | OpenGL | Portable directory in `.tar.gz` |
| Windows amd64 | Windows x64, MSVC | Windows 11 | OpenGL preferred; Direct3D 11 compatibility fallback | Portable ZIP |
| Windows arm64 | Native Windows ARM64, MSVC | Windows 11 on ARM | OpenGL preferred; Direct3D 11 compatibility fallback | Portable ZIP |

Qt currently defaults to Metal on macOS, Direct3D 11 on Windows, and OpenGL on other desktop platforms; it does **not** automatically choose OpenGL on Windows. Select/test the requested backend explicitly before window construction. A minimal rendering preflight can choose Direct3D 11 on Windows when OpenGL cannot provide a working context; show the active renderer in local diagnostics. This limited compatibility adjustment protects the Windows ARM goal. Linux/Windows software rendering can be an optional recovery mode if it passes UI tests; it is not a substitute for platform qualification. [Qt renderer documentation](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html).

These OS floors follow the selected Qt/toolchain baseline and need confirmation at M0. Qt's platform documentation lists macOS 14.4+, Ubuntu 24.04 ARM/x64, and Windows ARM64 with MSVC. ARM64EC is not the target. Linux bundles are not promised to work on every distribution; glibc, display system, GPU drivers, and essential OS libraries remain host dependencies. [Qt supported platforms](https://doc.qt.io/qt-6/supported-platforms.html).

Use pinned hosted-runner labels after checking current availability and a pinned Qt patch version/checksum. Native ARM runners are available for public repositories; this repository was verified public during planning. Runner availability does not establish usable graphics drivers or physical-device acceptance. [GitHub runner reference](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).

## 3. Intended source structure

The following directories describe future implementation; creating this plan does not create a runnable scaffold.

```text
src/core/            # dates, IDs, units, state, commands, deterministic daily systems
src/persistence/     # save/replay serialization and migrations, no UI dependency
src/content/         # typed definitions, validation, scenario/catalog loading
src/headless/        # scenario runner, replay and benchmark commands
src/app/             # application lifecycle and dependency wiring
src/bridge/          # Qt models, immutable snapshots, command adapters
qml/                 # rooms, reusable controls, tables, inspectors, accessibility
data/                # versioned, validated game definitions, no executable scripts
assets/              # licensed local visuals/audio/fonts, provenance and credits
tests/               # unit, deterministic, integration, scenario and migration fixtures
tools/               # small content/build validation tools
cmake/               # dependencies, presets, deployment support
.github/workflows/   # validation and native build/package jobs
docs/                # design, decisions, operating plan, and evidence
agents/              # reusable role briefs and task handoff format
```

Dependency direction is `app → bridge/UI + persistence/content → core`; the core has no dependency on Qt, filesystem locations, JSON library types, renderer, audio, or network facilities. Persistence and content may use a pinned permissively licensed JSON library; start with `nlohmann/json` and record its exact version and MIT license at implementation. Qt types terminate at the bridge. Tests can link only the layer they exercise.

## 4. State, commands, and UI integration

`GameState` owns the calendar, world profiles, companies, markets, research, model lineage, data rights, sites, policies, objective windows, and RNG streams. Stable typed IDs replace raw cross-entity pointers. Each content reference is validated. Core systems mutate through transactions with named causes, not UI callbacks.

Commands such as `StartTraining`, `OfferContract`, `HireTeam`, `SetBudgetPolicy`, `PublishArtifact`, and `BuildSite` carry explicit arguments and an expected state revision. Validation returns a stable reason code and player-readable explanation. Revalidate on commit: a forecast or screen becoming stale must not spend unavailable cash. An accepted command is recorded with its authoritative order; rejected commands have no partial effect.

The simulation runs on one worker thread. Presentation receives immutable snapshots/diffs at a capped rate (initial target 10 Hz while running, immediate on pause/command result). QML never reads mutable core state or advances time. The main thread remains available for keyboard, menus, dialogs, and rendering. Keep the commit order single-threaded until profiling demonstrates a need for parallel pure calculations.

```mermaid
flowchart LR
    UI[QML rooms and management views] --> CMD[Validated commands]
    CMD --> CORE[Deterministic simulation]
    DATA[Validated content definitions] --> CORE
    CORE --> SNAP[Immutable UI snapshots]
    SNAP --> UI
    CORE --> SAVE[Local saves and replay data]
    CORE --> HEAD[Headless scenarios and tests]
```

Forecast previews run on a copied/summary state without consuming real RNG or mutating the simulation. Preview ranges disclose unknown demand, research outcomes, and hidden rival information; they are not a way to inspect the future's random draws.

## 5. Data and extension model

Use versioned JSON definitions for scenarios, rivals, hardware families, datasets, research nodes, model recipes, products, markets, policies, events, and objective rules. Validate required fields, units, unique IDs, references, graph cycles, ranges, and budget/ownership totals. Keep balance numbers out of QML and narrative prose. Examples in [the examples directory](examples/README.md) illustrate the format; production schemas and loaders are M1 work.

An event definition has ID/version, trigger facts, eligibility, cooldown/max occurrences, speaker/context, explanatory text, choices, commitment costs, typed effects, delayed consequences, and content tags. Use a restricted expression/effect vocabulary; never execute arbitrary C++, JavaScript, Python, shell code, or remote content from a scenario/save. Localization keys live separately from mechanics. Material surprises need a traceable causal fact.

The first release ships trusted built-in content. Locally loaded data-only scenario packs are a later feature with schema validation and a visible custom-content marker. They may alter scoring and invalidate standard objectives; they cannot enable networking. Workshop integration and executable plugins are outside 1.0.

## 6. Save/load, autosave, and campaign history

Use a documented UTF-8 JSON envelope with `.pgpsave` extension for the initial format. Include format version, simulation version, content versions/hashes, seed, date, all RNG states, state revision, active commitments, queued events, objective accumulators, and bounded historical summaries. Integrity hashes detect accidental corruption, not hostile tampering or encrypted secrets. Plain-text fictional names are acceptable.

Local preferences and an autosave ring are allowed. Use the platform's standard writable application-data/settings locations, make them discoverable in the UI, and support a portable save location plus Save As/Export. Default: three rotating local autosaves at a completed monthly boundary and before major irreversible in-game commitments. Allow disabling autosaves. Preferences are small, local, and contain no account identity or device identifier. Do not store passwords, collect OS account names, or ask for real personal information.

Writes use a temporary sibling, flush, and atomic replacement where supported; keep the previous valid version until the new file is committed. Never delete a user's original during migration. Validate lengths, counts, type/range constraints, references, and a configurable file-size bound before constructing state. Load errors preserve current play and explain recovery options. Future unsupported formats are refused, not guessed. Experimental pre-1.0 saves can have explicitly announced breaks; stable 1.0 saves require tested migrations in later releases.

A bounded command journal supports debugging/replay when included in a local save. Full developer replays are explicit local exports. Nothing uploads a save or diagnostic file. Campaign history is gameplay state, not an analytics pipeline. Old daily records compact into monthly/yearly summaries while retaining important events and financial totals.

## 7. Privacy and the absence of a backend

The game is entirely self-contained after installation. There is **no publisher-operated game server, runtime API key, player account, telemetry endpoint, analytics SDK, crash uploader, remote AI, advertising SDK, remote font, CDN dependency, cloud save, or in-game updater**. It does not collect player identifiers, hardware fingerprints, location, contacts, or behavioral analytics. Local game state, settings, and saves are the only normal persistence.

Itch's client and Snap handle their own installation/update behavior; they are separate applications/services. The game does not implement a local HTTP server or embed a browser merely to look app-like. A website/store link in the credits opens the system browser only after an explicit click and contains no player/session payload. Credits remain useful without following links.

Developer tooling and CI necessarily fetch dependencies and push artifacts; this does not add networking to the distributed game. A linked framework may contain networking capabilities, but application code and QML must not initiate requests or listen on sockets. Block remote resource URLs in content/loading paths. Verify release behavior with network-denied execution and attempted-connection observation through representative play, not only a search for the word telemetry.

Developer diagnostics are compiled out by default using `PGP_DEVELOPER_DIAGNOSTICS=0` and `#if PGP_DEVELOPER_DIAGNOSTICS`, not `#ifdef` on a macro defined as zero. Debug output is local and bounded; diagnostic exports are explicit and inspectable. There is no telemetry implementation to enable in either build type. Do not promise that preprocessor symbols remove framework capabilities or OS-generated crash handling.

## 8. Build command contract

M0/M1 will implement the following commands. They are **not available yet**:

| Command | Required behavior |
| --- | --- |
| `./make.sh` | Configure and compile an optimized Release build, no launch, no upload |
| `./make.sh --debugging` | Explicit developer Debug build with local diagnostic support |
| `./make.sh --debug` | Alias for `--debugging` |
| `./make.sh --core-only` | Build headless/core targets without downloading or requiring Qt |
| `./make.sh --test` | Build the selected configuration and run its appropriate tests; propagate failure |
| `./make.sh --package` | Produce a local distributable only when explicitly requested; CI is the normal packaging path |
| `./make.sh --clean` | Remove only known generated directories owned by this build wrapper |
| `./make.sh --help` | Describe flags, dependencies, output paths, and configuration |

Use out-of-source CMake presets under ignored `build/`. Select compiler-appropriate Release optimization, `NDEBUG`, and link-time optimization where tested; do not hardcode GCC's `-O3` into an MSVC build. Keep assertions in test targets even when the application is Release. Debug flags must never silently carry into a later default build. An explicit build preset/output directory identifies configuration.

Discover the local Qt path through an argument/environment override or a documented local preset. No `/opt/Qt` or developer-home path may remain as a runtime dependency. The core-only path must remain usable on a clean environment with a C++20 compiler, CMake, and Ninja. Dependencies are pinned and documented; no unversioned downloads at runtime or from a player's launch.

## 9. CI, packaging, and publication

On pull requests: validate docs/data, build/test the core across compiler families, and build a minimal native UI for each supported architecture as infrastructure permits. Before a tag/release candidate: run the complete five-artifact native matrix, package from clean staging directories, inspect dependencies, include source/license/credit material and checksums, and run clean-machine smoke tests. Keep artifact/cache retention short; do not store SDKs or binaries in Git. Pin action revisions and dependency versions during implementation.

Deploy Qt libraries, QML imports, accessibility/platform plugins, image support, audio dependencies, and compiler runtime libraries actually needed. Use Qt deployment tooling/CMake deployment APIs, `macdeployqt`, and `windeployqt` where applicable; Linux uses a controlled staging manifest and runtime-path inspection. Bundle required redistributable user-space dependencies. System libc, OS frameworks, and graphics drivers are documented exceptions; never copy arbitrary host libraries until a bundle happens to launch. Qt documents both library and QML deployment requirements. [Qt deployment guide](https://doc.qt.io/qt-6/deployment.html).

Ad-hoc sign the complete macOS bundle after bundling frameworks. No notarization service or paid Apple account is required by the baseline; provide accurate installation instructions and test the actual downloaded artifact. Never tell players to disable their whole system's security. Windows signing/notarization upgrades can be considered later without changing gameplay or privacy. Do not auto-launch on install or after builds.

Use Qt Multimedia only for local audio, with PCM WAV effects and Ogg Vorbis music if the packaged backend supports it on all targets. Verify its transitive decoder/license inventory in M0. No video cutscenes ship in 1.0; animate QML surfaces. If audio deployment fails, the slice remains playable silently while a separately recorded audio solution is selected. Do not infer worldwide patent freedom from a codec name or expiry claim in an AI draft.

Package source archives and the corresponding dependency/license information appropriate to the distributed GPL/LGPL components; preserve third-party license identities. Dynamic Qt linking is the baseline. A repository link alone is not assumed to satisfy every corresponding-source obligation. [Qt open-source obligations](https://www.qt.io/development/open-source-lgpl-obligations).

Publication is a separate explicit release task. A normal documentation/code push does not authorize publishing an itch page, uploading a public game build, configuring paid services, or submitting Snap packages. There is no in-game release channel or update check. Snap manifests and store submission are deferred until the primary desktop packages pass release gates.

## 10. Performance and delivery risks

Initial measurement targets: UI response to ordinary actions <100 ms; 60 fps on the declared minimum GPU with 30 fps recovery on low-power systems; peak working memory <1 GB for the standard 100-year campaign; saves <25 MB after normal history compaction; save/load <2 seconds on a reference SSD; headless 100-year run <60 seconds on a modern development CPU. These are targets, not measured results. Record named hardware/configuration, entity counts, and actual percentiles before publishing minimum requirements.

Largest risks are platform packaging (especially ARM graphics/audio), explaining a complex simulation, boring century-scale repetition, political/economic oversimplification, unbounded feature growth, and save compatibility. M0 proves the toolchain; M3 proves one enjoyable loop; M8 proves campaign endurance; M9 proves distribution. Failed gates result in a scoped fix or a recorded product decision, never a false claim of platform support.
