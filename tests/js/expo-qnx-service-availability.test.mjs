import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import path from 'node:path';
import vm from 'node:vm';

const require = createRequire(import.meta.url);
const { createQnxResolveRequest } = require('../../packages/metro-config-qnx/src/index.js');
const {
  createMarathonResolveRequest,
} = require('../../packages/metro-config-marathon/src/index.js');
const ts = require('@typescript/typescript6');
const root = path.resolve(import.meta.dirname, '../..');
const expoRequire = createRequire(path.join(root, 'packages/expo-modules-core-qnx/package.json'));
const sourceFile = path.join(
  path.dirname(expoRequire.resolve('expo-modules-core')),
  'requireNativeModule.js'
);
const wrapperFile = path.join(
  root,
  'packages/metro-config-qnx/src/runtime/expo-require-native-module.js'
);
/** @type {any} */
const resolve = createQnxResolveRequest(path.join(root, 'packages/react-native-qnx/Libraries'));
const original = { type: 'sourceFile', filePath: sourceFile };
const context = {
  originModulePath: path.join(path.dirname(sourceFile), 'index.ts'),
  resolveRequest: () => original,
};
// Public Expo barrel imports this exact installed source; no app-specific API needed.
assert.equal(resolve(context, './requireNativeModule', 'qnx').filePath, wrapperFile);
assert.equal(resolve(context, './requireNativeModule', 'ios'), original);
assert.equal(resolve(context, './requireNativeModule', 'marathon'), original);
assert.equal(
  createMarathonResolveRequest()(context, './requireNativeModule', 'marathon'),
  original
);
assert.equal(
  resolve(
    { ...context, originModulePath: wrapperFile },
    '@rn-qnx/internal-expo-native-module',
    'qnx'
  ).filePath,
  sourceFile
);
// An app cannot use the private bypass; it still reaches the ordinary resolver.
assert.equal(resolve(context, '@rn-qnx/internal-expo-native-module', 'qnx').filePath, wrapperFile);
assert.throws(
  () =>
    resolve(
      {
        ...context,
        resolveRequest: () => ({
          ...original,
          filePath: '/other/node_modules/expo-modules-core/src/requireNativeModule.ts',
        }),
      },
      './requireNativeModule',
      'qnx'
    ),
  /Multiple Expo Modules Core/
);

// Execute Expo's actual installed TypeScript lookup implementation, not a
// replacement provider. Only the native registry/installer are host fixtures.
const upstreamSource = await readFile(sourceFile, 'utf8');
const compiled = ts.transpileModule(upstreamSource, {
  compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 },
}).outputText;
/** @type {Record<string, unknown>} */
const modules = {};
/** @type {Error | undefined} */
let installationError;
let installs = 0;
const upstreamExports = {};
const host = {
  exports: upstreamExports,
  expo: { modules },
  console,
  require: (/** @type {string} */ name) => {
    if (name === 'react-native') return { TurboModuleRegistry: { get: () => null } };
    if (name === './NativeModulesProxy') return { default: {} };
    if (name === './TurboModuleToExpoModuleProxy')
      return { createTurboModuleToExpoProxy: () => null };
    if (name === './ensureNativeModulesAreInstalled')
      return {
        ensureNativeModulesAreInstalled: () => {
          installs++;
          if (installationError) throw installationError;
        },
      };
    throw new Error('Unexpected upstream dependency: ' + name);
  },
};
vm.runInNewContext(compiled, host, { filename: sourceFile });
const wrapper = { exports: /** @type {any} */ ({}) };
vm.runInNewContext(
  await readFile(wrapperFile, 'utf8'),
  {
    module: wrapper,
    require: (/** @type {string} */ name) => {
      assert.equal(name, '@rn-qnx/internal-expo-native-module');
      return upstreamExports;
    },
  },
  { filename: wrapperFile }
);
const api = wrapper.exports;
for (const moduleName of [
  'ExpoCamera',
  'ExponentAccelerometer',
  'ExpoNotificationPermissionsModule',
]) {
  assert.equal(api.requireOptionalNativeModule(moduleName), null);
  assert.throws(
    () => api.requireNativeModule(moduleName),
    (/** @type {any} */ error) => {
      assert.equal(error.code, 'ERR_QNX_EXPO_MODULE_UNAVAILABLE');
      assert.equal(error.moduleName, moduleName);
      assert.match(error.message, /not a permission-denied/);
      return true;
    }
  );
}
let permissionRequests = 0;
const nativeCamera = {
  requestCameraPermissionsAsync() {
    permissionRequests++;
    return Promise.resolve({ granted: false });
  },
};
modules.ExpoCamera = nativeCamera;
assert.equal(api.requireNativeModule('ExpoCamera'), nativeCamera);
assert.equal(api.requireOptionalNativeModule('ExpoCamera'), nativeCamera);
assert.equal(permissionRequests, 0, 'lookup must never request permission or activate a device');
assert.deepEqual(await api.requireNativeModule('ExpoCamera').requestCameraPermissionsAsync(), {
  granted: false,
});
assert.equal(permissionRequests, 1, 'real module calls retain original receiver and result');
modules.ExpoCamera = false;
assert.throws(() => api.requireNativeModule('ExpoCamera'), /unavailable/);
delete modules.ExpoCamera;
assert.equal(
  api.requireOptionalNativeModule('ExpoCamera'),
  null,
  'do not cache stale registrations'
);
installationError = new Error('installer failed');
assert.throws(
  () => api.requireNativeModule('ExpoCamera'),
  (error) => error === installationError
);
assert.throws(
  () => api.requireOptionalNativeModule('ExpoCamera'),
  (error) => error === installationError
);
assert.ok(installs > 0);
console.log(
  'Expo QNX public lookup: unavailable services fail explicitly; real registrations and installer errors preserved. Host fixtures only.'
);
