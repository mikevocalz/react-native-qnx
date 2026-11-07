#!/usr/bin/env bash
set -euo pipefail

# scripts/log-qnx.sh
# Streams slog2 output from the configured QNX target.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/qnx-target.sh
source "${SCRIPT_DIR}/lib/qnx-target.sh"

rnqnx_target_init
echo "Streaming slog2info from $(rnqnx_target_address)..."
rnqnx_ssh "slog2info -w"
