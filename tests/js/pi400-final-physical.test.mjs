import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import path from 'node:path';
import { main } from '../../scripts/final-pi400-test.mjs';

const file = path.resolve('scripts/final-pi400-test.mjs');
const plan = spawnSync(process.execPath, [file, 'plan'], {
  encoding: 'utf8',
  timeout: 10000,
  maxBuffer: 1024 * 1024,
});
assert.equal(plan.status, 0, plan.stderr);
const report = JSON.parse(plan.stdout);
assert.equal(report.issue, 43);
assert.equal(report.mode, 'plan');
assert.equal(report.board, 'pi400');
assert.equal(report.stages.length, 5);
assert.ok(
  report.stages.every((/** @type {{status: string}} */ stage) => stage.status === 'not-run')
);
assert.equal(report.physicalVerified, false);
assert.equal(report.stockQstiQualified, false);
assert.equal(report.releaseAuthorized, false);

const help = spawnSync(process.execPath, [file, '--help'], {
  encoding: 'utf8',
  timeout: 10000,
});
assert.equal(help.status, 0, help.stderr);
assert.match(help.stdout, /RNQNX_FINAL_TEST_CONFIRM=pi400/);

const preserved = {
  confirm: process.env.RNQNX_FINAL_TEST_CONFIRM,
  board: process.env.RNQNX_PI_BOARD,
};
try {
  delete process.env.RNQNX_FINAL_TEST_CONFIRM;
  delete process.env.RNQNX_PI_BOARD;
  await assert.rejects(
    main(['execute', '--consumer-project', '/nonexistent']),
    /confirmation|board/
  );
} finally {
  if (preserved.confirm === undefined) delete process.env.RNQNX_FINAL_TEST_CONFIRM;
  else process.env.RNQNX_FINAL_TEST_CONFIRM = preserved.confirm;
  if (preserved.board === undefined) delete process.env.RNQNX_PI_BOARD;
  else process.env.RNQNX_PI_BOARD = preserved.board;
}

await assert.rejects(main(['execute']), /confirmation|board/);
await assert.rejects(
  main(['plan', '--consumer-project', '/tmp/something']),
  /Plan does not accept/
);
await assert.rejects(
  main(['plan', '--consumer-project', 'x', '--consumer-project', 'y']),
  /Duplicate option/
);
await assert.rejects(main(['other']), /Choose plan or execute/);
await assert.rejects(main(['plan', '--unexpected']), /Unknown option/);

console.log(
  'Pi400 final test: plan/report, bounded stages, permission gate and invalid-input cases pass; physical hardware NOT run.'
);
