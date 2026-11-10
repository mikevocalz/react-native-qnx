import assert from 'node:assert/strict';
import { mkdtemp, mkdir, readFile, writeFile, symlink, rm, copyFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { resolve, join } from 'node:path';
import { spawnSync } from 'node:child_process';
import { validateReadiness } from '../../scripts/validate-expo-qnx-readiness.mjs';

const root = resolve(import.meta.dirname, '../..');
/** @type {{requirements: Array<Record<string, any>>, referenceApplication?: unknown}} */
const baseline = JSON.parse(
  await readFile(join(root, 'docs/expo-delivery/qnx-readiness-gates.json'), 'utf8')
);
await validateReadiness(root); // Validate the real inventory, not only fixtures.
const temporary = await mkdtemp(join(tmpdir(), 'expo-qnx-readiness-'));
try {
  await mkdir(join(temporary, 'docs/expo-delivery'), { recursive: true });
  await mkdir(join(temporary, 'scripts'));
  await mkdir(join(temporary, 'evidence'));
  const script = join(temporary, 'scripts/validate-expo-qnx-readiness.mjs');
  await copyFile(join(root, 'scripts/validate-expo-qnx-readiness.mjs'), script);
  await writeFile(
    join(temporary, 'evidence/receipt.md'),
    'Fixture receipt, not QNX qualification.\n'
  );
  await writeFile(join(temporary, 'evidence/empty.md'), ' \n');
  await symlink(join(temporary, 'evidence/receipt.md'), join(temporary, 'evidence/link.md'));
  await symlink(join(temporary, 'evidence'), join(temporary, 'linked-directory'));
  const pending = structuredClone(baseline);
  for (const gate of pending.requirements) {
    gate.status = 'pending';
    gate.evidence = null;
  }
  /** @param {unknown} manifest */
  const save = async (manifest) =>
    writeFile(
      join(temporary, 'docs/expo-delivery/qnx-readiness-gates.json'),
      JSON.stringify(manifest)
    );
  /** @param {...string} args */
  const cli = (...args) =>
    spawnSync(process.execPath, [script, ...args], { cwd: tmpdir(), encoding: 'utf8' });
  /** @param {(manifest: typeof baseline) => void} mutate
   * @param {RegExp} pattern */
  const reject = async (mutate, pattern) => {
    const manifest = structuredClone(pending);
    mutate(manifest);
    await save(manifest);
    await assert.rejects(validateReadiness(temporary), pattern);
  };
  await save(pending);
  assert.equal(cli().status, 0);
  assert.equal(cli('--strict').status, 1);
  assert.equal(cli('--strict', '--include-optional').status, 1);
  for (const args of [['--unknown'], ['--strict', '--strict'], ['--include-optional']]) {
    assert.equal(cli(...args).status, 2);
  }
  const corePassed = structuredClone(pending);
  for (const gate of corePassed.requirements.filter((gate) => !gate.optional)) {
    gate.status = 'passed';
    gate.evidence = 'evidence/receipt.md';
  }
  await save(corePassed);
  assert.equal(cli('--strict').status, 0, 'premium Forge must not block open core');
  assert.equal(cli('--strict', '--include-optional').status, 1);
  const forge = corePassed.requirements.find((gate) => gate.optional);
  assert.ok(forge);
  forge.status = 'passed';
  forge.evidence = 'evidence/receipt.md';
  await save(corePassed);
  assert.equal(cli('--strict', '--include-optional').status, 0);

  await reject((m) => m.requirements.pop(), /retain every/);
  await reject((m) => m.requirements.push(m.requirements[0]), /duplicate/);
  await reject((m) => {
    m.requirements[0].id = 'unknown';
  }, /Unknown gate/);
  await reject((m) => {
    m.requirements[0].optional = true;
  }, /classification/);
  await reject((m) => {
    const optional = m.requirements.find((g) => g.optional);
    assert.ok(optional);
    optional.optional = false;
  }, /classification/);
  for (const value of [undefined, '', '  ', null]) {
    await reject((m) => {
      m.referenceApplication = value;
    }, /referenceApplication/);
  }
  for (const field of ['area', 'implementation', 'acceptance']) {
    await reject((m) => {
      m.requirements[0][field] = ' ';
    }, /missing/);
  }
  await reject((m) => {
    m.requirements[0].status = 'ready';
  }, /invalid status/);
  await reject((m) => {
    m.requirements[0].evidence = 'evidence/receipt.md';
  }, /pending gate/);
  /** @type {Array<[string | null, RegExp]>} */
  const invalidEvidence = [
    [null, /without an evidence path/],
    ['evidence/missing.md', /not found/],
    ['evidence/empty.md', /must not be empty/],
    ['evidence', /must be a file/],
    ['/tmp/receipt.md', /absolute/],
    ['C:\\receipt.md', /absolute/],
    ['evidence\\receipt.md', /portable/],
    ['../outside.md', /escapes/],
    ['.', /escapes/],
    ['evidence/link.md', /symlink/],
    ['linked-directory/receipt.md', /symlink/],
  ];
  for (const [path, pattern] of invalidEvidence) {
    await reject((m) => {
      m.requirements[0].status = 'passed';
      m.requirements[0].evidence = path;
    }, pattern);
  }
  await save(null);
  assert.equal(cli().status, 1);
  await writeFile(join(temporary, 'docs/expo-delivery/qnx-readiness-gates.json'), '{');
  assert.equal(cli().status, 1);
  assert.equal(cli('--typo').status, 2, 'bad arguments fail before reading JSON');
  console.log('Expo QNX readiness regression tests passed.');
} finally {
  await rm(temporary, { recursive: true, force: true });
}
