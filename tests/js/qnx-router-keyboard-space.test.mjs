// @ts-nocheck: React Native host doubles; the pinned BottomTabView and both
// tab-height contexts below are the real expo-router implementation.
import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';
import path from 'node:path';

const require = createRequire(import.meta.url);
const root = process.cwd();
const src = path.join(root, 'packages/expo-router-qnx/src');
const routerRequire = createRequire(path.join(src, 'BottomTabBar.js'));
const routerRoot = path.dirname(routerRequire.resolve('expo-router/package.json'));
const bottomTabs = path.join(routerRoot, 'build/react-navigation/bottom-tabs');
const React = require('react');
const TestRenderer = require('react-test-renderer');
const { act } = React;
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const base = require('./helpers/mock-react-native.cjs');
const originalLoad = Module._load;
const keyboardListeners = new Map();
let keyboardVisible = false;
let nativeTabBar = false;
const Keyboard = {
  isVisible: () => keyboardVisible,
  addListener(name, callback) {
    if (!keyboardListeners.has(name)) keyboardListeners.set(name, new Set());
    keyboardListeners.get(name).add(callback);
    return { remove: () => keyboardListeners.get(name).delete(callback) };
  },
};
const reactNative = {
  ...base,
  Keyboard,
  I18nManager: { isRTL: false },
  Animated: {
    ...base.Animated,
    parallel: (animations) => ({
      start: (callback) => {
        for (const animation of animations) animation.start();
        callback?.({ finished: true });
      },
    }),
  },
};
let safeArea;
function SafeAreaProviderCompat({ children, style }) {
  return React.createElement('SafeArea', { style }, children);
}
SafeAreaProviderCompat.initialMetrics = {
  frame: { width: 400, height: 600 },
  insets: { top: 0, right: 0, bottom: 10, left: 0 },
};
const elements = {
  SafeAreaProviderCompat,
  Screen: ({ children }) => children,
  Header: () => null,
  getHeaderTitle: (options, name) => options.title ?? name,
};
Module._load = function load(request, parent, isMain) {
  if (request === 'react-native') return reactNative;
  if (request === 'react-native-safe-area-context') return safeArea;
  if (request === 'react-native/Libraries/ReactNative/UIManager')
    return { default: { hasViewManagerConfig: () => nativeTabBar } };
  if (request === 'react-native/Libraries/NativeComponent/NativeComponentRegistry')
    return {
      get: (name) => React.forwardRef((props, ref) => React.createElement(name, { ...props, ref })),
    };
  if (parent?.filename === path.join(bottomTabs, 'views/BottomTabView.js')) {
    if (request === '../../elements') return elements;
    if (request === './BottomTabBar') return require(path.join(src, 'BottomTabBar.js'));
    if (request === './ScreenFallback')
      return require(path.join(src, 'shims/tabs-screen-fallback.js'));
    if (request === '../utils/useAnimatedHashMap')
      return {
        useAnimatedHashMap: (state) =>
          React.useMemo(
            () =>
              Object.fromEntries(
                state.routes.map((route) => [route.name, new base.Animated.Value(0)])
              ),
            [state.routes]
          ),
      };
  }
  return originalLoad.call(this, request, parent, isMain);
};
safeArea = require(path.join(src, 'shims/safe-area-context.js'));
const { BottomTabView } = require(path.join(bottomTabs, 'views/BottomTabView.js'));
const { BottomTabBarHeightContext } = require(
  path.join(bottomTabs, 'utils/BottomTabBarHeightContext.js')
);

function ScenePadding() {
  return React.createElement('ScenePadding', {
    paddingBottom: React.useContext(BottomTabBarHeightContext),
  });
}
const state = { index: 0, routes: [{ key: 'home', name: 'Home' }] };
const navigationProps = {
  state,
  emitter: { emit: () => ({ defaultPrevented: false }) },
  navigateToTab() {},
  popNestedStackToTop() {},
  safeAreaInsets: { bottom: 10 },
};
function element(options = {}) {
  return React.createElement(BottomTabView, {
    ...navigationProps,
    descriptors: {
      home: {
        route: state.routes[0],
        navigation: {},
        options: { tabBarPosition: 'bottom', qnxIcon: 'H', tabBarHideOnKeyboard: true, ...options },
        render: () => React.createElement(ScenePadding),
      },
    },
  });
}
function emitKeyboard(visible) {
  keyboardVisible = visible;
  act(() => {
    for (const listener of keyboardListeners.get(visible ? 'keyboardDidShow' : 'keyboardDidHide') ??
      [])
      listener();
  });
}
const padding = (renderer) => renderer.root.findByType('ScenePadding').props.paddingBottom;
const visibleBars = (renderer) =>
  renderer.root.findAll(
    (node) =>
      node.type === 'RNQnxTabBar' ||
      (node.type === 'View' && node.props.accessibilityRole === 'tablist')
  ).length;

try {
  for (const native of [false, true]) {
    nativeTabBar = native;
    keyboardVisible = false;
    let renderer;
    act(() => {
      renderer = TestRenderer.create(element());
    });
    assert.equal(padding(renderer), 70);
    assert.equal(visibleBars(renderer), 1);
    emitKeyboard(true);
    assert.equal(visibleBars(renderer), 0, 'the settled hidden bar occupies no scene layout');
    assert.equal(
      padding(renderer),
      0,
      'the real navigator reports no reserved bottom space while hidden'
    );
    emitKeyboard(false);
    assert.equal(visibleBars(renderer), 1);
    assert.equal(padding(renderer), 70, 'the inset returns with the visible bar');
    act(() => {
      renderer.update(element({ tabBarStyle: { height: 84 } }));
    });
    assert.equal(padding(renderer), 94, 'a changed height updates the navigator context');
    act(() => {
      renderer.update(element({ qnxTabBarHidden: true }));
    });
    assert.equal(padding(renderer), 0, 'an explicitly hidden bar reserves no space');
    act(() => {
      renderer.update(element({ tabBarPosition: 'left' }));
    });
    assert.equal(padding(renderer), 0, 'a side rail reserves no bottom space');
    act(() => {
      renderer.unmount();
    });

    keyboardVisible = true;
    act(() => {
      renderer = TestRenderer.create(element());
    });
    assert.equal(
      visibleBars(renderer),
      0,
      'mounting with an already visible keyboard starts hidden'
    );
    assert.equal(padding(renderer), 0);
    act(() => {
      renderer.update(element({ tabBarHideOnKeyboard: false }));
    });
    assert.equal(visibleBars(renderer), 1);
    assert.equal(padding(renderer), 70);
    act(() => {
      renderer.update(element());
    });
    assert.equal(
      visibleBars(renderer),
      0,
      'enabling the option resynchronizes current keyboard state'
    );
    assert.equal(padding(renderer), 0);
    act(() => {
      renderer.unmount();
    });
    assert.equal(keyboardListeners.get('keyboardDidShow').size, 0);
    assert.equal(keyboardListeners.get('keyboardDidHide').size, 0);
  }
  console.log(
    'qnx-router-keyboard-space: real BottomTabView context, JS/native bars and cleanup passed'
  );
} finally {
  Module._load = originalLoad;
}
