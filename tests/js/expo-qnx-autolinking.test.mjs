import assert from 'node:assert/strict';
import { mkdtemp, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const autolinking = require('../../packages/expo-qnx-autolinking/src');
const withQnxProject = require('../../packages/expo-qnx-config-plugins');

const moduleRoot = await mkdtemp(path.join(os.tmpdir(), 'rnqnx-expo-module-'));
await writeFile(path.join(moduleRoot, 'CMakeLists.txt'), 'add_library(fake_qnx STATIC fake.cpp)\n');

const descriptors = autolinking.resolveQnxSearchResults({
  'fake-expo-module': {
    path: moduleRoot,
    version: '1.2.3',
    config: {
      toJSON() {
        return {
          platforms: ['qnx'],
          qnx: {
            target: 'fake_qnx',
            modules: ['FakeExpoModule'],
          },
        };
      },
    },
  },
});

assert.equal(descriptors.length, 1);
assert.equal(descriptors[0].target, 'fake_qnx');
assert.deepEqual(descriptors[0].modules, ['FakeExpoModule']);

const cmake = autolinking.generateQnxCMake(descriptors);
assert.match(cmake, /add_subdirectory/);
assert.match(cmake, /fake_qnx/);
assert.match(cmake, /RNQNX_EXPO_QNX_MODULE_TARGETS/);

assert.throws(
  () =>
    autolinking.resolveQnxSearchResults({
      broken: {
        path: moduleRoot,
        version: '1.0.0',
        config: { toJSON: () => ({ platforms: ['qnx'] }) },
      },
    }),
  /has no qnx config object/
);

const config = withQnxProject(
  { name: 'app', extra: { existing: true } },
  { targetArch: 'x86_64', autolinkingJson: 'custom/qnx.json' }
);
assert.equal(config.extra.existing, true);
assert.equal(config.extra.rnQnx.targetArch, 'x86_64');
assert.equal(config.extra.rnQnx.autolinkingJson, 'custom/qnx.json');
assert.equal(config.extra.rnQnx.autolinkingCMake, 'build/generated/expo-qnx-modules.cmake');

console.log('expo-qnx-autolinking.test.mjs passed');

const registered = { ...descriptors[0], registrationFunction: 'registerContract' };
const wired = autolinking.generateQnxCMake([registered]);
assert.match(wired, /registerContract\(registry\)/);
assert.match(wired, /RUNTIME_READY TRUE/);
assert.doesNotMatch(cmake, /RUNTIME_READY/);
assert.throws(
  () => autolinking.generateQnxCMake([{ ...registered, registrationFunction: 'bad();' }]),
  /registration function/
);
assert.throws(
  () => autolinking.generateQnxCMake([{ ...registered, modules: ['ExpoApplication'] }]),
  /reserved/
);
assert.throws(
  () =>
    autolinking.generateQnxCMake([
      registered,
      { ...registered, target: 'second', binaryName: 'second', modules: ['Second'] },
    ]),
  /registration function/
);
assert.throws(
  () =>
    autolinking.generateQnxCMake([
      registered,
      { ...registered, target: 'second', binaryName: 'second', registrationFunction: 'second' },
    ]),
  /reserved/
);

assert.throws(
  () =>
    autolinking.generateQnxCMake([
      { ...registered, registrationFunction: 'rnqnxRegisterExpoAutolinkedModules' },
    ]),
  /registration function/
);

// Discovery-only CMake has never installed runtime modules: overlapping names
// (including builtins) are metadata, not collisions until runtime is requested.
const legacyDuplicate = { ...descriptors[0], target: 'legacy_second', binaryName: 'legacy_second' };
assert.doesNotThrow(() => autolinking.generateQnxCMake([descriptors[0], legacyDuplicate]));
assert.doesNotThrow(() =>
  autolinking.generateQnxCMake([{ ...descriptors[0], modules: ['ExpoApplication'] }])
);
// A mixed batch still receives full runtime name checks, even if the collision
// belongs to a discovery-only entry encountered before the registered provider.
assert.throws(() => autolinking.generateQnxCMake([legacyDuplicate, registered]), /reserved/);
