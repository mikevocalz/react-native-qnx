#!/usr/bin/env node

import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import {
  advisoryId,
  classifyAudit,
  installedBraces,
  verifyBraces,
} from './lib/braces-security.mjs';

/** @param {string[]} args */
function pnpm(args) {
  const result = spawnSync('pnpm', args, { encoding: 'utf8', maxBuffer: 16 * 1024 * 1024 });
  if (result.error) throw result.error;
  assert.equal(result.signal, null, 'pnpm terminated unexpectedly');
  if (result.stderr) process.stderr.write(result.stderr);
  return result;
}

try {
  const inventory = pnpm(['list', 'braces', '--recursive', '--depth', 'Infinity', '--json']);
  assert.equal(inventory.status, 0, 'Could not inspect installed dependencies');
  const paths = installedBraces(JSON.parse(inventory.stdout));
  for (const packagePath of paths) verifyBraces(packagePath);
  console.log(
    `Verified patched source and security regressions for ${paths.length} installed braces package(s).`
  );

  // Do not add this GHSA to audit.ignore: retain the full registry finding.
  const audit = pnpm(['audit', '--audit-level', 'moderate', '--json']);
  process.stdout.write(audit.stdout);
  assert.ok(audit.status === 0 || audit.status === 1, 'Registry audit failed');
  const { blocked, mitigated } = classifyAudit(JSON.parse(audit.stdout));
  if (mitigated) {
    console.log(
      `${advisoryId}: mitigated by verified local backport; registry still reports braces@3.0.3. See docs/security/braces.md.`
    );
  }
  assert.ok(
    audit.status === 0 || blocked.length > 0 || mitigated > 0,
    'Unexplained registry audit failure'
  );
  assert.equal(blocked.length, 0, 'Unmitigated moderate-or-higher advisories remain');
} catch (error) {
  console.error(error);
  process.exitCode = 1;
}
