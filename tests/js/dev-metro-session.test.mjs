import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import test from 'node:test';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const rnRequire = createRequire(require.resolve('react-native/package.json'));
const clientPath = rnRequire.resolve('metro-runtime/src/modules/HMRClient');
const { startMetroSession } = require('../../packages/react-native-qnx/dev/MetroDevSession.js');

function harness() {
  /** @type {any[]} */
  const sockets = [];
  const timers = new Map();
  const heartbeats = new Map();
  /** @type {any[]} */
  const reports = [];
  /** @type {any[]} */
  const reloads = [];
  /** @type {any[]} */
  const injections = [];
  let nextId = 0;
  const state = { count: 7, label: 'before' };
  class WebSocket {
    constructor(/** @type {any} */ url) {
      this.url = url;
      /** @type {any[]} */
      this.sent = [];
      this.closed = 0;
      /** @type {any} */
      this.onopen = undefined;
      /** @type {any} */
      this.onmessage = undefined;
      /** @type {any} */
      this.onclose = undefined;
      sockets.push(this);
    }
    send(/** @type {any} */ value) {
      this.sent.push(JSON.parse(value));
    }
    close() {
      ++this.closed;
    }
    open() {
      this.onopen?.();
    }
    message(/** @type {any} */ type, /** @type {any} */ body) {
      this.onmessage?.({ data: JSON.stringify({ type, body }) });
    }
    disconnect() {
      this.onclose?.({ code: 1006 });
    }
  }
  const context = vm.createContext({
    module: { exports: {} },
    require: createRequire(clientPath),
    global: { WebSocket, state },
    setInterval: (/** @type {any} */ callback, /** @type {any} */ delay) => {
      const id = ++nextId;
      heartbeats.set(id, { callback, delay });
      return id;
    },
    clearInterval: (/** @type {any} */ id) => heartbeats.delete(id),
  });
  context.global.globalEvalWithSourceUrl = (
    /** @type {any} */ source,
    /** @type {any} */ sourceURL
  ) => {
    injections.push({ source, sourceURL });
    return vm.runInContext(source, context, { filename: sourceURL });
  };
  vm.runInContext(readFileSync(clientPath, 'utf8'), context, { filename: clientPath });
  const config = Object.freeze({
    bundleUrl: 'http://localhost:8081/index.bundle?platform=qnx&dev=true&hot=true',
    hotUrl: 'ws://localhost:8081/hot',
    generation: 12,
  });
  const session = startMetroSession({
    Client: context.module.exports,
    config,
    report: (/** @type {any} */ kind, /** @type {any} */ message) =>
      reports.push({ kind, message }),
    reload: (/** @type {any} */ reason) => reloads.push(reason),
    schedule: (/** @type {any} */ callback, /** @type {any} */ delay) => {
      const id = ++nextId;
      timers.set(id, { callback, delay });
      return id;
    },
    cancel: (/** @type {any} */ id) => timers.delete(id),
  });
  function retry() {
    const [id, task] = /** @type {any} */ (timers.entries().next().value);
    timers.delete(id);
    task.callback();
  }
  return {
    session,
    sockets,
    timers,
    heartbeats,
    reports,
    reloads,
    injections,
    config,
    state,
    retry,
  };
}

test('pinned Metro injects modules into one runtime with unchanged source identities', () => {
  assert.equal(rnRequire('metro-runtime/package.json').version, '0.87.1');
  const h = harness();
  const socket = h.sockets[0];
  socket.open();
  assert.deepEqual(socket.sent, [
    { type: 'register-entrypoints', entryPoints: [h.config.bundleUrl] },
  ]);
  assert.equal(socket.url, h.config.hotUrl);
  const sourceURL = 'http://localhost:8081/Counter.bundle?modulesOnly=true&shallow=true';
  socket.message('update-start', {});
  socket.message('update', {
    added: [],
    modified: [{ module: [4, "global.state.label = 'after'"], sourceURL }],
  });
  socket.message('update-done', {});
  assert.equal(
    h.state.count,
    7,
    'same evaluation context (not a useState/UI acceptance assertion)'
  );
  assert.equal(h.state.label, 'after');
  assert.equal(h.injections[0].sourceURL, sourceURL);
  assert.equal(h.reloads.length, 0);
  assert.equal(h.reports.at(-1).kind, 'ready');
  h.session.dispose();
  assert.equal(h.heartbeats.size, 0);
});

test('compile and injected runtime errors can be followed by a corrected module', () => {
  const h = harness();
  const socket = h.sockets[0];
  socket.open();
  socket.message('update-start', {});
  socket.message('error', { type: 'TransformError', message: 'Unexpected token' });
  socket.message('update-done', {});
  assert.equal(h.reports.at(-1).kind, 'error');
  socket.message('update-start', {});
  socket.message('update', {
    added: [
      {
        module: [5, "throw new Error('broken module')"],
        sourceURL: 'http://localhost:8081/broken.js',
      },
    ],
    modified: [],
  });
  socket.message('update-done', {});
  assert.match(h.reports.at(-1).message, /broken module/);
  socket.message('update-start', {});
  socket.message('update', {
    added: [],
    modified: [
      {
        module: [5, "global.state.label = 'corrected'"],
        sourceURL: 'http://localhost:8081/broken.js',
      },
    ],
  });
  socket.message('update-done', {});
  assert.equal(h.reports.at(-1).kind, 'ready');
  assert.equal(h.state.label, 'corrected');
  assert.equal(h.sockets.length, 1);
  assert.equal(h.reloads.length, 0);
  h.session.dispose();
});

test('reconnect invalidates old callbacks, re-registers exact URL and disposes producers', () => {
  const h = harness();
  const first = h.sockets[0];
  first.open();
  const lateMessage = first.onmessage;
  first.disconnect();
  assert.equal(h.heartbeats.size, 0);
  assert.equal(first.onopen, null);
  assert.equal(h.timers.size, 1);
  lateMessage({
    data: JSON.stringify({
      type: 'update',
      body: {
        added: [],
        modified: [{ module: [4, 'global.state.count = 99'], sourceURL: 'late.js' }],
      },
    }),
  });
  assert.equal(h.state.count, 7);
  h.retry();
  h.sockets[1].open();
  assert.deepEqual(h.sockets[1].sent[0].entryPoints, [h.config.bundleUrl]);
  h.session.dispose();
  h.session.dispose();
  assert.equal(h.sockets[1].closed, 1);
  assert.equal(h.heartbeats.size, 0);
  assert.equal(h.timers.size, 0);
});

test('reconnect attempts are bounded and pending retry is cancellable', () => {
  const h = harness();
  for (let attempt = 0; attempt < 6; ++attempt) {
    h.sockets.at(-1).disconnect();
    assert.equal(h.timers.size, 1);
    assert.ok([...h.timers.values()][0].delay <= 4000);
    h.retry();
  }
  h.sockets.at(-1).disconnect();
  assert.equal(h.timers.size, 0);
  assert.match(h.reports.at(-1).message, /limit reached/);
  h.session.dispose();
  const pending = harness();
  pending.sockets[0].disconnect();
  pending.session.dispose();
  assert.equal(pending.timers.size, 0);
});

test('graph loss requests a full reload rather than whole-bundle evaluation', () => {
  const h = harness();
  h.sockets[0].message('error', { type: 'GraphNotFoundError', message: 'graph disappeared' });
  assert.deepEqual(h.reloads, ['Metro graph lost: GraphNotFoundError']);
  assert.equal(h.injections.length, 0);
  h.session.dispose();
});

test('pinned React Refresh distinguishes compatible families from hook-signature resets', () => {
  assert.equal(rnRequire('react-refresh/package.json').version, '0.14.2');
  const refresh = rnRequire('react-refresh/runtime');
  function Before() {}
  function Compatible() {}
  function ChangedHooks() {}
  refresh.setSignature(Before, 'useState{count}');
  refresh.register(Before, 'rnqnx-fixture-family');
  refresh.setSignature(Compatible, 'useState{count}');
  refresh.register(Compatible, 'rnqnx-fixture-family');
  const family = refresh.getFamilyByType(Before);
  assert.ok(refresh.performReactRefresh().updatedFamilies.has(family));
  refresh.setSignature(ChangedHooks, 'useState{count}\nuseEffect{}');
  refresh.register(ChangedHooks, 'rnqnx-fixture-family');
  assert.ok(refresh.performReactRefresh().staleFamilies.has(family));
});
