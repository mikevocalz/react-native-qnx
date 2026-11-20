import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { readFile } from 'node:fs/promises';

// docs/getting-started/public-react-native-entry.md lists every export of the
// public entry. Adding, removing or re-pointing an export in PublicAPI.js must
// change the table in the same commit.
const source = await readFile(
  'packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js',
  'utf8'
);
const doc = await readFile('docs/getting-started/public-react-native-entry.md', 'utf8');
const support = JSON.parse(await readFile('docs/component-support.json', 'utf8'));
const hostByName = new Map(
  support.components.map((/** @type {{name: string, host: string}} */ c) => [c.name, c.host])
);

/** @type {Map<string, string>} */
const exported = new Map();
const getter = /get (\w+)\(\) \{[\s\S]*?require\('react-native\/Libraries\/([^']+)'\)/g;
for (const match of source.matchAll(getter)) exported.set(match[1], match[2]);
assert.ok(exported.size > 0, 'no getters found in PublicAPI.js');

/** @type {Map<string, string[]>} */
const documented = new Map();
for (const line of doc.split('\n')) {
  const cells = line.split('|').map((cell) => cell.trim());
  if (cells.length < 7 || !/^`\w+`$/.test(cells[1])) continue;
  documented.set(cells[1].slice(1, -1), cells.slice(2, 6));
}

for (const [name, modulePath] of exported) {
  const row = documented.get(name);
  assert.ok(row, `PublicAPI.js exports ${name}; add it to public-react-native-entry.md`);
  const [backing, origin, supportRow] = row;
  assert.equal(backing, `\`${modulePath}\``, `${name}: backing module is stale`);
  const override = existsSync(`packages/react-native-qnx/Libraries/${modulePath}.js`);
  assert.equal(origin, override ? 'QNX override' : 'upstream', `${name}: Source column is stale`);
  assert.equal(supportRow, hostByName.get(name) ?? 'none', `${name}: Support row column is stale`);
}
for (const name of documented.keys()) {
  assert.ok(exported.has(name), `${name} is documented but PublicAPI.js does not export it`);
}
assert.match(
  doc,
  new RegExp(`It exports ${exported.size} names`),
  `the export count in the doc must be ${exported.size}`
);

// Components that have a support row but no export are listed by name.
const unexported = support.components
  .map((/** @type {{name: string}} */ c) => c.name)
  .filter((/** @type {string} */ n) => !exported.has(n));
const section = doc.slice(doc.indexOf('## Names with a support row'));
for (const name of unexported) {
  assert.ok(
    section.includes(`\`${name}\``),
    `${name} has a support row, is not exported and is not listed`
  );
}

console.log(`public entry doc matches PublicAPI.js (${exported.size} exports)`);
