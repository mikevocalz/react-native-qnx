import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  cpSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';

const script = resolve('docs-site/scripts/assemble-published.mjs');
const temp = mkdtempSync(join(tmpdir(), 'pages-assembly-fixture-'));
/** @param {string} value */
const sha = (value) => createHash('sha256').update(value).digest('hex');
/** @param {string} path @param {string} value */
function write(path, value) {
  mkdirSync(resolve(path, '..'), { recursive: true });
  writeFileSync(path, value);
}
/** @param {string} live @param {string} candidate @param {string} hash @param {string} output */
function run(live, candidate, hash, output) {
  return spawnSync(process.execPath, [script, live, candidate, hash, output], {
    encoding: 'utf8',
    timeout: 10_000,
  });
}
try {
  const live = join(temp, 'live'),
    candidate = join(temp, 'candidate'),
    sourceCommit = 'a'.repeat(40),
    id = `candidate-${sourceCommit.slice(0, 12)}`;
  write(join(live, 'index.html'), 'unchanged live home');
  write(join(live, 'docs.html'), 'unchanged live docs');
  write(join(live, 'v/older/docs.html'), 'keep previous published snapshot');
  write(join(candidate, 'docs.html'), 'frozen candidate docs');
  write(join(candidate, '_next/chunk.js'), 'frozen asset');
  const receipt = {
    schema: 1,
    kind: 'candidate',
    id,
    sourceCommit,
    basePath: `/react-native-qnx/v/${id}`,
    files: { 'docs.html': sha('frozen candidate docs'), '_next/chunk.js': sha('frozen asset') },
  };
  const receiptText = JSON.stringify(receipt);
  write(join(candidate, 'snapshot.json'), receiptText);
  const output = join(temp, 'assembled');
  const result = run(live, candidate, sha(receiptText), output);
  assert.equal(result.status, 0, result.stderr);
  assert.equal(readFileSync(join(output, 'index.html'), 'utf8'), 'unchanged live home');
  assert.equal(readFileSync(join(output, 'docs.html'), 'utf8'), 'unchanged live docs');
  assert.equal(
    readFileSync(join(output, 'v/older/docs.html'), 'utf8'),
    'keep previous published snapshot'
  );
  assert.equal(readFileSync(join(output, 'v', id, 'docs.html'), 'utf8'), 'frozen candidate docs');
  const proof = JSON.parse(readFileSync(`${output}.assembly.json`, 'utf8'));
  assert.equal(proof.deployed, false);
  assert.equal(Object.keys(proof.preservedLiveFiles).length, 3);
  assert.equal(proof.sourceCommit, sourceCommit);
  // Idempotent frozen copy is preserved; an existing output is never overwritten.
  assert.equal(run(output, candidate, sha(receiptText), join(temp, 'same')).status, 0);
  assert.notEqual(run(live, candidate, sha(receiptText), output).status, 0);
  assert.equal(readFileSync(join(output, 'docs.html'), 'utf8'), 'unchanged live docs');
  write(join(candidate, 'docs.html'), 'tampered candidate');
  assert.notEqual(run(live, candidate, sha(receiptText), join(temp, 'tampered')).status, 0);
  assert.equal(existsSync(join(temp, 'tampered')), false);
  write(join(candidate, 'docs.html'), 'frozen candidate docs');
  assert.notEqual(run(live, candidate, 'b'.repeat(64), join(temp, 'bad-receipt')).status, 0);
  const wrongBase = JSON.stringify({ ...receipt, basePath: `/another-project/v/${id}` });
  write(join(candidate, 'snapshot.json'), wrongBase);
  assert.notEqual(run(live, candidate, sha(wrongBase), join(temp, 'wrong-base')).status, 0);
  const wrongId = JSON.stringify({ ...receipt, id: '../escape' });
  write(join(candidate, 'snapshot.json'), wrongId);
  assert.notEqual(run(live, candidate, sha(wrongId), join(temp, 'wrong-id')).status, 0);
  write(join(candidate, 'snapshot.json'), receiptText);
  write(join(candidate, 'unlisted.html'), 'not in receipt');
  assert.notEqual(run(live, candidate, sha(receiptText), join(temp, 'unlisted')).status, 0);
  rmSync(join(candidate, 'unlisted.html'));
  cpSync(candidate, join(live, 'v', id), { recursive: true });
  write(join(live, 'v', id, 'docs.html'), 'another snapshot at same immutable ID');
  const collision = run(live, candidate, sha(receiptText), join(temp, 'collision'));
  assert.notEqual(collision.status, 0);
  assert.match(collision.stderr, /Immutable snapshot collision/);
  assert.equal(existsSync(join(temp, 'collision')), false);
  assert.equal(existsSync(join(temp, 'collision.assembly-lock')), false);
  symlinkSync(join(candidate, 'docs.html'), join(candidate, 'linked'));
  assert.notEqual(run(live, candidate, sha(receiptText), join(temp, 'symlink')).status, 0);
  rmSync(join(candidate, 'linked'));
  // Dot-prefixed directory names must not bypass input/output containment.
  assert.notEqual(run(live, candidate, sha(receiptText), join(candidate, '..nested')).status, 0);
  assert.equal(existsSync(join(candidate, '..nested')), false);
  console.log(
    'Pages assembly: live bytes/history preserved, candidate hashes/identity checked, immutable collisions/unsafe output/symlinks refused; no deployment (host fixture)'
  );
} finally {
  rmSync(temp, { recursive: true, force: true });
}
