import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';
import path from 'node:path';

const require = createRequire(import.meta.url);
/** @type {any} */
const globals = globalThis;
/** @type {any} */
const ModuleInternals = Module;
const marathon = require('../../packages/react-native-marathon');
const libraries = marathon.OVERRIDES_ROOT;

// Libraries that import React Native internals get small stand-ins, because
// those files are Flow source that plain Node cannot load.
const FakeView = (/** @type {unknown} */ props) => props;
class FakeEmitter {
  /** @type {Map<string, Set<(event: unknown) => void>>} */
  listeners = new Map();
  /** @param {string} type @param {(event: unknown) => void} listener */
  addListener(type, listener) {
    const set = this.listeners.get(type) ?? new Set();
    set.add(listener);
    this.listeners.set(type, set);
    return { remove: () => set.delete(listener) };
  }
  /** @param {string} type @param {unknown} event */
  emit(type, event) {
    for (const listener of this.listeners.get(type) ?? []) listener(event);
  }
}
const deviceEvents = new FakeEmitter();
const originalLoad = ModuleInternals._load;
ModuleInternals._load = function (/** @type {string} */ request, /** @type {unknown[]} */ ...rest) {
  if (request === 'react-native/Libraries/Components/View/View') return { default: FakeView };
  if (request === 'react-native/Libraries/EventEmitter/RCTDeviceEventEmitter') {
    return { default: deviceEvents };
  }
  if (request === 'react-native/Libraries/vendor/emitter/EventEmitter') {
    return { default: FakeEmitter };
  }
  return originalLoad.call(this, request, ...rest);
};

/** @param {string} relative */
function fresh(relative) {
  const file = path.join(libraries, relative);
  delete require.cache[require.resolve(file)];
  return require(file);
}

/** @param {() => void} body @returns {string[]} warnings */
function captureWarnings(body) {
  const original = console.warn;
  /** @type {string[]} */
  const warnings = [];
  console.warn = (/** @type {unknown} */ message) => warnings.push(String(message));
  try {
    body();
  } finally {
    console.warn = original;
  }
  return warnings;
}

// ---- StatusBar: records requests, applies nothing, says so once in dev.
globals.__DEV__ = true;
{
  const StatusBar = fresh('Components/StatusBar/StatusBar.js');
  assert.equal(StatusBar.default, StatusBar);
  assert.equal(StatusBar.currentHeight, 0);
  assert.deepEqual(StatusBar.getRequestedState(), {
    hidden: false,
    barStyle: 'default',
    backgroundColor: 'black',
    translucent: false,
    networkActivityIndicatorVisible: false,
  });
  const warnings = captureWarnings(() => {
    StatusBar.setHidden(true);
    StatusBar.setHidden(false);
    StatusBar.setBarStyle('light-content');
  });
  assert.equal(warnings.length, 2, 'one warning per setter, not per call');
  assert.match(warnings[0], /StatusBar\.setHidden .*not applied.*Marathon shell/);
  assert.equal(StatusBar.getRequestedState().barStyle, 'light-content');
  assert.equal(StatusBar.getRequestedState().hidden, false);

  const first = StatusBar.pushStackEntry({ hidden: true });
  const second = StatusBar.pushStackEntry({ barStyle: 'dark-content' });
  assert.equal(StatusBar.getRequestedState().hidden, true);
  assert.equal(StatusBar.getRequestedState().barStyle, 'dark-content');
  const replaced = StatusBar.replaceStackEntry(second, { barStyle: 'default' });
  assert.equal(StatusBar.getRequestedState().barStyle, 'default');
  StatusBar.popStackEntry(replaced);
  StatusBar.popStackEntry(first);
  assert.equal(StatusBar.getRequestedState().hidden, false, 'popped entries stop applying');
  assert.equal(StatusBar.getRequestedState().barStyle, 'light-content');

  globals.__DEV__ = false;
  const quiet = fresh('Components/StatusBar/StatusBar.js');
  assert.deepEqual(
    captureWarnings(() => quiet.setHidden(true)),
    [],
    'no warnings in production'
  );
  globals.__DEV__ = true;

  // Mounted component pushes on mount, replaces on update, pops on unmount.
  const React = require('react');
  /** @type {any} */
  const TestRenderer = require('react-test-renderer');
  globals.IS_REACT_ACT_ENVIRONMENT = true;
  const Fresh = fresh('Components/StatusBar/StatusBar.js');
  /** @type {any} */
  let tree;
  TestRenderer.act(() => {
    tree = TestRenderer.create(React.createElement(Fresh, { hidden: true }));
  });
  assert.equal(tree.toJSON(), null, 'renders nothing');
  assert.equal(Fresh.getRequestedState().hidden, true);
  TestRenderer.act(() => tree.update(React.createElement(Fresh, { hidden: false })));
  assert.equal(Fresh.getRequestedState().hidden, false);
  TestRenderer.act(() => tree.update(React.createElement(Fresh, { barStyle: 'dark-content' })));
  assert.equal(Fresh.getRequestedState().barStyle, 'dark-content');
  TestRenderer.act(() => tree.unmount());
  assert.equal(Fresh.getRequestedState().barStyle, 'default');
}

// ---- KeyboardAvoidingView: forwards view props, drops avoidance props.
{
  const KeyboardAvoidingView = fresh('Components/Keyboard/KeyboardAvoidingView.js');
  const onLayout = () => {};
  const element = KeyboardAvoidingView({
    behavior: 'padding',
    keyboardVerticalOffset: 40,
    enabled: true,
    contentContainerStyle: { flex: 1 },
    style: { flex: 1 },
    testID: 'kav',
    onLayout,
    children: 'child',
  });
  assert.equal(element.type, FakeView);
  assert.deepEqual(Object.keys(element.props).sort(), ['children', 'onLayout', 'style', 'testID']);
  assert.equal(element.props.testID, 'kav');
  assert.equal(element.props.onLayout, onLayout);
  assert.equal(element.props.children, 'child');
}

// ---- Linking: Marathon wording, no QNX wording.
{
  const Linking = fresh('Linking/Linking.js');
  assert.equal(Linking.default, Linking);
  assert.equal(await Linking.getInitialURL(), null);
  assert.equal(await Linking.canOpenURL('https://example.com'), false);
  await assert.rejects(Linking.openURL('https://example.com'), (error) => {
    assert.ok(error instanceof Error);
    assert.match(error.message, /Marathon/);
    assert.doesNotMatch(error.message, /QNX/);
    return true;
  });
  await assert.rejects(Linking.openSettings(), /Marathon/);
  await assert.rejects(Linking.sendIntent(), /Android only/);
  /** @type {string[]} */
  const seen = [];
  const subscription = Linking.addEventListener('url', (/** @type {{url: string}} */ event) =>
    seen.push(event.url)
  );
  Linking._emitURL('marathon://a');
  subscription.remove();
  Linking._emitURL('marathon://b');
  assert.deepEqual(seen, ['marathon://a']);
}

// ---- BackHandler: newest listener first, stops at the first handled event.
{
  const BackHandler = fresh('Utilities/BackHandler.js');
  assert.equal(BackHandler.default, BackHandler);
  assert.equal(BackHandler._emitBackPress(), false, 'no listener means not handled');
  /** @type {string[]} */
  const calls = [];
  const a = BackHandler.addEventListener('hardwareBackPress', () => {
    calls.push('a');
    return true;
  });
  const b = BackHandler.addEventListener('hardwareBackPress', () => {
    calls.push('b');
    return false;
  });
  assert.equal(BackHandler._emitBackPress(), true);
  assert.deepEqual(calls, ['b', 'a']);
  b.remove();
  a.remove();
  assert.equal(BackHandler._emitBackPress(), false);
  assert.equal(
    captureWarnings(() => {
      BackHandler.exitApp();
      BackHandler.exitApp();
    }).length,
    1,
    'exitApp without a host function warns once'
  );
}

// ---- BackHandler bridge: the host emits `hardwareBackPress` when the shell's
// XF86Back reaches the window, and exitApp calls the host's exit function.
{
  deviceEvents.listeners.clear();
  const BackHandler = fresh('Utilities/BackHandler.js');
  /** @type {string[]} */
  const calls = [];
  const lower = BackHandler.addEventListener('hardwareBackPress', () => {
    calls.push('lower');
    return true;
  });
  const upper = BackHandler.addEventListener('hardwareBackPress', () => {
    calls.push('upper');
    return false;
  });
  deviceEvents.emit('hardwareBackPress', undefined);
  assert.deepEqual(calls, ['upper', 'lower'], 'device event runs newest first, stops at handled');
  upper.remove();
  deviceEvents.emit('hardwareBackPress', undefined);
  assert.deepEqual(calls, ['upper', 'lower', 'lower']);
  lower.remove();
  calls.length = 0;
  deviceEvents.emit('hardwareBackPress', undefined);
  assert.deepEqual(calls, [], 'no listener: nothing runs and nothing throws');
  assert.equal(deviceEvents.listeners.get('hardwareBackPress')?.size, 1, 'one subscription');

  let exits = 0;
  globals.__rnqnxMarathonExitApp = () => {
    exits += 1;
  };
  assert.equal(captureWarnings(() => BackHandler.exitApp()).length, 0);
  assert.equal(exits, 1, 'exitApp calls the host exit function');
  delete globals.__rnqnxMarathonExitApp;
}

ModuleInternals._load = originalLoad;
console.log('marathon-overrides.test.mjs passed');
