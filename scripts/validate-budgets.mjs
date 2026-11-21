#!/usr/bin/env node
// Performance budgets: schema check, derive/apply a baseline, and the host
// regression check. Reads docs/releases/performance-budgets.json and the JSON
// files that `rnqnx_playground_host --timing-json` writes.
//
//   node scripts/validate-budgets.mjs --check-file
//   node scripts/validate-budgets.mjs --check-run <dir>
//   node scripts/validate-budgets.mjs --derive <dir>
//   node scripts/validate-budgets.mjs --apply-baseline <dir> --source <text>
//
// A result file is named <workload>-<n>.json. Nothing here loosens a ceiling:
// --apply-baseline refuses to raise one that already holds a measured value.

import { readdir, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

export const BUDGETS_PATH = 'docs/releases/performance-budgets.json';
export const LANES = ['host', 'qnx-vm', 'pi400', 'release-board'];
export const METRICS = [
  'startupToFirstFrameMs',
  'frameIntervalP95Ms',
  'frameIntervalMaxMs',
  'peakRssKiB',
];
const STATUSES = ['measured', 'unmeasured', 'not-applicable', 'blocked'];
const MIN_WINDOW_FRAMES = 30;

/** Nearest-rank percentile of an ascending array. */
/**
 * @param {number[]} sorted
 * @param {number} p
 */
export function percentile(sorted, p) {
  if (sorted.length === 0) throw new RangeError('percentile of an empty set');
  const rank = Math.max(1, Math.ceil((p / 100) * sorted.length));
  return sorted[rank - 1];
}

/** @param {number[]} values */
export function median(values) {
  const sorted = [...values].sort((a, b) => a - b);
  const mid = sorted.length >> 1;
  return sorted.length % 2 ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2;
}

/** Validates one --timing-json result and returns the four metrics (null when not applicable). */
/**
 * @param {any} result
 * @param {{ needsFrames: boolean }} options
 * @returns {Record<string, number | null>}
 */
export function summarizeRun(result, { needsFrames }) {
  /** @param {string} why */
  const fail = (why) => {
    throw new Error(`invalid timing result: ${why}`);
  };
  if (result?.schema !== 1) fail('schema is not 1');
  if (result.clock !== 'steady_clock') fail('clock is not steady_clock');
  if (result.droppedFrames !== 0) fail(`droppedFrames is ${result.droppedFrames}, not 0`);
  if (!Array.isArray(result.frameMs) || result.frameMs.length !== result.totalFrames) {
    fail('frameMs does not hold totalFrames records');
  }
  for (let i = 1; i < result.frameMs.length; i++) {
    if (!(result.frameMs[i] >= result.frameMs[i - 1])) fail('frameMs is not ascending');
  }
  if (!(result.firstFrameMs > 0)) fail('firstFrameMs is not a positive number');
  if (!result.frameMs.includes(result.firstFrameMs)) fail('firstFrameMs is not one of frameMs');
  if (!(result.surfaceStartedMs <= result.firstFrameMs)) fail('first frame precedes surface start');
  if (!Number.isFinite(result.peakRssKiB) || result.peakRssKiB <= 0) fail('peakRssKiB is missing');
  /** @type {Record<string, number | null>} */
  const metrics = {
    startupToFirstFrameMs: result.firstFrameMs,
    frameIntervalP95Ms: null,
    frameIntervalMaxMs: null,
    peakRssKiB: result.peakRssKiB,
  };
  if (needsFrames) {
    if (result.wheelNotchesSent < 1 || result.wheelFirstMs == null || result.wheelLastMs == null) {
      fail('no wheel script ran, so there is no animated window');
    }
    /** @type {number[]} */
    const all = result.frameMs;
    const window = all.filter((t) => t >= result.wheelFirstMs && t <= result.wheelLastMs);
    if (window.length < MIN_WINDOW_FRAMES) {
      fail(`only ${window.length} swaps in the wheel window, need ${MIN_WINDOW_FRAMES}`);
    }
    const gaps = window
      .slice(1)
      .map((t, i) => t - window[i])
      .sort((a, b) => a - b);
    metrics.frameIntervalP95Ms = percentile(gaps, 95);
    metrics.frameIntervalMaxMs = gaps[gaps.length - 1];
  }
  return metrics;
}

/**
 * @param {number} maxValue
 * @param {number} factor
 */
export function ceilingFor(maxValue, factor) {
  return Math.ceil(maxValue * factor);
}

/** Returns a list of problems; empty when the budgets file is consistent. */
/**
 * @param {any} file
 * @returns {string[]}
 */
export function checkBudgetsFile(file) {
  /** @type {string[]} */
  const problems = [];
  /** @param {string} m */
  const add = (m) => problems.push(m);
  if (file.schemaVersion !== 1) add('schemaVersion must be 1');
  if (file.gate !== 'performance') add('gate must be "performance"');
  const method = file.method ?? {};
  if (!(method.regressionFactor >= 1)) add('method.regressionFactor must be at least 1');
  if (!(method.hostRunsPerCheck >= 3)) add('method.hostRunsPerCheck must be at least 3');
  if (!(method.baselineMinRuns >= 20)) add('method.baselineMinRuns must be at least 20');
  if (
    !file.knownConstraints?.some((/** @type {any} */ c) => c.setting === 'HERMESVM_ALLOW_JIT=0')
  ) {
    add('knownConstraints must record HERMESVM_ALLOW_JIT=0');
  }
  for (const lane of LANES) {
    if (!file.lanes?.[lane]?.label) add(`lanes.${lane}.label is missing`);
  }
  for (const w of ['hello-world', 'list-1000', 'drawer-drag']) {
    const workload = file.workloads?.[w];
    if (!workload) {
      add(`workload ${w} is missing`);
      continue;
    }
    for (const metric of METRICS) {
      const entry = workload.metrics?.[metric];
      if (!entry) {
        add(`${w}.${metric} is missing`);
        continue;
      }
      for (const lane of LANES) {
        const b = entry.budgets?.[lane];
        const where = `${w}.${metric}.${lane}`;
        if (!b || !STATUSES.includes(b.status)) {
          add(`${where} needs a status of ${STATUSES.join(', ')}`);
          continue;
        }
        if (b.status === 'measured') {
          const base = b.baseline ?? {};
          if (!(base.runs >= method.baselineMinRuns))
            add(`${where} baseline has fewer than ${method.baselineMinRuns} runs`);
          for (const k of ['min', 'p50', 'max']) {
            if (!Number.isFinite(base[k])) add(`${where} baseline.${k} is not a number`);
          }
          if (!base.source) add(`${where} baseline.source is missing`);
          if (ceilingFor(base.max, method.regressionFactor) !== b.ceiling) {
            add(
              `${where} ceiling ${b.ceiling} is not ceil(baseline.max * ${method.regressionFactor})`
            );
          }
          if (lane !== 'host' && !base.source?.includes(file.lanes[lane]?.label ?? '\0')) {
            add(`${where} baseline.source does not name the ${lane} lane label`);
          }
        } else {
          if (lane === 'host' && file.lanes?.host?.enforced && b.status === 'unmeasured') {
            add(`${where} is unmeasured but the host lane is enforced`);
          }
          for (const k of ['baseline', 'ceiling']) {
            if (k in b) add(`${where} is ${b.status} and must not carry ${k}`);
          }
          if (!b.reason) add(`${where} is ${b.status} and needs a reason`);
        }
      }
    }
  }
  if (file.workloads?.['drawer-drag']) {
    for (const metric of METRICS) {
      if (file.workloads['drawer-drag'].metrics?.[metric]?.budgets?.host?.status === 'measured') {
        add(
          'drawer-drag cannot be measured on the host: the gesture path is blocked in fixtures.js'
        );
      }
    }
  }
  return problems;
}

/**
 * @param {string} dir
 * @param {string} workload
 * @returns {Promise<any[]>}
 */
async function readResults(dir, workload) {
  const names = (await readdir(dir))
    .filter((n) => n.startsWith(`${workload}-`) && n.endsWith('.json'))
    .sort();
  const out = [];
  for (const name of names) out.push(JSON.parse(await readFile(path.join(dir, name), 'utf8')));
  return out;
}

/** Compares the median of each metric across runs with the host ceilings. */
/**
 * @param {any} file
 * @param {string} dir
 */
export async function checkRun(file, dir) {
  const rows = [];
  let failed = false;
  for (const workload of ['hello-world', 'list-1000']) {
    const results = await readResults(dir, workload);
    if (results.length < file.method.hostRunsPerCheck) {
      throw new Error(
        `${workload}: ${results.length} result files, need ${file.method.hostRunsPerCheck}`
      );
    }
    const runs = results.map((r) => summarizeRun(r, { needsFrames: workload === 'list-1000' }));
    for (const metric of METRICS) {
      const budget = file.workloads[workload].metrics[metric].budgets.host;
      if (budget.status !== 'measured') continue;
      const value = median(runs.map((r) => /** @type {number} */ (r[metric])));
      const ok = value <= budget.ceiling;
      if (!ok) failed = true;
      rows.push({
        workload,
        metric,
        runs: runs.length,
        median: value,
        ceiling: budget.ceiling,
        ok,
      });
    }
  }
  return { rows, failed };
}

/** Per workload and metric: min, p50 and max across the run files. */
/**
 * @param {any} file
 * @param {string} dir
 * @returns {Promise<Record<string, any>>}
 */
export async function derive(file, dir) {
  /** @type {Record<string, any>} */
  const out = {};
  for (const workload of ['hello-world', 'list-1000']) {
    const results = await readResults(dir, workload);
    if (results.length === 0) continue;
    const runs = results.map((r) => summarizeRun(r, { needsFrames: workload === 'list-1000' }));
    out[workload] = { runs: runs.length };
    for (const metric of METRICS) {
      const values = runs
        .map((r) => r[metric])
        .filter((v) => v != null)
        .sort((a, b) => a - b);
      if (values.length === 0) continue;
      out[workload][metric] = {
        min: values[0],
        p50: percentile(values, 50),
        max: values[values.length - 1],
      };
    }
  }
  return out;
}

/**
 * @param {any} file
 * @param {string} dir
 * @param {string} source
 */
export async function applyBaseline(file, dir, source) {
  const derived = await derive(file, dir);
  for (const [workload, metrics] of Object.entries(derived)) {
    for (const metric of METRICS) {
      if (!metrics[metric]) continue;
      const current = file.workloads[workload].metrics[metric].budgets.host;
      if (current.status === 'not-applicable' || current.status === 'blocked') continue;
      const ceiling = ceilingFor(metrics[metric].max, file.method.regressionFactor);
      if (current.status === 'measured' && ceiling > current.ceiling) {
        throw new Error(
          `${workload}.${metric}: refusing to raise ceiling ${current.ceiling} to ${ceiling}; that needs an ADR and the owner`
        );
      }
      file.workloads[workload].metrics[metric].budgets.host = {
        status: 'measured',
        baseline: { runs: derived[workload].runs, ...metrics[metric], source },
        ceiling,
      };
    }
  }
  return file;
}

/** @param {string[]} argv */
async function main(argv) {
  const root = process.cwd();
  const file = JSON.parse(await readFile(path.join(root, BUDGETS_PATH), 'utf8'));
  const flag = argv[0];
  if (flag === '--check-file') {
    const problems = checkBudgetsFile(file);
    for (const p of problems) console.error(`BUDGETS: ${p}`);
    console.log(
      problems.length === 0
        ? 'BUDGETS: file is consistent'
        : `BUDGETS: ${problems.length} problem(s)`
    );
    return problems.length === 0 ? 0 : 1;
  }
  if (flag === '--check-run') {
    const { rows, failed } = await checkRun(file, path.resolve(argv[1]));
    for (const r of rows) {
      console.log(
        `BUDGETS host ${r.ok ? 'ok  ' : 'FAIL'} ${r.workload} ${r.metric} median=${r.median} ceiling=${r.ceiling} runs=${r.runs}`
      );
    }
    return failed ? 1 : 0;
  }
  if (flag === '--derive') {
    console.log(JSON.stringify(await derive(file, path.resolve(argv[1])), null, 2));
    return 0;
  }
  if (flag === '--apply-baseline') {
    const source = argv[argv.indexOf('--source') + 1];
    if (!source || source.startsWith('--')) throw new Error('--source <text> is required');
    const next = await applyBaseline(file, path.resolve(argv[1]), source);
    const problems = checkBudgetsFile(next);
    if (problems.length > 0) throw new Error(problems.join('; '));
    await writeFile(path.join(root, BUDGETS_PATH), `${JSON.stringify(next, null, 2)}\n`);
    return 0;
  }
  console.error(
    'usage: validate-budgets.mjs --check-file | --check-run <dir> | --derive <dir> | --apply-baseline <dir> --source <text>'
  );
  return 2;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main(process.argv.slice(2)).then(
    (code) => process.exit(code),
    (error) => {
      console.error(`BUDGETS: ${error.message}`);
      process.exit(1);
    }
  );
}
