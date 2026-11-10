import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const withPublicEntry = require('../../packages/metro-config-qnx/public-entry.js');
const initializer = path.resolve('packages/react-native-qnx/Libraries/Core/InitializeCore.js');
const source = await readFile(initializer, 'utf8');
const prefix = 'react-native/';
const expected = [
  'Libraries/Core/setUpGlobals',
  'src/private/setup/setUpDOM',
  'src/private/setup/setUpPerformance',
  'Libraries/Core/polyfillPromise',
  'Libraries/Core/setUpTimers',
  'Libraries/Core/setUpErrorHandling',
  'Libraries/Core/setUpRegeneratorRuntime',
  'Libraries/Core/setUpXHR',
  'Libraries/Core/setUpAlert',
  'Libraries/Core/setUpNavigator',
  'Libraries/Core/setUpBatchedBridge',
  'Libraries/Core/setUpSegmentFetcher',
  'Libraries/ReactNative/AppRegistry',
  'src/private/featureflags/ReactNativeFeatureFlags',
];
for (const enabled of [false, true]) {
  const loaded = /** @type {string[]} */ ([]);
  const called = /** @type {string[]} */ ([]);
  vm.runInNewContext(source, {
    /** @param {string} name */
    require(name) {
      loaded.push(name);
      if (name.endsWith('ReactNativeFeatureFlags'))
        return {
          enableIntersectionObserverByDefault: () => enabled,
          enableMutationObserverByDefault: () => enabled,
          enableResizeObserverByDefault: () => enabled,
        };
      return {
        default() {
          called.push(name);
        },
      };
    },
  });
  const observers = ['setUpIntersectionObserver', 'setUpMutationObserver', 'setUpResizeObserver'];
  assert.deepEqual(loaded, [
    ...expected.map((name) => prefix + name),
    ...(enabled ? observers.map((name) => prefix + 'src/private/setup/' + name) : []),
  ]);
  assert.deepEqual(
    called,
    ['setUpDOM', 'setUpPerformance', ...(enabled ? observers : [])].map(
      (name) => prefix + 'src/private/setup/' + name
    )
  );
  assert.ok(!loaded.some((name) => /DevTools|DeveloperTools|LogBox|ReactRefresh/.test(name)));
}
for (const platform of /** @type {const} */ (['qnx', 'marathon'])) {
  /** @returns {{type: 'sourceFile', filePath: string}} */
  const fallback = () => ({ type: 'sourceFile', filePath: '/upstream.js' });
  const config = withPublicEntry(
    {
      resolver: { resolveRequest: fallback },
      serializer: { getModulesRunBeforeMainModule: () => ['/caller/preload.js'] },
    },
    platform
  );
  const preloads = config.serializer?.getModulesRunBeforeMainModule;
  const resolveRequest = config.resolver?.resolveRequest;
  assert.ok(preloads);
  assert.ok(resolveRequest);
  // This resolver branch only inspects the import/platform, not Metro's context.
  const context = /** @type {any} */ ({});
  assert.deepEqual(preloads('index.js'), ['/caller/preload.js', initializer]);
  const selected = resolveRequest(context, 'react-native/setup-env', platform);
  assert.equal(selected.type, 'sourceFile');
  if (selected.type !== 'sourceFile') throw new Error('Expected source file');
  assert.equal(selected.filePath, initializer);
  for (const other of ['ios', 'android', 'web']) {
    const unmodified = resolveRequest(context, 'react-native/setup-env', other);
    assert.equal(unmodified.type, 'sourceFile');
    if (unmodified.type !== 'sourceFile') throw new Error('Expected upstream source');
    assert.equal(unmodified.filePath, '/upstream.js');
  }
}
console.log(
  'QNX runtime initialization order, feature gates, shared public setup module and preserved caller preloads passed (source contract, not a native runtime test).'
);
