#!/bin/bash
# Build one Hermes bytecode bundle per Advanced card for single-card screenshots.
#
#   scripts/vm-proof/build-bundles.sh <out-dir> <hermesc> <card-id>...
#
# Needs build/gallery/app.bundle.js from `bundle-qnx --app component-gallery`
# (plain JavaScript). apps/component-gallery/index.js holds the marker
# __RNQNX_PROOF_CARD__; each bundle gets the marker replaced by one card id, so
# the same native build shows one card as the whole app. Prints the sha256 of
# every bundle.
set -euo pipefail
out=$1
hermesc=$2
shift 2
src=build/gallery/app.bundle.js
[ -f "$src" ] || { echo "missing $src" >&2; exit 1; }
count=$(grep -o '__RNQNX_PROOF_CARD__' "$src" | wc -l)
[ "$count" -ge 1 ] || { echo "marker not found in $src" >&2; exit 1; }
mkdir -p "$out"
for id in "$@"; do
  case "$id" in *[!a-z0-9-]*) echo "bad card id: $id" >&2; exit 1 ;; esac
  sed "s/__RNQNX_PROOF_CARD__/$id/g" "$src" > "$out/proof-$id.js"
  "$hermesc" -O -emit-binary -out "$out/proof-$id.hbc" "$out/proof-$id.js"
  rm "$out/proof-$id.js"
  (cd "$out" && shasum -a 256 "proof-$id.hbc" 2>/dev/null || sha256sum "proof-$id.hbc")
done
