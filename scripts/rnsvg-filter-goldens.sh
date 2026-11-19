#!/usr/bin/env bash
# Regenerates the filter goldens: one PNG per SVG in
# tests/native/qt-rnsvg/golden/filter, rendered by resvg on a white
# background. The PNGs are never edited by hand; change the SVG and rerun.
#
# resvg 0.50.0 produced the committed images. Install it with
# `brew install resvg` or `cargo install resvg --version 0.50.0`. The script
# stops when another version is found, because a different rasteriser version
# changes the pixels the tests compare against.
set -euo pipefail

EXPECTED_RESVG="0.50.0"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dir="$root/tests/native/qt-rnsvg/golden/filter"

if ! command -v resvg >/dev/null 2>&1; then
  echo "resvg not found; install resvg $EXPECTED_RESVG" >&2
  exit 1
fi
found="$(resvg --version | awk '{print $NF}')"
if [[ "$found" != "$EXPECTED_RESVG" && "${RNSVG_GOLDEN_ANY_RESVG:-0}" != "1" ]]; then
  echo "resvg $found found, goldens are defined by $EXPECTED_RESVG (set RNSVG_GOLDEN_ANY_RESVG=1 to override)" >&2
  exit 1
fi

count=0
for svg in "$dir"/*.svg; do
  resvg --background white "$svg" "${svg%.svg}.png" 2> >(grep -v '^Warning' >&2 || true)
  count=$((count + 1))
done
echo "rendered $count goldens with resvg $found"
