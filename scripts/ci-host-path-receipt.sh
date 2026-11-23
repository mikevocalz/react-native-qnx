#!/usr/bin/env bash
set -euo pipefail

# scripts/ci-host-path-receipt.sh <out-dir>
# Runs the host half of the developer path and writes a receipt:
#   templates pack -> init -> install -> bundle-qnx -> deploy-bundle (hermesc + push)
# Label: host. The push goes through a local stand-in for ssh and scp that copies
# into <out-dir>/target. Nothing here reaches a QNX VM, a Pi 400 or a board.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:?usage: ci-host-path-receipt.sh <out-dir>}"
APP="ci-receipt"
mkdir -p "${OUT}/target" "${OUT}/bin" "${OUT}/logs"
OUT="$(cd "${OUT}" && pwd)"
cd "${ROOT}"

step() { printf '== %s\n' "$*"; }
timed() {
  local name="$1"
  shift
  local start end
  start="$(date +%s)"
  "$@" >"${OUT}/logs/${name}.log" 2>&1 || {
    echo "FAILED: ${name}; log follows" >&2
    cat "${OUT}/logs/${name}.log" >&2
    return 1
  }
  end="$(date +%s)"
  printf '%s\t%s\n' "${name}" "$((end - start))" >>"${OUT}/steps.tsv"
}

cat >"${OUT}/bin/ssh" <<'STUB'
#!/bin/bash
eval "${@: -1}"
STUB
cat >"${OUT}/bin/scp" <<'STUB'
#!/bin/bash
src="${@: -2:1}"; dst="${@: -1}"; cp "$src" "${dst#*:}"
STUB
chmod +x "${OUT}/bin/ssh" "${OUT}/bin/scp"
: >"${OUT}/steps.tsv"

step "template tarballs"
timed templates-pack pnpm qnx templates pack --out "${OUT}/templates"
step "init"
timed init pnpm qnx init "${APP}" --template qnx
step "install"
timed install pnpm install --no-frozen-lockfile
step "bundle"
timed bundle pnpm qnx bundle-qnx --app "${APP}" --out "build/${APP}.bundle.js"
step "hermesc and push (local stand-in for ssh)"
PATH="${OUT}/bin:${PATH}" QNX_TARGET_DIR="${OUT}/target" QNX_TARGET_IP=localhost \
  QNX_TARGET_USER=ci timed deploy-bundle pnpm qnx deploy-bundle --app "${APP}"

sha() { openssl dgst -sha256 "$1" | awk '{print $NF}'; }
HBC="${ROOT}/build/${APP}.hbc"
PUSHED="${OUT}/target/bundles/${APP}.hbc"
test -s "${HBC}"
test -s "${PUSHED}"
[[ "$(sha "${HBC}")" == "$(sha "${PUSHED}")" ]]

node - "${OUT}" "${ROOT}" "${APP}" <<'NODE'
const fs = require('node:fs');
const path = require('node:path');
const [out, root, app] = process.argv.slice(2);
const manifest = JSON.parse(fs.readFileSync(path.join(out, 'templates', 'templates-manifest.json'), 'utf8'));
const lock = JSON.parse(fs.readFileSync(path.join(root, 'template', 'templates.lock.json'), 'utf8'));
for (const entry of manifest.templates) {
  if (lock.templates[entry.template].contentSha256 !== entry.contentSha256) process.exit(1);
}
const crypto = require('node:crypto');
const sha = (file) => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const steps = Object.fromEntries(
  fs.readFileSync(path.join(out, 'steps.tsv'), 'utf8').trim().split('\n').map((line) => {
    const [name, seconds] = line.split('\t');
    return [name, Number(seconds)];
  })
);
const receipt = {
  schemaVersion: 1,
  label: 'host',
  runner: process.env.RUNNER_OS ?? process.platform,
  commit: process.env.GITHUB_SHA ?? null,
  templates: manifest.templates.map(({ template, version, contentSha256, tarballSha256 }) => ({
    template,
    version,
    contentSha256,
    tarballSha256,
  })),
  app,
  bundleSha256: sha(path.join(root, 'build', `${app}.bundle.js`)),
  bytecodeSha256: sha(path.join(root, 'build', `${app}.hbc`)),
  pushedBytecodeSha256: sha(path.join(out, 'target', 'bundles', `${app}.hbc`)),
  stepSeconds: steps,
  notRun: [
    'QNX native build',
    'QNX VM run',
    'Pi 400 run',
    'release board run',
    'ssh to a real target (a local stand-in copied the file)',
    'first-time licensed developer run',
  ],
};
fs.writeFileSync(path.join(out, 'receipt.json'), JSON.stringify(receipt, null, 2) + '\n');
console.log(JSON.stringify(receipt, null, 2));
NODE
