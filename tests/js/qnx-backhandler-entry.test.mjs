import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const source = readFileSync('packages/react-native-qnx/Libraries/Utilities/BackHandler.js', 'utf8');
const dispatcher = { addEventListener() {}, exitApp() {} };
const loaded = /** @type {string[]} */ ([]);
const module = { exports: /** @type {any} */ ({}) };
vm.runInNewContext(source, {
  module,
  /** @param {string} name */
  require(name) {
    loaded.push(name);
    return { default: name.endsWith('BackHandler.android') ? dispatcher : {} };
  },
});
assert.equal(module.exports.default, dispatcher);
assert.equal(module.exports, dispatcher);
assert.equal(loaded.length, 2);
const disabled = { exports: /** @type {any} */ ({}) };
assert.doesNotThrow(() =>
  vm.runInNewContext(source, {
    module: disabled,
    require: () => ({ default: null }),
  })
);
assert.throws(() => disabled.exports.addEventListener(), /RNQNX_ENABLE_BACK_HANDLER/);
assert.throws(() => disabled.exports.exitApp(), /RNQNX_ENABLE_BACK_HANDLER/);

// The platform package must not depend on a hoisted Babel runtime helper merely
// to forward RN's dispatcher. Transform with the actual repository Babel preset.
const presetRequire = createRequire(require.resolve('babel-preset-expo/package.json'));
const babel = presetRequire('@babel/core');
const preset = require.resolve('babel-preset-expo');
const transformed = babel.transformSync(source, {
  filename: 'BackHandler.js',
  babelrc: false,
  configFile: false,
  presets: [preset],
}).code;
assert.doesNotMatch(transformed, /@babel\/runtime\/helpers/);
console.log('BackHandler CommonJS forwarding avoids an undeclared Babel helper dependency.');
