#!/usr/bin/env bash
# Builds the Stage 5 GPU capability probe for QNX SDP 8.0 aarch64le.
# Run inside the qnx805-build container (or any shell with qnxsdp-env.sh sourced).
#   scripts/webgpu/build-probe.sh [output]   default: build/webgpu/qnx_gpu_probe
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
out="${1:-$root/build/webgpu/qnx_gpu_probe}"
: "${QNX_TARGET:?source qnxsdp-env.sh first}"
mkdir -p "$(dirname "$out")"
qcc -Vgcc_ntoaarch64le -O2 -Wall -Wno-missing-field-initializers -o "$out" \
  "$root/native/webgpu/probe/qnx_gpu_probe.c" -lEGL -lGLESv2 -lscreen
echo "$out"
