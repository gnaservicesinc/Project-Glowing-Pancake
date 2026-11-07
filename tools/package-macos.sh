#!/usr/bin/env bash
# Stage a relocatable local development application; never launch or publish.
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
[[ "$(uname -s)" == Darwin ]] || { echo 'Mac packaging requires macOS.' >&2; exit 2; }
build_dir="${1:-$project_root/build/release}"
qt_root="${2:-/opt/Qt/6.12.0/macos}"
source_bundle="$build_dir/bin/Project Glowing Pancake.app"
destination="$project_root/dist/Project Glowing Pancake.app"
[[ -d "$source_bundle" && -x "$qt_root/bin/macdeployqt" ]] || { echo 'Build the game and select a Qt SDK with macdeployqt.' >&2; exit 2; }
[[ ! -L "$project_root/dist" && ! -L "$destination" ]] || { echo 'Refusing a redirected package destination.' >&2; exit 2; }
if [[ -e "$destination" ]]; then
    [[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$destination/Contents/Info.plist")" == org.projectglowingpancake.game ]] || {
        echo 'The package destination belongs to a different application.' >&2; exit 2;
    }
fi
mkdir -p -- "$project_root/dist"
package_stage="$(mktemp -d "$project_root/dist/.macos-package.XXXXXX")"
trap '/bin/rm -rf -- "$package_stage" || /bin/rm -rf -- "$package_stage"' EXIT
staged_bundle="$package_stage/Project Glowing Pancake.app"
/usr/bin/ditto "$source_bundle" "$staged_bundle"
# Deploy only the platform and image plugins this game uses. The SDK's optional
# SQL/TLS/network-discovery plugins have unrelated external dependencies.
mkdir -p "$staged_bundle/Contents/PlugIns/platforms" "$staged_bundle/Contents/PlugIns/imageformats"
cp "$qt_root/plugins/platforms/libqcocoa.dylib" "$staged_bundle/Contents/PlugIns/platforms/"
cp "$qt_root/plugins/imageformats/libqjpeg.dylib" "$staged_bundle/Contents/PlugIns/imageformats/"
cp "$qt_root/plugins/imageformats/libqsvg.dylib" "$staged_bundle/Contents/PlugIns/imageformats/"
PATH="$qt_root/bin:/usr/bin:/bin:/usr/sbin:/sbin" "$qt_root/bin/macdeployqt" "$staged_bundle" \
    "-qmldir=$project_root/qml/game" -no-plugins -always-overwrite -verbose=1 \
    "-executable=$staged_bundle/Contents/PlugIns/platforms/libqcocoa.dylib" \
    "-executable=$staged_bundle/Contents/PlugIns/imageformats/libqjpeg.dylib" \
    "-executable=$staged_bundle/Contents/PlugIns/imageformats/libqsvg.dylib"
mkdir -p "$staged_bundle/Contents/Resources/Licenses"
cp "$project_root/LICENSE" "$staged_bundle/Contents/Resources/Licenses/Game-GPL-3.0.txt"
cp "$project_root/assets/notices/Qt-LGPL-3.0.txt" "$staged_bundle/Contents/Resources/Licenses/Qt-LGPL-3.0.txt"
cp "$project_root/assets/audio/CREDITS.json" "$staged_bundle/Contents/Resources/Licenses/Music-Credits.json"
cp "$project_root/assets/audio/CC-BY-4.0.txt" "$staged_bundle/Contents/Resources/Licenses/CC-BY-4.0.txt"
cp "$project_root/assets/menu/PROVENANCE.md" "$staged_bundle/Contents/Resources/Licenses/Menu-Provenance.md"
cp "$project_root/third_party/json/LICENSE.MIT" "$staged_bundle/Contents/Resources/Licenses/nlohmann-json-MIT.txt"
cp "$project_root/third_party/audio/miniaudio/LICENSE" "$staged_bundle/Contents/Resources/Licenses/miniaudio.txt"
cp "$project_root/third_party/audio/stb/LICENSE" "$staged_bundle/Contents/Resources/Licenses/stb.txt"
cp "$project_root/assets/maps/CENSUS_TERMS.txt" "$staged_bundle/Contents/Resources/Licenses/Census.txt"
cat > "$staged_bundle/Contents/Resources/qt.conf" <<'CONF'
[Paths]
Prefix = ..
Plugins = PlugIns
QmlImports = Resources/qml
CONF
/usr/bin/plutil -lint "$staged_bundle/Contents/Info.plist"
python3 "$project_root/tools/validate_macos_bundle.py" "$staged_bundle"
/usr/bin/codesign --force --deep --sign - "$staged_bundle"
/usr/bin/codesign --verify --deep --strict "$staged_bundle"
if [[ -d "$destination" ]]; then mv "$destination" "$package_stage/previous.app"; fi
mv "$staged_bundle" "$destination"
printf 'Packaged local Mac application: %s\n' "$destination"
