// Renders the router package's JavaScript fallbacks and the gallery's page
// measurement with react-test-renderer, against tests/js/helpers/mock-react-native.cjs.
import assert from 'node:assert/strict';
import Module, { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);

const mockPath = path.join(root, 'tests/js/helpers/mock-react-native.cjs');
const originalLoad = Module._load;
Module._load = function load(request, parent, isMain) {
  if (request === 'react-native') return originalLoad.call(this, mockPath, parent, isMain);
  return originalLoad.call(this, request, parent, isMain);
};

const React = require('react');
const TestRenderer = require('react-test-renderer');
const { act } = React;
globalThis.IS_REACT_ACT_ENVIRONMENT = true;

/** @param {any} element */
function render(element) {
  /** @type {any} */
  let renderer;
  act(() => {
    renderer = TestRenderer.create(element);
  });
  return renderer;
}
/** @param {any} node @param {(n: any) => boolean} match @returns {any[]} */
function findAll(node, match) {
  // Host elements only: a mocked component renders one host element with the same props.
  return node.root.findAll((n) => typeof n.type === 'string' && match(n));
}
const layoutEvent = (/** @type {number} */ width, /** @type {number} */ height) => ({
  nativeEvent: { layout: { x: 0, y: 0, width, height } },
});

// ---- Gallery pages: each Screen reports its own width --------------------------
const ui = require(path.join(root, 'apps/component-gallery/ui.js'));
const { Screen, useViewport, h } = ui;

function WidthProbe({ label }) {
  const { width } = useViewport();
  return h('Probe', { label, width });
}

const pages = render(
  h(
    'Root',
    null,
    // A tab page next to a rail, and a drawer page without one, mounted together.
    h(Screen, null, h(WidthProbe, { label: 'tab' })),
    h(Screen, null, h(WidthProbe, { label: 'drawer' }))
  )
);
const frames = findAll(pages, (n) => n.props && n.props.collapsable === false && n.props.onLayout);
assert.equal(frames.length, 2);
act(() => frames[0].props.onLayout(layoutEvent(784, 560)));
act(() => frames[1].props.onLayout(layoutEvent(880, 560)));
const widthOf = (/** @type {string} */ label) =>
  findAll(pages, (n) => n.type === 'Probe' && n.props.label === label)[0].props.width;
assert.equal(widthOf('tab'), 784, 'the tab page keeps its own width after a wider page lays out');
assert.equal(widthOf('drawer'), 880);
// A later layout of the drawer page does not move the tab page.
act(() => frames[1].props.onLayout(layoutEvent(700, 560)));
assert.equal(widthOf('tab'), 784);
// A resize reaches the page that was resized, and only that page.
act(() => frames[0].props.onLayout(layoutEvent(420, 560)));
assert.equal(widthOf('tab'), 420);
assert.equal(widthOf('drawer'), 700);
// Zero and non-finite sizes (a hidden page) are ignored.
act(() => frames[0].props.onLayout(layoutEvent(0, 0)));
assert.equal(widthOf('tab'), 420);

// The card width a grid derives fits inside the page at every window width.
for (const windowWidth of [360, 586, 700, 880, 1280, 1920, 2560]) {
  for (const rail of [0, 96]) {
    const page = windowWidth - rail;
    const inner = Math.max(0, page - 2 * 28);
    const columns = ui.gridColumns(inner);
    const card = Math.floor((inner - ui.GAP * (columns - 1)) / columns);
    assert.ok(card * columns + ui.GAP * (columns - 1) <= inner, `${windowWidth}/${rail}`);
    assert.ok(rail + 28 + inner + 28 <= windowWidth, 'cards end before the right padding');
  }
}

// ---- Inactive tabs are display: none -------------------------------------------
const { MaybeScreen, MaybeScreenContainer } = require(
  path.join(root, 'packages/expo-router-qnx/src/shims/tabs-screen-fallback.js')
);
const screens = render(
  h(
    MaybeScreenContainer,
    { enabled: true, hasTwoStates: true, style: { flex: 1 } },
    h(MaybeScreen, { enabled: true, active: 2, style: [{ position: 'absolute' }], key: 'on' }),
    h(MaybeScreen, { enabled: true, active: 0, style: [{ position: 'absolute' }], key: 'off' })
  )
);
const views = findAll(screens, (n) => n.type === 'View');
const container = views[0];
assert.equal(container.props.enabled, undefined, 'react-native-screens props are not forwarded');
assert.equal(container.props.hasTwoStates, undefined);
const [focused, inactive] = views.slice(1);
assert.equal(focused.props.pointerEvents, undefined);
assert.deepEqual(focused.props.style, [{ position: 'absolute' }]);
assert.deepEqual(inactive.props.style, [[{ position: 'absolute' }], { display: 'none' }]);
assert.equal(inactive.props.pointerEvents, 'none');

// ---- JavaScript tab bar -----------------------------------------------------------
const { JsTabBar } = require(path.join(root, 'packages/expo-router-qnx/src/JsTabBar.js'));
/** @type {string[]} */
const pressed = [];
const bar = render(
  h(JsTabBar, {
    tabs: [
      { key: 'a', title: 'Home', icon: 'H', badge: '', accessibilityLabel: 'Home tab' },
      { key: 'b', title: 'Inbox', icon: 'I', badge: '3', accessibilityLabel: 'Inbox tab' },
    ],
    activeKey: 'b',
    tint: { active: '#0f0', inactive: '#888', background: '#001' },
    layout: 'bottom',
    onTabPress: (/** @type {string} */ key) => pressed.push(key),
  })
);
const list = findAll(bar, (n) => n.type === 'View' && n.props.accessibilityRole === 'tablist')[0];
assert.ok(list);
const items = findAll(bar, (n) => n.type === 'Pressable');
assert.equal(items.length, 2);
assert.equal(items[0].props.accessibilityRole, 'tab');
assert.deepEqual(items[0].props.accessibilityState, { selected: false });
assert.deepEqual(items[1].props.accessibilityState, { selected: true });
assert.equal(items[1].props.accessibilityLabel, 'Inbox tab');
const texts = findAll(bar, (n) => n.type === 'Text').map((n) => String(n.props.children));
assert.ok(texts.includes('3'), 'the badge text is drawn');
assert.ok(texts.includes('Inbox') && texts.includes('Home'));
act(() => items[0].props.onPress());
assert.deepEqual(pressed, ['a']);

// ---- JavaScript drawer ---------------------------------------------------------------
const { JsDrawer } = require(path.join(root, 'packages/expo-router-qnx/src/JsDrawer.js'));
const drawerProps = {
  renderDrawerContent: () => h('DrawerBody'),
  layout: { width: 800, height: 600 },
  drawerWidth: 280,
  children: h('Content'),
};
const closed = render(h(JsDrawer, { ...drawerProps, open: false, onClose() {}, onOpen() {} }));
assert.equal(findAll(closed, (n) => n.type === 'DrawerBody').length, 0, 'a closed drawer mounts no panel content');
assert.equal(findAll(closed, (n) => n.props && n.props.accessibilityLabel === 'Close drawer').length, 0);
const opened = render(h(JsDrawer, { ...drawerProps, open: true, onClose() {}, onOpen() {} }));
assert.equal(findAll(opened, (n) => n.type === 'DrawerBody').length, 1);
const scrim = findAll(opened, (n) => n.type === 'Pressable' && n.props.accessibilityLabel === 'Close drawer');
assert.equal(scrim.length, 1);
let closedBy = 0;
const closable = render(h(JsDrawer, { ...drawerProps, open: true, onClose: () => closedBy++, onOpen() {} }));
act(() => findAll(closable, (n) => n.type === 'Pressable')[0].props.onPress());
assert.equal(closedBy, 1, 'pressing the scrim closes the drawer');
const permanent = render(
  h(JsDrawer, { ...drawerProps, open: false, drawerType: 'permanent', drawerPosition: 'right', onClose() {}, onOpen() {} })
);
const permanentRow = findAll(permanent, (n) => n.type === 'View')[0];
assert.equal(permanentRow.props.style[1].flexDirection, 'row-reverse', 'a permanent right drawer sits on the right');
assert.equal(findAll(permanent, (n) => n.type === 'DrawerBody').length, 1);

Module._load = originalLoad;
console.log('qnx-router-rendering: ok');
