import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const app = path.join(root, 'apps/component-conformance');
const require = createRequire(import.meta.url);
const { SCENARIOS, createDocumentRows } = require(path.join(app, 'fixtures.js'));


assert.equal(SCENARIOS.length, 8);
assert.equal(new Set(SCENARIOS.map((scenario) => scenario.id)).size, SCENARIOS.length);
assert.equal(SCENARIOS.filter((scenario) => scenario.status === 'not-run').length, 3);
assert.equal(SCENARIOS.filter((scenario) => scenario.status === 'blocked').length, 5);
assert.ok(SCENARIOS.every((scenario) => scenario.status !== 'passed'));

const rows = createDocumentRows();
assert.equal(rows.length, 1000);
assert.deepEqual(rows[0], {
  id: 'document-0001',
  title: 'Document 0001',
  subtitle: 'Synthetic conformance item 0001',
});
assert.equal(rows[499].id, 'document-0500');
assert.equal(rows.at(-1).id, 'document-1000');
assert.deepEqual(createDocumentRows(3), rows.slice(0, 3));
for (const bad of [0, -1, 1001, 1.5, NaN, Infinity, '1000']) {
  assert.throws(() => createDocumentRows(bad), RangeError);
}

const entry = await readFile(path.join(app, 'index.js'), 'utf8');
assert.match(entry, /AppRegistry\.registerComponent\('RNQnxPlayground'/);
assert.match(entry, /require\('zustand'\)/);
assert.match(entry, /require\('react-native\/Libraries\/Lists\/FlatList'\)/);
assert.match(entry, /require\('\.\/fixtures\.js'\)/);
assert.doesNotMatch(entry, /require\('react-native'\)/);
assert.doesNotMatch(entry, /nitro-forge|NitroForge|premium\/assets/i);

const metro = await readFile(path.join(app, 'metro.config.js'), 'utf8');
assert.match(metro, /getQnxMetroConfig/);
assert.match(metro, /workspaceRoot/);

console.log(
  'qnx-component-conformance.test.mjs passed (deterministic fixtures, no false device passes, source-checkout entry contract; no QNX run)'
);
