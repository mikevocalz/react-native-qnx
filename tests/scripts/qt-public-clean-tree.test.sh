#!/usr/bin/env bash
set -euo pipefail
script=$(cd "$(dirname "$0")/../../scripts/qt-public" && pwd)/assert-clean-tree.sh
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export QT_PUBLIC_ROOT="$tmp/public" QT_PUBLIC_BUILD="$tmp/build" STAGE_LOG_DIR="$tmp/logs"
mkdir -p "$QT_PUBLIC_ROOT/download" "$QT_PUBLIC_BUILD" "$STAGE_LOG_DIR"
bash "$script" >/dev/null
for path in "$QT_PUBLIC_ROOT/src" "$QT_PUBLIC_ROOT/src-svg" "$QT_PUBLIC_BUILD/host-build" "$QT_PUBLIC_BUILD/qnx-build" "$QT_PUBLIC_BUILD/svg-qnx" "$QT_PUBLIC_BUILD/imageformats-qnx" "$STAGE_LOG_DIR/stages.tsv" "$STAGE_LOG_DIR/disk-samples.tsv" "$STAGE_LOG_DIR/stage-attempts.tsv" "$STAGE_LOG_DIR/attempt-previous"; do
  mkdir -p "$(dirname "$path")"
  touch "$path"
  if bash "$script" >"$tmp/output" 2>&1; then echo "FAIL: accepted $path" >&2; exit 1; fi
  grep -Fq "$path" "$tmp/output"
  rm "$path"
done
ln -s "$tmp/missing" "$QT_PUBLIC_ROOT/src"
if bash "$script" >"$tmp/output" 2>&1; then echo 'FAIL: accepted dangling symlink' >&2; exit 1; fi
rm "$QT_PUBLIC_ROOT/src"
bash "$script" >/dev/null
# Exercise the caller too: it does not enable errexit, so it must propagate
# preflight failure explicitly before starting a stage or creating timing logs.
touch "$QT_PUBLIC_ROOT/src"
if REQUIRE_CLEAN=1 ONLY=unexpected bash "${script%/*}/single-pass-build.sh" >"$tmp/output" 2>&1; then
  echo 'FAIL: orchestrator accepted reused source' >&2; exit 1
fi
grep -Fq "$QT_PUBLIC_ROOT/src" "$tmp/output"
test ! -e "$STAGE_LOG_DIR/stages.tsv"
test ! -e "$STAGE_LOG_DIR/disk-samples.tsv"
if grep -q 'unknown stage\|=== .* start' "$tmp/output"; then
  echo 'FAIL: orchestrator continued after failed preflight' >&2; exit 1
fi
rm "$QT_PUBLIC_ROOT/src"
printf '%s\n' 'PASS: pristine, 10 stale paths including attempt evidence, dangling symlink, restored pristine'
