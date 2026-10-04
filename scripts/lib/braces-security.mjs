import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import vm from 'node:vm';

export const advisoryId = 'GHSA-vfj7-8cjw-p6xm';

// npm braces 3.0.3, with the lib-only backport from upstream PR #72,
// commit 28d440b5dd449dbf1fe6f3506cf94ecca4d02660. See docs/security/braces.md.
const expectedHashes = {
  'package.json': '56f08b888a4f30dc7cf8a7dbb36ffe92b737912ba36abe9d069d32167c957ac7',
  'index.js': '332ea07c7b006361aad12aa994ca75dc1db8e8382b884909e2f38f10b85c88a4',
  'lib/compile.js': 'b651f7715e6db8942ce61d3394357b4d81c8ece88240aa31a458ea1165edd195',
  'lib/constants.js': 'f9fb688959232eee3e6ad7906a5b0e3234815db49ee857ef86983d65b917dc7c',
  'lib/expand.js': '7ea3e14c2b2b256ef244fd3d83b8fcaa20aa2232b4e6d768c3bb6ab567f66cf5',
  'lib/parse.js': '72aabaadaa555cdfbd07fbd7c7f743373e4dc8eec04a97550cc57bbeec30eb6c',
  'lib/stringify.js': '49dc2d8bafa74f34715a18a845bcb82ce66caaf3bab4cf117998e06b1f9a50a9',
  'lib/utils.js': 'b5a7596aa67730412b3c029ef09e84e6b67b8e445cffd35d1d295549c89066c7',
};

/** @param {unknown} tree */
export function installedBraces(tree) {
  /** @type {Set<string>} */
  const paths = new Set();
  /** @param {any} node */
  function visit(node) {
    if (!node || typeof node !== 'object') return;
    for (const group of ['dependencies', 'devDependencies', 'optionalDependencies']) {
      for (const [name, dependency] of Object.entries(node[group] ?? {})) {
        if (name === 'braces') {
          assert.equal(
            dependency.version,
            '3.0.3',
            'Review mitigation when braces version changes'
          );
          assert.equal(typeof dependency.path, 'string', 'Missing installed braces path');
          paths.add(dependency.path);
        }
        visit(dependency);
      }
    }
  }
  assert.ok(Array.isArray(tree) && tree.length > 0, 'Missing pnpm dependency inventory');
  for (const project of tree) visit(project);
  assert.ok(paths.size > 0, 'No installed braces found; review/remove the temporary mitigation');
  return [...paths];
}

/** @param {string} packagePath */
export function verifyBraces(packagePath) {
  const manifest = JSON.parse(readFileSync(path.join(packagePath, 'package.json'), 'utf8'));
  assert.equal(manifest.name, 'braces');
  assert.equal(manifest.version, '3.0.3');
  for (const [file, expected] of Object.entries(expectedHashes)) {
    const actual = createHash('sha256')
      .update(readFileSync(path.join(packagePath, file)))
      .digest('hex');
    assert.equal(actual, expected, `Unverified braces implementation: ${file}`);
  }
  const require = createRequire(path.join(packagePath, 'package.json'));
  const braces = require(path.join(packagePath, 'index.js'));
  for (const method of ['parse', 'compile', 'expand', 'stringify']) {
    for (const [open, close] of [
      ['{', '}'],
      ['(', ')'],
    ]) {
      const valid = open.repeat(100) + 'a' + close.repeat(100);
      const invalid = open.repeat(101) + 'a' + close.repeat(101);
      assert.doesNotThrow(() => braces[method](valid));
      for (const options of [{}, { maxDepth: 10000 }, { maxDepth: Infinity }, { maxDepth: NaN }]) {
        assert.throws(() => braces[method](invalid, options), /exceeds max depth/);
      }
      assert.throws(
        () => braces[method](open.repeat(4000) + 'a' + close.repeat(4000)),
        /exceeds max depth/
      );
      assert.doesNotThrow(() => braces[method](open + 'a' + close, { maxDepth: 1.5 }));
      assert.throws(
        () => braces[method](open.repeat(2) + 'a' + close.repeat(2), { maxDepth: 1.5 }),
        /exceeds max depth/
      );
    }
  }
  for (const method of ['compile', 'expand', 'stringify']) {
    /** @type {any} */
    let ast = { type: 'text', value: 'a' };
    for (let i = 0; i < 101; i++) ast = { type: 'brace', nodes: [ast] };
    assert.throws(() => braces[method]({ type: 'root', nodes: [ast] }), /exceeds max depth/);
    /** @type {any} */
    const cycle = { type: 'root', nodes: [] };
    cycle.nodes.push(cycle);
    assert.throws(() => braces[method](cycle), /exceeds max depth/);
  }
  /** @type {any} */
  const ast = { type: 'paren', nodes: [{ type: 'text', value: 'a' }] };
  ast.parent = ast;
  assert.throws(
    () => vm.runInNewContext('braces.expand(ast)', { braces, ast }, { timeout: 1000 }),
    /parent chain contains a cycle/
  );
  assert.deepEqual(braces.expand('foo/({a,b})'), ['foo/(a)', 'foo/(b)']);
  assert.deepEqual(braces.expand('file{1..3}.{js,ts}'), [
    'file1.js',
    'file1.ts',
    'file2.js',
    'file2.ts',
    'file3.js',
    'file3.ts',
  ]);
  for (const pattern of ['{{a}}', '{a,{b}}', '{{x}y}', '{a,{b,{c}}', '{}{a}']) {
    assert.equal(braces.stringify(braces.parse(pattern), { escapeInvalid: true }), pattern);
  }
}

/**
 * Classify only after every installed braces implementation has been verified.
 * @param {any} report
 */
export function classifyAudit(report) {
  const isRecord = (/** @type {any} */ value) =>
    value !== null && typeof value === 'object' && !Array.isArray(value);
  assert.ok(
    isRecord(report) && !('error' in report) && isRecord(report.advisories),
    'Invalid or failed registry audit'
  );
  assert.ok(isRecord(report.metadata?.vulnerabilities), 'Missing registry audit metadata');
  for (const severity of ['info', 'low', 'moderate', 'high', 'critical']) {
    const count = report.metadata.vulnerabilities[severity];
    assert.ok(Number.isInteger(count) && count >= 0, 'Malformed audit vulnerability counts');
  }
  const blocked = [];
  let mitigated = 0;
  for (const advisory of Object.values(report.advisories)) {
    assert.ok(advisory && typeof advisory === 'object', 'Malformed advisory');
    const item = /** @type {any} */ (advisory);
    assert.ok(
      ['info', 'low', 'moderate', 'high', 'critical'].includes(item.severity),
      'Unknown advisory severity'
    );
    if (item.github_advisory_id === advisoryId) {
      assert.equal(item.module_name, 'braces');
      assert.equal(item.severity, 'high', 'Review changed advisory severity');
      assert.equal(
        item.patched_versions,
        null,
        'An upstream release is available; remove the temporary mitigation'
      );
      assert.equal(item.vulnerable_versions, '<=3.0.3');
      assert.ok(
        Array.isArray(item.findings) && item.findings.length > 0,
        'Missing braces findings'
      );
      for (const finding of item.findings) {
        assert.equal(finding.version, '3.0.3', 'Unverified braces advisory version');
        assert.ok(
          Array.isArray(finding.paths) && finding.paths.length > 0,
          'Missing braces dependency paths'
        );
        assert.ok(
          finding.paths.every(
            (/** @type {unknown} */ p) => typeof p === 'string' && p.endsWith('>braces')
          ),
          'Unexpected braces dependency path'
        );
      }
      mitigated++;
    } else if (['moderate', 'high', 'critical'].includes(item.severity)) {
      blocked.push(item);
    }
  }
  return { blocked, mitigated };
}
