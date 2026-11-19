# Building the game and Content Studio

## Requirements and commands

Use CMake 3.24+, Ninja, a C++20 compiler, Python 3.10+ (standard library only),
and Bash for `make.sh`. Python validates metadata at build time; the built core,
headless executable and probe have no Python runtime dependency. CTest is bundled
with CMake. The desktop configuration additionally requires Qt **6.12.0** Core,
Gui, Network, Qml, Quick and QuickControls2. It builds the game and editor without linking Qt into core/headless targets.
The interface probe and GUI test walkthroughs require `--enable-tests`. Qt Multimedia is not required. Nothing
downloads dependencies or launches a program during builds.

| Command | Implemented behavior |
| --- | --- |
| `./make.sh` | Both applications in Release, optimized for the local CPU; stages the game on Mac; no tests or launch |
| `./make.sh --package --jobs 4` | Mac game app with bundled Qt, resources and icon in `dist`; no launch |
| `./script/build_and_run.sh` | Explicitly build, package and launch the Mac app; also used by the Codex Run action |
| `./make.sh --test` | Build then run CTest; propagate failures |
| `./make.sh --enable-debugging` | Debug in `build/debug`, local developer diagnostics enabled (`--debugging`/`--debug` aliases) |
| `./make.sh --enable-tests` | Include test targets and GUI checks without running tests |
| `./make.sh --portable` | Release without local CPU tuning |
| `./make.sh --game-only` | Build only the game |
| `./make.sh --enable-distribution` | Build the Release game in `build/distribution`, with development menus, editor targets and playtest code excluded |
| `./make.sh --disable-distribution` | Build the ordinary development edition, the default |
| `./make.sh --core-only` | Release in `build/core-release`, no Qt discovery/linkage |
| `./make.sh --editor` | Build only Content Studio in `build/release/bin/pgp-editor` |
| `./make.sh --editor --test` | Build the editor and test its library, draft workflow and typed flow forms |
| `./make.sh --core-only --debug --test` | Debug core and tests in `build/core-debug` |
| `./make.sh --qt-root /path/to/Qt/prefix` | Select an installed desktop Qt prefix |
| `./make.sh -j11 -l11` | Build with up to 11 parallel jobs and a Ninja load limit of 11, the defaults |
| `./make.sh --jobs 2` | Lower compilation parallelism for this run |
| `./make.sh --core-only --clean` | Remove only the selected wrapper-owned tree; no rebuild |
| `./make.sh --help` | Show flags, paths and requirements |

Normal builds explicitly configure `BUILD_TESTING=OFF`, `PGP_BUILD_EDITOR=ON`
and `PGP_NATIVE_OPTIMIZATION=ON`, even after a test-enabled cache. Release uses
`-O3`, `NDEBUG`, and compiler-supported host CPU tuning (`-mcpu=native` on this
arm64 Mac, `-march=native` on x86). Developer diagnostics and GUI walkthrough
code are excluded. `--enable-tests` includes checks but does not run them;
`--test` explicitly builds/runs the suite and may be expensive. Prefer a focused
target and CTest filter while developing a selected feature. `--portable`
disables host tuning for a build intended for other CPUs. Caches are retained.

Use `--enable-distribution --package` to stage a Mac distribution build. The
distribution flags accept `=true` and `=false`, or a separate boolean value.
The last distribution flag applies. Direct CMake uses `-DPGP_DISTRIBUTION=ON`.
Distribution requires Release and rejects Debug or a multi-configuration build
that offers other modes. Use `--editor` separately for Content Studio.

For example, a focused setup check is explicitly opted in:

```sh
./make.sh --enable-tests
cmake --build build/release --target pgp-founder-journey-test --parallel 4
ctest --test-dir build/release -R '^runtime[.]founder-journey$' --output-on-failure
./make.sh  # Return to both apps with testing/debugging disabled.
```

`--all` remains reserved. On macOS, a desktop game build stages a relocatable local
development app in `dist/Project Glowing Pancake.app`, including its icon,
menu video, Qt frameworks/plugins/QML imports and notices. It is ad-hoc signed;
notarized distribution and other-platform packages remain later gates.
Cleaning uses the same configuration flags as building; for example
`./make.sh --debug --core-only --clean` removes only `build/core-debug`.
Unknown existing directories, symlinked build paths and owner-marker mismatches
are refused. Changed toolchain selections require an explicit clean of that
configuration. User saves and unrelated outputs are outside cleanup.

For a Mac Debug check using the installed Qt SDK and a plain executable,
configure a separate build tree with `-DPGP_MACOS_BUNDLE=OFF` and leave off
`--package`. Game QML and assets compiled into the executable remain available.


## Verify a change on Mac

Use the existing Release tree. The focused test targets and GUI walkthrough options are available with `./make.sh --enable-tests`. Run `ctest --test-dir build/release -N` to list the available checks. A device and research pass can use:

```sh
ctest --test-dir build/release -R '^(bridge\.(founding|devices)|runtime\.workplace-research)$' --output-on-failure
mkdir -p .work
run_dir=$(mktemp -d "$PWD/.work/device-check.XXXXXX")
game="build/release/bin/Project Glowing Pancake.app/Contents/MacOS/Project Glowing Pancake"
"$game" --smoke-test --device-interface-smoke --portable-root "$run_dir"
```

Pass `--playtest-setup` with the device walkthrough to exercise the configurable start. Use a fresh portable directory for a new company. The in-game development reset affects its selected local playtest storage. Keep build caches between runs.

## Content and packaging

`./make.sh --editor` builds the standalone Content Studio. Select the repository checkout when authoring. See the [editor guide](CONTENT_EDITOR.md) for drafts and [versioning](VERSIONING.md) for reviewed snapshots. To verify repository content, run `build/release/bin/pgp-content-tool check --checkout "$PWD"`.

`./make.sh --package` stages a relocatable signed Mac development app in `dist`. For a distribution configuration use `--enable-distribution`. The [release guide](QA_AND_RELEASE.md) tracks the native platform matrix and independent qualification.
