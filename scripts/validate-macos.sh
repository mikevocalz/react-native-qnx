#!/usr/bin/env bash
set -euo pipefail

# scripts/validate-macos.sh
# Runs all macOS-side validation without requiring the QNX SDK.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "[macOS host] Validating react-native-qnx..."

cd "${PROJECT_ROOT}"

if [[ ! -d "node_modules" ]]; then
  echo "Installing Node dependencies..."
  pnpm install
fi

echo ""
echo "[1/8] Format check"
pnpm format:check

echo ""
echo "[2/8] TypeScript type check"
pnpm typecheck

echo ""
echo "[3/8] Installed dependency and source-contract validation"
pnpm validate:dependencies

echo ""
echo "[4/8] JavaScript tests"
pnpm test:js

echo ""
echo "[5/8] C++ host tests"
pnpm test:cpp:host

echo ""
echo "[6/8] Package build and manifest validation"
pnpm build:packages
pnpm validate:manifests

echo ""
echo "[7/8] Repository architecture validation"
pnpm validate:architecture

echo ""
echo "[8/8] Nitro code-generation applicability check"
pnpm codegen:check

echo ""
echo "========================================"
echo "macOS host validation passed."
echo "QNX cross-compilation has not been performed."
echo "QNX runtime behavior remains unverified."
echo "========================================"
