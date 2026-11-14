// @ts-nocheck: Module._load test doubles.
// QtDrawer, the adapter for the native RNQnxDrawer: every drawer prop that
// react-native-drawer-layout passes reaches the native item or the panel, and
// native events become onOpen, onClose and the transition callbacks.
import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);
const mockPath = path.join(root, 'tests/js/helpers/mock-react-native.cjs');
const originalLoad = Module._load;
let dismissed = 0;
const reactNative = { ...require(mockPath), Keyboard: { dismiss: () => dismissed++ } };
const React = require('react');

// A native component the way the host registers one: a host element that keeps its props.
const nativeFake = {
  get: (/** @type {string} */ name) => {
    const Component = React.forwardRef((/** @type {any} */ props, /** @type {any} */ ref) =>
      React.createElement(name, { ...props, ref })
    );
    return Component;
  },
};
Module._load = function load(request, parent, isMain) {
  if (request === 'react-native') return reactNative;
  if (request === 'react-native/Libraries/ReactNative/UIManager')
    return { default: { hasViewManagerConfig: () => true } };
  if (request === 'react-native/Libraries/NativeComponent/NativeComponentRegistry')
    return nativeFake;
  return originalLoad.call(this, request, parent, isMain);
};

const TestRenderer = require('react-test-renderer');
const { act } = React;
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const { QtDrawer } = require(path.join(root, 'packages/expo-router-qnx/src/QtDrawer.js'));
const { Drawer } = require(path.join(root, 'packages/expo-router-qnx/src/drawer-layout.js'));
const { JsDrawer } = require(path.join(root, 'packages/expo-router-qnx/src/JsDrawer.js'));
globalThis.__DEV__ = true;

/** @param {any} element */
function render(element) {
  /** @type {any} */
  let renderer;
  act(() => {
    renderer = TestRenderer.create(element);
  });
  return renderer;
}
/** @param {any} renderer @param {string} type @returns {any} */
const hostOf = (renderer, type) =>
  renderer.root.findAll((/** @type {any} */ n) => n.type === type)[0];
const flat = (/** @type {any} */ style) =>
  Array.isArray(style) ? Object.assign({}, ...style.map(flat)) : style || {};

const base = {
  renderDrawerContent: () => React.createElement('Body'),
  layout: { width: 800, height: 600 },
  children: React.createElement('Content'),
  open: false,
  onOpen() {},
  onClose() {},
};

// ---- native item props -----------------------------------------------------------
{
  const native = hostOf(render(React.createElement(QtDrawer, base)), 'RNQnxDrawer');
  assert.equal(native.props.open, false);
  assert.equal(native.props.side, 'left');
  assert.equal(native.props.drawerType, 'front');
  assert.equal(native.props.swipeEnabled, true);
  assert.equal(native.props.edgeWidth, 24, 'swipeEdgeWidth defaults to 24');
  assert.equal(
    native.props.drawerWidth,
    320,
    'the default width is 80% of the layout, at most 320'
  );
  assert.equal(native.props.scrimColor, 'rgba(0,0,0,0.5)');
  assert.equal(native.props.accessibilityLabel, 'Close drawer');
}
{
  const native = hostOf(
    render(
      React.createElement(QtDrawer, {
        ...base,
        open: true,
        drawerPosition: 'right',
        drawerType: 'slide',
        swipeEnabled: false,
        swipeEdgeWidth: 64,
        drawerStyle: { width: 300, backgroundColor: '#123456', borderRadius: 12 },
        overlayStyle: [{ backgroundColor: 'rgba(10,20,30,0.4)' }, { opacity: 1 }],
        overlayAccessibilityLabel: 'Dismiss menu',
      })
    ),
    'RNQnxDrawer'
  );
  assert.equal(native.props.open, true);
  assert.equal(native.props.side, 'right');
  assert.equal(native.props.drawerType, 'slide');
  assert.equal(native.props.swipeEnabled, false);
  assert.equal(native.props.edgeWidth, 64, 'swipeEdgeWidth reaches the native edge strip');
  assert.equal(native.props.drawerWidth, 300, 'drawerStyle.width sets the panel width');
  assert.equal(
    native.props.scrimColor,
    'rgba(10,20,30,0.4)',
    'the overlay color reaches the scrim'
  );
  assert.equal(native.props.accessibilityLabel, 'Dismiss menu');
}
for (const drawerType of ['front', 'back', 'slide']) {
  const native = hostOf(
    render(React.createElement(QtDrawer, { ...base, drawerType })),
    'RNQnxDrawer'
  );
  assert.equal(native.props.drawerType, drawerType);
}
{
  // An unknown type draws as `front`.
  const native = hostOf(
    render(React.createElement(QtDrawer, { ...base, drawerType: 'wat' })),
    'RNQnxDrawer'
  );
  assert.equal(native.props.drawerType, 'front');
}

// ---- panel and content --------------------------------------------------------------
{
  const renderer = render(
    React.createElement(QtDrawer, {
      ...base,
      drawerWidth: 260,
      drawerStyle: { backgroundColor: '#123456', borderRadius: 12, left: 99, width: 10 },
    })
  );
  const [panel, content] = hostOf(renderer, 'RNQnxDrawer').children;
  const panelStyle = flat(panel.props.style);
  assert.equal(panelStyle.backgroundColor, '#123456', 'drawerStyle paints the panel');
  assert.equal(panelStyle.borderRadius, 12);
  assert.equal(panelStyle.position, 'absolute');
  assert.equal(panelStyle.left, 0, 'placement is not overridable');
  assert.equal(panelStyle.width, 260, 'the native item moves a panel of the declared width');
  assert.equal(flat(content.props.style).right, 0);
}

// ---- events ------------------------------------------------------------------------------
{
  /** @type {string[]} */
  const calls = [];
  const renderer = render(
    React.createElement(QtDrawer, {
      ...base,
      onOpen: () => calls.push('open'),
      onClose: () => calls.push('close'),
      onTransitionStart: (/** @type {boolean} */ closing) =>
        calls.push(`start:${closing ? 'closing' : 'opening'}`),
      onTransitionEnd: (/** @type {boolean} */ closed) =>
        calls.push(`end:${closed ? 'closed' : 'opened'}`),
    })
  );
  const native = hostOf(renderer, 'RNQnxDrawer');
  act(() => native.props.onRequestChange({ nativeEvent: { open: true, reason: 'swipe' } }));
  act(() => native.props.onRequestChange({ nativeEvent: { open: false, reason: 'escape' } }));
  act(() => native.props.onTransitionEnd({ nativeEvent: { open: true } }));
  act(() => native.props.onTransitionEnd({ nativeEvent: { open: false } }));
  assert.deepEqual(calls, ['open', 'close', 'end:opened', 'end:closed']);
  act(() =>
    renderer.update(
      React.createElement(QtDrawer, {
        ...base,
        open: true,
        onTransitionStart: (/** @type {boolean} */ c) =>
          calls.push(`start:${c ? 'closing' : 'opening'}`),
      })
    )
  );
  assert.equal(calls.at(-1), 'start:opening');
}

// ---- permanent ----------------------------------------------------------------------------
for (const position of ['left', 'right']) {
  const renderer = render(
    React.createElement(QtDrawer, {
      ...base,
      drawerType: 'permanent',
      drawerPosition: position,
      drawerWidth: 240,
      drawerStyle: { backgroundColor: '#abcdef' },
    })
  );
  assert.equal(
    renderer.root.findAll((/** @type {any} */ n) => n.type === 'RNQnxDrawer').length,
    0,
    'no native view'
  );
  const row = renderer.root.findAll((/** @type {any} */ n) => n.type === 'View')[0];
  assert.equal(flat(row.props.style).flexDirection, position === 'right' ? 'row-reverse' : 'row');
  const panel = renderer.root.findAll((/** @type {any} */ n) => n.type === 'View')[1];
  assert.equal(flat(panel.props.style).width, 240);
  assert.equal(flat(panel.props.style).backgroundColor, '#abcdef');
  assert.equal(renderer.root.findAll((/** @type {any} */ n) => n.type === 'Body').length, 1);
}

// ---- the drawer-layout replacement picks the native item ---------------------------------
{
  const native = hostOf(
    render(React.createElement(Drawer, { ...base, swipeEdgeWidth: 40 })),
    'RNQnxDrawer'
  );
  assert.equal(native.props.edgeWidth, 40);
}

// ---- gestures, swipeMinDistance, keyboard, progress, unsupported props ------------------
{
  const { resetWarnings } = require(path.join(root, 'packages/expo-router-qnx/src/devWarnings.js'));
  const { DrawerProgressContext } = require(
    path.join(root, 'packages/expo-router-qnx/src/drawerContexts.js')
  );
  /** @type {string[]} */
  const events = [];
  /** @type {any} */
  let progress;
  const Probe = () => ((progress ??= React.useContext(DrawerProgressContext)), null);
  const props = {
    ...base,
    swipeMinDistance: 72,
    onGestureStart: () => events.push('start'),
    onGestureEnd: () => events.push('end'),
    onGestureCancel: () => events.push('cancel'),
    children: React.createElement(Probe),
  };
  const renderer = render(React.createElement(QtDrawer, props));
  const native = hostOf(renderer, 'RNQnxDrawer');
  assert.equal(native.props.swipeMinDistance, 72, 'swipeMinDistance reaches the native item');
  assert.ok(
    progress && typeof progress.interpolate === 'function',
    'useDrawerProgress has a value on the native path'
  );
  assert.equal(progress.value, 0);
  act(() => native.props.onGestureStart({ nativeEvent: { open: false } }));
  act(() => native.props.onGestureEnd({ nativeEvent: { open: false } }));
  act(() => native.props.onGestureCancel({ nativeEvent: { open: false } }));
  assert.deepEqual(events, ['start', 'end', 'cancel'], 'gesture callbacks');
  assert.equal(
    dismissed,
    1,
    'a drag blurs the focused input by default (keyboardDismissMode on-drag)'
  );
  const quiet = hostOf(
    render(React.createElement(QtDrawer, { ...props, keyboardDismissMode: 'none' })),
    'RNQnxDrawer'
  );
  act(() => quiet.props.onGestureStart({ nativeEvent: { open: false } }));
  assert.equal(dismissed, 1, "keyboardDismissMode 'none' leaves the input alone");
  act(() => renderer.update(React.createElement(QtDrawer, { ...props, open: true })));
  assert.equal(progress.value, 1, 'the progress follows open');

  // Unsupported props: one development warning per name, none in release builds.
  const warnings = /** @type {string[]} */ ([]);
  const originalWarn = console.warn;
  console.warn = (/** @type {string} */ message) => warnings.push(message);
  resetWarnings();
  const unsupported = {
    hideStatusBarOnOpen: true,
    statusBarAnimation: 'fade',
    configureGestureHandler: () => {},
  };
  render(React.createElement(QtDrawer, { ...base, ...unsupported }));
  render(React.createElement(QtDrawer, { ...base, ...unsupported }));
  render(React.createElement(JsDrawer, { ...base, ...unsupported }));
  assert.equal(warnings.length, 3, 'one warning for each of the three props, once');
  for (const name of Object.keys(unsupported)) {
    assert.ok(
      warnings.some((w) => w.includes(`"${name}"`)),
      `the warning names ${name}`
    );
  }
  // hideStatusBarOnOpen=false asks for nothing.
  resetWarnings();
  warnings.length = 0;
  render(React.createElement(QtDrawer, { ...base, hideStatusBarOnOpen: false }));
  assert.equal(warnings.length, 0);
  globalThis.__DEV__ = false;
  resetWarnings();
  render(React.createElement(QtDrawer, { ...base, ...unsupported }));
  assert.equal(warnings.length, 0, 'release builds are silent');
  globalThis.__DEV__ = true;
  console.warn = originalWarn;
}
{
  // JsDrawer: gesture callbacks, swipeMinDistance and keyboardDismissMode.
  /** @type {any} */
  let captured;
  // Not a literal: a literal would pull React Native's type declarations into tsc.
  const rnName = 'react-native';
  const rn = require(rnName);
  const originalCreate = rn.PanResponder.create;
  rn.PanResponder.create = (/** @type {any} */ config) => (
    (captured = config),
    { panHandlers: {} }
  );
  /** @type {string[]} */
  const events = [];
  /** @type {string[]} */
  const calls = [];
  const drawer = render(
    React.createElement(JsDrawer, {
      ...base,
      drawerWidth: 200,
      swipeMinDistance: 100,
      onOpen: () => calls.push('open'),
      onGestureStart: () => events.push('start'),
      onGestureEnd: () => events.push('end'),
      onGestureCancel: () => events.push('cancel'),
    })
  );
  const before = dismissed;
  act(() => captured.onPanResponderGrant());
  assert.equal(dismissed, before + 1);
  // 90 px is 45% of 200 (past the 40% rule) but under swipeMinDistance 100: it does not open.
  act(() => captured.onPanResponderRelease({}, { dx: 90 }));
  assert.deepEqual(calls, []);
  act(() => captured.onPanResponderGrant());
  act(() => captured.onPanResponderRelease({}, { dx: 110 }));
  assert.deepEqual(calls, ['open']);
  act(() => captured.onPanResponderTerminate());
  assert.deepEqual(events, ['start', 'end', 'start', 'end', 'cancel']);
  act(() => drawer.unmount());
  for (const drawerPosition of ['left', 'right']) {
    for (const open of [false, true]) {
      const changes = [];
      const sign = (drawerPosition === 'right' ? -1 : 1) * (open ? -1 : 1);
      const flingDrawer = render(
        React.createElement(JsDrawer, {
          ...base,
          drawerPosition,
          drawerWidth: 200,
          swipeMinDistance: 100,
          open,
          onOpen: () => changes.push('open'),
          onClose: () => changes.push('close'),
        })
      );
      act(() => captured.onPanResponderGrant());
      act(() => captured.onPanResponderRelease({}, { dx: sign * 60, vx: sign * 0.5 }));
      assert.deepEqual(changes, [], 'exactly 500 px/s still obeys minimum distance');
      act(() => captured.onPanResponderGrant());
      act(() => captured.onPanResponderRelease({}, { dx: sign * 60, vx: sign * 0.6 }));
      assert.deepEqual(
        changes,
        [open ? 'close' : 'open'],
        `${drawerPosition} short fling changes state`
      );
      act(() => flingDrawer.unmount());
    }
  }
  rn.PanResponder.create = originalCreate;
}

Module._load = originalLoad;
// The stubbed native registry must not leak into tests that load the router package later.
for (const key of Object.keys(require.cache)) {
  if (key.startsWith(path.join(root, 'packages/expo-router-qnx/src'))) delete require.cache[key];
}
console.log('qnx-router-native-drawer: ok');
