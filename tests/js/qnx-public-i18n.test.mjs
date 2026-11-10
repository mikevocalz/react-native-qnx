import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const preset = require.resolve('babel-preset-expo');
const { transformSync } = createRequire(preset)('@babel/core');
const upstreamFile = require.resolve('react-native/Libraries/ReactNative/I18nManager');
const upstreamRequire = createRequire(upstreamFile);
const source = await readFile(upstreamFile, 'utf8');
const transformed = transformSync(source, {
  filename: upstreamFile,
  configFile: false,
  babelrc: false,
  presets: [preset],
});

// Evaluate the installed RN implementation with its optional native module
// absent. This verifies RN's actual fallback, not a new QNX no-op provider.
const module = { exports: /** @type {any} */ ({}) };
vm.runInNewContext(transformed.code, {
  module,
  exports: module.exports,
  require(/** @type {string} */ name) {
    if (name === './NativeI18nManager') return { __esModule: true, default: null };
    return upstreamRequire(name);
  },
});
const manager = module.exports.default;
assert.equal(manager.isRTL, false);
assert.equal(manager.doLeftAndRightSwapInRTL, true);
assert.deepEqual(JSON.parse(JSON.stringify(manager.getConstants())), {
  isRTL: false,
  doLeftAndRightSwapInRTL: true,
});
assert.equal(manager.allowRTL(true), undefined);
assert.equal(manager.forceRTL(true), undefined);
assert.equal(manager.swapLeftAndRightInRTL(false), undefined);
assert.equal(manager.isRTL, false, 'No native RTL provider means mutations cannot change layout');
assert.equal(manager.doLeftAndRightSwapInRTL, true);
console.log('public I18nManager: installed RN fixed-LTR fallback passed (no native RTL support)');
