import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { link, mkdir, mkdtemp, readFile, realpath, rm, symlink, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { tmpdir } from 'node:os';
import {
  candidateSnapshot,
  createDossier,
  verifyDossier,
} from '../../scripts/qnx-release-dossier.mjs';

const script = path.resolve('scripts/qnx-release-dossier.mjs');
const root = await realpath(await mkdtemp(path.join(tmpdir(), 'rnqnx-dossier-')));
const file = 'build/candidate/dossier.json';
const location = path.join(root, file);
let checks = 0;
/** @param {unknown} condition */
function check(condition) {
  assert.ok(condition);
  checks += 1;
}
/** @param {() => Promise<unknown>} action @param {RegExp} error */
async function rejects(action, error) {
  await assert.rejects(action, error);
  checks += 1;
}
/** @param {string[]} args */
function git(args) {
  const result = spawnSync('git', args, { cwd: root, encoding: 'utf8', timeout: 10000 });
  assert.equal(result.status, 0, result.stderr);
  return result.stdout;
}
/** @param {string[]} args */
function cli(args) {
  return spawnSync(process.execPath, [script, ...args], {
    cwd: root,
    encoding: 'utf8',
    timeout: 15000,
  });
}
/** @param {unknown} value */
async function save(value) {
  await writeFile(location, JSON.stringify(value, null, 2) + '\n');
}
try {
  git(['init', '-q']);
  git(['config', 'user.name', 'Synthetic evidence test']);
  git(['config', 'user.email', 'fixture@example.invalid']);
  await mkdir(path.join(root, 'docs/releases'), { recursive: true });
  const gates = [
    'distribution',
    'ci',
    'cold-boot',
    'device-parity',
    'performance',
    'developer-path',
    'documentation',
    'app-identity',
    'window-chrome',
    'expo',
    'dependencies',
  ];
  const source = {
    schemaVersion: 1,
    gates: gates.map((id) => ({
      id,
      title: id,
      status: 'pending',
      acceptance: 'Independent evidence required.',
      evidence: [],
    })),
  };
  await writeFile(
    path.join(root, 'package.json'),
    '{"name":"react-native-qnx-monorepo","private":true}\n'
  );
  await writeFile(path.join(root, 'pnpm-lock.yaml'), 'lockfileVersion: synthetic-fixture\n');
  await writeFile(path.join(root, 'docs/component-support.json'), '{"schemaVersion":1}\n');
  await writeFile(path.join(root, 'docs/releases/v1-readiness.json'), JSON.stringify(source));
  git(['add', '.']);
  git(['commit', '-qm', 'Synthetic fixture, not device evidence']);
  const before = git(['status', '--porcelain', '--untracked-files=no']);
  const created = await createDossier(root, file);
  check(created.releaseAuthorized === false);
  check(created.candidate === git(['rev-parse', 'HEAD']).trim());
  const empty = await verifyDossier(root, file);
  check(empty.receiptCount === 0 && empty.openGates.length === gates.length);
  check(empty.releaseAuthorized === false && empty.integrityVerified === true);
  await rejects(() => createDossier(root, file), /EEXIST/);
  await rejects(() => createDossier(root, 'outside.json'), /below build/);
  await rejects(() => createDossier(root, 'build/../outside.json'), /confined/);
  await rejects(() => createDossier(root, 'build//bad.json'), /confined/);
  const original = JSON.parse(await readFile(location, 'utf8'));
  await mkdir(path.join(root, 'build/candidate/artifacts'));
  const data = 'Synthetic fixture log only; no native or device execution.\n';
  const artifact = {
    path: 'artifacts/host.log',
    bytes: Buffer.byteLength(data),
    sha256: createHash('sha256').update(data).digest('hex'),
  };
  await writeFile(path.join(root, 'build/candidate/artifacts/host.log'), data);
  const receipt = {
    id: 'synthetic-host-1',
    gate: 'cold-boot',
    lane: 'linux-offscreen',
    outcome: 'passed',
    commit: original.candidate.commit,
    lockSha256: original.candidate.sources.find(
      (/** @type {{path: string}} */ item) => item.path === 'pnpm-lock.yaml'
    ).sha256,
    command: 'synthetic fixture (not executed)',
    environment: 'Node fixture repository',
    limits: 'No physical boot; this is host supporting evidence only.',
    author: 'fixture-author',
    reviewer: null,
    artifacts: [artifact],
  };
  const good = { ...original, receipts: [receipt] };
  await save(good);
  const goodBytes = await readFile(location);
  const verified = await verifyDossier(root, file);
  check(verified.receipts[0].lane === 'linux-offscreen');
  check(verified.openGates.includes('cold-boot') && !verified.releaseAuthorized);
  check(Buffer.compare(goodBytes, await readFile(location)) === 0);
  /** @type {Array<[unknown, RegExp]>} */
  const wrongDocuments = [
    [{ ...good, releaseAuthorized: true }, /cannot authorize/],
    [{ ...good, schemaVersion: 2 }, /cannot authorize/],
    [{ ...good, candidate: { ...good.candidate, commit: '0'.repeat(40) } }, /Candidate/],
    [{ ...good, candidate: { ...good.candidate, sourceTree: '0'.repeat(40) } }, /Candidate/],
    [{ ...good, candidate: { ...good.candidate, gates: [] } }, /Candidate/],
    [{ ...good, receipts: {} }, /128/],
    [{ ...good, receipts: Array(129).fill(receipt) }, /128/],
    [{ ...good, receipts: [receipt, receipt] }, /Duplicate receipt/],
  ];
  for (const [value, pattern] of wrongDocuments) {
    await save(value);
    await rejects(() => verifyDossier(root, file), /** @type {RegExp} */ (pattern));
  }
  /** @type {Array<[Record<string, unknown>, RegExp]>} */
  const wrongReceipts = [
    [{ commit: '0'.repeat(40) }, /candidate/],
    [{ lockSha256: '0'.repeat(64) }, /lockfile/],
    [{ gate: 'made-up' }, /gate/],
    [{ lane: 'device' }, /lane/],
    [{ outcome: 'success' }, /outcome/],
    [{ command: '' }, /command/],
    [{ environment: '' }, /environment/],
    [{ limits: '' }, /limitations/],
    [{ author: '' }, /author/],
    [{ reviewer: 'fixture-author' }, /independent/],
    [{ reviewer: '' }, /reviewer/],
    [{ lane: 'qnx-physical' }, /board/],
    [{ lane: 'marathon-physical', board: 'fixture' }, /image/],
    [{ artifacts: [] }, /needs artifacts/],
    [{ artifacts: [artifact, artifact] }, /Duplicate artifact/],
    [{ artifacts: [{ ...artifact, path: '../escape' }] }, /below dossier/],
    [{ artifacts: [{ ...artifact, path: 'artifacts/../../escape' }] }, /confined/],
    [{ artifacts: [{ ...artifact, sha256: 'x' }] }, /SHA-256/],
    [{ artifacts: [{ ...artifact, sha256: '0'.repeat(64) }] }, /integrity mismatch/],
    [{ artifacts: [{ ...artifact, bytes: 1 }] }, /integrity mismatch/],
    [{ artifacts: [{ ...artifact, bytes: -1 }] }, /byte count/],
    [{ artifacts: [{ ...artifact, bytes: 129 * 1024 * 1024 }] }, /byte count/],
    [{ artifacts: [{ ...artifact, path: 'artifacts/missing.log' }] }, /ENOENT/],
  ];
  for (const [patch, pattern] of wrongReceipts) {
    await save({ ...good, receipts: [{ ...receipt, ...patch }] });
    await rejects(() => verifyDossier(root, file), /** @type {RegExp} */ (pattern));
  }
  await save(good);
  await writeFile(path.join(root, 'build/candidate/artifacts/host.log'), 'altered\n');
  await rejects(() => verifyDossier(root, file), /integrity mismatch/);
  await rm(path.join(root, 'build/candidate/artifacts/host.log'));
  await symlink(
    path.join(root, 'package.json'),
    path.join(root, 'build/candidate/artifacts/host.log')
  );
  await rejects(() => verifyDossier(root, file), /symlinks/);
  await rm(path.join(root, 'build/candidate/artifacts/host.log'));
  await link(
    path.join(root, 'package.json'),
    path.join(root, 'build/candidate/artifacts/host.log')
  );
  await rejects(() => verifyDossier(root, file), /one link/);
  await rm(path.join(root, 'build/candidate/artifacts/host.log'));
  await writeFile(path.join(root, 'build/candidate/artifacts/host.log'), data);
  await symlink(root, path.join(root, 'build/redirect'));
  await rejects(() => createDossier(root, 'build/redirect/escape.json'), /symlinks/);
  await writeFile(location, '{invalid');
  await rejects(() => verifyDossier(root, file), /JSON|property|Unexpected/);
  await writeFile(location, ' '.repeat(1024 * 1024 + 1));
  await rejects(() => verifyDossier(root, file), /bounded/);
  await save(good);
  const run = cli(['verify', '--file', file]);
  check(run.status === 0 && JSON.parse(run.stdout).openGates.length === gates.length);
  for (const args of [
    ['create'],
    ['verify', '--file', file, '--file', file],
    ['verify', '--bogus'],
    ['run', '--file', file],
    ['verify', '--file', file, 'extra'],
  ]) {
    check(cli(args).status !== 0);
  }
  check(cli(['--help']).status === 0);
  check(git(['status', '--porcelain', '--untracked-files=no']) === before);
  await writeFile(path.join(root, 'package.json'), '{"name":"changed"}\n');
  await rejects(async () => candidateSnapshot(root), /Tracked/);
  git(['checkout', '--', 'package.json']);
  git(['commit', '--allow-empty', '-qm', 'different candidate, identical sources']);
  await rejects(() => verifyDossier(root, file), /stale receipts/);
  await writeFile(
    path.join(root, 'docs/releases/v1-readiness.json'),
    JSON.stringify({ ...source, gates: source.gates.slice(1) })
  );
  git(['add', 'docs/releases/v1-readiness.json']);
  git(['commit', '-qm', 'missing gate fixture']);
  await rejects(async () => candidateSnapshot(root), /every required v1 release gate/);
  console.log(
    `qnx-release-dossier: ${checks} checks passed (synthetic Git and artifacts only; no release qualification).`
  );
} finally {
  await rm(root, { recursive: true, force: true });
}
