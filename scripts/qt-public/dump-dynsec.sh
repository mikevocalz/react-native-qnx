#!/usr/bin/env bash
# Print the SHA-256 and the DT_RPATH / DT_RUNPATH of every Qt library, plugin
# and QML plugin in the build trees, straight from the linker output (before any
# staging). Runs in the build container. With the build-level RPATH fix every
# file reports a DT_RUNPATH of /data/qt612-public/lib and none reports a DT_RPATH.
set -euo pipefail
source "$(dirname "$0")/layout.sh"
source "$QNX_SDP/qnxsdp-env.sh" >/dev/null
for root in "$QT_PUBLIC_BUILD/qnx-build/qtbase" "$QT_PUBLIC_BUILD/qnx-build/qtdeclarative" \
            "$QT_PUBLIC_BUILD/svg-qnx" "$QT_PUBLIC_BUILD/imageformats-qnx"; do
  [ -d "$root" ] || continue
  find "$root" \( -path '*/lib/*.so.6.12.0' -o -path '*/plugins/*/*.so' -o -path '*/qml/*/*.so' \) \
    -type f | sort | while read -r f; do
    printf '%s  %s\n' "$(sha256sum "$f" | cut -d' ' -f1)" "$f"
    ntoaarch64-readelf -d "$f" | grep -E '\((RPATH|RUNPATH)\)' | sed 's/^ *0x[0-9a-f]* //' || true
  done
done
