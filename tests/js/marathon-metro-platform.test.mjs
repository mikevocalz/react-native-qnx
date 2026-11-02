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

assert.equal(MARATHON_PLATFORM, 'marathon');
assert.notEqual(marathon.OVERRIDES_ROOT, qnx.OVERRIDES_ROOT);

const nativeSource = path.join(
  process.cwd(),
  'node_modules', 'react-native', 'Libraries', 'Utilities', 'Platform.js'
);
const textInputSource = path.join(
  process.cwd(),
  'node_modules', 'react-native', 'Libraries', 'Components', 'TextInput',
  'TextInput.js'
);
const source = (filePath) => ({
  resolveRequest() {
    return { type: 'sourceFile', filePath };
  },
});
const resolver = createMarathonResolveRequest(
  marathon.OVERRIDES_ROOT,
  qnx.OVERRIDES_ROOT
);
const marathonPlatform = resolver(source(nativeSource), './Platform', 'marathon');
assert.equal(
  marathonPlatform.filePath,
  path.join(marathon.OVERRIDES_ROOT, 'Utilities', 'Platform.js'),
  'Marathon Platform overrides QNX identity'
);
const sharedQnxOverride = resolver(source(textInputSource), './TextInput', 'marathon');
assert.equal(
  sharedQnxOverride.filePath,
  path.join(qnx.OVERRIDES_ROOT, 'Components', 'TextInput', 'TextInput.js'),
  'Portable QNX UI component JS overrides reused on Marathon'
);
assert.equal(
  resolver(source(nativeSource), './Platform', 'qnx').filePath,
  nativeSource,
  'Marathon resolver must not affect qnx'
);
assert.equal(
  resolver(source(nativeSource), './Platform', 'ios').filePath,
  nativeSource,
  'Marathon resolver must not affect iOS'
);
assert.equal(
  resolver(source('/tmp/external/package.js'), 'package', 'marathon').filePath,
  '/tmp/external/package.js',
  'Third-party source is not rewritten'
);
assert.equal(
  resolver({ resolveRequest() { return { type: 'assetFiles', filePaths: [] }; } },
    'image.png', 'marathon').type,
  'assetFiles',
  'Asset resolutions are unchanged'
);

const platform = require('../../packages/react-native-marathon/Libraries/Utilities/Platform');
assert.equal(platform.OS, 'marathon');
assert.equal(platform.select({ marathon: 'MAR', qnx: 'QNX', native: 'fallback' }), 'MAR');
assert.equal(platform.select({ native: 'native' }), 'native');
assert.equal(platform.select({ default: 'fallback' }), 'fallback');
assert.throws(() => platform.Version, /must install __RNQNX_PLATFORM_CONSTANTS/);
globalThis.__RNQNX_PLATFORM_CONSTANTS = { osVersion: 'test-only', isTesting: true };
assert.equal(platform.Version, 'test-only');
delete globalThis.__RNQNX_PLATFORM_CONSTANTS;

console.log('marathon-metro-platform.test.mjs passed');
