import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import vm from 'node:vm';

const root = process.cwd();
const app = path.join(root, 'apps/component-conformance');
const require = createRequire(import.meta.url);
const fixture = require(path.join(app, 'fixtures.js'));
/** @type {Array<{ id: string, status: string }>} */
const SCENARIOS = fixture.SCENARIOS;
const { createDocumentRows } = fixture;

assert.equal(SCENARIOS.length, 10);
assert.equal(new Set(SCENARIOS.map((scenario) => scenario.id)).size, SCENARIOS.length);
assert.equal(SCENARIOS.filter((scenario) => scenario.status === 'not-run').length, 5);
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
assert.match(entry, /require\('react-native'\)/);
assert.match(entry, /require\('\.\/fixtures\.js'\)/);
assert.doesNotMatch(entry, /react-native\/Libraries\//);
assert.match(entry, /page: 'gallery'/);
for (const component of [
  'Image',
  'Modal',
  'SectionList',
  'FlatList',
  'TextInput',
  'Switch',
  'ActivityIndicator',
]) {
  assert.ok(
    new RegExp('\\bh\\(\\s*' + component + ',').test(entry),
    'Missing component render: ' + component
  );
}
const config = JSON.parse(await readFile(path.join(app, 'app.json'), 'utf8'));
assert.equal(config.expo.name, 'QNX Components');
assert.equal(config.expo.extra.rnqnx.appId, 'org.rnqnx.showcase');
assert.equal(config.expo.extra.rnqnx.componentName, 'RNQnxPlayground');
const png = await readFile(path.join(app, config.expo.icon));
assert.equal(png.subarray(0, 8).toString('hex'), '89504e470d0a1a0a');
assert.equal(png.readUInt32BE(16), 1024);
assert.equal(png.readUInt32BE(20), 1024);
assert.doesNotMatch(entry, /nitro-forge|NitroForge|premium\/assets/i);

// Only JS registration/state is exercised here, not a renderer or native input.
const registrations = new Map();
const sandbox = vm.createContext({
  /** @param {string} name */
  require(name) {
    if (name === 'react-native') {
      return {
        AppRegistry: {
          /** @param {string} name @param {() => {name: string}} factory */
          registerComponent: (name, factory) => registrations.set(name, factory),
        },
      };
    }
    if (name === './fixtures.js') return fixture;
    return require(name);
  },
});
vm.runInContext(entry, sandbox, { timeout: 1000 });
assert.deepEqual([...registrations.keys()], ['RNQnxPlayground']);
assert.equal(registrations.get('RNQnxPlayground')().name, 'ConformanceApp');
const store = vm.runInContext('useConformance', sandbox);
assert.equal(store.getState().page, 'gallery');
assert.equal(store.getState().modal, false);
store.getState().press();
store.getState().press();
store.getState().setModal(true);
store.getState().setText('Synthetic edited text');
store.getState().setToggle(true);
store.getState().goTo('sections');
store.getState().chooseRow('document-0021');
store.getState().goTo('gallery');
assert.equal(store.getState().presses, 2);
assert.equal(store.getState().text, 'Synthetic edited text');
assert.equal(store.getState().toggle, true);
assert.equal(store.getState().row, 'document-0021');
assert.equal(store.getState().modal, true);
store.getState().setModal(false);
assert.equal(store.getState().modal, false);
assert.equal(store.getState().presses, 2);
let widthUpdates = 0;
const unsubscribe = store.subscribe(() => widthUpdates++);
store.getState().setWidth(1920);
store.getState().setWidth(1920);
assert.equal(widthUpdates, 1);
unsubscribe();

const metro = await readFile(path.join(app, 'metro.config.js'), 'utf8');
assert.match(metro, /getQnxMetroConfig/);
assert.match(metro, /workspaceRoot/);
assert.match(metro, /withPublicEntry/);

console.log(
  'qnx-component-conformance.test.mjs passed (deterministic fixtures, JS registration/state retention, public entry/assets; no renderer or QNX run)'
);
