// @ts-nocheck: test harness around untyped Babel and react-test-renderer.
// Host test for pull-to-refresh on QNX. RefreshControl never fired onRefresh:
// upstream ScrollView mounts a refreshControl only on ios and android, and the
// QNX RefreshControl did nothing, so the callback had no path to run. The QNX
// ScrollView (packages/react-native-qnx/Libraries/Components/ScrollView) now
// detects a pull from touch and scroll events. This test runs the real
// override and the real PullToRefresh helper with host elements for the native
// scroll views. It is not Pi 400 or QNX VM evidence.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import vm from 'node:vm';

const root = process.cwd();
const require = createRequire(import.meta.url);
const React = require('react');
const { act, create } = require('react-test-renderer');
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
globalThis.__DEV__ = true;
const h = React.createElement;

const scrollDir = path.join(root, 'packages/react-native-qnx/Libraries/Components/ScrollView');
const presetRequire = createRequire(require.resolve('babel-preset-expo'));
const babel = presetRequire('@babel/core');

// --- The helper alone ------------------------------------------------------
const { createPullToRefresh, PULL_THRESHOLD } = require(path.join(scrollDir, 'PullToRefresh.js'));
{
  const pull = createPullToRefresh(50);
  pull.start(100, 0, 1, true);
  assert.equal(pull.move(120, 1), false, 'short of the threshold');
  assert.equal(pull.move(150, 1), true, 'fires when the finger has travelled the threshold');
  assert.equal(pull.move(300, 1), false, 'fires once per gesture');
  pull.end();
  pull.start(100, 0, 1, true);
  assert.equal(pull.move(160, 1), true, 'a new gesture can fire again');
  pull.end();

  pull.start(100, 0, 1, true);
  assert.equal(pull.move(60, 1), false, 'an upward drag never fires');
  assert.equal(pull.move(200, 1), true, 'travel is measured from where the finger went down');
  pull.end();

  pull.start(100, 40, 1, true);
  assert.equal(pull.move(300, 1), false, 'does not start below the top');
  pull.start(100, 0, 1, true);
  pull.scrolled(12);
  assert.equal(pull.move(300, 1), false, 'content that scrolled was not pulled from the top');
  pull.start(100, 0, 1, true);
  pull.scrolled(0);
  assert.equal(pull.move(300, 1), true, 'scrolling back to the top does not disarm');
  pull.start(100, 0, 2, true);
  assert.equal(pull.move(300, 2), false, 'two fingers are not a pull');
  pull.start(100, 0, 1, true);
  assert.equal(pull.move(300, 2), false, 'a second finger cancels the pull');
  assert.equal(pull.move(400, 1), false, 'and it stays cancelled');
  pull.start(100, 0, 1, false);
  assert.equal(pull.move(300, 1), false, 'disabled or already refreshing');
  pull.start(Number.NaN, 0, 1, true);
  assert.equal(pull.move(300, 1), false, 'a touch without a position is ignored');
  pull.start(100, 0, 1, true);
  pull.end();
  assert.equal(pull.move(300, 1), false, 'a finished gesture stays finished');
  assert.equal(PULL_THRESHOLD, 72);
}

// --- The real QNX ScrollView ------------------------------------------------
const rnRoot = path.dirname(require.resolve('react-native/package.json'));
const host = (name) => {
  const Component = React.forwardRef((props, ref) => h(name, { ...props, ref }));
  Component.displayName = name;
  return Component;
};
const flatten = (style) =>
  Array.isArray(style)
    ? Object.assign({}, ...style.map(flatten))
    : style && typeof style === 'object'
      ? { ...style }
      : {};
class AnimatedValue {
  setOffset() {}
  setValue() {}
  addListener() {
    return 'id';
  }
  removeListener() {}
}
const subscription = { remove() {} };
// Everything the ScrollView imports that needs a native module or a running app.
const stubs = {
  'Libraries/Components/ScrollView/ScrollViewNativeComponent': { default: host('RCTScrollView') },
  'Libraries/Components/ScrollView/ScrollContentViewNativeComponent': {
    default: host('RCTScrollContentView'),
  },
  'Libraries/Animated/AnimatedImplementation': { default: { Value: AnimatedValue } },
  'Libraries/Interaction/FrameRateLogger': { default: {} },
  'Libraries/ReactNative/RendererProxy': { findNodeHandle: () => 1 },
  'Libraries/ReactNative/UIManager': { default: {} },
  'Libraries/StyleSheet/flattenStyle': { default: flatten },
  'Libraries/StyleSheet/splitLayoutProps': { default: () => ({ outer: {}, inner: {} }) },
  'Libraries/StyleSheet/StyleSheet': {
    default: { create: (styles) => styles, flatten, compose: (a, b) => [a, b] },
  },
  'Libraries/Utilities/Dimensions': { default: { get: () => ({ width: 100, height: 100 }) } },
  'Libraries/Utilities/dismissKeyboard': { default: () => {} },
  'Libraries/Utilities/Platform': {
    default: { OS: 'qnx', select: (spec) => spec.qnx ?? spec.default },
  },
  'Libraries/Components/Keyboard/Keyboard': {
    default: { addListener: () => subscription, isVisible: () => false, metrics: () => null },
  },
  'Libraries/Components/TextInput/TextInputState': {
    default: { currentlyFocusedInput: () => null, blurTextInput() {} },
  },
  'Libraries/Components/View/View': { default: host('View') },
  'Libraries/Components/ScrollView/ScrollViewCommands': { default: {} },
  'Libraries/Components/ScrollView/ScrollViewStickyHeader': { default: host('StickyHeader') },
};
const compiled = new Map();
const cache = new Map();
function load(file) {
  if (cache.has(file)) return cache.get(file).exports;
  const module = { exports: {} };
  cache.set(file, module);
  if (!compiled.has(file)) {
    compiled.set(
      file,
      babel.transformSync(readFileSync(file, 'utf8'), {
        filename: file,
        babelrc: false,
        configFile: false,
        presets: [presetRequire.resolve('@react-native/babel-preset')],
      }).code
    );
  }
  const localRequire = (request) => {
    // The override spells every React Native import as react-native/Libraries/...
    // (Metro cannot resolve upstream-relative paths from outside React Native's
    // tree), apart from its own PullToRefresh helper.
    if (request === './PullToRefresh') return load(path.join(scrollDir, 'PullToRefresh.js'));
    const prefix = 'react-native/';
    if (request.startsWith(prefix)) {
      const key = request.slice(prefix.length);
      if (stubs[key]) return { __esModule: true, ...stubs[key] };
      return load(path.join(rnRoot, `${key}.js`));
    }
    if (request.startsWith('.')) {
      const withExt = `${path.resolve(path.dirname(file), request)}.js`;
      return load(withExt);
    }
    return createRequire(path.join(rnRoot, 'package.json'))(request);
  };
  const wrapper = vm.runInThisContext(
    `(function (exports, require, module, __filename, __dirname) {${compiled.get(file)}\n})`,
    { filename: file }
  );
  wrapper(module.exports, localRequire, module, file, path.dirname(file));
  return module.exports;
}
const ScrollView = load(path.join(scrollDir, 'ScrollView.js')).default;

const touch = (pageY) => ({ pageY });
const down = (view, pageY, fingers = 1) =>
  act(() =>
    view.props.onTouchStart({ nativeEvent: { pageY, touches: Array(fingers).fill(touch(pageY)) } })
  );
const move = (view, pageY, fingers = 1) =>
  act(() =>
    view.props.onTouchMove({ nativeEvent: { pageY, touches: Array(fingers).fill(touch(pageY)) } })
  );
const up = (view) => act(() => view.props.onTouchEnd({ nativeEvent: { touches: [] }, target: 1 }));
const cancel = (view) => act(() => view.props.onTouchCancel({ nativeEvent: { touches: [] } }));
const scrollTo = (view, y) =>
  act(() =>
    view.props.onScroll({
      nativeEvent: {
        contentOffset: { x: 0, y },
        contentSize: { width: 100, height: 500 },
        layoutMeasurement: { width: 100, height: 100 },
      },
    })
  );
function mount(props) {
  let renderer;
  act(() => {
    renderer = create(h(ScrollView, props, h('Text', null, 'row')));
  });
  return { renderer, view: () => renderer.root.findByType('RCTScrollView') };
}
const control = (props) => h('RefreshControl', props);

{
  const calls = [];
  const { view } = mount({
    refreshControl: control({ refreshing: false, onRefresh: () => calls.push('refresh') }),
  });
  down(view(), 200);
  move(view(), 230);
  assert.deepEqual(calls, [], 'a short pull does not refresh');
  move(view(), 280);
  assert.deepEqual(calls, ['refresh'], 'a pull of at least the threshold calls onRefresh');
  move(view(), 400);
  up(view());
  assert.deepEqual(calls, ['refresh'], 'once per gesture');

  down(view(), 200);
  move(view(), 290);
  cancel(view());
  assert.deepEqual(
    calls,
    ['refresh', 'refresh'],
    'a cancel after the threshold has already refreshed'
  );

  // Content scrolled away from the top: dragging down is scrolling back up.
  scrollTo(view(), 150);
  down(view(), 200);
  move(view(), 400);
  assert.equal(calls.length, 2, 'no refresh from below the top');
  up(view());
  scrollTo(view(), 0);
  down(view(), 200);
  move(view(), 300);
  assert.equal(calls.length, 3, 'back at the top, a pull refreshes again');
  up(view());

  down(view(), 200, 2);
  move(view(), 400, 2);
  assert.equal(calls.length, 3, 'two fingers are not a pull');
}

{
  const calls = [];
  const refreshing = mount({
    refreshControl: control({ refreshing: true, onRefresh: () => calls.push('x') }),
  });
  down(refreshing.view(), 200);
  move(refreshing.view(), 400);
  const disabled = mount({
    refreshControl: control({
      refreshing: false,
      enabled: false,
      onRefresh: () => calls.push('y'),
    }),
  });
  down(disabled.view(), 200);
  move(disabled.view(), 400);
  const horizontal = mount({
    horizontal: true,
    refreshControl: control({ refreshing: false, onRefresh: () => calls.push('z') }),
  });
  down(horizontal.view(), 200);
  move(horizontal.view(), 400);
  const none = mount({ onTouchMove: () => calls.push('own') });
  down(none.view(), 200);
  move(none.view(), 400);
  assert.deepEqual(
    calls,
    ['own'],
    'refreshing, disabled, horizontal and no control never refresh; the app handler still runs'
  );
}

{
  // The app's own touch and scroll handlers keep running, and the control element is not mounted.
  const seen = [];
  const { renderer, view } = mount({
    onTouchStart: () => seen.push('start'),
    onTouchMove: () => seen.push('move'),
    onTouchEnd: () => seen.push('end'),
    onScroll: () => seen.push('scroll'),
    refreshControl: control({ refreshing: false, onRefresh() {} }),
  });
  down(view(), 10);
  move(view(), 20);
  up(view());
  scrollTo(view(), 5);
  assert.deepEqual(seen, ['start', 'move', 'end', 'scroll']);
  assert.equal(
    renderer.root.findAllByType('RefreshControl').length,
    0,
    'no native pull view exists to mount'
  );
}

console.log(
  'pull-to-refresh: tracker and QNX ScrollView refreshControl contract passed (host test renderer only)'
);
