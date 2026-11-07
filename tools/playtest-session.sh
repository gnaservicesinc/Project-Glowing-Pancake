#!/usr/bin/env bash
# Explicit local launch only; no telemetry, tester contact or result collection.
# Prepared for v1 human testing after all components are in place and the user is ready.
set -euo pipefail
if [[ "${1:-}" == "--help" || $# -ne 1 ]]; then
    printf '%s\n' 'Usage: tools/playtest-session.sh S01' 'For deferred v1 human testing when the user is ready.' 'Creates a fresh ignored local session folder and explicitly opens the built game.'
    [[ "${1:-}" == "--help" ]] && exit 0
    exit 2
fi
session_id="$1"
if [[ ! "$session_id" =~ ^S[0-9]{2}$ ]]; then
    printf '%s\n' 'Use an anonymous session ID such as S01.' >&2
    exit 2
fi
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
game_binary="$project_root/build/release/bin/pgp-game"
game_bundle="$project_root/build/release/bin/Project Glowing Pancake.app"
if [[ "$(uname -s)" == Darwin ]]; then
    game_binary="$game_bundle/Contents/MacOS/Project Glowing Pancake"
fi
session_root="$project_root/.work/playtests/$session_id"
if [[ ! -x "$game_binary" ]]; then
    printf '%s\n' 'Build the optimized game first with ./make.sh --jobs 4.' >&2
    exit 2
fi
mkdir -p -- "$project_root/.work/playtests"
if ! mkdir -- "$session_root"; then
    printf '%s\n' 'That session folder already exists. Choose a new anonymous ID; existing saves were preserved.' >&2
    exit 2
fi
cp -- "$project_root/project-versions.json" "$session_root/project-versions.json"
"$game_binary" --build-info > "$session_root/build-identity.txt"
if [[ -f "$project_root/data/runtime/content-manifest.json" ]]; then
    cp -- "$project_root/data/runtime/content-manifest.json" "$session_root/content-manifest.json"
fi
cp -- "$project_root/docs/playtests/M3-session-report-template.md" "$session_root/session-report.md"
printf '%s\n' 'No human result has been recorded. Follow docs/playtests/M3-session-guide.md and obtain consent before taking notes.'
if [[ "$(uname -s)" == Darwin ]]; then
    exec /usr/bin/open -n "$game_bundle" --args --portable-root "$session_root/local-game"
fi
exec "$game_binary" --portable-root "$session_root/local-game"
