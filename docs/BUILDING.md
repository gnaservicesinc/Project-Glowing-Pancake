# Building the development skeleton

M0-02 implements build entry points and the headless scaffold. There is no game
window, simulation, Content Studio, playback backend or distributable package yet.
The [implementation log](IMPLEMENTATION_LOG.md) records acceptance evidence.

## Requirements and commands

Use CMake 3.24+, Ninja, a C++20 compiler and Bash for `make.sh`. CTest is bundled
with CMake. The desktop configuration additionally requires Qt **6.12.0** Core,
Gui, Qml, Quick and QuickControls2. It currently validates discovery without
linking Qt into core/headless targets. Qt Multimedia is not required. Nothing
downloads dependencies or launches a program during builds.

| Command | Implemented behavior |
| --- | --- |
| `./make.sh` | Optimized Release desktop skeleton in `build/release` |
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

## Extension points

`src/core` builds `pgp::core`, a standard C++ library without Qt or network
dependencies. `src/headless` supplies the CLI and links that library. Public core
headers live under `src/core/include/pgp/core`; project-only build options are an
interface target in `cmake/ProjectOptions.cmake`. Tests link the layer exercised.
Add domain primitives in M1-01 and application/resource code in M0-03 without
reversing those dependencies. Content, persistence and the separately packaged
editor will be separate targets as their backlog tasks arrive.

`project-versions.json` remains unchanged: this is a development skeleton, not a
release. The shared version reader/build metadata is M0-05. No dependency fetch,
package upload or product-version rewrite is hidden in configuration.

Build behavior follows the CMake contracts for [presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html),
[build types](https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html)
and [Apple SDK selection before language initialization](https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_SYSROOT.html).
