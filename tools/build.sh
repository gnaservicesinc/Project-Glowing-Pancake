#!/usr/bin/env bash
set -euo pipefail

fail() { printf 'Build error: %s\n' "$*" >&2; exit 2; }
usage() {
    cat <<'USAGE'
Usage: ./make.sh [options]

  (no options)      Build the Release desktop skeleton; never launch it
  --core-only      Build core/headless without finding or downloading Qt
  --editor         Build the separate Content Studio
  --package        Build and stage the Mac game app with Qt, icon and resources
  --debugging      Build Debug with local developer diagnostics (--debug alias)
  --test           Build, then run CTest; failures return a nonzero exit code
  --clean          Remove only the selected, verified wrapper-owned build tree
  --qt-root PATH   Qt 6.12.0 prefix (or set QT_ROOT/CMAKE_PREFIX_PATH)
  --jobs N         Limit parallel build jobs to a positive integer
  --help           Show this help without configuring or building

Requires Bash, CMake, Ninja and a C++20 compiler; tests also use CTest.
Outputs: build/{release,debug,core-release,core-debug} beneath this checkout.
On macOS, DEVELOPER_DIR (or xcode-select -p) selects one existing Xcode compiler
and SDK. Global settings are never changed. The known local Qt installation
is a build-time fallback only. No SDKs or dependencies are downloaded.
--package stages dist/Project Glowing Pancake.app on macOS. This is a local
development build, not a notarized release. Builds never launch applications.
USAGE
}

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
configuration=Release
core_only=OFF
editor=OFF
package=0
run_tests=0
clean=0
qt_root="${QT_ROOT:-}"
jobs=""

while (($#)); do
    case "$1" in
        --help|-h) usage; exit 0 ;;
        --debugging|--debug) configuration=Debug ;;
        --core-only) core_only=ON ;;
        --editor) editor=ON ;;
        --package) package=1 ;;
        --test) run_tests=1 ;;
        --clean) clean=1 ;;
        --qt-root)
            (($# >= 2)) && [[ -n "$2" && "$2" != --* ]] || fail '--qt-root needs a directory'
            qt_root="$2"; shift ;;
        --jobs)
            (($# >= 2)) && [[ "$2" =~ ^[1-9][0-9]*$ ]] || fail '--jobs needs a positive integer'
            jobs="$2"; shift ;;
        --all) fail "$1 is not implemented yet; see docs/BACKLOG.md" ;;
        *) fail "unknown option: $1 (use --help)" ;;
    esac
    shift
done
[[ "$clean" == 0 || "$run_tests" == 0 ]] || fail '--clean cannot be combined with --test'
[[ "$editor" == OFF || "$core_only" == OFF ]] || fail '--editor needs desktop Qt'
[[ "$editor" == OFF || "$clean" == 0 ]] || fail '--clean selects a whole build tree; omit --editor'
[[ "$package" == 0 || ( "$editor" == OFF && "$core_only" == OFF && "$clean" == 0 && "$(uname -s)" == Darwin ) ]] || fail '--package requires a Mac desktop game build'

preset=release
[[ "$configuration" != Debug ]] || preset=debug
[[ "$core_only" != ON ]] || preset="core-$preset"
build_parent="$project_root/build"
build_dir="$build_parent/$preset"
owner_file="$build_dir/.pgp-build-owner"
signature_file="$build_dir/.pgp-build-signature"

# Refuse redirected parents/trees before touching any generated path. An owner
# marker prevents cleanup from removing unrelated pre-existing build outputs.
[[ ! -L "$build_parent" && ! -L "$build_dir" ]] || fail 'build paths must not be symbolic links'
if [[ -e "$build_parent" ]]; then
    [[ -d "$build_parent" ]] || fail 'build exists and is not a directory'
    [[ "$(cd -- "$build_parent" && pwd -P)" == "$build_parent" ]] || fail 'build parent escapes this checkout'
fi
if [[ -e "$build_dir" ]]; then
    [[ -d "$build_dir" && ! -L "$owner_file" && -f "$owner_file" ]] || fail "unowned build tree: $build_dir"
    [[ "$(cat -- "$owner_file")" == "$project_root" ]] || fail "build tree belongs to another checkout: $build_dir"
    [[ ! -L "$signature_file" ]] || fail 'build signature must not be a symbolic link'
fi

if [[ "$clean" == 1 ]]; then
    if [[ -d "$build_dir" ]]; then
        rm -rf -- "$build_dir"
        printf 'Removed owned build tree: %s\n' "$build_dir"
    else
        printf 'Nothing to clean: %s\n' "$build_dir"
    fi
    exit 0
fi

for required in cmake ninja; do
    command -v "$required" >/dev/null 2>&1 || fail "missing required tool: $required"
done
if [[ "$run_tests" == 1 ]]; then
    command -v ctest >/dev/null 2>&1 || fail 'missing required tool: ctest'
fi

cmake_args=(--preset "$preset" "-DPGP_BUILD_EDITOR=$editor")
developer_dir=""
compiler=""
sdk=""
architecture=""
if [[ "$(uname -s)" == Darwin ]]; then
    selected_developer="${DEVELOPER_DIR:-$(/usr/bin/xcode-select -p)}"
    [[ -d "$selected_developer" ]] || fail "selected developer directory does not exist: $selected_developer"
    developer_dir="$(cd -- "$selected_developer" && pwd -P)"
    export DEVELOPER_DIR="$developer_dir"
    compiler="$(/usr/bin/xcrun --sdk macosx --find clang++)"
    c_compiler="$(/usr/bin/xcrun --sdk macosx --find clang)"
    sdk="$(/usr/bin/xcrun --sdk macosx --show-sdk-path)"
    compiler="$(cd -- "$(dirname -- "$compiler")" && pwd -P)/$(basename -- "$compiler")"
    c_compiler="$(cd -- "$(dirname -- "$c_compiler")" && pwd -P)/$(basename -- "$c_compiler")"
    sdk="$(cd -- "$sdk" && pwd -P)"
    case "$compiler" in "$developer_dir"/*) ;; *) fail 'compiler is outside the selected developer directory' ;; esac
    case "$sdk" in "$developer_dir"/*) ;; *) fail 'SDK is outside the selected developer directory' ;; esac
    [[ -z "${CXX:-}" || "$CXX" == "$compiler" ]] || fail 'CXX conflicts with selected Xcode; unset it or use the selected compiler path'
    [[ -z "${CC:-}" || "$CC" == "$c_compiler" ]] || fail 'CC conflicts with selected Xcode; unset it or use the selected compiler path'
    architecture="$(uname -m)"
    cmake_args+=("-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_OSX_SYSROOT=$sdk"
        "-DCMAKE_OSX_ARCHITECTURES=$architecture" '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.4'
        "-DPGP_SELECTED_DEVELOPER_DIR=$developer_dir")
    printf 'Xcode: %s\nCompiler: %s\nSDK: %s\nArchitecture: %s\n' "$developer_dir" "$compiler" "$sdk" "$architecture"
    if [[ -z "$qt_root" && -z "${CMAKE_PREFIX_PATH:-}" && -d /opt/Qt/6.12.0/macos ]]; then
        qt_root=/opt/Qt/6.12.0/macos
    fi
fi
if [[ "$core_only" == ON ]]; then
    qt_root=""
elif [[ -n "$qt_root" ]]; then
    [[ -d "$qt_root" ]] || fail "Qt prefix does not exist: $qt_root"
    qt_root="$(cd -- "$qt_root" && pwd -P)"
    cmake_args+=("-DCMAKE_PREFIX_PATH=$qt_root")
fi
prefix_identity="${CMAKE_PREFIX_PATH:-}"
[[ "$core_only" != ON ]] || prefix_identity=""

# Keep configuration identities separate and reject a stale toolchain before
# CMake can silently rewrite its compiler cache. --clean is an explicit recovery.
signature="$(printf '%s\n' 'pgp-build-signature-v1' "$project_root" "$preset" "$configuration" "$core_only" \
    "$developer_dir" "$compiler" "$sdk" "$architecture" "$qt_root" \
    "$prefix_identity" "${CC:-}" "${CXX:-}")"
if [[ -f "$signature_file" ]]; then
    [[ "$(cat -- "$signature_file")" == "$signature" ]] || fail "cached toolchain/configuration changed; run --clean for the selected preset ($preset) first"
elif [[ -e "$build_dir/CMakeCache.txt" ]]; then
    fail "cache lacks wrapper toolchain identity; clean this owned tree first: $build_dir"
fi

mkdir -p -- "$build_dir"
printf '%s\n' "$project_root" > "$owner_file"
printf '%s\n' "$signature" > "$signature_file"
cd -- "$project_root"
cmake "${cmake_args[@]}"
build_args=(--build --preset "$preset")
if [[ "$editor" == ON ]]; then
    build_args+=(--target pgp-editor)
    [[ "$run_tests" == 0 ]] || build_args+=(pgp-studio-library-test)
fi
[[ -z "$jobs" ]] || build_args+=(--parallel "$jobs")
cmake "${build_args[@]}"
if [[ "$core_only" == OFF && "$editor" == OFF && "$(uname -s)" == Darwin && -d "$build_dir/bin/Project Glowing Pancake.app" &&
      -f "$build_dir/bin/pgp-game" && ! -L "$build_dir/bin/pgp-game" ]]; then
    rm -f -- "$build_dir/bin/pgp-game" # Retire only the wrapper-owned pre-bundle game output.
fi
if [[ "$package" == 1 ]]; then
    package_qt_root="$qt_root"
    if [[ -z "$package_qt_root" ]]; then
        package_qt_root="$(sed -n 's/^Qt6_DIR:PATH=//p' "$build_dir/CMakeCache.txt")/../../.."
    fi
    bash "$project_root/tools/package-macos.sh" "$build_dir" "$package_qt_root"
fi
if [[ "$run_tests" == 1 ]]; then
    test_args=(--preset "$preset")
    [[ "$editor" == OFF ]] || test_args+=(-R '^studio[.]')
    ctest "${test_args[@]}"
fi
printf 'Built %s (%s); no application was launched.\n' "$preset" "$build_dir"
