import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const require = createRequire(import.meta.url);
const {
  MARATHON_PLATFORM,
  createMarathonResolveRequest,
  getMarathonMetroConfig,
} = require('../../packages/metro-config-marathon/src');
const { getQnxMetroConfig } = require('../../packages/metro-config-qnx/src');
const qnx = require('../../packages/react-native-qnx');
const marathon = require('../../packages/react-native-marathon');

/** @param {unknown} resolution */
function sourceFilePath(resolution) {
  assert.ok(
    resolution !== null && typeof resolution === 'object' && 'filePath' in resolution,
    'Expected a source-file resolution'
  );
  return resolution.filePath;
}

/** @param {unknown} resolution */
function resolutionKind(resolution) {
  assert.ok(
    resolution !== null && typeof resolution === 'object' && 'type' in resolution,
    'Expected a Metro resolution'
  );
  return resolution.type;
}

assert.equal(MARATHON_PLATFORM, 'marathon');
assert.notEqual(marathon.OVERRIDES_ROOT, qnx.OVERRIDES_ROOT);

const nativeSource = path.join(
  process.cwd(),
  'node_modules',
  'react-native',
  'Libraries',
  'Utilities',
  'Platform.js'
);
const textInputSource = path.join(
  process.cwd(),
  'node_modules',
  'react-native',
  'Libraries',
  'Components',
  'TextInput',
  'TextInput.js'
);

/** @param {string} filePath */
const source = (filePath) => ({
  originModulePath: path.join(process.cwd(), 'apps', 'rn-playground', 'index.js'),
  resolveRequest() {
    return { type: 'sourceFile', filePath };
  },
});

const resolver = createMarathonResolveRequest(marathon.OVERRIDES_ROOT, qnx.OVERRIDES_ROOT);
assert.equal(
  sourceFilePath(resolver(source(nativeSource), './Platform', 'marathon')),
  path.join(marathon.OVERRIDES_ROOT, 'Utilities', 'Platform.js'),
  'Marathon must override the QNX platform identity'
);
assert.equal(
  sourceFilePath(resolver(source(textInputSource), './TextInput', 'marathon')),
  path.join(qnx.OVERRIDES_ROOT, 'Components', 'TextInput', 'TextInput.js'),
  'Portable Qt/React component source should be reused'
);
assert.equal(
  sourceFilePath(resolver(source(nativeSource), './Platform', 'qnx')),
  nativeSource,
  'Marathon resolver must not affect QNX'
);
assert.equal(
  sourceFilePath(resolver(source(nativeSource), './Platform', 'ios')),
  nativeSource,
  'Marathon resolver must not affect iOS'
);
assert.equal(
  sourceFilePath(resolver(source('/tmp/external/package.js'), 'package', 'marathon')),
  '/tmp/external/package.js',
  'Third-party code must remain unchanged'
);
assert.equal(
  resolutionKind(
    resolver(
      {
        originModulePath: '/app/index.js',
        resolveRequest() {
          return { type: 'assetFiles', filePaths: [] };
        },
      },
      'image.png',
      'marathon'
    )
  ),
  'assetFiles',
  'Metro assets must resolve normally'
);

const platform = require('../../packages/react-native-marathon/Libraries/Utilities/Platform');
assert.equal(platform.OS, 'marathon');
assert.equal(platform.select({ marathon: 'MAR', qnx: 'QNX', native: 'fallback' }), 'MAR');
assert.equal(platform.select({ native: 'native' }), 'native');
assert.equal(platform.select({ default: 'fallback' }), 'fallback');

assert.throws(
  () => platform.Version,
  /must install __RNQNX_PLATFORM_CONSTANTS/,
  'Do not invent the OS version before the native host has started'
);
Object.defineProperty(globalThis, '__RNQNX_PLATFORM_CONSTANTS', {
  configurable: true,
  value: { osVersion: 'test-only', isTesting: true },
});
assert.equal(platform.Version, 'test-only');
Reflect.deleteProperty(globalThis, '__RNQNX_PLATFORM_CONSTANTS');

// ---- Platform identity details
assert.equal(
  /** @type {any} */ (platform).default,
  platform,
  'default export must be the same object'
);
assert.equal(platform.isTV, false);
assert.equal(platform.isPad, false);
assert.equal(platform.isVision, false);
assert.equal(platform.isVisionOS, false);
assert.equal(
  platform.select({ marathon: 'M', native: 'N', default: 'D' }),
  'M',
  'marathon key beats native and default'
);
assert.equal(
  platform.select({ qnx: 'Q', ios: 'I', default: 'D' }),
  'D',
  'qnx and ios keys are never selected on Marathon'
);
assert.equal(platform.select({ marathon: undefined, default: 'D' }), undefined);
{
  /** @type {any} */
  const globals = globalThis;
  const original = globals.__DEV__;
  const install = (/** @type {Record<string, unknown>} */ constants) => {
    platform.__constants = null;
    Object.defineProperty(globalThis, '__RNQNX_PLATFORM_CONSTANTS', {
      configurable: true,
      value: constants,
    });
  };
  globals.__DEV__ = false;
  install({ osVersion: '1', isTesting: true });
  assert.equal(platform.isTesting, false, 'isTesting is false outside __DEV__');
  assert.equal(platform.isDisableAnimations, false, 'falls back to isTesting');
  globals.__DEV__ = true;
  install({ osVersion: '1', isTesting: true });
  assert.equal(platform.isTesting, true);
  assert.equal(platform.isDisableAnimations, true);
  install({ osVersion: '1', isTesting: true, isDisableAnimations: false });
  assert.equal(platform.isDisableAnimations, false, 'explicit host value wins');
  globals.__DEV__ = original;
  Reflect.deleteProperty(globalThis, '__RNQNX_PLATFORM_CONSTANTS');
  platform.__constants = null;
}

// ---- Fallback order against real files: Marathon, then QNX, then React Native.
{
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'rnqnx-marathon-order-'));
  try {
    const marathonRoot = path.join(temp, 'marathon', 'Libraries');
    const qnxRoot = path.join(temp, 'qnx', 'Libraries');
    /** @param {string} file */
    const touch = (file) => {
      fs.mkdirSync(path.dirname(file), { recursive: true });
      fs.writeFileSync(file, '');
    };
    touch(path.join(marathonRoot, 'Both', 'Both.js'));
    touch(path.join(qnxRoot, 'Both', 'Both.js'));
    touch(path.join(qnxRoot, 'OnlyQnx', 'OnlyQnx.js'));
    const rn = (/** @type {string} */ relative) =>
      path.join(temp, 'node_modules', 'react-native', 'Libraries', relative);
    const order = createMarathonResolveRequest(marathonRoot, qnxRoot);
    assert.equal(
      sourceFilePath(order(source(rn('Both/Both.js')), './Both', 'marathon')),
      path.join(marathonRoot, 'Both', 'Both.js'),
      'Marathon override must beat the QNX override'
    );
    assert.equal(
      sourceFilePath(order(source(rn('OnlyQnx/OnlyQnx.js')), './OnlyQnx', 'marathon')),
      path.join(qnxRoot, 'OnlyQnx', 'OnlyQnx.js'),
      'QNX override is the fallback'
    );
    assert.equal(
      sourceFilePath(order(source(rn('Neither/Neither.js')), './Neither', 'marathon')),
      rn('Neither/Neither.js'),
      'React Native own file when no override exists'
    );
    assert.equal(
      sourceFilePath(order(source(rn('Both/Both.js')), './Both', 'qnx')),
      rn('Both/Both.js'),
      'the QNX platform must not see Marathon overrides'
    );
    assert.equal(
      sourceFilePath(order(source(rn('Both/Both.js')), './Both', null)),
      rn('Both/Both.js')
    );

    // Dev-only React Native modules that exist as .ios.js/.android.js only.
    const originModulePath = rn('../src/private/devsupport/rndevtools/Origin.js');
    const privateFile = path.join(
      temp,
      'qnx',
      'src',
      'private',
      'devsupport',
      'rndevtools',
      'ReactDevToolsSettingsManager.js'
    );
    touch(privateFile);
    const failing = {
      originModulePath,
      resolveRequest() {
        throw new Error('Unable to resolve module');
      },
    };
    assert.equal(
      sourceFilePath(order(failing, './ReactDevToolsSettingsManager', 'marathon')),
      privateFile,
      'dev bundles must find the shared private fallback'
    );
    assert.throws(
      () => order(failing, './ReactDevToolsSettingsManager', 'qnx'),
      /Unable to resolve/,
      'only Marathon requests are recovered'
    );
    assert.throws(
      () => order(failing, 'left-pad', 'marathon'),
      /Unable to resolve/,
      'a missing package must still fail'
    );
    assert.throws(
      () => order(failing, './NoSuchPrivateModule', 'marathon'),
      /Unable to resolve/,
      'an unknown relative module must still fail'
    );
  } finally {
    fs.rmSync(temp, { recursive: true, force: true });
  }
}

// ---- Expo polyfill entry resolves to the shared polyfill on Marathon only.
{
  const polyfill = path.join(qnx.OVERRIDES_ROOT, '..', 'expo', 'polyfill.js');
  const entry = '/x/node_modules/expo-modules-core/build/polyfill/index.js';
  assert.equal(
    sourceFilePath(resolver(source(entry), './polyfill', 'marathon')),
    path.resolve(polyfill)
  );
  assert.equal(sourceFilePath(resolver(source(entry), './polyfill', 'ios')), entry);
}

// ---- Config isolation: Marathon is additive and QNX is untouched.
{
  const appRoot = path.join(process.cwd(), 'apps', 'rn-playground');
  const qnxConfig = getQnxMetroConfig(appRoot);
  const marathonConfig = getMarathonMetroConfig(appRoot, {
    resolver: { unstable_conditionsByPlatform: { marathon: ['custom'] } },
  });
  assert.ok(!qnxConfig.resolver.platforms.includes('marathon'));
  assert.ok(!('marathon' in (qnxConfig.resolver.unstable_conditionsByPlatform ?? {})));
  assert.ok(marathonConfig.resolver.platforms.includes('marathon'));
  assert.ok(!marathonConfig.resolver.platforms.includes('qnx'));
  assert.deepEqual(marathonConfig.resolver.unstable_conditionsByPlatform.marathon, [
    'custom',
    'marathon',
  ]);
  assert.equal(
    new Set(marathonConfig.resolver.platforms).size,
    marathonConfig.resolver.platforms.length,
    'platform list has no duplicates'
  );
  // The factories must not leak state into each other.
  assert.ok(!getQnxMetroConfig(appRoot).resolver.platforms.includes('marathon'));
}

console.log('marathon-metro-platform.test.mjs passed');
