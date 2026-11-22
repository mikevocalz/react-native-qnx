#!/usr/bin/env bash
# Guard only the process group started for this build; never signal other builds.
set -euo pipefail
exec python3 -I "$(dirname "$0")/guarded-run.py" "$@"
