import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import path from 'node:path';

const root = process.cwd();
const require = createRequire(import.meta.url);
const { createExpoRouterResolveRequest } = require(
  path.join(root, 'packages/expo-router-qnx/src/resolver.js')
);
const { buildTabs, resolveTabBarLayout, tintsFor } = require(
  path.join(root, 'packages/expo-router-qnx/src/tabBarModel.js')
);
const { qnxTabsScreenOptions } = require(
  path.join(root, 'packages/expo-router-qnx/src/QnxTabs.js')
);
const swipe = require(path.join(root, 'packages/expo-router-qnx/src/swipeExclusion.js'));

// Resolver: the qnx platform swaps four native-backed packages; others do not.
const real = { type: 'sourceFile', filePath: '/real/module.js' };
const resolve = createExpoRouterResolveRequest(() => real, {
  routerContext: '/app/app-ctx.js',
});
for (const name of [
  'react-native-safe-area-context',
  'react-native-screens',
  'react-native-screens/experimental',
  'react-native-drawer-layout',
]) {
  const qnx = resolve({}, name, 'qnx');
  assert.equal(qnx.type, 'sourceFile');
  assert.match(qnx.filePath, /expo-router-qnx/);
  assert.equal(resolve({}, name, 'ios'), real, `${name} resolves normally off qnx`);
}
assert.equal(resolve({}, 'expo-router/_ctx', 'qnx').filePath, '/app/app-ctx.js');
assert.equal(resolve({}, 'expo-router/_ctx', 'android'), real);

const tabBar = createExpoRouterResolveRequest(() => ({
  type: 'sourceFile',
  filePath: '/n/expo-router/build/react-navigation/bottom-tabs/views/BottomTabBar.js',
}));
assert.match(
  tabBar({}, './BottomTabBar', 'qnx').filePath,
  /expo-router-qnx[\\/]src[\\/]BottomTabBar\.js$/
);
assert.match(tabBar({}, './BottomTabBar', 'ios').filePath, /expo-router[\\/]build/);

const winter = createExpoRouterResolveRequest(() => ({
  type: 'sourceFile',
  filePath: '/n/expo/build/winter/fetch/index.js',
}));
assert.match(winter({}, './fetch', 'qnx').filePath, /shims[\\/]expo-fetch\.js$/);

// Layout: auto follows the width, fixed values do not.
assert.equal(resolveTabBarLayout('auto', 699), 'bottom');
assert.equal(resolveTabBarLayout('auto', 700), 'side');
assert.equal(resolveTabBarLayout('auto', 500, 480), 'side');
assert.equal(resolveTabBarLayout('bottom', 1600), 'bottom');
assert.equal(resolveTabBarLayout('side', 300), 'side');
assert.equal(qnxTabsScreenOptions({ width: 1280 }).tabBarPosition, 'left');
assert.equal(qnxTabsScreenOptions({ width: 400 }).tabBarPosition, 'bottom');
assert.equal(qnxTabsScreenOptions({ width: 400, layout: 'side' }).tabBarPosition, 'left');
assert.equal(qnxTabsScreenOptions({ width: 400, headerShown: false }).headerShown, false);

// Tab model: labels, badges, hidden tabs and tints.
const state = {
  index: 0,
  routes: [
    { key: 'a', name: 'index' },
    { key: 'b', name: 'advanced' },
    { key: 'c', name: 'hidden' },
  ],
};
const descriptors = {
  a: { route: state.routes[0], options: { title: 'Components', qnxIcon: 'X', tabBarBadge: 3 } },
  b: { route: state.routes[1], options: { tabBarLabel: 'Advanced' } },
  c: { route: state.routes[2], options: { hidden: true } },
};
const tabs = buildTabs(state, descriptors);
assert.deepEqual(
  tabs.map((/** @type {any} */ { key, title, icon, badge }) => ({ key, title, icon, badge })),
  [
    { key: 'a', title: 'Components', icon: 'X', badge: '3' },
    { key: 'b', title: 'Advanced', icon: '', badge: '' },
  ]
);
assert.equal(tintsFor({ tabBarActiveTintColor: '#fff' }).active, '#fff');
assert.equal(tintsFor({ tabBarStyle: [{ backgroundColor: '#111' }] }).background, '#111');

// Swipe exclusion: the drawer's edge zone starts right of the rail.
/** @type {number[]} */
const seen = [];
const off = swipe.subscribeSwipeExclusion((/** @type {number} */ value) => seen.push(value));
swipe.setSwipeExclusionLeft(96);
swipe.setSwipeExclusionLeft(96);
swipe.setSwipeExclusionLeft(-4);
off();
assert.deepEqual(seen, [96, 0]);
assert.equal(swipe.getSwipeExclusionLeft(), 0);

// Deep imports get the same replacement.
assert.match(
  resolve({}, 'react-native-screens/lib/commonjs/index', 'qnx').filePath,
  /screens\.js$/
);
assert.equal(resolve({}, 'react-native-screens-extra', 'qnx'), real);

// The shims export every name expo-router's compiled code reads from the
// packages they replace (skipped when expo-router is not installed).
const { readdirSync, readFileSync, statSync, existsSync } = await import('node:fs');
const routerBuild = path.join(root, 'apps/component-gallery/node_modules/expo-router/build');
if (existsSync(routerBuild)) {
  /** @param {string} dir @param {string[]} out */
  const walk = (dir, out = []) => {
    for (const entry of readdirSync(dir)) {
      const full = path.join(dir, entry);
      if (statSync(full).isDirectory()) walk(full, out);
      else if (full.endsWith('.js')) out.push(full);
    }
    return out;
  };
  const source = walk(routerBuild)
    .map((file) => readFileSync(file, 'utf8'))
    .join('\n');
  const vm = await import('node:vm');
  /** @param {string} file */
  const load = (file) => {
    const mod = { exports: /** @type {any} */ ({}) };
    const stub = new Proxy({}, { get: () => () => ({ Provider: {}, Consumer: {} }) });
    vm.runInNewContext(readFileSync(path.join(root, file), 'utf8'), {
      module: mod,
      require: () => stub,
    });
    return mod.exports;
  };
  const shims = {
    safeArea: [
      load('packages/expo-router-qnx/src/shims/safe-area-context.js'),
      /_reactnativesafeareacontext\(\)\.([A-Za-z]+)/g,
    ],
    screens: [
      load('packages/expo-router-qnx/src/shims/screens.js'),
      /_reactnativescreens\(\)\.([A-Za-z]+)/g,
    ],
  };
  for (const [shim, pattern] of Object.values(shims)) {
    for (const match of source.matchAll(pattern)) {
      if (['Stack', 'Split', 'Tabs'].includes(match[1])) continue;
      assert.ok(match[1] in shim, `shim is missing ${match[1]}`);
    }
  }
}

console.log('expo-router-qnx tests passed.');
