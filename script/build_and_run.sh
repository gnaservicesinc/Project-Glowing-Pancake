#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
mode="${1:-run}"
case "$mode" in run|--debug|--logs|--verify) ;; *) echo "Usage: $0 [--debug|--logs|--verify]" >&2; exit 2 ;; esac
cd -- "$project_root"
pkill -x 'Project Glowing Pancake' >/dev/null 2>&1 || true
./make.sh --package --jobs 4
bundle="$project_root/dist/Project Glowing Pancake.app"
case "$mode" in
    --debug) exec lldb -- "$bundle/Contents/MacOS/Project Glowing Pancake" ;;
    --logs)
        /usr/bin/open -n "$bundle"
        exec /usr/bin/log stream --info --style compact --predicate 'process == "Project Glowing Pancake"' ;;
    --verify)
        /usr/bin/open -n "$bundle"
        sleep 1
        pgrep -x 'Project Glowing Pancake' >/dev/null
        printf 'Launched %s\n' "$bundle" ;;
    run) /usr/bin/open -n "$bundle" ;;
esac
