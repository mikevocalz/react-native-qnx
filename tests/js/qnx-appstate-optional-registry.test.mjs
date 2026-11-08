import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const file = new URL('../../packages/react-native-qnx/Libraries/AppState/NativeAppState.js', import.meta.url);
const source = await readFile(file, 'utf8');
const moduleName = 'react-native/Libraries/TurboModule/TurboModuleRegistry';

function load(module) {
  const exports = { exports: {} };
  let lookups = 0;
  vm.runInNewContext(source, {
    module: exports,
    require(name) {
      assert.equal(name, moduleName);
      return {
        get(name) {
          assert.equal(name, 'AppState');
          lookups++;
          return module;
        },
        getEnforcing() {
          throw new Error('AppState is optional on unqualified native targets');
        },
      };
    },
  });
  assert.equal(lookups, 1);
  return exports.exports;
}

assert.equal(load(null), null, 'Disabled native capability must be unavailable');
const actual = Object.freeze({ getConstants: () => ({ initialAppState: 'active' }) });
assert.equal(load(actual), actual, 'Supported capability must be the actual registered module');
assert.doesNotMatch(source, /getEnforcing|fake\s*module|dummy\s*module/i);
console.log('AppState optional registry: null when disabled, actual native module when enabled (host VM only).');
