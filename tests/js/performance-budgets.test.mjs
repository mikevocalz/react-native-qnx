import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

import {
  applyBaseline,
  BUDGETS_PATH,
  ceilingFor,
  checkBudgetsFile,
  checkRun,
  median,
  percentile,
  summarizeRun,
} from '../../scripts/validate-budgets.mjs';

const root = process.cwd();
const committed = JSON.parse(await readFile(path.join(root, BUDGETS_PATH), 'utf8'));
const clone = () => structuredClone(committed);

// The committed file is consistent and never carries a device number.
assert.deepEqual(checkBudgetsFile(committed), []);
for (const workload of Object.values(committed.workloads)) {
  for (const metric of Object.values(workload.metrics)) {
    for (const lane of ['qnx-vm', 'pi400', 'release-board']) {
      const budget = metric.budgets[lane];
      assert.notEqual(budget.status, 'measured', 'no device lane has a measurement in S4');
      assert.equal('ceiling' in budget || 'baseline' in budget, false);
    }
  }
}
assert.equal(
  committed.knownConstraints.some((/** @type {any} */ c) => c.setting === 'HERMESVM_ALLOW_JIT=0'),
  true
);
assert.equal(committed.workloads['drawer-drag'].metrics.peakRssKiB.budgets.host.status, 'blocked');

// Mutations of the file are caught.
{
  const hostGap = clone();
  hostGap.workloads['list-1000'].metrics.peakRssKiB.budgets.host = {
    status: 'unmeasured',
    reason: 'x',
  };
  assert.match(
    checkBudgetsFile(hostGap).join('\n'),
    /list-1000.peakRssKiB.host is unmeasured but the host lane is enforced/
  );

  const withGuess = clone();
  withGuess.workloads['hello-world'].metrics.peakRssKiB.budgets.pi400.ceiling = 90000;
  assert.match(checkBudgetsFile(withGuess).join('\n'), /pi400.*must not carry ceiling/);

  const noJit = clone();
  noJit.knownConstraints = [];
  assert.match(checkBudgetsFile(noJit).join('\n'), /HERMESVM_ALLOW_JIT=0/);

  const drawer = clone();
  drawer.workloads['drawer-drag'].metrics.peakRssKiB.budgets.host = {
    status: 'measured',
    baseline: { runs: 20, min: 1, p50: 1, max: 1, source: 'host' },
    ceiling: 2,
  };
  assert.match(checkBudgetsFile(drawer).join('\n'), /drawer-drag cannot be measured on the host/);

  const loose = clone();
  loose.workloads['hello-world'].metrics.startupToFirstFrameMs.budgets.host = {
    status: 'measured',
    baseline: { runs: 20, min: 100, p50: 110, max: 120, source: 'host' },
    ceiling: 1000,
  };
  assert.match(
    checkBudgetsFile(loose).join('\n'),
    /ceiling 1000 is not ceil\(baseline.max \* 1.5\)/
  );
}

// Statistics.
assert.equal(percentile([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 95), 10);
assert.equal(percentile([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 50), 5);
assert.equal(median([9, 1, 5]), 5);
assert.equal(median([1, 2, 3, 4]), 2.5);
assert.equal(ceilingFor(120, 1.5), 180);
assert.throws(() => percentile([], 50), RangeError);

// A synthetic result in the probe's schema. The numbers exist only to drive the code.
function fakeResult({ first = 100, interval = 16, notches = 40, dropped = 0, rss = 50000 } = {}) {
  const frameMs = [first];
  const wheelFirst = first + 500;
  for (let t = wheelFirst; t <= wheelFirst + 4000; t += interval) frameMs.push(t);
  return {
    schema: 1,
    clock: 'steady_clock',
    firstFrameMs: first,
    surfaceStartedMs: first - 10,
    bundleEvaluatedMs: first - 20,
    frameMs,
    totalFrames: frameMs.length,
    droppedFrames: dropped,
    wheelNotchesSent: notches,
    wheelFirstMs: notches > 0 ? wheelFirst : null,
    wheelLastMs: notches > 0 ? wheelFirst + 3900 : null,
    rssKiB: rss - 100,
    peakRssKiB: rss,
  };
}

const good = summarizeRun(fakeResult(), { needsFrames: true });
assert.equal(good.startupToFirstFrameMs, 100);
assert.equal(good.frameIntervalP95Ms, 16);
assert.equal(good.frameIntervalMaxMs, 16);
assert.equal(good.peakRssKiB, 50000);
assert.equal(
  summarizeRun(fakeResult({ notches: 0 }), { needsFrames: false }).frameIntervalP95Ms,
  null
);

// Invalid runs are rejected, not averaged in.
assert.throws(
  () => summarizeRun(fakeResult({ dropped: 1 }), { needsFrames: true }),
  /droppedFrames is 1/
);
assert.throws(() => summarizeRun(fakeResult({ rss: 0 }), { needsFrames: true }), /peakRssKiB/);
assert.throws(
  () => summarizeRun(fakeResult({ notches: 0 }), { needsFrames: true }),
  /no wheel script/
);
assert.throws(
  () =>
    summarizeRun({ ...fakeResult(), frameMs: [100, 90], totalFrames: 2 }, { needsFrames: false }),
  /not ascending/
);

// Apply a baseline, then check runs against it, then show that a regression fails.
const dir = await mkdtemp(path.join(tmpdir(), 'budgets-test-'));
try {
  // Start from a file with no host numbers, as it was before the first baseline.
  let file = clone();
  for (const workload of ['hello-world', 'list-1000']) {
    for (const metric of Object.values(file.workloads[workload].metrics)) {
      if (metric.budgets.host.status === 'measured') {
        metric.budgets.host = { status: 'unmeasured', reason: 'reset for the test' };
      }
    }
  }
  for (let i = 0; i < 20; i++) {
    await writeFile(
      path.join(dir, `hello-world-${i}.json`),
      JSON.stringify(fakeResult({ first: 100 + (i % 5) }))
    );
    await writeFile(
      path.join(dir, `list-1000-${i}.json`),
      JSON.stringify(fakeResult({ first: 200 + (i % 5), interval: 16 + (i % 3), rss: 60000 + i }))
    );
  }
  file = await applyBaseline(file, dir, 'host test fixture');
  const afterApply = checkBudgetsFile(file);
  assert.deepEqual(afterApply, []);
  const startup = file.workloads['hello-world'].metrics.startupToFirstFrameMs.budgets.host;
  assert.equal(startup.baseline.max, 104);
  assert.equal(startup.ceiling, 156);
  assert.equal(
    file.workloads['list-1000'].metrics.frameIntervalMaxMs.budgets.host.baseline.max,
    18
  );

  const pass = await checkRun(file, dir);
  assert.equal(pass.failed, false);
  assert.equal(pass.rows.length, 6);

  const regressed = await mkdtemp(path.join(tmpdir(), 'budgets-regress-'));
  try {
    for (let i = 0; i < 5; i++) {
      await writeFile(
        path.join(regressed, `hello-world-${i}.json`),
        JSON.stringify(fakeResult({ first: 400 }))
      );
      await writeFile(
        path.join(regressed, `list-1000-${i}.json`),
        JSON.stringify(fakeResult({ first: 200, rss: 60000 }))
      );
    }
    const result = await checkRun(file, regressed);
    assert.equal(result.failed, true);
    const failing = result.rows.filter((r) => !r.ok);
    assert.deepEqual(
      failing.map((r) => `${r.workload}.${r.metric}`),
      ['hello-world.startupToFirstFrameMs']
    );
    assert.equal(failing[0].median, 400);
    assert.equal(failing[0].ceiling, 156);

    // Too few runs is an error, not a pass.
    await rm(path.join(regressed, 'hello-world-4.json'));
    await assert.rejects(checkRun(file, regressed), /4 result files, need 5/);
  } finally {
    await rm(regressed, { recursive: true, force: true });
  }

  // A baseline never raises a measured ceiling.
  for (let i = 0; i < 20; i++) {
    await writeFile(
      path.join(dir, `hello-world-${i}.json`),
      JSON.stringify(fakeResult({ first: 300 }))
    );
  }
  await assert.rejects(applyBaseline(file, dir, 'slower'), /refusing to raise ceiling 156/);
} finally {
  await rm(dir, { recursive: true, force: true });
}

// The CLI exits nonzero on a broken file.
{
  const cli = spawnSync(process.execPath, ['scripts/validate-budgets.mjs', '--check-file'], {
    cwd: root,
    encoding: 'utf8',
  });
  assert.equal(cli.status, 0, cli.stderr);
  assert.match(cli.stdout, /BUDGETS: file is consistent/);
}
console.log('performance budgets test passed');
