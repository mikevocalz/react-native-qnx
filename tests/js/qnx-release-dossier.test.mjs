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
  check(empty.receiptCount === 0 && empty.openGates.length === 7);
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
  check(verified.receipts[0].verification === 'integrity-only');
  check(verified.receipts[0].captureComplete === null);
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

  const target = {
    id: 'synthetic-custom-pi400',
    lane: 'qnx-physical',
    board: 'synthetic-RaspberryPi400',
    image: 'synthetic-custom-image',
    runtime: 'synthetic-Hermes-runtime',
    binarySha256: 'c'.repeat(64),
    bundleSha256: 'd'.repeat(64),
  };
  const capturePath = 'artifacts/boot/capture.json';
  const captureLocation = path.join(root, 'build/candidate', capturePath);
  const rawLocation = path.join(root, 'build/candidate/artifacts/boot/raw.log');
  await mkdir(path.dirname(rawLocation));
  await writeFile(rawLocation, data);
  const rawArtifact = { path: 'raw.log', sha256: artifact.sha256 };
  const provenance = {
    sourceCommit: receipt.commit,
    lockfileSha256: receipt.lockSha256,
    ...target,
    targetId: target.id,
    captureMethod: 'Synthetic Node fixture only, not a device observation.',
    limitations: 'No boots or workload measurements were performed.',
  };
  /** @param {number} [count] @returns {Record<string, any>} */
  function boots(count = 20) {
    return {
      schemaVersion: 1,
      kind: 'cold-boot',
      provenance: { ...provenance },
      artifacts: [{ ...rawArtifact }],
      recoveryProcedure: 'Synthetic recovery description; not executed.',
      attempts: Array.from({ length: count }, (_, index) => ({
        sequence: index + 1,
        bootId: `fixture-boot-${index + 1}`,
        startedAt: new Date(Date.UTC(2026, 0, 1, 0, index)).toISOString(),
        completedAt: new Date(Date.UTC(2026, 0, 1, 0, index, 30)).toISOString(),
        reset: 'power-cycle',
        status: 'passed',
        graphicsReady: true,
        appReady: true,
        recoveryAvailable: true,
        log: rawArtifact.path,
        reason: 'Synthetic injected failed/blocked attempt.',
      })),
    };
  }
  /** @returns {Record<string, any>} */
  function performance() {
    return {
      schemaVersion: 1,
      kind: 'performance',
      provenance: { ...provenance },
      artifacts: [{ ...rawArtifact }],
      workloads: Object.fromEntries(
        ['hello-world', 'list-1000', 'drawer-drag'].map((id) => [
          id,
          {
            status: 'recorded',
            clock: 'monotonic',
            frameBoundary: 'presented',
            refreshHz: 50,
            warmupIntervals: 1,
            startupMs: [100, 120],
            frameIntervalsMs: [999, 20, 30, 10],
            rssBytes: [1048576, 2097152],
          },
        ])
      ),
    };
  }
  /** @param {Record<string, any>} capture @param {Record<string, any>} [itemPatch]
   * @param {Record<string, any>} [documentPatch] */
  async function saveCapture(capture, itemPatch = {}, documentPatch = {}) {
    const contents = JSON.stringify(capture);
    await writeFile(captureLocation, contents);
    const item = {
      ...receipt,
      id: 'synthetic-capture',
      gate: capture.kind,
      lane: target.lane,
      board: target.board,
      image: target.image,
      evidence: { kind: capture.kind, target: { ...target }, capture: capturePath },
      artifacts: [
        {
          path: capturePath,
          bytes: Buffer.byteLength(contents),
          sha256: createHash('sha256').update(contents).digest('hex'),
        },
      ],
      ...itemPatch,
    };
    const document = {
      ...original,
      releaseTarget: { ...target },
      receipts: [item],
      ...documentPatch,
    };
    await save(document);
    return document;
  }
  async function inspectCapture() {
    const result = await verifyDossier(root, file);
    check(result.integrityVerified && !result.releaseAuthorized && result.openGates.length === 7);
    const item = /** @type {Record<string, any>} */ (result.receipts[0]);
    check(item.verification === 'candidate-capture-v1');
    return item;
  }
  for (const count of [0, 19, 20]) {
    await saveCapture(boots(count));
    const result = await inspectCapture();
    check(result.captureComplete === (count === 20));
    check(result.qualification.consecutiveFinalPowerCycles === count);
    check(
      result.artifactVerification.verifiedFiles === 1 && !result.qualification.releaseGatePassed
    );
  }
  const incompleteCli = await saveCapture(boots(19));
  const incompleteRun = cli(['verify', '--file', file]);
  check(
    incompleteRun.status === 0 && !JSON.parse(incompleteRun.stdout).receipts[0].captureComplete
  );
  check(
    JSON.stringify(incompleteCli) === JSON.stringify(JSON.parse(await readFile(location, 'utf8')))
  );
  for (const reset of ['reboot', 'app-relaunch']) {
    const capture = boots();
    capture.attempts[19].reset = reset;
    await saveCapture(capture);
    const result = await inspectCapture();
    check(!result.captureComplete && result.qualification.consecutiveFinalPowerCycles === 0);
  }
  for (const status of ['failed', 'blocked']) {
    const capture = boots(21);
    capture.attempts[10].status = status;
    await saveCapture(capture);
    check(!(await inspectCapture()).captureComplete);
    capture.attempts[10].status = 'passed';
    capture.attempts[0].status = status;
    await saveCapture(capture);
    check((await inspectCapture()).captureComplete);
  }
  const invalidBoots = [boots(), boots(), boots()];
  invalidBoots[0].attempts[19].bootId = invalidBoots[0].attempts[0].bootId;
  invalidBoots[1].attempts[1].startedAt = invalidBoots[1].attempts[0].startedAt;
  invalidBoots[2].attempts[0].recoveryAvailable = false;
  for (const capture of invalidBoots) {
    await saveCapture(capture);
    await rejects(() => verifyDossier(root, file), /Duplicate boot|overlap|passing boot/);
  }
  for (const key of [
    'sourceCommit',
    'lockfileSha256',
    'binarySha256',
    'bundleSha256',
    'targetId',
    'lane',
    'board',
    'image',
    'runtime',
  ]) {
    const capture = boots();
    capture.provenance[key] =
      key === 'sourceCommit'
        ? '0'.repeat(40)
        : key.endsWith('Sha256')
          ? '0'.repeat(64)
          : key === 'lane'
            ? 'qnx-vm'
            : 'different-fixture';
    await saveCapture(capture);
    await rejects(() => verifyDossier(root, file), /Capture .*differs/);
  }
  for (const key of Object.keys(target)) {
    const releaseTarget = {
      ...target,
      [key]: key.endsWith('Sha256')
        ? '0'.repeat(64)
        : key === 'lane'
          ? 'qnx-vm'
          : 'synthetic-stock-Pi4',
    };
    await saveCapture(boots(), {}, { releaseTarget });
    const result = await inspectCapture();
    check(
      !result.captureComplete && result.blockers.some((/** @type {string} */ b) => b.includes(key))
    );
    check(result.qualification.captureComplete);
  }
  for (const itemPatch of [
    { board: 'synthetic-stock-Pi4' },
    { image: 'stock-QSTI' },
    { lane: 'qnx-vm' },
  ]) {
    await saveCapture(boots(), itemPatch);
    await rejects(() => verifyDossier(root, file), /differs from receipt/);
  }
  await saveCapture(boots(), {}, { releaseTarget: null });
  check(!(await inspectCapture()).captureComplete);
  await saveCapture(boots(), {
    evidence: { kind: 'cold-boot', target: null, capture: capturePath },
  });
  check(!(await inspectCapture()).captureComplete);
  await saveCapture(boots(), {
    lane: 'qnx-vm',
    evidence: { kind: 'cold-boot', target: null, capture: capturePath },
  });
  await rejects(() => verifyDossier(root, file), /Capture lane\/board\/image differs/);
  for (const outcome of ['passed', 'failed', 'blocked', 'not-run']) {
    await saveCapture(boots(), { outcome });
    check((await inspectCapture()).captureComplete === (outcome === 'passed'));
    await saveCapture(boots(), {
      outcome,
      evidence: { kind: 'cold-boot', target, capture: null },
    });
    const result = await inspectCapture();
    check(
      !result.captureComplete &&
        result.qualification === null &&
        result.artifactVerification === null
    );
    if (outcome !== 'passed') {
      await saveCapture(boots(), {
        outcome,
        artifacts: [],
        evidence: { kind: 'cold-boot', target, capture: null },
      });
      check(!(await inspectCapture()).captureComplete);
    }
  }
  for (const lane of ['qnx-vm', 'marathon-physical', 'linux-offscreen']) {
    const capture = boots();
    capture.provenance.lane = lane === 'linux-offscreen' ? 'host' : lane;
    const nonphysicalTarget = { ...target, lane };
    await saveCapture(
      capture,
      { lane, evidence: { kind: 'cold-boot', target: nonphysicalTarget, capture: capturePath } },
      { releaseTarget: nonphysicalTarget }
    );
    check(!(await inspectCapture()).captureComplete);
  }
  await saveCapture(performance());
  const performanceResult = await inspectCapture();
  check(
    performanceResult.captureComplete && performanceResult.qualification.workloads.length === 3
  );
  check(performanceResult.qualification.workloads[0].frameIntervalsMs.count === 3);
  for (const boundary of ['queued', 'js-callback']) {
    const capture = performance();
    capture.workloads['drawer-drag'].frameBoundary = boundary;
    await saveCapture(capture);
    check(!(await inspectCapture()).captureComplete);
  }
  for (const status of ['blocked', 'not-run']) {
    const capture = performance();
    capture.workloads['list-1000'] = { status, reason: 'Synthetic unavailable instrumentation.' };
    await saveCapture(capture);
    check(!(await inspectCapture()).captureComplete);
  }
  for (const lane of ['host', 'qnx-vm']) {
    const capture = performance();
    capture.provenance.lane = lane;
    const nonphysicalTarget = { ...target, lane: lane === 'host' ? 'linux-offscreen' : lane };
    await saveCapture(
      capture,
      {
        lane: nonphysicalTarget.lane,
        evidence: { kind: 'performance', target: nonphysicalTarget, capture: capturePath },
      },
      { releaseTarget: nonphysicalTarget }
    );
    check(!(await inspectCapture()).captureComplete);
  }
  const badPerformance = performance();
  badPerformance.workloads['hello-world'].startupMs = [-1];
  await saveCapture(badPerformance);
  await rejects(() => verifyDossier(root, file), /positive finite/);
  for (const evidence of [
    { kind: 'performance', target, capture: capturePath },
    { kind: 'cold-boot', target, capture: '../outside.json' },
    { kind: 'cold-boot', target, capture: 'artifacts/unhashed.json' },
    { kind: 'cold-boot', target: { ...target, bundleSha256: 'invalid' }, capture: capturePath },
  ]) {
    await saveCapture(boots(), { evidence });
    await rejects(() => verifyDossier(root, file), /kind|hashed receipt artifact|SHA-256/);
  }
  const wrongKind = boots();
  wrongKind.kind = 'performance';
  await saveCapture(wrongKind, {
    gate: 'cold-boot',
    evidence: { kind: 'cold-boot', target, capture: capturePath },
  });
  await rejects(() => verifyDossier(root, file), /Capture kind/);
  for (const filename of ['../raw.log', '/raw.log', 'a/../raw.log', 'C:\\raw.log', 'missing.log']) {
    const capture = boots();
    capture.artifacts[0].path = filename;
    await saveCapture(capture);
    await rejects(() => verifyDossier(root, file), /confined|ENOENT/);
  }
  const missingLog = boots();
  missingLog.attempts[0].log = 'unhashed.log';
  await saveCapture(missingLog);
  await rejects(() => verifyDossier(root, file), /hashed raw log/);
  for (const artifacts of [
    [],
    [rawArtifact, rawArtifact],
    [{ ...rawArtifact, sha256: '0'.repeat(64) }],
  ]) {
    const capture = boots();
    capture.artifacts = artifacts;
    await saveCapture(capture);
    await rejects(() => verifyDossier(root, file), /1-256|Duplicate artifact|integrity mismatch/);
  }
  await saveCapture(boots());
  await writeFile(captureLocation, '{tampered');
  await rejects(() => verifyDossier(root, file), /integrity mismatch/);
  await saveCapture(boots());
  await writeFile(rawLocation, 'tampered raw capture');
  await rejects(() => verifyDossier(root, file), /integrity mismatch/);
  await rm(rawLocation);
  await symlink(path.join(root, 'package.json'), rawLocation);
  await rejects(() => verifyDossier(root, file), /symlinks/);
  await rm(rawLocation);
  await link(path.join(root, 'package.json'), rawLocation);
  await rejects(() => verifyDossier(root, file), /one link/);
  await rm(rawLocation);
  await writeFile(rawLocation, data);
  const unchanged = await saveCapture(boots());
  const beforeCapture = await readFile(captureLocation);
  const beforeRaw = await readFile(rawLocation);
  await inspectCapture();
  check(JSON.stringify(unchanged) === JSON.stringify(JSON.parse(await readFile(location, 'utf8'))));
  check(
    beforeCapture.equals(await readFile(captureLocation)) &&
      beforeRaw.equals(await readFile(rawLocation))
  );

  await symlink(root, path.join(root, 'build/redirect'));
  await rejects(() => createDossier(root, 'build/redirect/escape.json'), /symlinks/);
  await writeFile(location, '{invalid');
  await rejects(() => verifyDossier(root, file), /JSON|property|Unexpected/);
  await writeFile(location, ' '.repeat(1024 * 1024 + 1));
  await rejects(() => verifyDossier(root, file), /bounded/);
  await save(good);
  const run = cli(['verify', '--file', file]);
  check(run.status === 0 && JSON.parse(run.stdout).openGates.length === 7);
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
  await rejects(async () => candidateSnapshot(root), /seven/);
  console.log(
    `qnx-release-dossier: ${checks} checks passed (synthetic Git and artifacts only; no release qualification).`
  );
} finally {
  await rm(root, { recursive: true, force: true });
}
