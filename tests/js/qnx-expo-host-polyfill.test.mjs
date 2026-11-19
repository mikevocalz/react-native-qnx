// The QNX Expo host: Metro swaps expo-modules-core's polyfill entry for the
// platform package's, and that file gives globalThis.expo the JavaScript
// classes iOS and Android define natively. Host-only: no QNX runtime runs.
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import path from 'node:path';
import vm from 'node:vm';
import { readFile } from 'node:fs/promises';

const require = createRequire(import.meta.url);
const root = process.cwd();
const { createQnxResolveRequest } = require('../../packages/metro-config-qnx/src/index.js');
const overrides = path.join(root, 'packages/react-native-qnx/Libraries');
const polyfillPath = path.join(root, 'packages/react-native-qnx/expo/polyfill.js');
/** @type {(context: object, name: string, platform: string) => any} */
const resolveRequest = createQnxResolveRequest(overrides);

// Resolution: only expo-modules-core's polyfill entry, only for qnx.
const corePolyfill = '/x/node_modules/expo-modules-core/build/polyfill/index.js';
const context = (/** @type {string} */ filePath) => ({
  originModulePath: '/x/node_modules/expo-modules-core/build/index.js',
  resolveRequest: () => ({ type: 'sourceFile', filePath }),
});
assert.deepEqual(resolveRequest(context(corePolyfill), './polyfill', 'qnx'), {
  type: 'sourceFile',
  filePath: polyfillPath,
});
assert.deepEqual(resolveRequest(context(corePolyfill), './polyfill', 'ios'), {
  type: 'sourceFile',
  filePath: corePolyfill,
});
const webPolyfill = '/x/node_modules/expo-modules-core/build/polyfill/index.web.js';
assert.equal(resolveRequest(context(webPolyfill), './polyfill', 'qnx').filePath, webPolyfill);
const other = '/x/node_modules/other/build/polyfill/index.js';
assert.equal(resolveRequest(context(other), './polyfill', 'qnx').filePath, other);

const source = await readFile(polyfillPath, 'utf8');
/** Run the polyfill in a fresh global, optionally with a native globalThis.expo.
 * @param {object} [expo] @returns {any} */
function run(expo) {
  /** @type {any} */
  const sandbox = { module: { exports: {} }, console };
  sandbox.globalThis = sandbox;
  if (expo) sandbox.expo = expo;
  vm.runInNewContext(source, sandbox);
  return sandbox;
}

// With the native host: classes are added, existing members are kept, and
// each native module gets NativeModule's prototype.
/** @type {any} */
const constants = { deviceName: 'pi400' };
const uuidv4 = () => 'native';
const withHost = run({ modules: { ExponentConstants: constants }, uuidv4 });
const expo = withHost.expo;
assert.equal(expo.uuidv4, uuidv4);
assert.ok(constants instanceof expo.NativeModule);
assert.ok(constants instanceof expo.EventEmitter);
assert.equal(constants.deviceName, 'pi400');
assert.equal(constants.__expo_module_name__, 'ExponentConstants');
assert.ok(new expo.SharedRef() instanceof expo.SharedObject);
assert.equal(new expo.SharedRef().nativeRefType, 'unknown');

// The event API behaves like expo-modules-core's own EventEmitter.
class Battery extends expo.NativeModule {
  /** @type {string[]} */
  observed = [];
  /** @param {string} name */
  startObserving(name) {
    this.observed.push('start:' + name);
  }
  /** @param {string} name */
  stopObserving(name) {
    this.observed.push('stop:' + name);
  }
}
/** @type {any} */
const battery = new Battery();
/** @type {unknown[]} */
const seen = [];
const first = battery.addListener('level', (/** @type {unknown} */ value) => seen.push(value));
battery.addListener('level', () => {
  throw new Error('listener errors are contained');
});
const errors = [];
const quietConsole = console.error;
console.error = (error) => errors.push(error);
battery.emit('level', 0.5);
console.error = quietConsole;
assert.deepEqual(seen, [0.5]);
assert.equal(errors.length, 1);
assert.equal(battery.listenerCount('level'), 2);
first.remove();
battery.removeAllListeners('level');
assert.equal(battery.listenerCount('level'), 0);
assert.deepEqual(battery.observed, ['start:level', 'stop:level']);
assert.ok(!Object.keys(battery).includes('__listeners'));

// Without the host (no Expo modules linked) requireNativeModule finds nothing.
const bare = run();
assert.deepEqual(Object.keys(bare.expo.modules), []);
assert.equal(typeof bare.expo.NativeModule, 'function');

// expo.dispatchViewCommand runs a view function's command through React
// Native's dispatchCommand on the host view the component wraps; an unmounted
// view has nothing to command.
/** @type {unknown[][]} */
const dispatched = [];
const sandboxWithRenderer = run();
sandboxWithRenderer.require = (/** @type {string} */ request) => {
  assert.equal(request, 'react-native/Libraries/ReactNative/RendererProxy');
  return { dispatchCommand: (/** @type {unknown[]} */ ...call) => dispatched.push(call) };
};
const commandRun = (
  /** @type {string} */ name,
  /** @type {any} */ component,
  /** @type {unknown} */ args
) => {
  Object.assign(sandboxWithRenderer, { component, name, args });
  vm.runInNewContext('expo.dispatchViewCommand(component, name, args)', sandboxWithRenderer);
};
const hostView = { id: 'host-view' };
commandRun('startAnimating', { getNativeRef: () => hostView }, undefined);
commandRun('args', { getNativeRef: () => hostView }, [1]);
commandRun('reload', { getNativeRef: () => null }, undefined);
// The arguments come from another realm, so compare their JSON.
assert.equal(
  JSON.stringify(dispatched),
  JSON.stringify([
    [hostView, 'startAnimating', []],
    [hostView, 'args', [1]],
  ])
);
assert.equal(typeof withHost.expo.dispatchViewCommand, 'function');

console.log('QNX Expo host polyfill: resolver override and globalThis.expo classes verified.');
