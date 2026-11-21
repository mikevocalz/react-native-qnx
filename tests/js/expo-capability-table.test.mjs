import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import {
  CAPABILITY,
  SUPPORT_FILE,
  TABLE_FILE,
  capabilityOf,
  renderCapabilityTable,
  repositoryTable,
} from '../../scripts/expo-capability-table.mjs';

const list = JSON.parse(await readFile(SUPPORT_FILE, 'utf8'));
let checks = 0;
const ok = (/** @type {unknown} */ v, /** @type {string} */ m) => {
  assert.ok(v, m);
  checks++;
};

// Every status in the committed list has a capability label.
for (const status of new Set(list.packages.map((/** @type {{status: string}} */ p) => p.status)))
  ok(CAPABILITY[/** @type {keyof typeof CAPABILITY} */ (status)], `label for ${status}`);
assert.throws(() => capabilityOf('invented'), /No capability label/);
checks++;

// The committed file is the generated output, and every package has exactly one row.
const generated = await repositoryTable();
ok((await readFile(TABLE_FILE, 'utf8')) === generated, 'committed table equals generated table');
for (const p of list.packages)
  ok(generated.split('\n').filter((l) => l.startsWith(`| \`${p.name}\` |`)).length === 1, p.name);

// The counts line adds up to the package count.
const counts = generated.match(/^Counts: (.+)\.$/m)?.[1] ?? '';
const total = [...counts.matchAll(/(\d+) /g)].reduce((n, m) => n + Number(m[1]), 0);
assert.equal(total, list.packages.length);
checks++;

// Known rows land in the expected group.
const label = (/** @type {string} */ name) =>
  generated.match(new RegExp(`^\\| \`${name}\` \\| [^|]+ \\| ([^|]+) \\|`, 'm'))?.[1].trim();
assert.equal(label('react-native'), 'works');
assert.equal(label('expo-router'), 'shim');
assert.equal(label('expo-image'), 'shim');
assert.equal(label('expo-audio'), 'unsupported');
assert.equal(label('@expo/metro-runtime'), 'JS-only');
checks += 5;

// A changed status changes the output; a pipe in a reason cannot split a row.
const edited = structuredClone(list);
edited.packages[0].status = 'js-only';
edited.packages[0].reason = 'a | b';
const comment = (/** @type {string} */ t) => `<!-- ${t} -->`;
const changed = renderCapabilityTable(edited, { comment });
ok(changed !== generated, 'edited list renders differently');
ok(changed.includes('a \\| b'), 'pipe escaped');

// --check passes on the committed file and fails on a stale one.
const cli = new URL('../../scripts/expo-capability-table.mjs', import.meta.url).pathname;
const pass = spawnSync(process.execPath, [cli, '--check'], { encoding: 'utf8' });
assert.equal(pass.status, 0, pass.stderr);
assert.equal(spawnSync(process.execPath, [cli], { encoding: 'utf8' }).status, 2);
checks += 2;

console.log(`expo-capability-table: ${checks} checks passed.`);
