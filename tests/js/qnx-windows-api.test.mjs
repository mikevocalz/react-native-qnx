// @ts-nocheck: loads CommonJS modules through a Module._load hook and fakes the native class.
// The JS half of the windows API: error mapping, guard wrapping, listener
// removal and the singleton, against a fake native class that records calls.
import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';

const require = createRequire(import.meta.url);

/** @type {{ construct: () => object }} */
const host = {
  construct: () => {
    throw new Error('WINDOWS_UNAVAILABLE: no services');
  },
};
const originalLoad = Module._load;
Module._load = function load(request, ...rest) {
  if (request === 'nitro-core') {
    return {
      NitroModules: {
        getHybridClass(name) {
          assert.equal(name, 'QnxWindows');
          return function QnxWindows() {
            return host.construct();
          };
        },
      },
    };
  }
  return originalLoad.call(this, request, ...rest);
};

function freshModule() {
  const path = require.resolve('../../packages/qnx-nitro-runtime/index.js');
  delete require.cache[path];
  return require(path);
}

function fakeNative() {
  const calls = [];
  const native = {
    calls,
    capabilities: ['close', 'minimize'],
    guards: new Map(),
    listeners: new Map(),
    nextId: 0,
    answers: [],
    getState: () => ({
      sequence: 3,
      title: 't',
      width: 800,
      height: 600,
      x: 1,
      y: 2,
      focused: true,
      visible: true,
      maximized: false,
      controlsLayout: 'macos',
    }),
    setTitle: async (title) => {
      calls.push(['setTitle', title]);
    },
    setControlsLayout: async (layout) => {
      if (layout !== 'macos' && layout !== 'windows') throw new Error('INVALID_ARGUMENT: layout');
    },
    setBounds: async (rect) => rect,
    minimize: async () => {
      throw new Error('WINDOW_ACTION_UNSUPPORTED: minimize is not available');
    },
    close: async (force) => {
      calls.push(['close', force]);
      return true;
    },
    addCloseGuard(cb) {
      const id = ++native.nextId;
      native.guards.set(id, cb);
      return id;
    },
    resolveCloseGuard(requestId, guardId, allow) {
      native.answers.push([requestId, guardId, allow]);
    },
    addOnFocusChangedListener(cb) {
      const id = ++native.nextId;
      native.listeners.set(id, cb);
      return id;
    },
    addOnVisibilityChangedListener(cb) {
      const id = ++native.nextId;
      native.listeners.set(id, cb);
      return id;
    },
    removeListener(id) {
      calls.push(['removeListener', id]);
      native.guards.delete(id);
      native.listeners.delete(id);
    },
  };
  return native;
}

const tick = () => new Promise((resolve) => setImmediate(resolve));

// Unavailable host: the first call throws WINDOWS_UNAVAILABLE with a code.
{
  const { getQnxWindows } = freshModule();
  assert.throws(
    () => getQnxWindows(),
    (e) => e.code === 'WINDOWS_UNAVAILABLE' && !/WINDOWS_UNAVAILABLE:/.test(e.message)
  );
}

// Singleton and state mapping.
{
  const native = fakeNative();
  let constructed = 0;
  host.construct = () => {
    constructed += 1;
    return native;
  };
  const { getQnxWindows } = freshModule();
  const windows = getQnxWindows();
  assert.equal(getQnxWindows(), windows);
  assert.equal(constructed, 1);
  assert.deepEqual([...windows.capabilities], ['close', 'minimize']);
  assert.deepEqual(windows.getState(), {
    sequence: 3,
    title: 't',
    size: { width: 800, height: 600 },
    position: { x: 1, y: 2 },
    focused: true,
    visible: true,
    maximized: false,
    controlsLayout: 'macos',
  });

  // `CODE: reason` becomes error.code.
  await assert.rejects(
    windows.minimize(),
    (e) => e.code === 'WINDOW_ACTION_UNSUPPORTED' && e.message === 'minimize is not available'
  );
  await assert.rejects(windows.setControlsLayout('bogus'), (e) => e.code === 'INVALID_ARGUMENT');
  await windows.setTitle('x');
  assert.deepEqual(native.calls.at(-1), ['setTitle', 'x']);
  assert.equal(await windows.close(), true);
  assert.deepEqual(native.calls.at(-1), ['close', false]);
  await windows.close({ force: true });
  assert.deepEqual(native.calls.at(-1), ['close', true]);
  assert.deepEqual(await windows.setBounds({ x: 1, y: 2, width: 3, height: 4 }), {
    x: 1,
    y: 2,
    width: 3,
    height: 4,
  });
}

// Guards: sync guards become Promises, request fields map, only `true` allows,
// deadlineMs -1 is absent, throwing guards cancel and report once.
{
  const native = fakeNative();
  host.construct = () => native;
  const { getQnxWindows } = freshModule();
  const windows = getQnxWindows();
  const seen = [];
  const reported = [];
  globalThis.reportError = (e) => reported.push(e);
  let mode = 'sync-true';
  const off = windows.addCloseGuard((request) => {
    seen.push(request);
    if (mode === 'sync-true') return true;
    if (mode === 'async-false') return Promise.resolve(false);
    if (mode === 'undefined') return undefined;
    if (mode === 'throws') throw new Error('boom');
    return Promise.reject(new Error('rejected'));
  });
  const [guardId, cb] = [...native.guards.entries()][0];
  cb(10, guardId, 'control', true, -1);
  await tick();
  cb(11, guardId, 'system', false, 3000);
  await tick();
  assert.deepEqual(seen[0], { reason: 'control', cancellable: true });
  assert.deepEqual(seen[1], { reason: 'system', cancellable: false, deadlineMs: 3000 });
  assert.deepEqual(native.answers, [
    [10, guardId, true],
    [11, guardId, true],
  ]);
  for (const [m, id] of [
    ['async-false', 12],
    ['undefined', 13],
    ['throws', 14],
    ['rejects', 15],
  ]) {
    mode = m;
    cb(id, guardId, 'app', true, -1);
    await tick();
  }
  assert.deepEqual(native.answers.slice(2), [
    [12, guardId, false],
    [13, guardId, false],
    [14, guardId, false],
    [15, guardId, false],
  ]);
  assert.equal(reported.length, 2); // throw and reject, once each
  // Removal is idempotent.
  off();
  off();
  assert.equal(native.calls.filter((c) => c[0] === 'removeListener').length, 1);
  // A guard answer after teardown must not throw.
  native.resolveCloseGuard = () => {
    throw new Error('WINDOWS_DISPOSED: gone');
  };
  mode = 'sync-true';
  assert.doesNotThrow(() => cb(16, guardId, 'control', true, -1));
  await tick();
  assert.throws(
    () => windows.addCloseGuard('nope'),
    (e) => e.code === 'INVALID_ARGUMENT'
  );
  delete globalThis.reportError;
}

// Listeners pass booleans through and unsubscribe once.
{
  const native = fakeNative();
  host.construct = () => native;
  const { getQnxWindows } = freshModule();
  const windows = getQnxWindows();
  const focus = [];
  const off = windows.addOnFocusChangedListener((v) => focus.push(v));
  const visible = [];
  windows.addOnVisibilityChangedListener((v) => visible.push(v));
  const [[, focusCb], [, visibleCb]] = [...native.listeners.entries()];
  focusCb(false);
  visibleCb(true);
  assert.deepEqual(focus, [false]);
  assert.deepEqual(visible, [true]);
  off();
  off();
  assert.equal(native.calls.filter((c) => c[0] === 'removeListener').length, 1);
  // After teardown removal does not throw.
  native.removeListener = () => {
    throw new Error('WINDOWS_DISPOSED: gone');
  };
  assert.doesNotThrow(() => windows.addOnFocusChangedListener(() => {})());
}

Module._load = originalLoad;
console.log('qnx-windows-api.test.mjs passed');
