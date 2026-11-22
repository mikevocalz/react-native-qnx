#!/usr/bin/env bash
# One clean pass: extract, patch, host tools, Qt core for QNX aarch64le, qtsvg,
# qtimageformats. Times every stage and samples the disk use of the work
# directory, so a receipt can quote wall time and peak size from this log
# instead of from memory. Run inside a Linux x86_64 container that has QNX SDP
# 8.0.5 at $QNX_SDP (read-only is enough), the packages in README.md, and
# cmake 3.31.x on PATH.
#
# Environment (all required except the ones with defaults):
#   QT_PUBLIC_ROOT   archive, cve-patches, source tree, staged runtime
#   QT_PUBLIC_BUILD  host-build, qnx-build, svg-qnx, imageformats-qnx
#   QNX_SDP          directory holding qnxsdp-env.sh
#   WORK_DIR         directory measured for peak use (default: parent of QT_PUBLIC_ROOT)
#   STAGE_LOG_DIR    where per-stage logs and stages.tsv go (default: $QT_PUBLIC_ROOT/logs)
#   JOBS (passed to every build, scripts default 2 or 3), MIN_KB (default 4194304) for the free-space guard
#   GUARD_PATH       mount whose free space the guard watches (default: $WORK_DIR)
set -uo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "$here/layout.sh"
source "$here/qnx-patch-set.sh"
# Reject stale final-pass inputs before creating any attempt or timing logs.
if [[ "${REQUIRE_CLEAN:-0}" == 1 ]]; then
  bash "$here/assert-clean-tree.sh" || exit $?
fi
WORK_DIR=${WORK_DIR:-$(dirname "$QT_PUBLIC_ROOT")}
STAGE_LOG_DIR=${STAGE_LOG_DIR:-$QT_PUBLIC_ROOT/logs}
export GUARD_PATH=${GUARD_PATH:-$WORK_DIR} MIN_KB=${MIN_KB:-4194304}
mkdir -p "$STAGE_LOG_DIR"
# Atomic unique attempt directory: a second ONLY run must not truncate evidence.
attempt_dir=$(mktemp -d "$STAGE_LOG_DIR/attempt-XXXXXXXX") || exit $?
tsv=$STAGE_LOG_DIR/stages.tsv
attempt_tsv=$STAGE_LOG_DIR/stage-attempts.tsv
[ -e "$attempt_tsv" ] || printf 'stage\tstart_utc\tend_utc\twall_s\texit\tstage_log\tbuild_log\n' > "$attempt_tsv"
disk=$STAGE_LOG_DIR/disk-samples.tsv
[ -e "$tsv" ] || printf 'stage\tstart_utc\tend_utc\twall_s\texit\twork_dir_kib_after\tfree_kib_after\n' > "$tsv"
[ -e "$disk" ] || printf 'utc\twork_dir_kib\tfree_kib\n' > "$disk"

# Keep the timer inside Bash: killing the sampler cannot orphan a long sleep.
sampler_fifo=$attempt_dir/.sampler-wait
mkfifo "$sampler_fifo" || exit $?
exec 9<>"$sampler_fifo" || { rm -f "$sampler_fifo"; exit 1; }
rm "$sampler_fifo" || exit $?
sampler() {
  while :; do
    printf '%s\t%s\t%s\n' "$(date -u +%FT%TZ)" "$(du -sk "$WORK_DIR" 2>/dev/null | cut -f1)" \
      "$(df --output=avail -k "$GUARD_PATH" | tail -1 | tr -d ' ')" >> "$disk"
    IFS= read -r -t "${SAMPLE_S:-60}" -u 9 || :
  done
}
sampler >/dev/null 2>&1 & samp=$!
stop_sampler() {
  kill -TERM "$samp" 2>/dev/null || :
  wait "$samp" 2>/dev/null || :
  exec 9>&-
}
trap stop_sampler EXIT

stage() { # name command...
  local name=$1; shift
  local s e t0 t1 rc stage_log build_log
  stage_log=$attempt_dir/$name.log
  build_log=-
  if [ "$name" = host-tools ]; then build_log=$attempt_dir/host-build.log; fi
  if [ "$name" = qnx-core ]; then build_log=$attempt_dir/qnx-build.log; fi
  s=$(date -u +%FT%TZ); t0=$(date +%s)
  echo "=== $name start $s"
  "$@" 9>&- > "$stage_log" 2>&1; rc=$?
  e=$(date -u +%FT%TZ); t1=$(date +%s)
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$name" "$s" "$e" $((t1 - t0)) "$rc" \
    "$(du -sk "$WORK_DIR" 2>/dev/null | cut -f1)" \
    "$(df --output=avail -k "$GUARD_PATH" | tail -1 | tr -d ' ')" >> "$tsv"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$name" "$s" "$e" "$((t1 - t0))" "$rc" "$stage_log" "$build_log" >> "$attempt_tsv"
  echo "=== $name exit=$rc wall=$((t1 - t0))s log=$stage_log"
  [ "$rc" -eq 0 ] || exit "$rc"
}

only=${ONLY:-extract cve qnxpatch host qnx svg imageformats}
for st in $only; do
  case $st in
    extract)      stage extract "$here/extract-source.sh" ;;
    cve)          stage cve-patches "$here/apply-cve-patches.sh" ;;
    qnxpatch)     stage qnx-patches "$here/apply-qnx-patches.sh" ;;
    host)         stage host-tools "$here/guarded-run.sh" "$here/configure-host.sh" "$attempt_dir/host-build.log" ;;
    qnx)          stage qnx-core "$here/guarded-run.sh" "$here/configure-qnx.sh" "$attempt_dir/qnx-build.log" ;;
    svg)          stage qtsvg "$here/build-svg-qnx.sh" ;;
    imageformats) stage qtimageformats "$here/build-imageformats-qnx.sh" ;;
    *) echo "unknown stage $st" >&2; exit 2 ;;
  esac
done
