#!/usr/bin/env node
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { constants } from 'node:fs';
import { lstat, mkdir, open, realpath } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { isDeepStrictEqual, parseArgs } from 'node:util';
import { qualify, verifyArtifacts } from './lib/qualification.mjs';

/** @typedef {{id: string, lane: string, board: string, image: string, runtime: string,
 * binarySha256: string, bundleSha256: string}} TargetIdentity
 * @typedef {{kind: 'cold-boot' | 'performance', target: TargetIdentity | null,
 * capture: string | null}} CaptureEvidence
 */

const SOURCE_FILES = [
  'package.json',
  'pnpm-lock.yaml',
  'docs/component-support.json',
  'docs/releases/v1-readiness.json',
];
const GATES = [
  'distribution',
  'ci',
  'cold-boot',
  'device-parity',
  'performance',
  'developer-path',
  'documentation',
];
const LANES = [
  'hosted-ci',
  'source-review',
  'clean-host',
  'linux-offscreen',
  'linux-wayland',
  'qnx-cross-build',
  'qnx-vm',
  'qnx-physical',
  'marathon-shell',
  'marathon-physical',
];
const MAX_JSON = 1024 * 1024;
const MAX_ARTIFACT = 128 * 1024 * 1024;
const MAX_TOTAL = 512 * 1024 * 1024;
/** @type {(keyof TargetIdentity)[]} */
const TARGET_FIELDS = ['id', 'lane', 'board', 'image', 'runtime', 'binarySha256', 'bundleSha256'];
const help = `React Native QNX candidate evidence dossier

  create --file build/release-candidate/dossier.json
  verify --file build/release-candidate/dossier.json

Run from the repository root with clean TRACKED files. create writes one NEW
JSON file using committed HEAD; no evidence is invented or copied automatically.
Add receipts and their relative artifact paths explicitly. verify is read-only.
Exit 0: created / integrity verified; exit 1: invalid data or failed integrity.
Typed capture completeness is reported separately; incomplete captures can exit 0.
Neither exit status authorizes release. The existing pnpm validate:v1 gate
remains authoritative. No device, network, build, flash or publication occurs.
`;

/** @param {string | Buffer} value */
const hash = (value) => createHash('sha256').update(value).digest('hex');
/** @param {unknown} value @param {string} label @returns {Record<string, any>} */
function record(value, label) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error(`${label} must be an object.`);
  }
  return /** @type {Record<string, any>} */ (value);
}
/** @param {unknown} value @param {string} label @param {number} [maximum] */
function text(value, label, maximum = 4096) {
  if (
    typeof value !== 'string' ||
    !value.trim() ||
    value.length > maximum ||
    /[\x00-\x1f]/.test(value)
  ) {
    throw new Error(`Invalid ${label}.`);
  }
  return value;
}
/** @param {string} root @param {string[]} args */
function git(root, args) {
  const result = spawnSync('git', ['--no-optional-locks', '--no-replace-objects', ...args], {
    cwd: root,
    encoding: 'utf8',
    timeout: 10000,
    maxBuffer: 16 * 1024 * 1024,
    shell: false,
  });
  if (result.error || result.status !== 0) {
    throw new Error(`Git inspection failed: ${result.error?.message ?? result.stderr.trim()}`);
  }
  return result.stdout;
}
/** @param {string} root */
export function candidateSnapshot(root) {
  if (path.resolve(git(root, ['rev-parse', '--show-toplevel']).trim()) !== path.resolve(root)) {
    throw new Error('Run from the repository root.');
  }
  if (git(root, ['status', '--porcelain', '--untracked-files=no']).trim()) {
    throw new Error('Tracked source changes must be committed before freezing a candidate.');
  }
  const commit = git(root, ['rev-parse', '--verify', 'HEAD']).trim();
  if (!/^[a-f0-9]{40}$/.test(commit)) throw new Error('Expected a full Git SHA-1 commit identity.');
  const sourceTree = git(root, ['rev-parse', `${commit}^{tree}`]).trim();
  const sources = SOURCE_FILES.map((file) => {
    const contents = git(root, ['show', `${commit}:${file}`]);
    return { path: file, sha256: hash(contents), bytes: Buffer.byteLength(contents) };
  });
  const readiness = record(
    JSON.parse(git(root, ['show', `${commit}:${SOURCE_FILES[3]}`])),
    'Readiness'
  );
  if (
    readiness.schemaVersion !== 1 ||
    !Array.isArray(readiness.gates) ||
    readiness.gates.length !== GATES.length
  ) {
    throw new Error('Expected the existing seven v1 release gates.');
  }
  const ids = new Set();
  const gates = /** @type {unknown[]} */ (readiness.gates).map((/** @type {unknown} */ input) => {
    const gate = record(input, 'Gate');
    if (
      !GATES.includes(gate.id) ||
      ids.has(gate.id) ||
      !['pending', 'blocked', 'passed'].includes(gate.status)
    ) {
      throw new Error('Invalid, duplicate or missing v1 gate.');
    }
    ids.add(gate.id);
    return {
      id: gate.id,
      title: text(gate.title, 'gate title'),
      status: gate.status,
      acceptance: text(gate.acceptance, 'gate acceptance'),
    };
  });
  return { schemaVersion: 1, project: 'react-native-qnx', commit, sourceTree, sources, gates };
}

/** Resolve a caller-owned relative file without following path symlinks.
 * @param {string} root @param {unknown} relative @param {boolean} [createParents] */
async function confinedFile(root, relative, createParents = false) {
  const name = text(relative, 'relative path');
  if (
    path.isAbsolute(name) ||
    path.win32.isAbsolute(name) ||
    name.includes('\\') ||
    name.split('/').some((part) => !part || part === '.' || part === '..')
  ) {
    throw new Error('Use a confined relative path without dot or parent segments.');
  }
  let current = root;
  const parts = name.split('/');
  for (const [index, part] of parts.entries()) {
    current = path.join(current, part);
    const parent = index < parts.length - 1;
    if (parent && createParents) {
      try {
        await mkdir(current, { mode: 0o700 });
      } catch (error) {
        if (!(error instanceof Error && 'code' in error && error.code === 'EEXIST')) throw error;
      }
    }
    let info;
    try {
      info = await lstat(current);
    } catch (error) {
      if (error instanceof Error && 'code' in error && error.code === 'ENOENT') continue;
      throw error;
    }
    if (info.isSymbolicLink() || (parent ? !info.isDirectory() : !info.isFile())) {
      throw new Error('Paths must use real directories and regular files, never symlinks.');
    }
  }
  return current;
}
/** @param {string} file @param {number} limit @param {boolean} [asJson] */
async function inspectFile(file, limit, asJson = false) {
  const handle = await open(file, constants.O_RDONLY | constants.O_NOFOLLOW | constants.O_NONBLOCK);
  try {
    const stat = await handle.stat();
    if (!stat.isFile() || stat.nlink !== 1 || stat.size > limit) {
      throw new Error('Evidence must be a bounded regular file with one link.');
    }
    const digest = createHash('sha256');
    const chunks = [];
    const buffer = Buffer.alloc(64 * 1024);
    let bytes = 0;
    for (;;) {
      const { bytesRead } = await handle.read(buffer, 0, buffer.length, null);
      if (!bytesRead) break;
      bytes += bytesRead;
      if (bytes > limit) throw new Error('Evidence grew beyond its size limit.');
      digest.update(buffer.subarray(0, bytesRead));
      if (asJson) chunks.push(Buffer.from(buffer.subarray(0, bytesRead)));
    }
    const end = await handle.stat();
    if (
      bytes !== stat.size ||
      end.size !== stat.size ||
      end.mtimeMs !== stat.mtimeMs ||
      end.ctimeMs !== stat.ctimeMs
    ) {
      throw new Error('Evidence changed during inspection.');
    }
    return {
      bytes,
      sha256: digest.digest('hex'),
      json: asJson ? JSON.parse(Buffer.concat(chunks).toString('utf8')) : undefined,
    };
  } finally {
    await handle.close();
  }
}

/** @param {unknown} input @param {string} label @returns {TargetIdentity} */
function targetIdentity(input, label) {
  const value = record(input, label);
  for (const key of TARGET_FIELDS) text(value[key], `${label}.${key}`);
  if (!LANES.includes(value.lane)) throw new Error(`Invalid ${label}.lane.`);
  for (const key of ['binarySha256', 'bundleSha256']) {
    if (!/^[a-f0-9]{64}$/.test(value[key])) throw new Error(`Invalid ${label}.${key} SHA-256.`);
  }
  return /** @type {TargetIdentity} */ (
    Object.fromEntries(TARGET_FIELDS.map((key) => [key, value[key]]))
  );
}

/** @param {Record<string, any>} item @param {TargetIdentity | null} releaseTarget
 * @param {string} base @param {Map<string, Record<string, any>>} artifacts
 * @param {(bytes: number) => void} account */
async function verifyCapture(item, releaseTarget, base, artifacts, account) {
  if (item.evidence === undefined) {
    return {
      verification: 'integrity-only',
      captureComplete: null,
      blockers: ['cold-boot', 'performance'].includes(item.gate)
        ? ['No typed capture; add evidence for cold-boot/performance completeness checks.']
        : [],
    };
  }
  const input = record(item.evidence, 'Receipt evidence');
  if (!['cold-boot', 'performance'].includes(input.kind) || input.kind !== item.gate) {
    throw new Error(
      'Typed evidence kind must be cold-boot/performance and match its receipt gate.'
    );
  }
  /** @type {CaptureEvidence} */
  const evidence = {
    kind: input.kind,
    target: input.target === null ? null : targetIdentity(input.target, 'Evidence target'),
    capture: input.capture === null ? null : text(input.capture, 'capture path'),
  };
  const target = evidence.target;
  if (
    target &&
    (target.lane !== item.lane ||
      ((item.lane === 'qnx-physical' || item.lane === 'marathon-physical') &&
        (target.board !== item.board || target.image !== item.image)))
  ) {
    throw new Error('Evidence target differs from receipt lane/board/image.');
  }
  const blockers = [];
  if (!target)
    blockers.push('Missing receipt target identity; set evidence.target from the actual run.');
  if (!releaseTarget)
    blockers.push('Missing releaseTarget; the integrator must name the candidate target.');
  if (target && releaseTarget) {
    for (const key of TARGET_FIELDS) {
      if (target[key] !== releaseTarget[key])
        blockers.push(`Receipt target ${key} differs from releaseTarget.`);
    }
  }
  /** @type {ReturnType<typeof qualify> | null} */
  let qualification = null;
  /** @type {Awaited<ReturnType<typeof verifyArtifacts>> | null} */
  let artifactVerification = null;
  if (evidence.capture === null) {
    blockers.push(
      'No capture recorded; retain failed/blocked/not-run outcomes and collect evidence separately.'
    );
  } else {
    const name = text(evidence.capture, 'capture path');
    const descriptor = artifacts.get(name);
    if (!descriptor) throw new Error('Capture must reference a hashed receipt artifact.');
    const captureFile = await confinedFile(base, name);
    const observed = await inspectFile(captureFile, MAX_JSON, true);
    if (observed.bytes !== descriptor.bytes || observed.sha256 !== descriptor.sha256) {
      throw new Error('Capture integrity mismatch during inspection.');
    }
    const capture = record(observed.json, 'Capture');
    if (capture.kind !== evidence.kind)
      throw new Error('Capture kind differs from typed evidence.');
    qualification = qualify(capture);
    const p = qualification.provenance;
    if (p.sourceCommit !== item.commit || p.lockfileSha256 !== item.lockSha256) {
      throw new Error('Capture candidate/lockfile differs from receipt.');
    }
    // The qualifier uses "host" for all host-only dossier lanes.
    const lane = [
      'hosted-ci',
      'source-review',
      'clean-host',
      'linux-offscreen',
      'linux-wayland',
    ].includes(item.lane)
      ? 'host'
      : item.lane;
    if (
      p.lane !== lane ||
      ((item.lane === 'qnx-physical' || item.lane === 'marathon-physical') &&
        (p.board !== item.board || p.image !== item.image))
    ) {
      throw new Error('Capture lane/board/image differs from receipt.');
    }
    if (target) {
      /** @type {Record<string, string>} */
      const identities = { ...target, targetId: target.id, lane };
      for (const key of [
        'targetId',
        'lane',
        'board',
        'image',
        'runtime',
        'binarySha256',
        'bundleSha256',
      ]) {
        if (p[key] !== identities[key])
          throw new Error(`Capture ${key} differs from receipt target.`);
      }
    }
    const raw = capture.artifacts;
    if (!Array.isArray(raw) || raw.length === 0 || raw.length > 256) {
      throw new Error('Capture artifacts must contain 1-256 hashed raw files.');
    }
    const directory = path.dirname(captureFile);
    for (const input of raw) {
      const artifact = record(input, 'Capture artifact');
      const rawFile = await confinedFile(directory, artifact.path);
      const checked = await inspectFile(rawFile, 64 * 1024 * 1024);
      account(checked.bytes);
      if (checked.bytes === 0 || checked.sha256 !== artifact.sha256) {
        throw new Error(`Capture artifact integrity mismatch: ${artifact.path}`);
      }
    }
    artifactVerification = await verifyArtifacts(capture, directory);
    blockers.push(...qualification.blockers);
  }
  if (item.outcome !== 'passed') blockers.push(`Receipt outcome is ${item.outcome}, not passed.`);
  return {
    verification: 'candidate-capture-v1',
    kind: evidence.kind,
    target,
    capture: evidence.capture,
    captureComplete: blockers.length === 0,
    blockers,
    qualification,
    artifactVerification,
  };
}

/** @param {string} root @param {string} file */
export async function createDossier(root, file) {
  const candidate = candidateSnapshot(root);
  if (!file.startsWith('build/') || !file.endsWith('.json'))
    throw new Error('Dossier must be a .json file below build/.');
  const destination = await confinedFile(root, file, true);
  const document = { schemaVersion: 1, candidate, receipts: [], releaseAuthorized: false };
  const handle = await open(destination, 'wx', 0o600);
  try {
    await handle.writeFile(JSON.stringify(document, null, 2) + '\n');
  } finally {
    await handle.close();
  }
  return {
    action: 'created',
    candidate: candidate.commit,
    file,
    receiptCount: 0,
    releaseAuthorized: false,
  };
}

/** @param {string} root @param {string} file */
export async function verifyDossier(root, file) {
  const expected = candidateSnapshot(root);
  const input = await confinedFile(root, file);
  const document = record((await inspectFile(input, MAX_JSON, true)).json, 'Dossier');
  if (document.schemaVersion !== 1 || document.releaseAuthorized !== false)
    throw new Error('Dossier cannot authorize a release.');
  if (!isDeepStrictEqual(document.candidate, expected)) {
    throw new Error(
      'Candidate identity/source/gates differ from committed HEAD; do not reuse stale receipts.'
    );
  }
  if (!Array.isArray(document.receipts) || document.receipts.length > 128)
    throw new Error('At most 128 receipts are allowed.');
  const releaseTarget =
    document.releaseTarget == null ? null : targetIdentity(document.releaseTarget, 'releaseTarget');
  const ids = new Set();
  let total = 0;
  /** @param {number} bytes */
  const account = (bytes) => {
    total += bytes;
    if (total > MAX_TOTAL) throw new Error('Evidence budget exceeds 512 MiB.');
  };
  const receipts = [];
  const base = path.dirname(input);
  const lock = expected.sources.find((source) => source.path === 'pnpm-lock.yaml');
  for (const raw of document.receipts) {
    const item = record(raw, 'Receipt');
    const id = text(item.id, 'receipt ID', 128);
    if (ids.has(id)) throw new Error('Duplicate receipt ID.');
    ids.add(id);
    if (item.commit !== expected.commit || item.lockSha256 !== lock?.sha256)
      throw new Error('Receipt candidate/lockfile mismatch.');
    if (
      !GATES.includes(item.gate) ||
      !LANES.includes(item.lane) ||
      !['passed', 'failed', 'blocked', 'not-run'].includes(item.outcome)
    ) {
      throw new Error('Invalid receipt gate, evidence lane or outcome.');
    }
    text(item.command, 'observed command');
    text(item.environment, 'environment/toolchain');
    text(item.limits, 'capture limitations');
    text(item.author, 'receipt author', 128);
    if (item.reviewer !== null) {
      text(item.reviewer, 'declared reviewer', 128);
      if (item.reviewer === item.author)
        throw new Error('Author cannot be the declared independent reviewer.');
    }
    if (item.lane === 'qnx-physical' || item.lane === 'marathon-physical') {
      text(item.board, 'physical board');
      text(item.image, 'physical image');
    }
    if (
      !Array.isArray(item.artifacts) ||
      item.artifacts.length > 16 ||
      (item.outcome === 'passed' && item.artifacts.length === 0)
    ) {
      throw new Error('Passed receipt needs artifacts; at most 16 per receipt.');
    }
    const paths = new Set();
    const artifacts = new Map();
    for (const rawArtifact of item.artifacts) {
      const artifact = record(rawArtifact, 'Artifact');
      if (paths.has(artifact.path)) throw new Error('Duplicate artifact within a receipt.');
      paths.add(artifact.path);
      if (
        !Number.isSafeInteger(artifact.bytes) ||
        artifact.bytes <= 0 ||
        artifact.bytes > MAX_ARTIFACT ||
        typeof artifact.sha256 !== 'string' ||
        !/^[a-f0-9]{64}$/.test(artifact.sha256)
      ) {
        throw new Error('Invalid artifact byte count or SHA-256.');
      }
      account(artifact.bytes);
      if (typeof artifact.path !== 'string' || !artifact.path.startsWith('artifacts/'))
        throw new Error('Artifacts must stay below dossier artifacts/.');
      const observed = await inspectFile(await confinedFile(base, artifact.path), MAX_ARTIFACT);
      if (observed.bytes !== artifact.bytes || observed.sha256 !== artifact.sha256)
        throw new Error(`Artifact integrity mismatch: ${artifact.path}`);
      artifacts.set(artifact.path, artifact);
    }
    receipts.push({
      id,
      gate: item.gate,
      lane: item.lane,
      outcome: item.outcome,
      reviewerDeclared: item.reviewer !== null,
      artifactCount: item.artifacts.length,
      ...(await verifyCapture(item, releaseTarget, base, artifacts, account)),
    });
  }
  return {
    action: 'verified',
    candidate: expected.commit,
    integrityVerified: true,
    releaseTarget,
    receiptCount: receipts.length,
    receipts,
    gateSnapshot: expected.gates.map(({ id, status }) => ({ id, status })),
    openGates: expected.gates.filter(({ status }) => status !== 'passed').map(({ id }) => id),
    releaseAuthorized: false,
    limitations:
      'Hashes verify correspondence, not execution truth or reviewer identity. No receipt promotes a gate; run pnpm validate:v1 and independent review separately.',
  };
}

/** @param {string[]} argv @param {string} [cwd] */
export async function main(argv, cwd = process.cwd()) {
  const { values, positionals, tokens } = parseArgs({
    args: argv,
    strict: true,
    allowPositionals: true,
    tokens: true,
    options: { help: { type: 'boolean', short: 'h' }, file: { type: 'string' } },
  });
  const seen = new Set();
  for (const token of tokens) {
    if (token.kind !== 'option') continue;
    if (seen.has(token.name)) throw new Error(`Duplicate option --${token.name}.`);
    seen.add(token.name);
  }
  if (values.help || argv.length === 0) {
    console.log(help);
    return 0;
  }
  if (positionals.length !== 1 || !['create', 'verify'].includes(positionals[0]) || !values.file) {
    throw new Error('Use create|verify --file <project-relative JSON path>.');
  }
  const root = await realpath(cwd);
  const result =
    positionals[0] === 'create'
      ? await createDossier(root, values.file)
      : await verifyDossier(root, values.file);
  console.log(JSON.stringify(result, null, 2));
  return 0;
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    process.exitCode = await main(process.argv.slice(2));
  } catch (error) {
    console.error(`rnqnx release dossier: ${error instanceof Error ? error.message : error}`);
    process.exitCode = 1;
  }
}
