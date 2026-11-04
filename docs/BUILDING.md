# Building the development skeleton

M0-02 implements build entry points and the headless scaffold. M0-03 adds the
development interface probe, resource guards and optional playback adapter.
M0-05 validates the shared component versions and embeds configure-time identity.
There is no game simulation, Content Studio or distributable package yet.
The [implementation log](IMPLEMENTATION_LOG.md) records acceptance evidence.

## Requirements and commands

Use CMake 3.24+, Ninja, a C++20 compiler, Python 3.10+ (standard library only),
and Bash for `make.sh`. Python validates metadata at build time; the built core,
headless executable and probe have no Python runtime dependency. CTest is bundled
with CMake. The desktop configuration additionally requires Qt **6.12.0** Core,
Gui, Network, Qml, Quick and QuickControls2. It builds the probe without
linking Qt into core/headless targets. Qt Multimedia is not required. Nothing
downloads dependencies or launches a program during builds.

| Command | Implemented behavior |
| --- | --- |
| `./make.sh` | Optimized Release interface probe in `build/release`; no launch |
| `./make.sh --test` | Build then run CTest; propagate failures |
| `./make.sh --debugging` or `--debug` | Debug in `build/debug`, local developer diagnostics enabled |
| `./make.sh --core-only` | Release in `build/core-release`, no Qt discovery/linkage |
| `./make.sh --core-only --debug --test` | Debug core and tests in `build/core-debug` |
| `./make.sh --qt-root /path/to/Qt/prefix` | Select an installed desktop Qt prefix |
| `./make.sh --jobs 2` | Bound compilation parallelism |
| `./make.sh --core-only --clean` | Remove only the selected wrapper-owned tree; no rebuild |
| `./make.sh --help` | Show flags, paths and requirements |

`--editor`, `--all` and `--package` fail explicitly until their milestones exist.
Cleaning uses the same configuration flags as building; for example
`./make.sh --debug --core-only --clean` removes only `build/core-debug`.
Unknown existing directories, symlinked build paths and owner-marker mismatches
are refused. Changed toolchain selections require an explicit clean of that
configuration. User saves and unrelated outputs are outside cleanup.

`QT_ROOT` is an alternative to `--qt-root`; ordinary CMake prefix discovery via
`CMAKE_PREFIX_PATH` is also supported. On this Mac only, the wrapper can use the
existing `/opt/Qt/6.12.0/macos` as a build-time fallback. No such path is embedded
in the core/headless executable's runtime dependencies.

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

On Windows, use a developer shell for the native MSVC architecture. Windows and
Linux qualification will occur in M0-04; these commands have only been run on
macOS so far. Direct desktop CMake also selects/verifies one Xcode before enabling
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
targets in `src/platform`; QML stays in `qml/probe`. Add domain primitives in
M1-01 without reversing these dependencies. Content, persistence and the separately packaged
editor will be separate targets as their backlog tasks arrive.

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
