// Runs the component gallery's real route files through React and the real
// Yoga engine (tests/js/helpers/yoga-host.mjs) at 586x600, 880x600 and
// 1280x800, with a drawer destination mounted next to the tab pages, and
// checks what the Pi 400 showed: card right edges stay inside the page, and a
// page's layout does not depend on what any other page measured.
//
// Pre-fix guard: with the single shared width the first assertion block fails
// (the tab pages draw cards as wide as the drawer page). See
// docs/evidence/gallery-yoga-layout-2026-11-13.md for the red/green record.
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import path from 'node:path';
import { findBoxes, flattenStyle, frames, layoutWindow } from './helpers/yoga-host.mjs';

const require = createRequire(import.meta.url);
const loader = require('./helpers/gallery-loader.cjs');
const uninstall = loader.install();
const React = require('react');
const TestRenderer = require('react-test-renderer');
const { act } = React;
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const h = React.createElement;

const root = loader.root;
const gallery = (/** @type {string} */ file) =>
  require(path.join(root, 'apps/component-gallery', file));
const { View } = require('react-native');
const { MaybeScreen, MaybeScreenContainer } = require(
  path.join(root, 'packages/expo-router-qnx/src/shims/tabs-screen-fallback.js')
);
const { PANEL } = gallery('ui.js');

const TABS = ['index', 'advanced', 'documents', 'controls', 'more'];
const tabRoute = (/** @type {string} */ name) => gallery(`app/(tabs)/${name}.js`).default;
const DRAWER = ['components', 'media', 'settings', 'about'];
const drawerRoute = (/** @type {string} */ name) => gallery(`app/${name}.js`).default;

const WINDOWS = [
  { width: 586, height: 600 },
  { width: 880, height: 600 },
  { width: 1280, height: 800 },
];
const RAIL = 96;
const PAGE_PADDING = 28;

/**
 * The navigator as the gallery nests it: the drawer's scenes are stacked and
 * stay laid out (react-native-screens keeps an inactive scene attached); the
 * (tabs) scene is a rail plus the real tab fallback, one MaybeScreen per tab.
 * `state`: { drawer: names mounted, tab: focused tab, tabsMounted, drawerWidth }
 */
function Navigators({ ctl, window }) {
  const [state, setState] = React.useState(ctl.initial);
  ctl.set = (/** @type {any} */ patch) => setState((previous) => ({ ...previous, ...patch }));
  const fill = { position: 'absolute', left: 0, right: 0, top: 0, bottom: 0 };
  const scenes = [];
  if (state.tabsMounted) {
    const rail = state.bar === 'bottom' ? 0 : RAIL;
    scenes.push(
      h(
        View,
        {
          key: 'tabs',
          style: [fill, { flexDirection: state.bar === 'bottom' ? 'column-reverse' : 'row' }],
        },
        h(View, { key: 'bar', style: state.bar === 'bottom' ? { height: 64 } : { width: rail } }),
        h(
          MaybeScreenContainer,
          { enabled: true, hasTwoStates: true, style: { flex: 1 } },
          TABS.map((name) => {
            const Route = tabRoute(name);
            // Focused: 2. Others stay mounted; `attached` lays them out too.
            const active = name === state.tab ? 2 : state.attached ? 1 : 0;
            return h(
              MaybeScreen,
              { key: name, testID: `tab-${name}`, enabled: true, active, style: [fill] },
              h(View, { style: { flex: 1 } }, h(Route))
            );
          })
        )
      )
    );
  }
  for (const name of state.drawer) {
    const Route = drawerRoute(name);
    const width = state.drawerWidth ?? window.width;
    scenes.push(
      h(View, { key: name, style: [fill, { width }] }, h(View, { style: { flex: 1 } }, h(Route)))
    );
  }
  return h(View, { style: { width: window.width, height: window.height } }, scenes);
}

/** @param {{width:number,height:number}} window @param {any} initial */
function mount(window, initial) {
  const ctl = {
    initial: {
      drawer: [],
      tab: 'index',
      tabsMounted: false,
      attached: false,
      bar: 'rail',
      drawerWidth: undefined,
      ...initial,
    },
    set: (/** @type {any} */ _) => {},
  };
  /** @type {any} */
  let renderer;
  act(() => {
    renderer = TestRenderer.create(h(Navigators, { ctl, window }));
  });
  const lay = () => layoutWindow(renderer, window, act);
  return {
    lay,
    patch(/** @type {any} */ change) {
      act(() => ctl.set(change));
      return lay();
    },
    unmount() {
      act(() => renderer.unmount());
    },
  };
}

/** The page of tab `name`: the Screen inside its MaybeScreen (null when display: none). */
const tabPage = (/** @type {any} */ tree, /** @type {string} */ name) =>
  findBoxes(tree, (box) => box.props.testID === `tab-${name}`).flatMap((scene) =>
    screensOf(scene)
  )[0] ?? null;

/** Screen frames in a laid-out tree: one per mounted, displayed page. */
const screensOf = (/** @type {any} */ tree) =>
  findBoxes(
    tree,
    (box) =>
      box.props &&
      box.props.collapsable === false &&
      typeof box.props.onLayout === 'function' &&
      box.width > 0
  );

const isCard = (/** @type {any} */ box) => {
  const style = flattenStyle(box.props.style);
  return (
    style.backgroundColor === PANEL &&
    typeof style.borderRadius === 'number' &&
    style.borderRadius >= 12 &&
    box.width > 0
  );
};
const CARD_MIN = {
  index: 7,
  advanced: 1,
  documents: 0,
  controls: 0,
  more: 0,
  components: 5,
  media: 1,
  settings: 1,
  about: 1,
};

/** Every card ends inside the page's right padding; nothing leaves the page. */
function assertInside(
  /** @type {any} */ screen,
  /** @type {string} */ label,
  /** @type {string} */ name
) {
  const right = screen.x + screen.width;
  const cards = findBoxes(screen, isCard);
  assert.ok(
    cards.length >= CARD_MIN[name],
    `${label}: ${cards.length} cards found, expected at least ${CARD_MIN[name]}`
  );
  for (const card of cards) {
    assert.ok(
      card.x + card.width <= right - PAGE_PADDING + 0.5,
      `${label}: a ${card.width}px card ends at ${card.x + card.width}, past the page padding edge ${right - PAGE_PADDING} (page ${screen.x}..${right})`
    );
  }
  for (const box of findBoxes(screen, (b) => b.width > 0)) {
    assert.ok(
      box.x + box.width <= right + 0.5,
      `${label}: ${box.type} ends at ${box.x + box.width}, past the page edge ${right}`
    );
  }
}

// ---- 1. Right edges, tab pages beside a drawer destination ---------------------
for (const window of WINDOWS) {
  for (const bar of ['rail', 'bottom']) {
    for (const tab of TABS) {
      for (const drawerFirst of [true, false]) {
        const app = mount(window, {
          drawer: drawerFirst ? ['components'] : [],
          tab,
          tabsMounted: drawerFirst ? false : true,
          bar,
        });
        let tree = app.lay();
        tree = app.patch(drawerFirst ? { tabsMounted: true } : { drawer: ['components'] });
        const label = `${window.width}x${window.height} ${bar} tab=${tab} drawerFirst=${drawerFirst}`;
        const screen = tabPage(tree, tab);
        assert.ok(screen, `${label}: the ${tab} page was laid out`);
        assert.equal(
          screen.width,
          window.width - (bar === 'rail' ? RAIL : 0),
          `${label}: page width`
        );
        assertInside(screen, label, tab);
        app.unmount();
      }
    }
  }
}

// ---- 2. Drawer destinations fill the window and stay inside it -------------------
for (const window of WINDOWS) {
  for (const name of DRAWER) {
    const app = mount(window, { drawer: [name] });
    const tree = app.lay();
    const [screen] = screensOf(tree);
    assert.equal(screen.width, window.width, `${name} at ${window.width} is as wide as the window`);
    assertInside(screen, `${window.width} drawer ${name}`, name);
    app.unmount();
  }
}

// ---- 3. No page's layout depends on another page's measurement ------------------
for (const window of WINDOWS) {
  // Each tab page alone in the window: the reference layout.
  /** @type {Record<string, string[]>} */
  const baseline = {};
  for (const tab of TABS) {
    const alone = mount(window, { tab, tabsMounted: true });
    baseline[tab] = frames(tabPage(alone.lay(), tab));
    alone.unmount();
  }
  for (const focused of ['index', 'advanced', 'more']) {
    const label = `${window.width}x${window.height} focus=${focused}`;
    // All five tab pages stay laid out (attached), as react-native-screens keeps them.
    const app = mount(window, { tab: focused, tabsMounted: true, attached: true });
    let tree = app.lay();
    const same = (/** @type {string} */ what) => {
      for (const tab of TABS) {
        assert.deepEqual(
          frames(tabPage(tree, tab)),
          baseline[tab],
          `${label}: ${what} changed the ${tab} page`
        );
      }
    };
    same('the initial layout');
    // A drawer destination mounts after the tab pages have measured, then others.
    tree = app.patch({ drawer: ['components'] });
    same('mounting a drawer page');
    for (const name of ['media', 'settings', 'about']) {
      tree = app.patch({ drawer: ['components', name] });
      same(`mounting drawer ${name}`);
    }
    // The drawer pages re-measure at other widths (a resize of their scene only).
    for (const drawerWidth of [
      window.width + 160,
      Math.max(300, window.width - 120),
      window.width,
    ]) {
      tree = app.patch({ drawerWidth });
      same(`a drawer page at ${drawerWidth}px`);
    }
    app.unmount();
  }
}

uninstall();
console.log('qnx-gallery-yoga-layout: ok');
