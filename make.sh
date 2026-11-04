#!/usr/bin/env bash
# Keep the public entry point small; build orchestration lives beside other tools.
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
exec bash "$project_root/tools/build.sh" "$@"
