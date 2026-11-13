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
  if (request === 'react-native') return originalLoad.call(this, mockPath, parent, isMain);
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

Module._load = originalLoad;
// The stubbed native registry must not leak into tests that load the router package later.
for (const key of Object.keys(require.cache)) {
  if (key.startsWith(path.join(root, 'packages/expo-router-qnx/src'))) delete require.cache[key];
}
console.log('qnx-router-native-drawer: ok');
