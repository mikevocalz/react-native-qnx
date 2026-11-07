import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import vm from 'node:vm';

const root = process.cwd();
const exportsSource = await readFile(path.join(root, 'packages/react-native-qnx/Libraries/ReactNative/PublicAPI.js'), 'utf8');
/** @type {string[]} */
const loaded = [];
const values = { component: true };
const module = { exports: /** @type {any} */ ({}) };
vm.runInNewContext(exportsSource, {
  module,
  require: (/** @type {string} */ name) => {
    loaded.push(name);
    return { default: values, AppRegistry: values };
  },
});
assert.equal(loaded.length, 0, 'Public imports stay lazy until read');
const api = module.exports;
const expected = ['AppRegistry', 'View', 'Text', 'Image', 'ScrollView', 'FlatList', 'SectionList', 'TextInput', 'Pressable', 'ActivityIndicator', 'Switch', 'Modal', 'StyleSheet', 'Platform', 'Animated'];
assert.deepEqual(Object.keys(api), expected);
for (const name of expected) assert.equal(api[name], values);
assert.equal(loaded.length, expected.length);
for (const name of ['Alert', 'BackHandler', 'NonexistentExport']) {
  assert.throws(() => api[name], /not implemented/);
}
assert.equal(api.__esModule, undefined);
assert.equal(api.default, undefined);
assert.equal(api.then, undefined);
assert.equal(api[Symbol.toStringTag], undefined);
assert.ok(Object.isFrozen(api));

const resolverModule = { exports: /** @type {any} */ ({}) };
const resolverSource = await readFile(path.join(root, 'packages/metro-config-qnx/public-entry.js'), 'utf8');
vm.runInNewContext(resolverSource, {
  module: resolverModule,
  require: (/** @type {string} */ name) => {
    if (name === 'node:path') return path;
    if (name === '@rn-qnx/react-native-qnx') return { OVERRIDES_ROOT: '/fixture/Libraries' };
    throw new Error(`Unexpected dependency ${name}`);
  },
});
const wrap = resolverModule.exports;
const fallback = () => ({ type: 'sourceFile', filePath: '/original.js' });
const original = { resolver: { resolveRequest: fallback, platforms: ['ios', 'qnx', 'marathon'] } };
for (const platform of ['qnx', 'marathon']) {
  const config = wrap(original, platform);
  assert.notEqual(config, original);
  assert.equal(original.resolver.resolveRequest, fallback);
  assert.equal(config.resolver.resolveRequest({}, 'react-native', platform).filePath, '/fixture/Libraries/ReactNative/PublicAPI.js');
  for (const request of ['react-native/Libraries/Text/Text', 'react', './Platform']) {
    assert.equal(config.resolver.resolveRequest({}, request, platform).filePath, '/original.js');
  }
  for (const other of ['ios', 'android', platform === 'qnx' ? 'marathon' : 'qnx']) {
    assert.equal(config.resolver.resolveRequest({}, 'react-native', other).filePath, '/original.js');
  }
}
assert.throws(() => wrap(original, 'linux'), /qnx or marathon/);
assert.equal(wrap({}).resolver.resolveRequest({ resolveRequest: fallback }, 'react', 'qnx').filePath, '/original.js');
console.log('public-entry lazy exports, explicit unsupported errors, resolver opt-in and other-platform preservation passed (host VM only)');
