import assert from 'node:assert/strict';
import vm from 'node:vm';
import { createRequire } from 'node:module';

const babelRequire = createRequire(import.meta.url);
const { transformFileSync } = babelRequire('@babel/core');

// Execute the exact Babel-generated UI worklet. A nested, separately transformed
// callback used to capture its own uninitialized binding and lose mutable frame
// state. This is a JS transform regression, not a native frame/dispatcher test.
const file = 'apps/component-gallery/motion/WorkletsProof.js';
const transformed = transformFileSync(file, {
  configFile: false,
  babelrc: false,
  plugins: ['react-native-worklets/plugin'],
});
assert.ok(transformed?.code);
/** @type {Array<(timestamp: number) => void>} */
const frames = [];
/** @typedef {{frames: number, role: string, worstFrameMs: number, stats: {delivered: number}}} Report */
/** @type {Report[]} */
const reports = [];
const module = { exports: /** @type {any} */ ({}) };
const sandbox = {
  module,
  require(/** @type {string} */ name) {
    if (name === 'react') return { createElement() {} };
    if (name === 'react-native') return {};
    if (name === 'zustand' || name === 'zustand/vanilla') return {};
    if (name === 'react-native-worklets') {
      return {
        scheduleOnRN: (
          /** @type {(result: Report) => void} */ callback,
          /** @type {Report} */ result
        ) => callback(result),
      };
    }
    throw new Error(`Unexpected module ${name}`);
  },
};
vm.createContext(sandbox);
vm.runInContext('globalThis.global = globalThis;', sandbox);
vm.runInContext(transformed.code + '\nmodule.exports.probe = startFrameProbe;', sandbox);
const probe = module.exports.probe;
assert.ok(probe.__initData?.code, 'The entry must be transformed by the installed Worklets plugin');
const ui = vm.createContext({
  requestAnimationFrame(/** @type {(timestamp: number) => void} */ callback) {
    assert.equal(typeof callback, 'function', 'Every frame must re-arm a callable callback');
    frames.push(callback);
  },
  __rnqnxThreadRole: () => 'ui-worklet',
  __rnqnxFrameStats: () => ({ delivered: 30 }),
});
ui.global = ui;
const entry = vm.runInContext(probe.__initData.code, ui);
entry.call({ __closure: probe.__closure }, 30, (/** @type {Report} */ result) =>
  reports.push(result)
);
for (let n = 1; n <= 30; n += 1) {
  assert.equal(frames.length, 1, `Exactly one pending callback before frame ${n}`);
  const callback = frames.shift();
  assert.ok(callback);
  callback(16 * n);
}
assert.equal(frames.length, 0, 'The probe stops after its requested frame count');
assert.equal(reports.length, 1, 'The result is delivered once');
assert.equal(reports[0].frames, 30);
assert.equal(reports[0].role, 'ui-worklet');
assert.equal(reports[0].worstFrameMs, 16);
assert.equal(reports[0].stats.delivered, 30);
console.log('Worklets frame probe: transformed UI closure completes 30 frames (host VM only)');
