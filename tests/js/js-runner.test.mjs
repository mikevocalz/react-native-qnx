import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtemp, mkdir, writeFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';

const runner = path.join(process.cwd(), 'scripts/run-js-tests.mjs');
const temporary = await mkdtemp(path.join(tmpdir(), 'rnqnx-test-runner-'));
function run() {
  const result = spawnSync(process.execPath, [runner], {
    cwd: temporary,
    encoding: 'utf8',
    timeout: 10000,
  });
  assert.equal(result.error, undefined);
  return result;
}
try {
  assert.equal(run().status, 1, 'missing test directory must fail');
  await mkdir(path.join(temporary, 'tests/js'), { recursive: true });
  assert.equal(run().status, 1, 'empty test directory must fail');
  await writeFile(
    path.join(temporary, 'tests/js/missing-input.test.mjs'),
    "import { readFile } from 'node:fs/promises';\nawait readFile('missing-receipt.json');\n"
  );
  const missingInput = run();
  assert.equal(missingInput.status, 1, 'ENOENT inside a test must not turn the suite into a skip');
  assert.match(missingInput.stderr, /ENOENT/);
  assert.doesNotMatch(missingInput.stdout, /All JavaScript tests passed/);
  console.log('js-runner.test.mjs passed (missing suite, empty suite and missing test input fail)');
} finally {
  await rm(temporary, { recursive: true, force: true });
}
