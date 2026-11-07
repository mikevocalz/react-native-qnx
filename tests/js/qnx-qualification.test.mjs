import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { mkdtemp, mkdir, readFile, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { distribution, qualify, verifyArtifacts } from '../../scripts/lib/qualification.mjs';

const provenance = {
  sourceCommit: 'a'.repeat(40),
  lockfileSha256: 'b'.repeat(64),
  binarySha256: 'c'.repeat(64),
  bundleSha256: 'd'.repeat(64),
  lane: 'qnx-physical',
  board: 'synthetic-pi400',
  image: 'synthetic-image',
  runtime: 'synthetic runtime; no device was used',
  captureMethod: 'synthetic unit-test observations',
  limitations: 'All values in this test are synthetic, not benchmark results.',
};
const raw = 'Synthetic raw capture. No QNX hardware was used.\n';
const digest = createHash('sha256').update(raw).digest('hex');
const artifacts = [{ path: 'raw.log', sha256: digest }];

function performance() {
  return {
    schemaVersion: 1,
    kind: 'performance',
    provenance: { ...provenance },
    artifacts: structuredClone(artifacts),
    workloads: Object.fromEntries(
      ['hello-world', 'list-1000', 'drawer-drag'].map((id) => [
        id,
        {
          status: 'recorded',
          clock: 'monotonic',
          frameBoundary: 'presented',
          refreshHz: 50,
          warmupIntervals: 1,
          startupMs: [100, 120, 110],
          frameIntervalsMs: [999, 20, 30, 10, 20],
          rssBytes: [1048576, 2097152],
        },
      ])
    ),
  };
}

/** @param {number} [count] */
function boots(count = 20) {
  return {
    schemaVersion: 1,
    kind: 'cold-boot',
    provenance: { ...provenance },
    artifacts: structuredClone(artifacts),
    recoveryProcedure: 'Synthetic supervised recovery procedure.',
    attempts: Array.from({ length: count }, (_, index) => ({
      sequence: index + 1,
      bootId: `synthetic-${index + 1}`,
      startedAt: new Date(Date.UTC(2026, 0, 1, 0, index)).toISOString(),
      completedAt: new Date(Date.UTC(2026, 0, 1, 0, index, 30)).toISOString(),
      status: 'passed',
      reset: 'power-cycle',
      graphicsReady: true,
      appReady: true,
      recoveryAvailable: true,
      log: 'raw.log',
      reason: 'Synthetic injected failure.',
    })),
  };
}

let cases = 0;
/** @param {string} label @param {() => void | Promise<void>} fn */
async function check(label, fn) {
  await fn();
  cases += 1;
  console.log(`qualification: ${label}`);
}

const dir = await mkdtemp(path.join(tmpdir(), 'rnqnx-qualification-'));
try {
  await writeFile(path.join(dir, 'raw.log'), raw);
  await check('nearest-rank percentiles and input preserved', () => {
    const values = [5, 1, 4, 2, 3];
    assert.deepEqual(distribution(values), { count: 5, min: 1, p50: 3, p95: 5, p99: 5, max: 5 });
    assert.deepEqual(values, [5, 1, 4, 2, 3]);
    assert.throws(() => distribution([]), /zero samples/);
  });
  await check('physical-labelled complete report never promotes release', () => {
    const report = qualify(performance());
    assert.equal(report.captureComplete, true);
    assert.equal(report.releaseGatePassed, false);
    assert.ok('workloads' in report);
    const result = report.workloads[0];
    assert.equal(result.frameIntervalsMs?.count, 4);
    assert.equal(result.frameIntervalsMs?.max, 30);
    assert.equal(result.intervalsOverBudget, 1);
  });
  for (const lane of ['host', 'qnx-vm', 'marathon-physical']) {
    await check(`reject ${lane} promotion`, () => {
      const input = performance();
      input.provenance.lane = lane;
      assert.equal(qualify(input).captureComplete, false);
    });
  }
  for (const boundary of ['queued', 'js-callback']) {
    await check(`label ${boundary} as incomplete presentation evidence`, () => {
      const input = performance();
      input.workloads['hello-world'].frameBoundary = boundary;
      assert.equal(qualify(input).captureComplete, false);
    });
  }
  for (const invalid of [0, -1, NaN, Infinity]) {
    await check(`reject invalid timing ${invalid}`, () => {
      const input = performance();
      input.workloads['hello-world'].startupMs = [invalid];
      assert.throws(() => qualify(input), /positive finite/);
    });
  }
  await check('empty sample array rejected', () => {
    const input = performance();
    input.workloads['hello-world'].frameIntervalsMs = [];
    assert.throws(() => qualify(input), /samples/);
  });
  await check('exhausted warmup rejected', () => {
    const input = performance();
    input.workloads['hello-world'].warmupIntervals = 5;
    assert.throws(() => qualify(input), /warmupIntervals/);
  });
  await check('fractional RSS rejected', () => {
    const input = performance();
    input.workloads['hello-world'].rssBytes = [1.5];
    assert.throws(() => qualify(input), /integer bytes/);
  });
  await check('missing workload rejected', () => {
    const input = performance();
    delete input.workloads['drawer-drag'];
    assert.throws(() => qualify(input), /drawer-drag/);
  });
  await check('unrecognized schema and abbreviated source rejected', () => {
    const input = performance();
    input.schemaVersion = 2;
    assert.throws(() => qualify(input), /schemaVersion/);
    input.schemaVersion = 1;
    input.provenance.sourceCommit = 'abc1234';
    assert.throws(() => qualify(input), /full 40-character/);
  });
  await check('20 power cycles complete; 19 do not', () => {
    assert.equal(qualify(boots()).captureComplete, true);
    assert.equal(qualify(boots(19)).captureComplete, false);
    assert.equal(qualify(boots(0)).captureComplete, false);
  });
  for (const reset of ['reboot', 'app-relaunch']) {
    await check(`${reset} is not a cold power cycle`, () => {
      const input = boots();
      input.attempts[19].reset = reset;
      assert.equal(qualify(input).captureComplete, false);
    });
  }
  await check('failure resets consecutive sequence', () => {
    const input = boots(21);
    input.attempts[10].status = 'failed';
    assert.equal(qualify(input).captureComplete, false);
    input.attempts[10].status = 'passed';
    input.attempts[0].status = 'failed';
    assert.equal(qualify(input).captureComplete, true);
  });
  await check('duplicate boot identity rejected', () => {
    const input = boots();
    input.attempts[19].bootId = input.attempts[0].bootId;
    assert.throws(() => qualify(input), /Duplicate boot/);
  });
  await check('overlapping timestamps rejected', () => {
    const input = boots();
    input.attempts[1].startedAt = input.attempts[0].startedAt;
    assert.throws(() => qualify(input), /overlap/);
  });
  await check('sequence gaps and contradictory passing boot rejected', () => {
    const input = boots();
    input.attempts[0].sequence = 2;
    assert.throws(() => qualify(input), /no gaps/);
    input.attempts[0].sequence = 1;
    input.attempts[0].appReady = false;
    assert.throws(() => qualify(input), /passing boot/);
  });
  await check('raw capture hash actually verified', async () => {
    assert.equal((await verifyArtifacts(boots(), dir)).verifiedFiles, 1);
  });
  await check('tampered raw capture rejected', async () => {
    const input = boots();
    input.artifacts[0].sha256 = '0'.repeat(64);
    await assert.rejects(verifyArtifacts(input, dir), /checksum mismatch/);
  });
  for (const filename of ['../raw.log', '/raw.log', 'a/../raw.log', 'C:\\raw.log']) {
    await check(`unsafe artifact path ${filename}`, async () => {
      const input = boots();
      input.artifacts[0].path = filename;
      await assert.rejects(verifyArtifacts(input, dir), /relative/);
    });
  }
  await check('symlink file and directory rejected', async () => {
    await symlink(path.join(dir, 'raw.log'), path.join(dir, 'link.log'));
    await mkdir(path.join(dir, 'real'));
    await writeFile(path.join(dir, 'real/raw.log'), raw);
    await symlink(path.join(dir, 'real'), path.join(dir, 'link-dir'));
    for (const filename of ['link.log', 'link-dir/raw.log']) {
      const input = boots();
      input.artifacts[0].path = filename;
      await assert.rejects(verifyArtifacts(input, dir), /symlinks/);
    }
  });
  await check('empty, absent and duplicate artifacts rejected', async () => {
    const input = boots();
    input.artifacts = [];
    await assert.rejects(verifyArtifacts(input, dir), /1-256/);
    input.artifacts = [artifacts[0], artifacts[0]];
    await assert.rejects(verifyArtifacts(input, dir), /Duplicate artifact/);
    input.artifacts = [{ path: 'missing.log', sha256: digest }];
    await assert.rejects(verifyArtifacts(input, dir), /ENOENT/);
  });
  await check('every boot needs a hashed log', async () => {
    const input = boots();
    input.attempts[0].log = 'missing.log';
    await assert.rejects(verifyArtifacts(input, dir), /hashed raw log/);
  });
  await check('CLI preserves input and fails closed for incomplete evidence', async () => {
    const file = path.join(dir, 'capture.json');
    const original = JSON.stringify(boots(19));
    await writeFile(file, original);
    const script = path.resolve('scripts/qualify-qnx-evidence.mjs');
    const result = spawnSync(process.execPath, [script, file, '--require-complete'], {
      encoding: 'utf8',
      timeout: 5000,
    });
    assert.equal(result.error, undefined);
    assert.equal(result.status, 2, result.stderr);
    assert.equal(JSON.parse(result.stdout).captureComplete, false);
    assert.equal(await readFile(file, 'utf8'), original);
    const invalid = spawnSync(process.execPath, [script, '--unknown'], {
      encoding: 'utf8',
      timeout: 5000,
    });
    assert.equal(invalid.status, 1);
  });
  console.log(`qnx-qualification: ${cases} cases passed; synthetic host fixtures only.`);
} finally {
  await rm(dir, { recursive: true, force: true });
}
