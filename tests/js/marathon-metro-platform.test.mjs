import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import path from 'node:path';

const require = createRequire(import.meta.url);
const {
  MARATHON_PLATFORM,
  createMarathonResolveRequest,
} = require('../../packages/metro-config-marathon/src');
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

console.log('marathon-metro-platform.test.mjs passed');
