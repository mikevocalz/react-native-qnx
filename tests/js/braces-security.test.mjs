import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, cpSync, readFileSync, writeFileSync, rmSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {
  advisoryId,
  classifyAudit,
  installedBraces,
  verifyBraces,
} from '../../scripts/lib/braces-security.mjs';

const inventory = spawnSync(
  'pnpm',
  ['list', 'braces', '--recursive', '--depth', 'Infinity', '--json'],
  { encoding: 'utf8', maxBuffer: 16 * 1024 * 1024 }
);
assert.equal(inventory.status, 0, inventory.stderr);
const paths = installedBraces(JSON.parse(inventory.stdout));
for (const packagePath of paths) verifyBraces(packagePath);

// Missing inventories, unexpected versions, and unpatched/tampered copies fail closed.
assert.throws(() => installedBraces([]));
assert.throws(() => installedBraces([{}]));
assert.throws(() =>
  installedBraces([
    {
      dependencies: {
        first: { dependencies: { braces: { version: '3.0.3', path: paths[0] } } },
        second: { dependencies: { braces: { version: '2.3.2', path: '/tmp/old-braces' } } },
      },
    },
  ])
);
assert.throws(() =>
  installedBraces([{ dependencies: { braces: { version: '3.0.4', path: '/tmp/braces' } } }])
);
assert.throws(() => installedBraces([{ dependencies: { braces: { version: '3.0.3' } } }]));
const temp = mkdtempSync(path.join(os.tmpdir(), 'rnqnx-braces-security-'));
try {
  const copy = path.join(temp, 'braces');
  cpSync(paths[0], copy, { recursive: true });
  const file = path.join(copy, 'lib/constants.js');
  writeFileSync(file, readFileSync(file, 'utf8').replace('MAX_DEPTH: 100', 'MAX_DEPTH: 10000'));
  assert.throws(() => verifyBraces(copy), /Unverified braces implementation/);
  const multiple = installedBraces([
    {
      dependencies: {
        first: { dependencies: { braces: { version: '3.0.3', path: paths[0] } } },
        second: { dependencies: { braces: { version: '3.0.3', path: copy } } },
      },
    },
  ]);
  assert.equal(multiple.length, 2);
  assert.throws(() => multiple.forEach(verifyBraces), /Unverified braces implementation/);
  cpSync(paths[0], copy, { recursive: true });
  const manifestPath = path.join(copy, 'package.json');
  const manifest = JSON.parse(readFileSync(manifestPath, 'utf8'));
  manifest.main = 'other.js';
  writeFileSync(manifestPath, JSON.stringify(manifest));
  assert.throws(() => verifyBraces(copy), /Unverified braces implementation: package.json/);
  cpSync(paths[0], copy, { recursive: true });
  rmSync(path.join(copy, 'lib/compile.js'));
  assert.throws(() => verifyBraces(copy), /ENOENT/);
} finally {
  rmSync(temp, { recursive: true, force: true });
}

const bracesFinding = {
  github_advisory_id: advisoryId,
  module_name: 'braces',
  vulnerable_versions: '<=3.0.3',
  severity: 'high',
  patched_versions: null,
  findings: [{ version: '3.0.3', paths: ['.>metro>micromatch>braces'] }],
};
/** @param {any[]} advisories */
const report = (advisories) => ({
  advisories: Object.fromEntries(advisories.map((item, i) => [i, item])),
  metadata: {
    vulnerabilities: { info: 0, low: 0, moderate: 0, high: advisories.length, critical: 0 },
  },
});
assert.deepEqual(classifyAudit(report([bracesFinding])), { blocked: [], mitigated: 1 });
assert.deepEqual(classifyAudit(report([])), { blocked: [], mitigated: 0 });
for (const severity of ['moderate', 'high', 'critical']) {
  const newFinding = { ...bracesFinding, github_advisory_id: 'GHSA-new-advisory', severity };
  assert.deepEqual(classifyAudit(report([bracesFinding, newFinding])), {
    blocked: [newFinding],
    mitigated: 1,
  });
}
for (const mutation of [
  { module_name: 'another-package' },
  { patched_versions: '>=3.0.4' },
  { severity: 'critical' },
  { vulnerable_versions: '<=3.0.4' },
  { findings: [] },
  { findings: [{ version: '3.0.2', paths: ['.>braces'] }] },
  { findings: [{ version: '3.0.3', paths: [] }] },
  { findings: [{ version: '3.0.3', paths: ['.>another-package'] }] },
  { severity: 'unknown' },
]) {
  assert.throws(() => classifyAudit(report([{ ...bracesFinding, ...mutation }])));
}
for (const invalid of [
  null,
  {},
  { error: { code: 'REGISTRY_DOWN' } },
  { advisories: [] },
  { advisories: {} },
  { advisories: 1, metadata: { vulnerabilities: {} } },
  { advisories: {}, metadata: { vulnerabilities: 1 } },
]) {
  assert.throws(() => classifyAudit(invalid));
}
console.log('braces security: patched-source, regression, tamper, and audit-policy checks passed.');
