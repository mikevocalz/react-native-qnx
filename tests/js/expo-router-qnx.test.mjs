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
  tabs.map(({ key, title, icon, badge }) => ({ key, title, icon, badge })),
  [
    { key: 'a', title: 'Components', icon: 'X', badge: '3' },
    { key: 'b', title: 'Advanced', icon: '', badge: '' },
  ]
);
assert.equal(tintsFor({ tabBarActiveTintColor: '#fff' }).active, '#fff');
assert.equal(tintsFor({ tabBarStyle: [{ backgroundColor: '#111' }] }).background, '#111');

// Swipe exclusion: the drawer's edge zone starts right of the rail.
const seen = [];
const off = swipe.subscribeSwipeExclusion((value) => seen.push(value));
swipe.setSwipeExclusionLeft(96);
swipe.setSwipeExclusionLeft(96);
swipe.setSwipeExclusionLeft(-4);
off();
assert.deepEqual(seen, [96, 0]);
assert.equal(swipe.getSwipeExclusionLeft(), 0);

console.log('expo-router-qnx tests passed.');
