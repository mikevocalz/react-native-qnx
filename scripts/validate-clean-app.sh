#!/usr/bin/env bash
set -euo pipefail

# Exercise the committed source tree in a new directory. The optional renderer
# is a previously built LINUX host binary, never a physical QNX target.
if [[ "${1:-}" == "--help" ]]; then
  echo 'Usage: bash scripts/validate-clean-app.sh [--renderer <Linux-host-binary>]'
  exit 0
fi
RENDERER=""
if (($#)); then
  if [[ "$#" != 2 || "$1" != --renderer ]]; then
    echo 'Use --help for the supported arguments.' >&2
    exit 2
  fi
  RENDERER="$(realpath "$2")"
  if [[ ! -f "$RENDERER" || ! -x "$RENDERER" ]]; then
    echo 'The supplied Linux host renderer must be an executable file.' >&2
    exit 2
  fi
fi
if [[ "$(uname -s)" != Linux ]]; then
  echo 'This reproducible source-checkout test requires Linux and GNU timeout.' >&2
  exit 2
fi
for tool in git tar timeout pnpm node sha256sum; do
  command -v "$tool" >/dev/null || { echo "Missing prerequisite: $tool" >&2; exit 2; }
done
if [[ "$(pnpm --version)" != 12.8.1 ]] || ! node -e 'process.exit(Number(process.versions.node.split(".")[0]) >= 26 ? 0 : 1)'; then
  echo 'Use the repository Node 26+ and pnpm 12.8.1 prerequisites.' >&2
  exit 2
fi
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
git diff --quiet HEAD -- || { echo 'Commit tracked changes before testing the archived candidate.' >&2; exit 2; }
COMMIT="$(git rev-parse HEAD)"
if [[ -L build ]]; then
  echo 'Refusing a symlinked build directory.' >&2
  exit 2
fi
mkdir -p build
EVIDENCE="$(mktemp -d "$ROOT/build/clean-app-evidence.XXXXXX")"
WORK=""
cleanup() {
  local rc=$?
  trap - EXIT INT TERM
  printf '%s\n' "$rc" > "$EVIDENCE/exit-code.txt"
  if [[ -n "$WORK" ]]; then rm -rf -- "$WORK"; fi
  echo "Clean source-checkout evidence: $EVIDENCE (exit $rc)"
  exit "$rc"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
WORK="$(mktemp -d "${TMPDIR:-/tmp}/rnqnx-clean-app.XXXXXX")"
printf '%s\n' "$COMMIT" > "$EVIDENCE/commit.txt"
sha256sum pnpm-lock.yaml > "$EVIDENCE/source-lockfile.sha256"
node --version > "$EVIDENCE/node-version.txt"
pnpm --version > "$EVIDENCE/pnpm-version.txt"
# git archive includes only the committed tree, never local node_modules,
# untracked outputs, an SSH key or a private SDK installed outside the repo.
git archive --format=tar "$COMMIT" | tar -xf - -C "$WORK"
if [[ -e "$WORK/node_modules" ]]; then
  echo 'The source archive unexpectedly contains node_modules.' >&2
  exit 1
fi
run() {
  local name="$1"
  shift
  echo "Clean source checkout: $name"
  (
    cd "$WORK"
    timeout --signal=TERM --kill-after=5s 180s "$@"
  ) > "$EVIDENCE/$name.log" 2>&1 || {
    local rc=$?
    cat "$EVIDENCE/$name.log" >&2
    return "$rc"
  }
}
run 01-frozen-install pnpm install --frozen-lockfile
run 02-create pnpm qnx init dx-smoke
# The initial install populated the content store. Add only the new workspace
# importer offline; retain every pre-existing dependency pin.
run 03-link pnpm install --offline --no-frozen-lockfile
run 04-dependencies pnpm validate:dependencies
run 05-bundle pnpm qnx bundle-qnx --app dx-smoke --out build/dx-smoke.bundle.js
run 06-expo-inspect pnpm expo:qnx inspect --project apps/dx-smoke
run 07-expo-prepare pnpm expo:qnx prepare --project apps/dx-smoke
run 08-expo-check pnpm expo:qnx prepare --project apps/dx-smoke --check
run 09-device-plan pnpm qnx:device status --dry-run
run 10-evidence-plan pnpm qnx evidence verify --dry-run
# A second creation must fail, preserving the generated source byte-for-byte.
cp "$WORK/apps/dx-smoke/index.js" "$EVIDENCE/generated-entry.js"
if run 11-reject-overwrite pnpm qnx init dx-smoke; then
  echo 'Creating an existing app must fail without overwriting it.' >&2
  exit 1
fi
grep -F 'already exists' "$EVIDENCE/11-reject-overwrite.log" >/dev/null
cmp "$EVIDENCE/generated-entry.js" "$WORK/apps/dx-smoke/index.js"
# Tooling success must not make the intentionally pending release gates pass.
if run 12-release-remains-blocked pnpm validate:v1; then
  echo 'The unqualified candidate unexpectedly passed the v1 release gate.' >&2
  exit 1
fi
grep -F 'v1 is not release-ready' "$EVIDENCE/12-release-remains-blocked.log" >/dev/null
cp "$WORK/build/dx-smoke.bundle.js" "$EVIDENCE/generated-app.bundle.js"
cp "$WORK/apps/dx-smoke/package.json" "$EVIDENCE/generated-package.json"
cp "$WORK/apps/dx-smoke/build/expo-qnx/manifest.json" "$EVIDENCE/expo-prepare-manifest.json"
sha256sum "$WORK/pnpm-lock.yaml" | cut -d ' ' -f 1 > "$EVIDENCE/generated-lockfile-sha256.txt"
if [[ -n "$RENDERER" ]]; then
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RHI_BACKEND=software \
    timeout --signal=TERM --kill-after=5s 30s "$RENDERER" \
    --bundle "$EVIDENCE/generated-app.bundle.js" \
    --screenshot "$EVIDENCE/generated-app.png" \
    > "$EVIDENCE/13-host-render.log" 2>&1
  grep -F 'RNQNX_PLAYGROUND_SCREENSHOT' "$EVIDENCE/13-host-render.log" >/dev/null
  test -s "$EVIDENCE/generated-app.png"
  sha256sum "$RENDERER" > "$EVIDENCE/renderer.sha256"
fi
sha256sum --check "$EVIDENCE/source-lockfile.sha256" > "$EVIDENCE/source-lockfile-unchanged.log"
sha256sum "$EVIDENCE/generated-app.bundle.js" "$EVIDENCE/generated-entry.js" \
  "$EVIDENCE/expo-prepare-manifest.json" > "$EVIDENCE/outputs.sha256"
if [[ -n "$RENDERER" ]]; then sha256sum "$EVIDENCE/generated-app.png" >> "$EVIDENCE/outputs.sha256"; fi
printf '%s\n' \
  'RNQNX_CLEAN_SOURCE_APP_OK create install Expo-prepare bundle dry-runs overwrite-refusal' \
  'Evidence lane: Linux host. No QNX device, clean-machine toolchain, standalone package or release pass.' \
  > "$EVIDENCE/result.txt"
cat "$EVIDENCE/result.txt"
