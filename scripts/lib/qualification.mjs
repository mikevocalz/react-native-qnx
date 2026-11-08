import { createHash } from 'node:crypto';
import { constants } from 'node:fs';
import { lstat, open, realpath } from 'node:fs/promises';
import path from 'node:path';

const MAX_SAMPLES = 100000;
const SHA256 = /^[0-9a-f]{64}$/;
const WORKLOADS = ['hello-world', 'list-1000', 'drawer-drag'];

/** @param {unknown} value @param {string} label @returns {Record<string, unknown>} */
function record(value, label) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error(`${label} must be an object.`);
  }
  return /** @type {Record<string, unknown>} */ (value);
}

/** @param {unknown} value @param {string} label */
function text(value, label) {
  if (typeof value !== 'string' || !value.trim() || value.length > 2048) {
    throw new Error(`${label} must be a nonempty string of at most 2048 characters.`);
  }
  return value;
}

/** @param {unknown} value @param {string} label */
function positive(value, label) {
  if (typeof value !== 'number' || !Number.isFinite(value) || value <= 0) {
    throw new Error(`${label} must be a positive finite number.`);
  }
  return value;
}

/** @param {unknown} value @param {string} label @returns {number[]} */
function samples(value, label) {
  if (!Array.isArray(value) || value.length === 0 || value.length > MAX_SAMPLES) {
    throw new Error(`${label} must contain 1-${MAX_SAMPLES} samples.`);
  }
  return value.map((item) => positive(item, label));
}

/** Nearest-rank percentiles. Counts are always included; no confidence claim.
 * @param {number[]} values
 */
export function distribution(values) {
  const sorted = [...values].sort((a, b) => a - b);
  if (!sorted.length) throw new Error('Cannot summarize zero samples.');
  /** @param {number} fraction */
  const percentile = (fraction) => sorted[Math.ceil(sorted.length * fraction) - 1];
  return {
    count: sorted.length,
    min: sorted[0],
    p50: percentile(0.5),
    p95: percentile(0.95),
    p99: percentile(0.99),
    max: sorted[sorted.length - 1],
  };
}

/** @param {unknown} input */
function provenance(input) {
  const p = record(input, 'provenance');
  if (!/^[0-9a-f]{40}$/.test(text(p.sourceCommit, 'sourceCommit'))) {
    throw new Error('sourceCommit must be a full 40-character Git commit.');
  }
  for (const key of ['lockfileSha256', 'binarySha256', 'bundleSha256']) {
    if (!SHA256.test(text(p[key], key))) throw new Error(`${key} must be SHA-256.`);
  }
  if (!['host', 'qnx-physical', 'qnx-vm', 'marathon-physical'].includes(String(p.lane))) {
    throw new Error('Unknown evidence lane.');
  }
  for (const key of ['board', 'image', 'runtime', 'captureMethod', 'limitations']) {
    text(p[key], `provenance.${key}`);
  }
  return p;
}

/** @param {unknown} input */
export function qualify(input) {
  const document = record(input, 'evidence');
  if (document.schemaVersion !== 1) throw new Error('Unsupported evidence schemaVersion.');
  const p = provenance(document.provenance);
  const result =
    document.kind === 'performance'
      ? performance(document, p)
      : document.kind === 'cold-boot'
        ? coldBoot(document, p)
        : null;
  if (!result) throw new Error('kind must be performance or cold-boot.');
  return {
    schemaVersion: 1,
    kind: document.kind,
    provenance: p,
    ...result,
    releaseGatePassed: false,
    reviewRequired: 'Review raw captures and source/target identity; no gate is modified.',
  };
}

/** @param {Record<string, unknown>} document @param {Record<string, unknown>} p */
function performance(document, p) {
  const workloads = record(document.workloads, 'workloads');
  if (Object.keys(workloads).some((id) => !WORKLOADS.includes(id))) {
    throw new Error('Unknown performance workload.');
  }
  /** @type {string[]} */
  const blockers = [];
  if (p.lane !== 'qnx-physical') {
    blockers.push('Performance qualification needs QNX physical evidence.');
  }
  const results = WORKLOADS.map((id) => {
    const w = record(workloads[id], `workloads.${id}`);
    if (w.status === 'blocked' || w.status === 'not-run') {
      const reason = text(w.reason, `${id}.reason`);
      blockers.push(`${id}: ${w.status}: ${reason}`);
      return { id, status: w.status, reason };
    }
    if (w.status !== 'recorded') throw new Error(`${id}: unknown workload status.`);
    if (w.clock !== 'monotonic') {
      throw new Error(`${id}: a monotonic measurement clock is required.`);
    }
    if (!['presented', 'queued', 'js-callback'].includes(String(w.frameBoundary))) {
      throw new Error(`${id}: unknown frame boundary.`);
    }
    if (w.frameBoundary !== 'presented') {
      blockers.push(`${id}: frame timings are not presentation evidence.`);
    }
    const refreshHz = positive(w.refreshHz, `${id}.refreshHz`);
    const startup = samples(w.startupMs, `${id}.startupMs`);
    const intervals = samples(w.frameIntervalsMs, `${id}.frameIntervalsMs`);
    const rss = samples(w.rssBytes, `${id}.rssBytes`);
    if (rss.some((bytes) => !Number.isSafeInteger(bytes))) {
      throw new Error(`${id}: RSS samples must be integer bytes.`);
    }
    const warmup = w.warmupIntervals;
    if (
      typeof warmup !== 'number' ||
      !Number.isSafeInteger(warmup) ||
      warmup < 0 ||
      warmup >= intervals.length
    ) {
      throw new Error(`${id}: warmupIntervals must leave at least one measured interval.`);
    }
    const frames = intervals.slice(warmup);
    const budget = 1000 / refreshHz;
    return {
      id,
      status: 'recorded',
      frameBoundary: w.frameBoundary,
      refreshHz,
      excludedWarmupIntervals: warmup,
      startupMs: distribution(startup),
      frameIntervalsMs: distribution(frames),
      rssBytes: distribution(rss),
      frameBudgetMs: budget,
      intervalsOverBudget: frames.filter((ms) => ms > budget).length,
    };
  });
  return { captureComplete: blockers.length === 0, blockers, workloads: results };
}

/** @param {unknown} value @param {string} label */
function timestamp(value, label) {
  const original = text(value, label);
  const millis = Date.parse(original);
  if (!Number.isFinite(millis) || new Date(millis).toISOString() !== original) {
    throw new Error(`${label} must be an exact UTC timestamp with milliseconds.`);
  }
  return millis;
}

/** @param {Record<string, unknown>} document @param {Record<string, unknown>} p */
function coldBoot(document, p) {
  if (!Array.isArray(document.attempts) || document.attempts.length > 1000) {
    throw new Error('attempts must be an array of at most 1000 attempts.');
  }
  text(document.recoveryProcedure, 'recoveryProcedure');
  const ids = new Set();
  let previousEnd = -Infinity;
  let consecutive = 0;
  /** @type {string[]} */
  const blockers = [];
  if (p.lane !== 'qnx-physical') {
    blockers.push('Cold-boot qualification needs QNX physical evidence.');
  }
  document.attempts.forEach((value, index) => {
    const attempt = record(value, `attempt ${index + 1}`);
    if (attempt.sequence !== index + 1) {
      throw new Error('Cold-boot sequence must start at 1 with no gaps.');
    }
    const id = text(attempt.bootId, 'bootId');
    if (ids.has(id)) {
      throw new Error('Duplicate boot identity; relaunches do not count as new boots.');
    }
    ids.add(id);
    const start = timestamp(attempt.startedAt, 'startedAt');
    const end = timestamp(attempt.completedAt, 'completedAt');
    if (end <= start || start < previousEnd) {
      throw new Error('Cold-boot timestamps overlap or run backwards.');
    }
    previousEnd = end;
    if (!['power-cycle', 'reboot', 'app-relaunch'].includes(String(attempt.reset))) {
      throw new Error('Unknown reset kind.');
    }
    if (!['passed', 'failed', 'blocked'].includes(String(attempt.status))) {
      throw new Error('Unknown cold-boot status.');
    }
    for (const key of ['graphicsReady', 'appReady', 'recoveryAvailable']) {
      if (typeof attempt[key] !== 'boolean') throw new Error(`${key} must be boolean.`);
    }
    text(attempt.log, 'attempt.log');
    if (attempt.status !== 'passed') text(attempt.reason, 'failed/blocked attempt reason');
    if (
      attempt.status === 'passed' &&
      (attempt.graphicsReady !== true ||
        attempt.appReady !== true ||
        attempt.recoveryAvailable !== true)
    ) {
      throw new Error('A passing boot must include graphics, app and recovery observations.');
    }
    if (attempt.status === 'passed' && attempt.reset === 'power-cycle') consecutive += 1;
    else consecutive = 0;
  });
  if (consecutive < 20) {
    blockers.push(`Need 20 consecutive final power cycles; found ${consecutive}.`);
  }
  return {
    captureComplete: blockers.length === 0,
    blockers,
    attempts: document.attempts.length,
    consecutiveFinalPowerCycles: consecutive,
  };
}

/** Validate referenced regular files without executing or publishing them.
 * Paths resolve relative to the JSON manifest directory. Reject all symlink
 * components; this is an integrity check, not a sandbox against concurrent writes.
 * @param {unknown} input @param {string} directory
 */
export async function verifyArtifacts(input, directory) {
  const document = record(input, 'evidence');
  if (
    !Array.isArray(document.artifacts) ||
    document.artifacts.length === 0 ||
    document.artifacts.length > 256
  ) {
    throw new Error('artifacts must contain 1-256 hashed raw files.');
  }
  const root = await realpath(directory);
  const paths = new Set();
  for (const value of document.artifacts) {
    const artifact = record(value, 'artifact');
    const name = text(artifact.path, 'artifact.path');
    const parts = name.split('/');
    if (
      path.isAbsolute(name) ||
      name.includes('\\') ||
      parts.some((part) => !part || part === '.' || part === '..') ||
      /^[A-Za-z]:/.test(name)
    ) {
      throw new Error('Artifact paths must be relative, non-traversing slash-separated paths.');
    }
    if (paths.has(name)) throw new Error('Duplicate artifact path.');
    paths.add(name);
    const expected = text(artifact.sha256, 'artifact.sha256');
    if (!SHA256.test(expected)) throw new Error('Artifact checksum must be SHA-256.');
    let current = root;
    for (const part of parts) {
      current = path.join(current, part);
      if ((await lstat(current)).isSymbolicLink()) {
        throw new Error('Artifact symlinks are not accepted.');
      }
    }
    if (!(await lstat(current)).isFile()) throw new Error('Artifact is not a regular file.');
    const flags = constants.O_RDONLY | (constants.O_NOFOLLOW ?? 0) | (constants.O_NONBLOCK ?? 0);
    const handle = await open(current, flags);
    try {
      const stat = await handle.stat();
      if (!stat.isFile() || stat.size <= 0 || stat.size > 64 * 1024 * 1024) {
        throw new Error('Raw artifacts must be nonempty regular files of at most 64 MiB.');
      }
      const hash = createHash('sha256');
      const chunk = Buffer.alloc(65536);
      let total = 0;
      for (;;) {
        const { bytesRead } = await handle.read(chunk, 0, chunk.length, null);
        if (!bytesRead) break;
        total += bytesRead;
        if (total > 64 * 1024 * 1024) throw new Error('Artifact grew past the size limit.');
        hash.update(chunk.subarray(0, bytesRead));
      }
      if (hash.digest('hex') !== expected) throw new Error(`Artifact checksum mismatch: ${name}`);
    } finally {
      await handle.close();
    }
  }
  if (document.kind === 'cold-boot' && Array.isArray(document.attempts)) {
    for (const value of document.attempts) {
      if (!paths.has(record(value, 'attempt').log)) {
        throw new Error('Every boot must reference a hashed raw log.');
      }
    }
  }
  return { verifiedFiles: paths.size };
}
