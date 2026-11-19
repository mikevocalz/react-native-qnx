import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import path from 'node:path';
const require = createRequire(import.meta.url);
const { needsJsTabBar } = require(
  path.join(process.cwd(), 'packages/expo-router-qnx/src/tabBarModel.js')
);
/** @param {any[]} palettes */
const tabs = (palettes) =>
  palettes.map((palette) => ({
    icon: 'A',
    options: { tabBarBadgeStyle: palette },
  }));
const focused = { tabBarBadgeStyle: { backgroundColor: '#f00', color: '#fff' } };
assert.equal(
  needsJsTabBar(
    tabs([focused.tabBarBadgeStyle, { backgroundColor: '#00f', color: '#fff' }]),
    focused
  ),
  true
);
assert.equal(
  needsJsTabBar(
    tabs([focused.tabBarBadgeStyle, { backgroundColor: '#f00', color: '#000' }]),
    focused
  ),
  true
);
assert.equal(
  needsJsTabBar(
    tabs([focused.tabBarBadgeStyle, [{ backgroundColor: '#f00' }, { color: '#fff' }]]),
    focused
  ),
  false
);
assert.equal(
  needsJsTabBar(tabs([{}, { backgroundColor: '#e5484d', color: '#ffffff' }]), {}),
  false
);
console.log('qnx-router-badge-palette: ok');
