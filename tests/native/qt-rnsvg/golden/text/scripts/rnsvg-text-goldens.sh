#!/usr/bin/env bash
# Regenerates the PNG goldens for RnsvgTextTest from the SVG sources next to
# them. Run it after editing an SVG; never edit a PNG by hand.
#
#   tests/native/qt-rnsvg/golden/text/scripts/rnsvg-text-goldens.sh
#
# resvg 0.50.0 is the pixel oracle. System fonts are skipped so the output does
# not depend on the machine: only the bundled DejaVu faces are loaded.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
golden="$(cd "$here/.." && pwd)"
fonts="$(cd "$golden/../../fonts" && pwd)"

expected="0.50.0"
actual="$(resvg --version | awk '{print $NF}')"
if [[ "$actual" != "$expected" ]]; then
  echo "resvg $expected is required, found $actual" >&2
  exit 1
fi

for svg in "$golden"/*.svg; do
  png="${svg%.svg}.png"
  resvg --skip-system-fonts \
    --use-font-file "$fonts/DejaVuSans.ttf" \
    --use-font-file "$fonts/DejaVuSans-Bold.ttf" \
    --use-font-file "$fonts/DejaVuSansMono.ttf" \
    --use-font-file "$fonts/DejaVuSerif.ttf" \
    --font-family "DejaVu Sans" \
    --background white \
    "$svg" "$png"
  echo "wrote ${png#"$golden"/}"
done
