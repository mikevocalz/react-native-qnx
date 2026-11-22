import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';
import path from 'node:path';
const require = createRequire(import.meta.url);
const React = require('react');
const sdkRequire = createRequire(path.resolve('packages/expo-router-qnx/package.json'));
const sdk = path.dirname(sdkRequire.resolve('expo-router/package.json'));
assert.equal(sdkRequire('expo-router/package.json').version, '58.0.31');
const originalLoad = /** @type {any} */ (Module)._load;
let entryLoads = 0;
/** @type {any} */
const globals = globalThis;
const prefix = 'rnqnx_entry_fixture_';
globals.__METRO_GLOBAL_PREFIX__ = prefix;
const refresh = { isLikelyComponentType: (/** @type {unknown} */ value) => value === 'existing' };
globals[prefix + '__ReactRefresh'] = refresh;
const mode = process.env.NODE_ENV;
process.env.NODE_ENV = 'development';
/** @param {string} request @param {any} parent @param {boolean} isMain */
/** @type {any} */ (Module)._load = function (
  /** @type {string} */ request,
  /** @type {any} */ parent,
  /** @type {boolean} */ isMain
) {
  if (request === 'expo-router/head') {
    entryLoads += 1;
    return { Head: { Provider: 'UpstreamHeadProvider' } };
  }
  if (request === 'expo-router/build/fast-refresh') {
    entryLoads += 1;
    return originalLoad.call(this, path.join(sdk, 'build/fast-refresh.js'), parent, isMain);
  }
  return originalLoad.call(this, request, parent, isMain);
};
try {
  const adapter = require(path.resolve('packages/expo-router-qnx/src/index.js'));
  assert.equal(entryLoads, 0, 'Node tooling must not initialize Router app internals');
  const { QnxRouterProvider } = adapter;
  assert.equal(
    entryLoads,
    2,
    'entry provider must load pinned Head and real refresh compatibility'
  );
  const child = React.createElement('ConsumerExpoRoot', { context: 'consumer-route-context' });
  const wrapped = QnxRouterProvider({ children: child });
  assert.equal(wrapped.type, 'UpstreamHeadProvider');
  assert.equal(wrapped.props.children, child, 'provider preserves consumer ExpoRoot/context');
  assert.equal(adapter.QnxRouterProvider, QnxRouterProvider);
  assert.equal(entryLoads, 2, 'cached provider must not reinitialize entry hooks');
  const settings = {};
  refresh.isLikelyComponentType({ unstable_settings: settings });
  assert.equal(
    refresh.isLikelyComponentType(settings),
    true,
    'actual SDK route refresh patch retained'
  );
  assert.equal(
    refresh.isLikelyComponentType('existing'),
    true,
    'original refresh classifier retained'
  );
  assert.equal(refresh.isLikelyComponentType('other'), false);
  console.log(
    'Public Router entry: lazy adapter, pinned Head wrapper and actual SDK layout-refresh semantics pass.'
  );
} finally {
  /** @type {any} */ (Module)._load = originalLoad;
  delete globals.__METRO_GLOBAL_PREFIX__;
  delete globals[prefix + '__ReactRefresh'];
  if (mode === undefined) delete process.env.NODE_ENV;
  else process.env.NODE_ENV = mode;
}
