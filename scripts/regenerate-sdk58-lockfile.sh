#!/usr/bin/env bash
set -euo pipefail

# Exact dependency pins are proposed in package manifests in this draft PR.
# The lockfile must be regenerated on an npm-connected development/CI machine
# before it is merged. Never hand-edit pnpm-lock.yaml to fake a successful install.
cd "$(dirname "$0")/.."
echo "SDK58 stable/RN0.88 exact-pin upgrade: regenerate and verify pnpm lockfile"

node -e 'const p=require("./package.json"); if(p.devDependencies["react-native"] !== "0.88.1" || p.devDependencies.expo !== "58.0.22") throw Error("wrong upgrade branch");'
corepack pnpm install --lockfile-only
corepack pnpm install --frozen-lockfile
corepack pnpm validate:dependencies
corepack pnpm test:js
corepack pnpm typecheck
corepack pnpm validate:architecture
corepack pnpm audit:dependencies

echo "Review pnpm-lock.yaml, Hermes tags/CMake native source, and GitHub CI BEFORE marking a PR ready."
