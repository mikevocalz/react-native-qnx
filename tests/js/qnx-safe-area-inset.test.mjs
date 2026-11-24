// @ts-nocheck: loads CommonJS modules through a Module._load hook and fakes the native class.
// The safe-area-context shim reads the title bar inset from the windows API
// and follows its changes; without the API (no window chrome) it stays at zero.
import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';
import path from 'node:path';

const require = createRequire(import.meta.url);
const root = path.resolve(import.meta.dirname, '../..');
const shimPath = path.join(root, 'packages/expo-router-qnx/src/shims/safe-area-context.js');
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const React = require('react');
const TestRenderer = require('react-test-renderer');

const state = { windows: null, error: null };
const originalLoad = Module._load;
Module._load = function load(request, ...rest) {
  if (request === 'react-native') {
    return {
      View: 'View',
      useWindowDimensions: () => ({ width: 800, height: 600 }),
    };
  }
  if (request === '@rn-qnx/qnx-nitro-runtime') {
    return {
      getQnxWindows() {
        if (state.error) throw state.error;
        return state.windows;
      },
    };
  }
  return originalLoad.call(this, request, ...rest);
};

function load() {
  delete require.cache[shimPath];
  return require(shimPath);
}

function fakeWindows(initial) {
  const api = {
    inset: initial,
    listeners: new Set(),
    removed: 0,
    getTitleBarInset: () => api.inset,
    addOnInsetChangedListener(listener) {
      api.listeners.add(listener);
      return () => {
        api.removed += 1;
        api.listeners.delete(listener);
      };
    },
  };
  return api;
}
const bar = {
  top: 32,
  right: 0,
  bottom: 0,
  left: 0,
  overlay: { x: 0, y: 0, width: 80, height: 32 },
};
const none = { top: 0, right: 0, bottom: 0, left: 0, overlay: { x: 0, y: 0, width: 0, height: 0 } };

async function readInsetsThroughProvider(shim) {
  let seen;
  function Probe() {
    seen = shim.useSafeAreaInsets();
    return null;
  }
  let renderer;
  await TestRenderer.act(async () => {
    renderer = TestRenderer.create(
      React.createElement(shim.SafeAreaProvider, null, React.createElement(Probe))
    );
  });
  return { renderer, read: () => seen };
}

// A host without window chrome: WINDOWS_UNAVAILABLE falls back to zero.
{
  state.error = Object.assign(new Error('no services'), { code: 'WINDOWS_UNAVAILABLE' });
  const shim = load();
  assert.deepEqual(shim.initialWindowMetrics.insets, { top: 0, right: 0, bottom: 0, left: 0 });
  const { renderer, read } = await readInsetsThroughProvider(shim);
  assert.deepEqual(read(), { top: 0, right: 0, bottom: 0, left: 0 });
  await TestRenderer.act(async () => renderer.unmount());
}

// Any other failure is not hidden.
{
  state.error = new Error('boom');
  assert.throws(() => load(), /boom/);
  state.error = null;
}

// With the API: the inset reaches useSafeAreaInsets and follows changes.
{
  const windows = fakeWindows(bar);
  state.windows = windows;
  const shim = load();
  assert.deepEqual(shim.initialWindowMetrics.insets, { top: 32, right: 0, bottom: 0, left: 0 });
  const { renderer, read } = await readInsetsThroughProvider(shim);
  assert.deepEqual(read(), { top: 32, right: 0, bottom: 0, left: 0 });
  assert.equal(windows.listeners.size, 1);
  windows.inset = none;
  await TestRenderer.act(async () => {
    for (const listener of windows.listeners) listener(none);
  });
  assert.deepEqual(read(), { top: 0, right: 0, bottom: 0, left: 0 });
  await TestRenderer.act(async () => renderer.unmount());
  assert.equal(windows.listeners.size, 0, 'the provider unsubscribes on unmount');
  assert.equal(windows.removed, 1);
}

Module._load = originalLoad;
console.log('qnx-safe-area-inset.test.mjs passed');
