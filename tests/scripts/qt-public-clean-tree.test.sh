#!/usr/bin/env bash
set -euo pipefail
script=$(cd "$(dirname "$0")/../../scripts/qt-public" && pwd)/assert-clean-tree.sh
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export QT_PUBLIC_ROOT="$tmp/public" QT_PUBLIC_BUILD="$tmp/build" STAGE_LOG_DIR="$tmp/logs"
mkdir -p "$QT_PUBLIC_ROOT/download" "$QT_PUBLIC_BUILD" "$STAGE_LOG_DIR"
bash "$script" >/dev/null
for path in "$QT_PUBLIC_ROOT/src" "$QT_PUBLIC_ROOT/src-svg" "$QT_PUBLIC_BUILD/host-build" "$QT_PUBLIC_BUILD/qnx-build" "$QT_PUBLIC_BUILD/svg-qnx" "$QT_PUBLIC_BUILD/imageformats-qnx" "$STAGE_LOG_DIR/stages.tsv" "$STAGE_LOG_DIR/disk-samples.tsv"; do
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
printf '%s\n' 'PASS: pristine, 8 stale paths, dangling symlink, restored pristine'
