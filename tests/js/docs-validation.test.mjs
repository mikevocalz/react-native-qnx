import assert from 'node:assert/strict';
import { cp, mkdtemp, readFile, writeFile, symlink, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { syncDocs } from '../../scripts/lib/docs-model.mjs';

const root = process.cwd();
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-docs-'));
try {
  for (const file of [
    'package.json',
    '.project-state.json',
    'README.md',
    'docs',
    'LICENSE',
    'THIRD_PARTY_NOTICES.md',
    'SECURITY.md',
    'CODE_OF_CONDUCT.md',
    'CONTRIBUTING.md',
  ]) {
    await cp(path.join(root, file), path.join(temporary, file), { recursive: true });
  }
  await symlink(path.join(root, 'node_modules'), path.join(temporary, 'node_modules'), 'dir');
  const baseline = await syncDocs(temporary);
  assert.equal(baseline.components, 22);
  assert.equal(baseline.gates, 7);
  await assert.rejects(
    syncDocs(temporary, false, true),
    /v1 is not release-ready:[\s\S]*distribution[\s\S]*cold-boot[\s\S]*performance/
  );
  const stateFile = path.join(temporary, '.project-state.json');
  const state = await readFile(stateFile, 'utf8');
  await writeFile(stateFile, state.replace('0.88.1', '0.88.0-rc.3'));
  await assert.rejects(syncDocs(temporary), /project-state.json is stale/);
  await syncDocs(temporary, true);
  assert.equal(await readFile(stateFile, 'utf8'), state);
  const manifestFile = path.join(temporary, 'package.json');
  const manifest = await readFile(manifestFile, 'utf8');
  await writeFile(manifestFile, manifest.replace('"react": "19.3.0"', '"react": "0.0.0"'));
  await assert.rejects(
    syncDocs(temporary, true),
    /react: installed .* disagrees with package.json/
  );
  assert.equal(
    await readFile(stateFile, 'utf8'),
    state,
    'drift must fail before rewriting records'
  );
  await writeFile(manifestFile, manifest);
  const supportFile = path.join(temporary, 'docs/component-support.json');
  const support = await readFile(supportFile, 'utf8');
  const invalid = JSON.parse(support);
  invalid.components[0].evidence.pi400 = [];
  await writeFile(supportFile, JSON.stringify(invalid));
  await assert.rejects(syncDocs(temporary), /Device result requires evidence/);
  invalid.components[0] = JSON.parse(support).components[0];
  invalid.components[0].pi4Qsti = 'scoped';
  await writeFile(supportFile, JSON.stringify(invalid));
  await assert.rejects(syncDocs(temporary), /Device result requires evidence: View\/pi4Qsti/);
  invalid.components[0] = JSON.parse(support).components[0];
  invalid.components.push(invalid.components[0]);
  await writeFile(supportFile, JSON.stringify(invalid));
  await assert.rejects(syncDocs(temporary), /duplicate component/);
  await writeFile(supportFile, support);
  const gateFile = path.join(temporary, 'docs/releases/v1-readiness.json');
  const gates = await readFile(gateFile, 'utf8');
  const unproven = JSON.parse(gates);
  unproven.gates[0].status = 'passed';
  await writeFile(gateFile, JSON.stringify(unproven));
  await assert.rejects(syncDocs(temporary), /Passed release gate requires evidence/);
  const incomplete = JSON.parse(gates);
  incomplete.gates.pop();
  await writeFile(gateFile, JSON.stringify(incomplete));
  await assert.rejects(syncDocs(temporary), /retain every required release gate/);
  await writeFile(gateFile, gates);
  const readmeFile = path.join(temporary, 'README.md');
  const readme = await readFile(readmeFile, 'utf8');
  await writeFile(readmeFile, readme.replace('generated:dependencies:end', 'missing-marker'));
  await assert.rejects(syncDocs(temporary), /Missing or duplicate generated markers/);
  await writeFile(readmeFile, readme);
  console.log(
    'docs-validation.test.mjs passed (stale pins, installed drift, evidence requirements, duplicate components, markers and release blockers)'
  );
} finally {
  await rm(temporary, { recursive: true, force: true });
}
