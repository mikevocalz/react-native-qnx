// @ts-nocheck: controlled React Native animation and timer test doubles.
import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';
import path from 'node:path';

const require = createRequire(import.meta.url);
const React = require('react');
const TestRenderer = require('react-test-renderer');
const { act } = React;
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const mock = require('../helpers/mock-react-native.cjs');
const targets = [];
const panResponders = [];
const native = {
  ...mock,
  Animated: {
    ...mock.Animated,
    timing(value, config) {
      targets.push(config.toValue);
      return mock.Animated.timing(value, config);
    },
  },
  PanResponder: {
    create(handlers) {
      panResponders.push(handlers);
      return { panHandlers: {} };
    },
  },
};
const originalLoad = Module._load;
Module._load = function (request, parent, main) {
  return request === 'react-native' ? native : originalLoad.call(this, request, parent, main);
};
const { JsDrawer } = require(path.resolve('packages/expo-router-qnx/src/JsDrawer.js'));
const { setSwipeExclusionLeft } = require(
  path.resolve('packages/expo-router-qnx/src/swipeExclusion.js')
);
const realSetTimeout = globalThis.setTimeout;
const realClearTimeout = globalThis.clearTimeout;
const timers = new Map();
let nextTimer = 0;
globalThis.setTimeout = (callback, delay) => {
  assert.equal(delay, 400);
  const id = ++nextTimer;
  timers.set(id, callback);
  return id;
};
globalThis.clearTimeout = (id) => timers.delete(id);
const flushTimers = () =>
  act(() => {
    const pending = [...timers.values()];
    timers.clear();
    pending.forEach((callback) => callback());
  });
const base = {
  drawerWidth: 280,
  layout: { width: 800, height: 600 },
  renderDrawerContent: () => React.createElement('Panel'),
  children: React.createElement('Content'),
};
let renderer;
function mount(props) {
  act(() => {
    renderer = TestRenderer.create(React.createElement(JsDrawer, { ...base, ...props }));
  });
  return panResponders.at(-1);
}
function update(props) {
  act(() => renderer.update(React.createElement(JsDrawer, { ...base, ...props })));
}
function drag(pan, dx) {
  act(() => {
    pan.onPanResponderGrant();
    pan.onPanResponderMove(null, { dx, dy: 0 });
    pan.onPanResponderRelease(null, { dx, dy: 0 });
  });
}
const hasPanel = () => renderer.root.findAllByType('Panel').length === 1;
try {
  let opens = 0;
  const closed = { open: false, onOpen: () => opens++ };
  const pan = mount(closed);
  targets.length = 0;
  drag(pan, 180);
  assert.equal(opens, 1);
  assert.deepEqual(targets, [1], 'release must settle open while parent confirmation is pending');
  assert.ok(hasPanel());
  update({ ...closed, open: true });
  assert.equal(timers.size, 0, 'confirmation cancels the fallback');
  flushTimers();
  assert.ok(
    targets.every((value) => value === 1),
    'confirmation never reverses the animation'
  );
  act(() => renderer.unmount());

  const ignored = mount({ open: false, onOpen() {} });
  drag(ignored, 180);
  assert.ok(hasPanel());
  flushTimers();
  assert.equal(targets.at(-1), 0, 'an ignored request returns to the controlled state');
  assert.ok(!hasPanel());
  act(() => renderer.unmount());

  let closes = 0;
  const ignoredClose = mount({ open: true, onClose: () => closes++ });
  targets.length = 0;
  drag(ignoredClose, -180);
  assert.equal(closes, 1);
  assert.deepEqual(targets, [0]);
  assert.ok(!hasPanel());
  flushTimers();
  assert.equal(targets.at(-1), 1);
  assert.ok(hasPanel(), 'a rejected close restores panel content and input');
  act(() => renderer.unmount());

  const cancelled = mount({ open: false, onOpen: () => opens++ });
  const before = opens;
  act(() => {
    cancelled.onPanResponderGrant();
    cancelled.onPanResponderMove(null, { dx: 180, dy: 0 });
    cancelled.onPanResponderTerminate();
  });
  assert.equal(opens, before);
  assert.equal(targets.at(-1), 0);
  assert.equal(timers.size, 0);
  act(() => setSwipeExclusionLeft(96));
  const edge = renderer.root.findAllByType('View').at(-1);
  assert.equal(edge.props.style[1].left, 96, 'rail updates reach this instance through Zustand');
  drag(cancelled, 180);
  assert.equal(timers.size, 1);
  act(() => renderer.unmount());
  assert.equal(timers.size, 0, 'unmount cancels pending settlement');
} finally {
  Module._load = originalLoad;
  globalThis.setTimeout = realSetTimeout;
  globalThis.clearTimeout = realClearTimeout;
  setSwipeExclusionLeft(0);
}
console.log('qnx-router-drawer-settlement: ok');
