#!/usr/bin/env bash
# Reject reused sources, build objects or timing logs in release-qualification mode.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "$here/layout.sh"
log_dir=${STAGE_LOG_DIR:-$QT_PUBLIC_ROOT/logs}

# Downloaded archives/patches may exist; extracted or compiled trees may not.
# -L also rejects dangling symlinks, which -e would otherwise miss.
for path in \
  "$QT_PUBLIC_ROOT/src" \
  "$QT_PUBLIC_ROOT/src-svg" \
  "$QT_PUBLIC_BUILD/host-build" \
  "$QT_PUBLIC_BUILD/qnx-build" \
  "${SVG_BUILD:-$QT_PUBLIC_BUILD/svg-qnx}" \
  "${IMAGEFORMATS_BUILD:-$QT_PUBLIC_BUILD/imageformats-qnx}" \
  "$log_dir/stages.tsv" \
  "$log_dir/disk-samples.tsv" \
  "$log_dir/stage-attempts.tsv" \
  "$log_dir"/attempt-*; do
  if [[ -e "$path" || -L "$path" ]]; then
    printf 'CLEAN RUN REFUSED: pre-existing path: %s\n' "$path" >&2
    printf '%s\n' 'Use a new isolated source/build/log tree; do not delete evidence automatically.' >&2
    exit 3
  fi
done
printf '%s\n' 'Clean-tree preflight passed (no extracted source, build tree or prior timing logs).'
