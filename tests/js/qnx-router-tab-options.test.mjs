// expo-router Tabs options on QNX: each tab bar option either reaches the bar
// that draws it (the native Qt bar for the set it can draw, JsTabBar for the
// rest) or is covered by a test here. Groups: bar routing, item styling, custom
// pieces (icon, button, background), label and variant, per-tab tints, badge,
// accessibility and ids, hide on keyboard.
import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);
const base = require(path.join(root, 'tests/js/helpers/mock-react-native.cjs'));
const React = require('react');
const originalLoad = Module._load;

/** @type {Record<string, Set<() => void>>} */
const keyboardListeners = {};
const Keyboard = {
  addListener(/** @type {string} */ name, /** @type {() => void} */ fn) {
    (keyboardListeners[name] ??= new Set()).add(fn);
    return { remove: () => keyboardListeners[name].delete(fn) };
  },
};
const emitKeyboard = (/** @type {string} */ name) => keyboardListeners[name]?.forEach((fn) => fn());
let nativeTabBar = false;
const reactNative = { ...base, Keyboard, I18nManager: { isRTL: false } };
const nativeFake = {
  get: (/** @type {string} */ name) =>
    React.forwardRef((/** @type {any} */ props, /** @type {any} */ ref) =>
      React.createElement(name, { ...props, ref })
    ),
};
Module._load = function load(request, parent, isMain) {
  if (request === 'react-native') return reactNative;
  if (request === 'react-native/Libraries/ReactNative/UIManager')
    return {
      default: {
        hasViewManagerConfig: (/** @type {string} */ n) => nativeTabBar && n === 'RNQnxTabBar',
      },
    };
  if (request === 'react-native/Libraries/NativeComponent/NativeComponentRegistry')
    return nativeFake;
  return originalLoad.call(this, request, parent, isMain);
};

const TestRenderer = require('react-test-renderer');
const { act } = React;
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const src = path.join(root, 'packages/expo-router-qnx/src');
const { BottomTabBar } = require(path.join(src, 'BottomTabBar.js'));

/** @param {any} element */
function render(element) {
  /** @type {any} */
  let renderer;
  act(() => {
    renderer = TestRenderer.create(element);
  });
  return renderer;
}
const flat = (/** @type {any} */ style) =>
  Array.isArray(style) ? Object.assign({}, ...style.filter(Boolean).map(flat)) : style || {};
const hosts = (/** @type {any} */ renderer, /** @type {string} */ type) =>
  renderer.root.findAll((/** @type {any} */ n) => n.type === type);

/** A navigation state with `routes` and per-route options; `focused` is the active index. */
function bar(
  /** @type {Record<string, any>[]} */ routes,
  focused = 0,
  /** @type {Record<string, any>} */ shared = {}
) {
  const state = {
    index: focused,
    routes: routes.map((_, i) => ({ key: `r${i}`, name: `route${i}` })),
  };
  const descriptors = Object.fromEntries(
    routes.map((options, i) => [`r${i}`, { options: { title: `Tab ${i}`, ...shared, ...options } }])
  );
  /** @type {string[]} */
  const log = [];
  const element = React.createElement(BottomTabBar, {
    state,
    descriptors,
    emitter: {
      emit: (/** @type {any} */ event) => (
        log.push(`${event.type}:${event.target}`),
        { defaultPrevented: false }
      ),
    },
    navigateToTab: (/** @type {string} */ key) => log.push(`navigate:${key}`),
    insets: { bottom: 0 },
  });
  return { element, log };
}

// ---- bar routing: native bar for what it draws, JsTabBar for the rest ------------
nativeTabBar = true;
{
  const plain = bar([{ qnxIcon: 'A' }, { qnxIcon: 'B' }], 0, {
    tabBarActiveTintColor: '#0f0',
    tabBarStyle: { backgroundColor: '#001', height: 70 },
  });
  assert.equal(
    hosts(render(plain.element), 'RNQnxTabBar').length,
    1,
    'plain options use the native bar'
  );
  const cases = /** @type {Array<[string, Record<string, any>]>} */ ([
    ['tabBarBackground', { tabBarBackground: () => null }],
    ['tabBarButton', { tabBarButton: () => null }],
    ['tabBarItemStyle', { tabBarItemStyle: { margin: 4 } }],
    ['tabBarLabelStyle', { tabBarLabelStyle: { fontSize: 9 } }],
    ['tabBarIconStyle', { tabBarIconStyle: { opacity: 0.5 } }],
    ['tabBarAllowFontScaling', { tabBarAllowFontScaling: false }],
    ['tabBarActiveBackgroundColor', { tabBarActiveBackgroundColor: '#123' }],
    ['tabBarInactiveBackgroundColor', { tabBarInactiveBackgroundColor: '#321' }],
    ['tabBarButtonTestID', { tabBarButtonTestID: 'tab-x' }],
    ['tabBarVariant', { tabBarVariant: 'material' }],
    ['tabBarLabelPosition', { tabBarLabelPosition: 'beside-icon' }],
    ['tabBarStyle (border)', { tabBarStyle: { borderTopWidth: 3 } }],
    ['tabBarBadgeStyle (size)', { tabBarBadge: 2, tabBarBadgeStyle: { minWidth: 30 } }],
    ['tabBarIcon without qnxIcon', { qnxIcon: undefined, tabBarIcon: () => null }],
  ]);
  for (const [name, options] of cases) {
    const renderer = render(bar([{ qnxIcon: 'A', ...options }, { qnxIcon: 'B' }]).element);
    assert.equal(
      hosts(renderer, 'RNQnxTabBar').length,
      0,
      `${name} is drawn by the JavaScript bar`
    );
    assert.equal(
      hosts(renderer, 'View').filter(
        (/** @type {any} */ n) => n.props.accessibilityRole === 'tablist'
      ).length,
      1
    );
  }
  // A tint only the focused tab sets cannot be one native pair.
  const mixed = render(
    bar([{ qnxIcon: 'A', tabBarActiveTintColor: '#f00' }, { qnxIcon: 'B' }]).element
  );
  assert.equal(hosts(mixed, 'RNQnxTabBar').length, 0);
}
nativeTabBar = false;

// ---- item styling, custom pieces, label position, variant, tints, badge ------------
{
  const styled = render(
    bar(
      [
        {
          qnxIcon: 'A',
          tabBarItemStyle: { margin: 7 },
          tabBarLabelStyle: { fontSize: 9 },
          tabBarIconStyle: { opacity: 0.5 },
          tabBarAllowFontScaling: false,
          tabBarButtonTestID: 'tab-a',
          tabBarBadge: 5,
          tabBarBadgeStyle: { backgroundColor: '#abc', color: '#def', minWidth: 30 },
          tabBarActiveBackgroundColor: '#112233',
          tabBarActiveTintColor: '#00ff00',
        },
        {
          qnxIcon: 'B',
          tabBarInactiveBackgroundColor: '#445566',
          tabBarInactiveTintColor: '#ff0000',
        },
      ],
      0,
      {
        tabBarStyle: { backgroundColor: '#010203', borderTopWidth: 4, height: 64 },
        tabBarBackground: () => React.createElement('Backdrop'),
      }
    ).element
  );
  const items = hosts(styled, 'Pressable');
  assert.equal(items.length, 2);
  assert.equal(items[0].props.testID, 'tab-a', 'tabBarButtonTestID');
  const first = flat(items[0].props.style);
  assert.equal(first.margin, 7, 'tabBarItemStyle');
  assert.equal(first.backgroundColor, '#112233', 'tabBarActiveBackgroundColor');
  assert.equal(
    flat(items[1].props.style).backgroundColor,
    '#445566',
    'tabBarInactiveBackgroundColor'
  );
  const labels = hosts(styled, 'Text').filter(
    (/** @type {any} */ n) => n.props.children === 'Tab 0' || n.props.children === 'Tab 1'
  );
  assert.equal(flat(labels[0].props.style).fontSize, 9, 'tabBarLabelStyle');
  assert.equal(labels[0].props.allowFontScaling, false, 'tabBarAllowFontScaling');
  assert.equal(
    flat(labels[0].props.style).color,
    '#00ff00',
    'the focused tab uses its own active tint'
  );
  assert.equal(
    flat(labels[1].props.style).color,
    '#ff0000',
    'an unfocused tab uses its own inactive tint'
  );
  const iconBox = hosts(styled, 'View').find(
    (/** @type {any} */ n) => flat(n.props.style).opacity === 0.5
  );
  assert.ok(iconBox, 'tabBarIconStyle');
  const badge = hosts(styled, 'View').find(
    (/** @type {any} */ n) => flat(n.props.style).minWidth === 30
  );
  assert.ok(badge, 'tabBarBadgeStyle keys beyond color reach the badge');
  assert.equal(flat(badge.props.style).backgroundColor, '#abc');
  const badgeText = hosts(styled, 'Text').find((/** @type {any} */ n) => n.props.children === '5');
  assert.equal(flat(badgeText.props.style).color, '#def');
  const list = hosts(styled, 'View').find(
    (/** @type {any} */ n) => n.props.accessibilityRole === 'tablist'
  );
  const listStyle = flat(list.props.style);
  assert.equal(listStyle.borderTopWidth, 4, 'tabBarStyle reaches the bar');
  assert.equal(listStyle.backgroundColor, '#010203');
  assert.equal(hosts(styled, 'Backdrop').length, 1, 'tabBarBackground');
}
{
  // tabBarButton replaces the pressable and receives the press handlers and children.
  /** @type {any} */
  let received;
  const Button = (/** @type {any} */ props) => (
    (received = props),
    React.createElement('CustomButton', { testID: props.testID }, props.children)
  );
  const { element, log } = bar([
    { qnxIcon: 'A', tabBarButton: Button, href: '/a' },
    { qnxIcon: 'B' },
  ]);
  const renderer = render(element);
  assert.equal(hosts(renderer, 'CustomButton').length, 1);
  assert.equal(received.href, '/a');
  assert.equal(received.accessibilityRole, 'tab');
  assert.equal(received.role, 'tab');
  assert.deepEqual(received.accessibilityState, { selected: true });
  act(() => received.onPress());
  assert.deepEqual(log, ['tabPress:r0']);
}
{
  const label = render(
    bar([{ qnxIcon: 'A', tabBarLabelPosition: 'beside-icon' }, { qnxIcon: 'B' }]).element
  );
  assert.equal(
    flat(hosts(label, 'Pressable')[0].props.style).flexDirection,
    'row',
    'label beside the icon'
  );
  const below = render(
    bar([
      { qnxIcon: 'A', tabBarLabelPosition: 'below-icon', tabBarItemStyle: {} },
      { qnxIcon: 'B' },
    ]).element
  );
  assert.notEqual(flat(hosts(below, 'Pressable')[0].props.style).flexDirection, 'row');
  const material = render(
    bar([{ qnxIcon: 'A', tabBarVariant: 'material' }, { qnxIcon: 'B' }]).element
  );
  const pill = hosts(material, 'View').find(
    (/** @type {any} */ n) => flat(n.props.style).borderRadius === 14
  );
  assert.ok(pill, 'the material variant draws an indicator pill around the icon');
  const hidden = render(
    bar([{ qnxIcon: 'A', tabBarShowLabel: false, tabBarItemStyle: {} }, { qnxIcon: 'B' }]).element
  );
  assert.equal(
    hosts(hidden, 'Text').filter((/** @type {any} */ n) =>
      String(n.props.children).startsWith('Tab ')
    ).length,
    0,
    'tabBarShowLabel'
  );
}
{
  // tabBarIcon is called with focused, color and size, and tabBarAccessibilityLabel names the tab.
  /** @type {any[]} */
  const calls = [];
  const renderer = render(
    bar([
      {
        tabBarIcon: (/** @type {any} */ p) => (calls.push(p), React.createElement('Icon')),
        tabBarAccessibilityLabel: 'First tab',
      },
      { tabBarIcon: () => React.createElement('Icon') },
    ]).element
  );
  assert.equal(hosts(renderer, 'Icon').length, 2);
  assert.equal(calls[0].focused, true);
  assert.equal(calls[0].size, 24);
  assert.equal(hosts(renderer, 'Pressable')[0].props.accessibilityLabel, 'First tab');
}

// ---- hide on keyboard ------------------------------------------------------------
{
  const { element } = bar([
    {
      qnxIcon: 'A',
      tabBarHideOnKeyboard: true,
      tabBarVisibilityAnimationConfig: { hide: { animation: 'timing', config: { duration: 1 } } },
    },
    { qnxIcon: 'B' },
  ]);
  const renderer = render(element);
  const tabBar = () =>
    hosts(renderer, 'View').filter(
      (/** @type {any} */ n) => n.props.accessibilityRole === 'tablist'
    ).length;
  assert.equal(tabBar(), 1);
  act(() => emitKeyboard('keyboardDidShow'));
  assert.equal(tabBar(), 0, 'the bar is hidden while the keyboard is shown');
  act(() => emitKeyboard('keyboardDidHide'));
  assert.equal(tabBar(), 1, 'and returns when it hides');
  act(() => renderer.unmount());
  assert.equal(keyboardListeners.keyboardDidShow.size, 0, 'listeners are removed on unmount');
  // Without the option, no listener is added.
  const plain = render(bar([{ qnxIcon: 'A' }, { qnxIcon: 'B' }]).element);
  assert.equal(keyboardListeners.keyboardDidShow.size, 0);
  act(() => plain.unmount());
}

Module._load = originalLoad;
for (const key of Object.keys(require.cache)) {
  if (key.startsWith(src)) delete require.cache[key];
}
console.log('qnx-router-tab-options: ok');
